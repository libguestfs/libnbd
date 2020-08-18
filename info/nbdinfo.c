/* NBD client library in userspace
 * Copyright (C) 2020 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>

#include <libnbd.h>

static bool list_all = false;
static bool probe_content, content_flag, no_content_flag;
static bool json_output = false;
static bool size_only = false;

static struct export_list {
  size_t len;
  char **names;
  char **descs;
} export_list;

static int collect_export (void *opaque, const char *name,
                           const char *desc);
static void list_one_export (struct nbd_handle *nbd, const char *desc,
                             bool first, bool last);
static void list_all_exports (struct nbd_handle *nbd1, const char *uri);
static void print_json_string (const char *);
static char *get_content (struct nbd_handle *, int64_t size);

static void __attribute__((noreturn))
usage (FILE *fp, int exitcode)
{
  fprintf (fp,
"\n"
"Display information and metadata about NBD servers and exports:\n"
"\n"
"    nbdinfo nbd://localhost\n"
"    nbdinfo \"nbd+unix:///?socket=/tmp/unixsock\"\n"
"    nbdinfo --size nbd://example.com\n"
"    nbdinfo --json nbd://example.com\n"
"    nbdinfo --list nbd://example.com\n"
"\n"
"Please read the nbdinfo(1) manual page for full usage.\n"
"\n"
);
  exit (exitcode);
}

static void
display_version (void)
{
  printf ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
}

int
main (int argc, char *argv[])
{
  enum {
    HELP_OPTION = CHAR_MAX + 1,
    LONG_OPTIONS,
    SHORT_OPTIONS,
    CONTENT_OPTION,
    NO_CONTENT_OPTION,
    JSON_OPTION,
    SIZE_OPTION,
  };
  const char *short_options = "LV";
  const struct option long_options[] = {
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "content",            no_argument,       NULL, CONTENT_OPTION },
    { "no-content",         no_argument,       NULL, NO_CONTENT_OPTION },
    { "json",               no_argument,       NULL, JSON_OPTION },
    { "list",               no_argument,       NULL, 'L' },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "short-options",      no_argument,       NULL, SHORT_OPTIONS },
    { "size",               no_argument,       NULL, SIZE_OPTION },
    { "version",            no_argument,       NULL, 'V' },
    { NULL }
  };
  int c;
  size_t i;
  struct nbd_handle *nbd;
  int64_t size;
  const char *protocol;
  int tls_negotiated;
  char *desc;

  for (;;) {
    c = getopt_long (argc, argv, short_options, long_options, NULL);
    if (c == -1)
      break;

    switch (c) {
    case HELP_OPTION:
      usage (stdout, EXIT_SUCCESS);

    case LONG_OPTIONS:
      for (i = 0; long_options[i].name != NULL; ++i) {
        if (strcmp (long_options[i].name, "long-options") != 0 &&
            strcmp (long_options[i].name, "short-options") != 0)
          printf ("--%s\n", long_options[i].name);
      }
      exit (EXIT_SUCCESS);

    case SHORT_OPTIONS:
      for (i = 0; short_options[i]; ++i) {
        if (short_options[i] != ':' && short_options[i] != '+')
          printf ("-%c\n", short_options[i]);
      }
      exit (EXIT_SUCCESS);

    case JSON_OPTION:
      json_output = true;
      break;

    case CONTENT_OPTION:
      content_flag = true;
      break;

    case NO_CONTENT_OPTION:
      no_content_flag = true;
      break;

    case SIZE_OPTION:
      size_only = true;
      break;

    case 'L':
      list_all = true;
      break;

    case 'V':
      display_version ();
      exit (EXIT_SUCCESS);

    default:
      usage (stderr, EXIT_FAILURE);
    }
  }

  /* There must be exactly 1 parameter, the URI. */
  if (argc - optind != 1)
    usage (stderr, EXIT_FAILURE);

  /* You can combine certain options. */
  if (list_all && size_only) {
    fprintf (stderr, "%s: you cannot use %s and %s together.\n",
             argv[0], "--list", "--size");
    exit (EXIT_FAILURE);
  }
  if (content_flag && no_content_flag) {
    fprintf (stderr, "%s: you cannot use %s and %s together.\n",
             argv[0], "--content", "--no-content");
    exit (EXIT_FAILURE);
  }

  /* Work out if we should probe content. */
  probe_content = !list_all;
  if (content_flag)
    probe_content = true;
  if (no_content_flag)
    probe_content = false;

  /* Open the NBD side. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_set_uri_allow_local_file (nbd, true); /* Allow ?tls-psk-file. */

  /* If using --list then we need opt mode in the handle. */
  if (list_all)
    nbd_set_opt_mode (nbd, true);
  else if (!size_only)
    nbd_set_full_info (nbd, true);

  if (nbd_connect_uri (nbd, argv[optind]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (list_all) {
    if (nbd_opt_list (nbd, (nbd_list_callback) {
          .callback = collect_export, .user_data = &export_list}) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    /* Disconnect from the server to move the handle into a closed
     * state, in case the server serializes further connections.
     * But we can ignore errors in this case.
     */
    nbd_opt_abort (nbd);
  }

  if (size_only) {
    size = nbd_get_size (nbd);
    if (size == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }

    printf ("%" PRIi64 "\n", size);
  }
  else {
    /* Print per-connection fields. */
    protocol = nbd_get_protocol (nbd);
    tls_negotiated = nbd_get_tls_negotiated (nbd);

    if (!json_output) {
      if (protocol) {
        printf ("protocol: %s", protocol);
        if (tls_negotiated >= 0)
          printf (" %s TLS", tls_negotiated ? "with" : "without");
        printf ("\n");
      }
    }
    else {
      printf ("{\n");
      if (protocol) {
        printf ("\"protocol\": ");
        print_json_string (protocol);
        printf (",\n");
      }

      if (tls_negotiated >= 0)
        printf ("\"TLS\": %s,\n", tls_negotiated ? "true" : "false");
    }

    if (!list_all) {
      desc = nbd_get_export_description (nbd);
      list_one_export (nbd, desc, true, true);
      free (desc);
    }
    else
      list_all_exports (nbd, argv[optind]);

    if (json_output)
      printf ("}\n");
  }

  for (i = 0; i < export_list.len; i++) {
    free (export_list.names[i]);
    free (export_list.descs[i]);
  }
  free (export_list.names);
  free (export_list.descs);
  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}

static int
collect_export (void *opaque, const char *name, const char *desc)
{
  struct export_list *l = opaque;
  char **names, **descs;

  names = realloc (l->names, (l->len + 1) * sizeof name);
  descs = realloc (l->descs, (l->len + 1) * sizeof desc);
  if (!names || !descs) {
    perror ("realloc");
    exit (EXIT_FAILURE);
  }
  l->names = names;
  l->descs = descs;
  l->names[l->len] = strdup (name);
  if (!l->names[l->len]) {
    perror ("strdup");
    exit (EXIT_FAILURE);
  }
  if (*desc) {
    l->descs[l->len] = strdup (desc);
    if (!l->descs[l->len]) {
      perror ("strdup");
      exit (EXIT_FAILURE);
    }
  }
  else
    l->descs[l->len] = NULL;
  l->len++;
  return 0;
}

static void
list_one_export (struct nbd_handle *nbd, const char *desc,
                 bool first, bool last)
{
  int64_t size;
  char *export_name = NULL;
  char *content = NULL;
  int is_rotational, is_read_only;
  int can_cache, can_df, can_fast_zero, can_flush, can_fua,
    can_multi_conn, can_trim, can_zero;
  int64_t block_minimum, block_preferred, block_maximum;

  /* Collect the metadata we are going to display. */
  size = nbd_get_size (nbd);
  if (size == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Prefer the server's version of the name, if available */
  export_name = nbd_get_canonical_export_name (nbd);
  if (export_name == NULL)
    export_name = nbd_get_export_name (nbd);
  if (export_name == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  content = get_content (nbd, size);
  is_rotational = nbd_is_rotational (nbd);
  is_read_only = nbd_is_read_only (nbd);
  can_cache = nbd_can_cache (nbd);
  can_df = nbd_can_df (nbd);
  can_fast_zero = nbd_can_fast_zero (nbd);
  can_flush = nbd_can_flush (nbd);
  can_fua = nbd_can_fua (nbd);
  can_multi_conn = nbd_can_multi_conn (nbd);
  can_trim = nbd_can_trim (nbd);
  can_zero = nbd_can_zero (nbd);
  block_minimum = nbd_get_block_size (nbd, LIBNBD_SIZE_MINIMUM);
  block_preferred = nbd_get_block_size (nbd, LIBNBD_SIZE_PREFERRED);
  block_maximum = nbd_get_block_size (nbd, LIBNBD_SIZE_MAXIMUM);

  if (!json_output) {
    printf ("export=");
    /* Might as well use the JSON function to get an escaped string here ... */
    print_json_string (export_name);
    printf (":\n");
    if (desc && *desc)
      printf ("\tdescription: %s\n", desc);
    printf ("\texport-size: %" PRIi64 "\n", size);
    if (content)
      printf ("\tcontent: %s\n", content);
    if (is_rotational >= 0)
      printf ("\t%s: %s\n", "is_rotational", is_rotational ? "true" : "false");
    if (is_read_only >= 0)
      printf ("\t%s: %s\n", "is_read_only", is_read_only ? "true" : "false");
    if (can_cache >= 0)
      printf ("\t%s: %s\n", "can_cache", can_cache ? "true" : "false");
    if (can_df >= 0)
      printf ("\t%s: %s\n", "can_df", can_df ? "true" : "false");
    if (can_fast_zero >= 0)
      printf ("\t%s: %s\n", "can_fast_zero", can_fast_zero ? "true" : "false");
    if (can_flush >= 0)
      printf ("\t%s: %s\n", "can_flush", can_flush ? "true" : "false");
    if (can_fua >= 0)
      printf ("\t%s: %s\n", "can_fua", can_fua ? "true" : "false");
    if (can_multi_conn >= 0)
      printf ("\t%s: %s\n", "can_multi_conn", can_multi_conn ? "true" : "false");
    if (can_trim >= 0)
      printf ("\t%s: %s\n", "can_trim", can_trim ? "true" : "false");
    if (can_zero >= 0)
      printf ("\t%s: %s\n", "can_zero", can_zero ? "true" : "false");
    if (block_minimum > 0)
      printf ("\t%s: %" PRId64 "\n", "block_size_minimum", block_minimum);
    if (block_preferred > 0)
      printf ("\t%s: %" PRId64 "\n", "block_size_preferred", block_preferred);
    if (block_maximum > 0)
      printf ("\t%s: %" PRId64 "\n", "block_size_maximum", block_maximum);
  }
  else {
    if (first)
      printf ("\"exports\": [\n");
    printf ("\t{\n");

    printf ("\t\"export-name\": ");
    print_json_string (export_name);
    printf (",\n");

    if (desc && *desc) {
      printf ("\t\"description\": ");
      print_json_string (desc);
      printf (",\n");
    }

    if (content) {
      printf ("\t\"content\": ");
      print_json_string (content);
      printf (",\n");
    }

    if (is_rotational >= 0)
      printf ("\t\"%s\": %s,\n",
              "is_rotational", is_rotational ? "true" : "false");
    if (is_read_only >= 0)
      printf ("\t\"%s\": %s,\n",
              "is_read_only", is_read_only ? "true" : "false");
    if (can_cache >= 0)
      printf ("\t\"%s\": %s,\n",
              "can_cache", can_cache ? "true" : "false");
    if (can_df >= 0)
      printf ("\t\"%s\": %s,\n",
              "can_df", can_df ? "true" : "false");
    if (can_fast_zero >= 0)
      printf ("\t\"%s\": %s,\n",
              "can_fast_zero", can_fast_zero ? "true" : "false");
    if (can_flush >= 0)
      printf ("\t\"%s\": %s,\n",
              "can_flush", can_flush ? "true" : "false");
    if (can_fua >= 0)
      printf ("\t\"%s\": %s,\n",
              "can_fua", can_fua ? "true" : "false");
    if (can_multi_conn >= 0)
      printf ("\t\"%s\": %s,\n",
              "can_multi_conn", can_multi_conn ? "true" : "false");
    if (can_trim >= 0)
      printf ("\t\"%s\": %s,\n",
              "can_trim", can_trim ? "true" : "false");
    if (can_zero >= 0)
      printf ("\t\"%s\": %s,\n",
              "can_zero", can_zero ? "true" : "false");

    if (block_minimum > 0)
      printf ("\t\"%s\": %" PRId64 ",\n", "block_size_minimum", block_minimum);
    if (block_preferred > 0)
      printf ("\t\"%s\": %" PRId64 ",\n", "block_size_preferred",
              block_preferred);
    if (block_maximum > 0)
      printf ("\t\"%s\": %" PRId64 ",\n", "block_size_maximum", block_maximum);

    /* Put this one at the end because of the stupid comma thing in JSON. */
    printf ("\t\"export-size\": %" PRIi64 "\n", size);

    if (last)
      printf ("\t} ]\n");
    else
      printf ("\t},\n");
  }

  free (content);
  free (export_name);
}

/* XXX Inefficient and hacky.  See TODO for a suggestion on how to
 * improve this.
 */
static void
list_all_exports (struct nbd_handle *nbd1, const char *uri)
{
  size_t i;

  if (export_list.len == 0 && json_output)
    printf ("\t\"exports\": []\n");

  for (i = 0; i < export_list.len; ++i) {
    const char *name;
    struct nbd_handle *nbd2;

    /* Connect to the original URI, but using opt mode to alter the export. */
    name = export_list.names[i];
    nbd2 = nbd_create ();
    if (nbd2 == NULL) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    nbd_set_uri_allow_local_file (nbd2, true); /* Allow ?tls-psk-file. */
    nbd_set_opt_mode (nbd2, true);

    if (nbd_connect_uri (nbd2, uri) == -1 ||
        nbd_set_export_name (nbd2, name) == -1 ||
        nbd_opt_go (nbd2) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }

    /* List the metadata of this export. */
    list_one_export (nbd2, export_list.descs[i], i == 0,
                     i + 1 == export_list.len);

    nbd_close (nbd2);
  }
}

static void
print_json_string (const char *s)
{
  putc ('"', stdout);
  for (; *s; s++) {
    switch (*s) {
    case '\\':
    case '"':
      putc ('\\', stdout);
      putc (*s, stdout);
      break;
    default:
      if (*s < ' ')
        printf ("\\u%04x", *s);
      else
        putc (*s, stdout);
    }
  }
  putc ('"', stdout);
}

/* Run the file(1) command on the first part of the export and save
 * the output.
 *
 * If file(1) doesn't work just return NULL because this is
 * best-effort.  This function will exit with an error on things which
 * shouldn't fail, such as out of memory or creating local files.
 */
static char *
get_content (struct nbd_handle *nbd, int64_t size)
{
  static char buf[8192];
  char template[] = "/tmp/XXXXXX";
  int fd = -1;
  FILE *fp = NULL;
  char *cmd = NULL;
  char *ret = NULL;
  ssize_t r;
  size_t len = 0;

  if (!probe_content)
    return NULL;

  /* Write the first part of the NBD export to a temporary file. */
  fd = mkstemp (template);
  if (fd == -1) {
    perror ("mkstemp");
    exit (EXIT_FAILURE);
  }
  if (size > sizeof buf)
    size = sizeof buf;
  if (nbd_pread (nbd, buf, size, 0, 0) == -1)
    goto out;
  if (write (fd, buf, size) == -1) {
    perror ("write");
    exit (EXIT_FAILURE);
  }
  close (fd);
  fd = -1;

  /* Run the file command. */
  if (asprintf (&cmd, "file -b %s", template) == -1) {
    perror ("asprintf");
    exit (EXIT_FAILURE);
  }

  fp = popen (cmd, "r");
  if (fp == NULL)
    goto out;
  r = getline (&ret, &len, fp);
  if (r == -1)
    goto out;

  /* Remove trailing \n. */
  if (r > 0 && ret[r-1] == '\n')
    ret[r-1] = '\0';

 out:
  if (fd >= 0)
    close (fd);
  unlink (template);
  if (fp)
    pclose (fp);
  free (cmd);
  return ret;                   /* caller frees */
}
