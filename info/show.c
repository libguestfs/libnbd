/* NBD client library in userspace
 * Copyright (C) 2020-2022 Red Hat Inc.
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

#include <libnbd.h>

#include "ansi-colours.h"
#include "human-size.h"
#include "string-vector.h"

#include "nbdinfo.h"

static void show_boolean (const char *name, bool cond);
static int collect_context (void *opaque, const char *name);
static char *get_content (struct nbd_handle *, int64_t size);

/* NB: Don't use global nbd handle since this can be called indirectly
 * from list_all_exports with a newly created handle.
 */
bool
show_one_export (struct nbd_handle *nbd, const char *desc,
                 bool first, bool last)
{
  int64_t i, size;
  char size_str[HUMAN_SIZE_LONGEST];
  bool human_size_flag;
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
  nbd_set_request_meta_context (nbd, false);
  if (nbd_aio_is_negotiating (nbd) &&
      nbd_opt_info (nbd) == -1 &&
      nbd_opt_go (nbd) == -1) {
    fprintf (stderr, "%s: %s", progname, nbd_get_error ());

    char *e = nbd_get_export_name (nbd);
    if (e) {
      if (e[0] == '\0')
        fprintf (stderr, " for the default export");
      else
        fprintf (stderr, " for export: %s", e);
    }
    free (e);
    fprintf (stderr, "\n");

    if (!list_all)
      fprintf (stderr, "%s: suggestion: "
               "to list all exports on the server, use --list\n",
               progname);

    return false;
  }
  size = nbd_get_size (nbd);
  if (size == -1) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  human_size (size_str, size, &human_size_flag);

  if (uri_is_meaingful ())
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
    ansi_colour (ANSI_FG_BOLD_BLACK, fp);
    fprintf (fp, "export=");
    /* Might as well use the JSON function to get an escaped string here ... */
    print_json_string (export_name);
    fprintf (fp, ":\n");
    if (desc && *desc)
      fprintf (fp, "\tdescription: %s\n", desc);
    if (human_size_flag)
      fprintf (fp, "\texport-size: %" PRIi64 " (%s)\n", size, size_str);
    else
      fprintf (fp, "\texport-size: %" PRIi64 "\n", size);
    if (content)
      fprintf (fp, "\tcontent: %s\n", content);
    if (uri)
      fprintf (fp, "\turi: %s\n", uri);
    ansi_restore (fp);
    ansi_colour (ANSI_FG_GREY, fp);
    if (show_context) {
      fprintf (fp, "\tcontexts:\n");
      for (i = 0; i < contexts.len; ++i)
        fprintf (fp, "\t\t%s\n", contexts.ptr[i]);
    }
    if (is_rotational >= 0)
      fprintf (fp, "\t%s: %s\n", "is_rotational",
               is_rotational ? "true" : "false");
    ansi_restore (fp);
    if (is_read_only >= 0)
      fprintf (fp, "\t%s: %s\n", "is_read_only",
               is_read_only ? "true" : "false");
    if (can_cache >= 0)
      show_boolean ("can_cache", can_cache);
    if (can_df >= 0)
      show_boolean ("can_df", can_df);
    if (can_fast_zero >= 0)
      show_boolean ("can_fast_zero", can_fast_zero);
    if (can_flush >= 0)
      show_boolean ("can_flush", can_flush);
    if (can_fua >= 0)
      show_boolean ("can_fua", can_fua);
    if (can_multi_conn >= 0)
      show_boolean ("can_multi_conn", can_multi_conn);
    if (can_trim >= 0)
      show_boolean ("can_trim", can_trim);
    if (can_zero >= 0)
      show_boolean ("can_zero", can_zero);
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
      for (i = 0; i < contexts.len; ++i) {
        fprintf (fp, "\t\t");
        print_json_string (contexts.ptr[i]);
        if (i+1 != contexts.len)
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
    fprintf (fp, "\t\"export-size\": %" PRIi64 ",\n", size);
    fprintf (fp, "\t\"export-size-str\": \"%s\"\n", size_str);

    if (last)
      fprintf (fp, "\t} ]\n");
    else
      fprintf (fp, "\t},\n");
  }

  string_vector_empty (&contexts);
  free (content);
  free (export_name);
  free (export_desc);
  free (uri);
  return true;
}

/* Used for displaying booleans in non-JSON output. */
static void
show_boolean (const char *name, bool cond)
{
  if (cond)
    ansi_colour (ANSI_FG_GREEN, fp);
  else
    ansi_colour (ANSI_FG_RED, fp);
  fprintf (fp, "\t%s: %s\n", name, cond ? "true" : "false");
  ansi_restore (fp);
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
