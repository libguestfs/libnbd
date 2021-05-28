/* NBD client library in userspace
 * Copyright (C) 2020-2021 Red Hat Inc.
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
#include <assert.h>

#include <libnbd.h>

#include "minmax.h"
#include "vector.h"
#include "version.h"

DEFINE_VECTOR_TYPE (string_vector, char *)

static const char *progname;
static FILE *fp;
static bool list_all = false;
static bool probe_content, content_flag, no_content_flag;
static bool json_output = false;
static const char *map = NULL;
static bool size_only = false;

struct export {
  char *name;
  char *desc;
};
DEFINE_VECTOR_TYPE (exports, struct export)
static exports export_list = empty_vector;

DEFINE_VECTOR_TYPE (uint32_vector, uint32_t)

static int collect_context (void *opaque, const char *name);
static int collect_export (void *opaque, const char *name,
                           const char *desc);
static bool list_one_export (struct nbd_handle *nbd, const char *desc,
                             bool first, bool last);
static bool list_all_exports (struct nbd_handle *nbd1, const char *uri);
static void print_json_string (const char *);
static char *get_content (struct nbd_handle *, int64_t size);
static int extent_callback (void *user_data, const char *metacontext,
                            uint64_t offset,
                            uint32_t *entries, size_t nr_entries,
                            int *error);
static void print_extents (uint32_vector *entries);

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
"    nbdinfo --map nbd://example.com\n"
"    nbdinfo --json nbd://example.com\n"
"    nbdinfo --list nbd://example.com\n"
"\n"
"Please read the nbdinfo(1) manual page for full usage.\n"
"\n"
);
  exit (exitcode);
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
    MAP_OPTION,
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
    { "map",                optional_argument, NULL, MAP_OPTION },
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
  char *output = NULL;
  size_t output_len = 0;
  bool list_okay = true;

  progname = argv[0];

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

    case MAP_OPTION:
      map = optarg ? optarg : "base:allocation";
      break;

    case SIZE_OPTION:
      size_only = true;
      break;

    case 'L':
      list_all = true;
      break;

    case 'V':
      display_version ("nbdinfo");
      exit (EXIT_SUCCESS);

    default:
      usage (stderr, EXIT_FAILURE);
    }
  }

  /* There must be exactly 1 parameter, the URI. */
  if (argc - optind != 1)
    usage (stderr, EXIT_FAILURE);

  /* You cannot combine certain options. */
  if (!!list_all + !!map + !!size_only > 1) {
    fprintf (stderr,
             "%s: you cannot use --list, --map and --size together.\n",
             progname);
    exit (EXIT_FAILURE);
  }
  if (content_flag && no_content_flag) {
    fprintf (stderr, "%s: you cannot use %s and %s together.\n",
             progname, "--content", "--no-content");
    exit (EXIT_FAILURE);
  }

  /* Work out if we should probe content. */
  probe_content = !list_all;
  if (content_flag)
    probe_content = true;
  if (no_content_flag)
    probe_content = false;
  if (map)
    probe_content = false;

  /* Try to write output atomically.  We spool output into a
   * memstream, pointed to by fp, and write it all at once at the end.
   * On error nothing should be printed on stdout.
   */
  fp = open_memstream (&output, &output_len);
  if (fp == NULL) {
    fprintf (stderr, "%s: ", progname);
    perror ("open_memstream");
    exit (EXIT_FAILURE);
  }

  /* Open the NBD side. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_set_uri_allow_local_file (nbd, true); /* Allow ?tls-psk-file. */

  /* Set optional modes in the handle. */
  if (!map && !size_only) {
    nbd_set_opt_mode (nbd, true);
    nbd_set_full_info (nbd, true);
  }
  if (map)
    nbd_add_meta_context (nbd, map);

  /* Connect to the server. */
  if (nbd_connect_uri (nbd, argv[optind]) == -1) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (list_all) {               /* --list */
    if (nbd_opt_list (nbd,
                      (nbd_list_callback) {.callback = collect_export}) == -1) {
      fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    if (probe_content)
      /* Disconnect from the server to move the handle into a closed
       * state, in case the server serializes further connections.
       * But we can ignore errors in this case.
       */
      nbd_opt_abort (nbd);
  }

  if (size_only) {              /* --size (!list_all) */
    size = nbd_get_size (nbd);
    if (size == -1) {
      fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
      exit (EXIT_FAILURE);
    }

    fprintf (fp, "%" PRIi64 "\n", size);
  }
  else if (map) {               /* --map (!list_all) */
    uint32_vector entries = empty_vector;
    uint64_t offset, align, max_len;
    size_t prev_entries_size;

    /* Did we get the requested map? */
    if (!nbd_can_meta_context (nbd, map)) {
      fprintf (stderr,
               "%s: --map: server does not support metadata context \"%s\"\n",
               progname, map);
      exit (EXIT_FAILURE);
    }
    align = nbd_get_block_size (nbd, LIBNBD_SIZE_MINIMUM) ?: 512;
    max_len = UINT32_MAX - align + 1;

    size = nbd_get_size (nbd);
    if (size == -1) {
      fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
      exit (EXIT_FAILURE);
    }

    for (offset = 0; offset < size;) {
      prev_entries_size = entries.size;
      if (nbd_block_status (nbd, MIN (size - offset, max_len), offset,
                            (nbd_extent_callback) { .callback = extent_callback,
                                                    .user_data = &entries },
                            0) == -1) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
      /* We expect extent_callback to add at least one extent to entries. */
      if (prev_entries_size == entries.size) {
        fprintf (stderr, "%s: --map: server did not return any extents\n",
                 progname);
        exit (EXIT_FAILURE);
      }
      assert ((entries.size & 1) == 0);
      for (i = prev_entries_size; i < entries.size; i += 2)
        offset += entries.ptr[i];
    }

    print_extents (&entries);
    free (entries.ptr);
  }
  else {                        /* not --size or --map */
    /* Print per-connection fields. */
    protocol = nbd_get_protocol (nbd);
    tls_negotiated = nbd_get_tls_negotiated (nbd);

    if (!json_output) {
      if (protocol) {
        fprintf (fp, "protocol: %s", protocol);
        if (tls_negotiated >= 0)
          fprintf (fp, " %s TLS", tls_negotiated ? "with" : "without");
        fprintf (fp, "\n");
      }
    }
    else {
      fprintf (fp, "{\n");
      if (protocol) {
        fprintf (fp, "\"protocol\": ");
        print_json_string (protocol);
        fprintf (fp, ",\n");
      }

      if (tls_negotiated >= 0)
        fprintf (fp, "\"TLS\": %s,\n", tls_negotiated ? "true" : "false");
    }

    if (!list_all)
      list_okay = list_one_export (nbd, NULL, true, true);
    else
      list_okay = list_all_exports (nbd, argv[optind]);

    if (json_output)
      fprintf (fp, "}\n");
  }

  for (i = 0; i < export_list.size; ++i) {
    free (export_list.ptr[i].name);
    free (export_list.ptr[i].desc);
  }
  free (export_list.ptr);
  nbd_opt_abort (nbd);
  nbd_shutdown (nbd, 0);
  nbd_close (nbd);

  /* Close the output stream and copy it to the real stdout. */
  if (fclose (fp) == EOF) {
    fprintf (stderr, "%s: ", progname);
    perror ("fclose");
    exit (EXIT_FAILURE);
  }
  if (puts (output) == EOF) {
    fprintf (stderr, "%s: ", progname);
    perror ("puts");
    exit (EXIT_FAILURE);
  }

  exit (list_okay ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int
collect_context (void *opaque, const char *name)
{
  string_vector *contexts = opaque;
  char *copy;

  copy = strdup (name);
  if (copy == NULL || string_vector_append (contexts, copy) == -1) {
    perror ("malloc");
    exit (EXIT_FAILURE);
  }
  return 0;
}

static int
collect_export (void *opaque, const char *name, const char *desc)
{
  struct export e;

  e.name = strdup (name);
  e.desc = strdup (desc);
  if (e.name == NULL || e.desc == NULL ||
      exports_append (&export_list, e) == -1) {
    perror ("malloc");
    exit (EXIT_FAILURE);
  }

  return 0;
}

static bool
list_one_export (struct nbd_handle *nbd, const char *desc,
                 bool first, bool last)
{
  int64_t i, size;
  char *export_name = NULL;
  char *export_desc = NULL;
  char *content = NULL;
  char *uri = NULL;
  int is_rotational, is_read_only;
  int can_cache, can_df, can_fast_zero, can_flush, can_fua,
    can_multi_conn, can_trim, can_zero;
  int64_t block_minimum, block_preferred, block_maximum;
  string_vector contexts = empty_vector;
  bool show_context = false;

  /* Collect the metadata we are going to display. If opt_info works,
   * great; if not (such as for legacy newstyle), we have to go all
   * the way with opt_go.  If we fail to connect (such as a server
   * advertising something it later refuses to serve), return rather
   * than exit, to allow output on the rest of the list.
   */
  if (nbd_aio_is_negotiating (nbd) &&
      nbd_opt_info (nbd) == -1 &&
      nbd_opt_go (nbd) == -1) {
    fprintf (stderr, "%s: %s: %s\n", progname, nbd_get_export_name (nbd),
             nbd_get_error ());
    return false;
  }
  size = nbd_get_size (nbd);
  if (size == -1) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  uri = nbd_get_uri (nbd);

  /* Prefer the server's version of the name, if available */
  export_name = nbd_get_canonical_export_name (nbd);
  if (export_name == NULL)
    export_name = nbd_get_export_name (nbd);
  if (export_name == NULL) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  /* Get description if list didn't already give us one */
  if (!desc)
    desc = export_desc = nbd_get_export_description (nbd);
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
  if (nbd_opt_list_meta_context (nbd,
             (nbd_context_callback) {.callback = collect_context,
                                       .user_data = &contexts}) != -1)
    show_context = true;

  /* Get content last, as it moves the connection out of negotiating */
  content = get_content (nbd, size);

  if (!json_output) {
    fprintf (fp, "export=");
    /* Might as well use the JSON function to get an escaped string here ... */
    print_json_string (export_name);
    fprintf (fp, ":\n");
    if (desc && *desc)
      fprintf (fp, "\tdescription: %s\n", desc);
    fprintf (fp, "\texport-size: %" PRIi64 "\n", size);
    if (content)
      fprintf (fp, "\tcontent: %s\n", content);
    if (uri)
      fprintf (fp, "\turi: %s\n", uri);
    if (show_context) {
      fprintf (fp, "\tcontexts:\n");
      for (i = 0; i < contexts.size; ++i)
        fprintf (fp, "\t\t%s\n", contexts.ptr[i]);
    }
    if (is_rotational >= 0)
      fprintf (fp, "\t%s: %s\n", "is_rotational",
               is_rotational ? "true" : "false");
    if (is_read_only >= 0)
      fprintf (fp, "\t%s: %s\n", "is_read_only",
               is_read_only ? "true" : "false");
    if (can_cache >= 0)
      fprintf (fp, "\t%s: %s\n", "can_cache", can_cache ? "true" : "false");
    if (can_df >= 0)
      fprintf (fp, "\t%s: %s\n", "can_df", can_df ? "true" : "false");
    if (can_fast_zero >= 0)
      fprintf (fp, "\t%s: %s\n", "can_fast_zero",
               can_fast_zero ? "true" : "false");
    if (can_flush >= 0)
      fprintf (fp, "\t%s: %s\n", "can_flush", can_flush ? "true" : "false");
    if (can_fua >= 0)
      fprintf (fp, "\t%s: %s\n", "can_fua", can_fua ? "true" : "false");
    if (can_multi_conn >= 0)
      fprintf (fp, "\t%s: %s\n", "can_multi_conn",
               can_multi_conn ? "true" : "false");
    if (can_trim >= 0)
      fprintf (fp, "\t%s: %s\n", "can_trim", can_trim ? "true" : "false");
    if (can_zero >= 0)
      fprintf (fp, "\t%s: %s\n", "can_zero", can_zero ? "true" : "false");
    if (block_minimum > 0)
      fprintf (fp, "\t%s: %" PRId64 "\n", "block_size_minimum", block_minimum);
    if (block_preferred > 0)
      fprintf (fp, "\t%s: %" PRId64 "\n", "block_size_preferred",
               block_preferred);
    if (block_maximum > 0)
      fprintf (fp, "\t%s: %" PRId64 "\n", "block_size_maximum", block_maximum);
  }
  else {
    if (first)
      fprintf (fp, "\"exports\": [\n");
    fprintf (fp, "\t{\n");

    fprintf (fp, "\t\"export-name\": ");
    print_json_string (export_name);
    fprintf (fp, ",\n");

    if (desc && *desc) {
      fprintf (fp, "\t\"description\": ");
      print_json_string (desc);
      fprintf (fp, ",\n");
    }

    if (content) {
      fprintf (fp, "\t\"content\": ");
      print_json_string (content);
      fprintf (fp, ",\n");
    }

    if (uri) {
      fprintf (fp, "\t\"uri\": ");
      print_json_string (uri);
      fprintf (fp, ",\n");
    }

    if (show_context) {
      fprintf (fp, "\t\"contexts\": [\n");
      for (i = 0; i < contexts.size; ++i) {
        fprintf (fp, "\t\t");
        print_json_string (contexts.ptr[i]);
        if (i+1 != contexts.size)
          fputc (',', fp);
        fputc ('\n', fp);
      }
      fprintf (fp, "\t],\n");
    }

    if (is_rotational >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "is_rotational", is_rotational ? "true" : "false");
    if (is_read_only >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "is_read_only", is_read_only ? "true" : "false");
    if (can_cache >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "can_cache", can_cache ? "true" : "false");
    if (can_df >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "can_df", can_df ? "true" : "false");
    if (can_fast_zero >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "can_fast_zero", can_fast_zero ? "true" : "false");
    if (can_flush >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "can_flush", can_flush ? "true" : "false");
    if (can_fua >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "can_fua", can_fua ? "true" : "false");
    if (can_multi_conn >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "can_multi_conn", can_multi_conn ? "true" : "false");
    if (can_trim >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "can_trim", can_trim ? "true" : "false");
    if (can_zero >= 0)
      fprintf (fp, "\t\"%s\": %s,\n",
              "can_zero", can_zero ? "true" : "false");

    if (block_minimum > 0)
      fprintf (fp, "\t\"%s\": %" PRId64 ",\n", "block_size_minimum",
               block_minimum);
    if (block_preferred > 0)
      fprintf (fp, "\t\"%s\": %" PRId64 ",\n", "block_size_preferred",
              block_preferred);
    if (block_maximum > 0)
      fprintf (fp, "\t\"%s\": %" PRId64 ",\n", "block_size_maximum",
               block_maximum);

    /* Put this one at the end because of the stupid comma thing in JSON. */
    fprintf (fp, "\t\"export-size\": %" PRIi64 "\n", size);

    if (last)
      fprintf (fp, "\t} ]\n");
    else
      fprintf (fp, "\t},\n");
  }

  string_vector_iter (&contexts, (void *) free);
  free (contexts.ptr);
  free (content);
  free (export_name);
  free (export_desc);
  free (uri);
  return true;
}

static bool
list_all_exports (struct nbd_handle *nbd1, const char *uri)
{
  size_t i;
  bool list_okay = true;

  if (export_list.size == 0 && json_output)
    fprintf (fp, "\"exports\": []\n");

  for (i = 0; i < export_list.size; ++i) {
    const char *name = export_list.ptr[i].name;
    struct nbd_handle *nbd2;

    if (probe_content) {
      /* Connect to the original URI, but using opt mode to alter the export. */
      nbd2 = nbd_create ();
      if (nbd2 == NULL) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
      nbd_set_uri_allow_local_file (nbd2, true); /* Allow ?tls-psk-file. */
      nbd_set_opt_mode (nbd2, true);
      nbd_set_full_info (nbd2, true);

      if (nbd_connect_uri (nbd2, uri) == -1 ||
          nbd_set_export_name (nbd2, name) == -1) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
    }
    else { /* ! probe_content */
      if (nbd_set_export_name (nbd1, name) == -1) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
      nbd2 = nbd1;
    }

    /* List the metadata of this export. */
    if (!list_one_export (nbd2, export_list.ptr[i].desc, i == 0,
                          i + 1 == export_list.size))
      list_okay = false;

    if (probe_content) {
      nbd_shutdown (nbd2, 0);
      nbd_close (nbd2);
    }
  }
  return list_okay;
}

static void
print_json_string (const char *str)
{
  const unsigned char *s = (const unsigned char *)str;
  fputc ('"', fp);
  for (; *s; s++) {
    switch (*s) {
    case '\\':
    case '"':
      fputc ('\\', fp);
      fputc (*s, fp);
      break;
    default:
      if (*s < ' ')
        fprintf (fp, "\\u%04x", *s);
      else
        fputc (*s, fp);
    }
  }
  fputc ('"', fp);
}

/* Run the file(1) command on the first part of the export and save
 * the output.
 *
 * If file(1) doesn't work just return NULL because this is
 * best-effort.  This function will exit with an error on things which
 * shouldn't fail, such as out of memory or creating local files.
 *
 * Must be called late, and only once per connection, as this kicks
 * the connection from negotiating to ready.
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

  if (nbd_aio_is_negotiating (nbd) &&
      nbd_opt_go (nbd) == -1) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Write the first part of the NBD export to a temporary file. */
  fd = mkstemp (template);
  if (fd == -1) {
    perror ("mkstemp");
    exit (EXIT_FAILURE);
  }
  if (size > sizeof buf)
    size = sizeof buf;
  if (size && nbd_pread (nbd, buf, size, 0, 0) == -1)
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

/* Callback handling --map. */
static void print_one_extent (uint64_t offset, uint64_t len, uint32_t type);
static char *extent_description (const char *metacontext, uint32_t type);

static int
extent_callback (void *user_data, const char *metacontext,
                 uint64_t offset,
                 uint32_t *entries, size_t nr_entries,
                 int *error)
{
  uint32_vector *list = user_data;
  size_t i;

  if (strcmp (metacontext, map) != 0)
    return 0;

  /* Just append the entries we got to the list.  They are printed in
   * print_extents below.
   */
  for (i = 0; i < nr_entries; ++i) {
    if (uint32_vector_append (list, entries[i]) == -1) {
      perror ("realloc");
      exit (EXIT_FAILURE);
    }
  }
  return 0;
}

static void
print_extents (uint32_vector *entries)
{
  size_t i, j;
  uint64_t offset = 0;          /* end of last extent printed + 1 */
  size_t last = 0;              /* last entry printed + 2 */

  if (json_output) fprintf (fp, "[\n");

  for (i = 0; i < entries->size; i += 2) {
    uint32_t type = entries->ptr[last+1];

    /* If we're coalescing and the current type is different from the
     * previous one then we should print everything up to this entry.
     */
    if (last != i && entries->ptr[i+1] != type) {
      uint64_t len;

      /* Calculate the length of the coalesced extent. */
      for (j = last, len = 0; j < i; j += 2)
        len += entries->ptr[j];
      print_one_extent (offset, len, type);
      offset += len;
      last = i;
    }
  }

  /* Print the last extent if there is one. */
  if (last != i) {
    uint32_t type = entries->ptr[last+1];
    uint64_t len;

    for (j = last, len = 0; j < i; j += 2)
      len += entries->ptr[j];
    print_one_extent (offset, len, type);
  }

  if (json_output) fprintf (fp, "\n]\n");
}

static void
print_one_extent (uint64_t offset, uint64_t len, uint32_t type)
{
  static bool comma = false;
  char *descr = extent_description (map, type);

  if (!json_output) {
    fprintf (fp, "%10" PRIu64 "  "
             "%10" PRIu64 "  "
             "%3" PRIu32,
             offset, len, type);
    if (descr)
      fprintf (fp, "  %s", descr);
    fprintf (fp, "\n");
  }
  else {
    if (comma)
      fprintf (fp, ",\n");

    fprintf (fp, "{ \"offset\": %" PRIu64 ", "
             "\"length\": %" PRIu64 ", "
             "\"type\": %" PRIu32,
             offset, len, type);
    if (descr) {
      fprintf (fp, ", \"description\": ");
      print_json_string (descr);
    }
    fprintf (fp, "}");
    comma = true;
  }

  free (descr);
}

static char *
extent_description (const char *metacontext, uint32_t type)
{
  char *ret;

  if (strcmp (metacontext, "base:allocation") == 0) {
    switch (type) {
    case 0: return strdup ("allocated");
    case 1: return strdup ("hole");
    case 2: return strdup ("zero");
    case 3: return strdup ("hole,zero");
    }
  }
  else if (strncmp (metacontext, "qemu:dirty-bitmap:", 18) == 0) {
    switch (type) {
    case 0: return strdup ("clean");
    case 1: return strdup ("dirty");
    }
  }
  else if (strcmp (metacontext, "qemu:allocation-depth") == 0) {
    switch (type) {
    case 0: return strdup ("unallocated");
    case 1: return strdup ("local");
    default:
      if (asprintf (&ret, "backing depth %u", type) == -1) {
        perror ("asprintf");
        exit (EXIT_FAILURE);
      }
      return ret;
    }
  }

  return NULL;   /* Don't know - description field will be omitted. */
}
