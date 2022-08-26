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

#include <libnbd.h>

#include "vector.h"

#include "nbdinfo.h"

struct export {
  char *name;
  char *desc;
};
DEFINE_VECTOR_TYPE (exports, struct export)
static exports export_list = empty_vector;

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

void
collect_exports (void)
{
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

void
free_exports (void)
{
  size_t i;

  for (i = 0; i < export_list.len; ++i) {
    free (export_list.ptr[i].name);
    free (export_list.ptr[i].desc);
  }
  free (export_list.ptr);
}

bool
list_all_exports (void)
{
  size_t i;
  bool list_okay = true;

  if (export_list.len == 0 && json_output)
    fprintf (fp, "\"exports\": []\n");

  for (i = 0; i < export_list.len; ++i) {
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
      nbd_set_request_meta_context (nbd2, false);
      nbd_set_full_info (nbd2, true);

      do_connect (nbd2);
      if (nbd_set_export_name (nbd2, name) == -1) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
    }
    else { /* ! probe_content */
      if (nbd_set_export_name (nbd, name) == -1) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
      nbd2 = nbd;
    }

    /* List the metadata of this export. */
    if (!show_one_export (nbd2, export_list.ptr[i].desc, i == 0,
                          i + 1 == export_list.len))
      list_okay = false;

    if (probe_content) {
      nbd_shutdown (nbd2, 0);
      nbd_close (nbd2);
    }
  }
  return list_okay;
}
