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

#include <libnbd.h>

static bool probe_content = true;
static bool json_output = false;
static bool size_only = false;

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
    NO_CONTENT_OPTION,
    JSON_OPTION,
    SIZE_OPTION,
  };
  const char *short_options = "V";
  const struct option long_options[] = {
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "json",               no_argument,       NULL, JSON_OPTION },
    { "no-content",         no_argument,       NULL, NO_CONTENT_OPTION },
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
  char *export_name = NULL;
  char *content = NULL;
  int is_rotational, is_read_only;
  int can_cache, can_df, can_fast_zero, can_flush, can_fua,
    can_multi_conn, can_trim, can_zero;

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

    case NO_CONTENT_OPTION:
      probe_content = false;
      break;

    case SIZE_OPTION:
      size_only = true;
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
  if (json_output && size_only) {
    fprintf (stderr, "%s: you cannot use --json and --size together.\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Open the NBD side. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_set_uri_allow_local_file (nbd, true); /* Allow ?tls-psk-file. */

  if (nbd_connect_uri (nbd, argv[optind]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  size = nbd_get_size (nbd);
  if (size == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (size_only) {
    printf ("%" PRIi64 "\n", size);
    goto out;
  }

  /* Collect the rest of the information we are going to display. */
  protocol = nbd_get_protocol (nbd);
  tls_negotiated = nbd_get_tls_negotiated (nbd);
  export_name = nbd_get_export_name (nbd);
  if (nbd == NULL) {
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

  if (!json_output) {
    if (protocol)
      printf ("protocol: %s", protocol);
    if (tls_negotiated >= 0)
      printf (" %s TLS", tls_negotiated ? "with" : "without");
    printf ("\n");
    printf ("export=");
    /* Might as well use the JSON function to get an escaped string here ... */
    print_json_string (export_name);
    printf (":\n");
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

    printf ("\"exports\": [\n");
    printf ("\t{\n");

    printf ("\t\"export-name\": ");
    print_json_string (export_name);
    printf (",\n");

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

    /* Put this one at the end because of the stupid comma thing in JSON. */
    printf ("\t\"export-size\": %" PRIi64 "\n", size);

    printf ("\t}\n");
    printf ("]\n");
    printf ("}\n");
  }

 out:
  nbd_close (nbd);
  free (content);
  free (export_name);
  exit (EXIT_SUCCESS);
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
  if (nbd_pread (nbd, buf, sizeof buf, 0, 0) == -1)
    goto out;
  if (write (fd, buf, sizeof buf) == -1) {
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
