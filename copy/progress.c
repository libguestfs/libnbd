/* NBD client library in userspace.
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
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <libnbd.h>

#include "nbdcopy.h"

/* Display the progress bar. */
void
progress_bar (off_t pos, int64_t size)
{
  static const char *spinner[] = { "◐", "◓", "◑", "◒" };
  static int tty = -1;
  double frac = (double) pos / size;
  char msg[80];
  size_t n, i;

  if (tty == -1) {
    tty = open ("/dev/tty", O_WRONLY);
    if (tty == -1)
      return;
  }

  if (frac < 0) frac = 0; else if (frac > 1) frac = 1;

  if (frac == 1) {
    snprintf (msg, sizeof msg, "● 100%% [********************]\n");
    progress = false; /* Don't print any more progress bar messages. */
  } else {
    snprintf (msg, sizeof msg, "%s %3d%% [--------------------]\r",
              spinner[(int)(4*frac)], (int)(100*frac));
    n = strcspn (msg, "-");
    for (i = 0; i < 20*frac; ++i)
      msg[n+i] = '*';
  }

  n = write (tty, msg, strlen (msg));
}
