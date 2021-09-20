/* nbd client library in userspace
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

#include "human-size.h"

char *
human_size (char *buf, uint64_t bytes, bool *human)
{
  static const char ext[][2] = { "E", "P", "T", "G", "M", "K", "" };
  size_t i;

  if (buf == NULL) {
    buf = malloc (HUMAN_SIZE_LONGEST);
    if (buf == NULL)
      return NULL;
  }

  /* Work out which extension to use, if any. */
  i = 6;
  if (bytes != 0) {
    while ((bytes & 1023) == 0) {
      bytes >>= 10;
      i--;
    }
  }

  /* Set the flag to true if we're going to add a human-readable extension. */
  if (human)
    *human = ext[i][0] != '\0';

  snprintf (buf, HUMAN_SIZE_LONGEST, "%" PRIu64 "%s", bytes, ext[i]);
  return buf;
}
