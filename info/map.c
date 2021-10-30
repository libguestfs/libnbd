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
#include <errno.h>
#include <assert.h>

#include <libnbd.h>

#include "minmax.h"
#include "vector.h"

#include "nbdinfo.h"

DEFINE_VECTOR_TYPE (uint32_vector, uint32_t)

static void print_extents (uint32_vector *entries);
static void print_totals (uint32_vector *entries, int64_t size);
static int extent_callback (void *user_data, const char *metacontext,
                            uint64_t offset,
                            uint32_t *entries, size_t nr_entries,
                            int *error);

void
do_map (void)
{
  size_t i;
  int64_t size;
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
    prev_entries_size = entries.len;
    if (nbd_block_status (nbd, MIN (size - offset, max_len), offset,
                          (nbd_extent_callback) {
                            .callback = extent_callback,
                            .user_data = &entries },
                          0) == -1) {
      fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    /* We expect extent_callback to add at least one extent to entries. */
    if (prev_entries_size == entries.len) {
      fprintf (stderr, "%s: --map: server did not return any extents\n",
               progname);
      exit (EXIT_FAILURE);
    }
    assert ((entries.len & 1) == 0);
    for (i = prev_entries_size; i < entries.len; i += 2)
      offset += entries.ptr[i];
  }

  if (!totals)
    print_extents (&entries);
  else
    print_totals (&entries, size);
  free (entries.ptr);
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

  for (i = 0; i < entries->len; i += 2) {
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

/* --map --totals suboption */
static void
print_totals (uint32_vector *entries, int64_t size)
{
  uint32_t type;
  bool comma = false;

  /* This is necessary to avoid a divide by zero below, but if the
   * size of the export is zero then we know we will not print any
   * information below so return quickly.
   */
  if (size == 0) {
    if (json_output) fprintf (fp, "[]\n");
    return;
  }

  if (json_output) fprintf (fp, "[\n");

  /* In the outer loop assume we have already printed all entries with
   * entry type < type.  Count all instances of type and at the same
   * time find the next type that exists > type.
   */
  type = 0;
  for (;;) {
    uint64_t next_type = (uint64_t)UINT32_MAX + 1;
    uint64_t c = 0;
    size_t i;

    for (i = 0; i < entries->len; i += 2) {
      uint32_t t = entries->ptr[i+1];

      if (t == type)
        c += entries->ptr[i];
      else if (type < t && t < next_type)
        next_type = t;
    }

    if (c > 0) {
      char *descr = extent_description (map, type);
      double percent = 100.0 * c / size;

      if (!json_output) {
        fprintf (fp, "%10" PRIu64 " %5.1f%% %3" PRIu32,
                 c, percent, type);
        if (descr)
          fprintf (fp, " %s", descr);
        fprintf (fp, "\n");
      }
      else {
        if (comma)
          fprintf (fp, ",\n");

        fprintf (fp,
                 "{ \"size\": %" PRIu64 ", "
                 "\"percent\": %g, "
                 "\"type\": %" PRIu32,
                 c, percent, type);
        if (descr) {
          fprintf (fp, ", \"description\": ");
          print_json_string (descr);
        }
        fprintf (fp, " }");
        comma = true;
      }

      free (descr);
    }

    if (next_type == (uint64_t)UINT32_MAX + 1)
      break;
    type = next_type;
  }

  if (json_output) fprintf (fp, "\n]\n");
}

static char *
extent_description (const char *metacontext, uint32_t type)
{
  char *ret;

  if (strcmp (metacontext, "base:allocation") == 0) {
    switch (type) {
    case 0: return strdup ("data");
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
    case 0: return strdup ("absent");
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
