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
#include <string.h>

#include "human-size.h"

static unsigned errors = 0;

static void
test (uint64_t bytes, const char *expected, bool expected_human_flag)
{
  char actual[HUMAN_SIZE_LONGEST];
  bool actual_human_flag;

  human_size (actual, bytes, &actual_human_flag);

  if (strcmp (actual, expected) == 0 &&
      actual_human_flag == expected_human_flag) {
    printf ("test-human-size: %" PRIu64 " -> \"%s\" (%s) OK\n",
            bytes, actual, actual_human_flag ? "true" : "false");
    fflush (stdout);
  }
  else {
    fprintf (stderr,
             "test-human-size: error: test case %" PRIu64 " "
             "expected \"%s\" (%s) "
             "but returned \"%s\" (%s)\n",
             bytes,
             expected, expected_human_flag ? "true" : "false",
             actual, actual_human_flag ? "true" : "false");
    errors++;
  }
}

int
main (int argc, char *argv[])
{
  test (0, "0", false);
  test (1, "1", false);
  test (512, "512", false);
  test (1023, "1023", false);
  test (1024, "1K", true);
  test (1025, "1025", false);
  test (2047, "2047", false);
  test (2048, "2K", true);
  test (3 * 1024, "3K", true);

  test (1023 * 1024, "1023K", true);
  test (1048575, "1048575", false);
  test (1048576, "1M", true);
  test (1048577, "1048577", false);

  test (UINT64_C (1073741824), "1G", true);

  test (UINT64_C (1099511627776), "1T", true);
  test (UINT64_C (1099511627777), "1099511627777", false);
  test (UINT64_C (1099511627776) + 1024, "1073741825K", true);

  test (UINT64_C (1125899906842624), "1P", true);

  test ((uint64_t)INT64_MAX+1, "8E", true);
  test (UINT64_MAX-1023, "18014398509481983K", true);
  test (UINT64_MAX, "18446744073709551615", false);

  exit (errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
