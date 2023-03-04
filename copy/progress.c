/* NBD client library in userspace.
 * Copyright Red Hat
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

#include <pthread.h>

#include <libnbd.h>

#include "array-size.h"

#include "nbdcopy.h"

/* Display the progress bar. */
static void
do_progress_bar (off_t pos, int64_t size)
{
  /* Note the spinner is covered with the cursor which usually makes
   * it appear inverse video.
   */
  static const char *spinner[] = { "▝", "▐", "▗", "▃", "▖", "▍", "▘", "▀" };
  static const char *spinner_100 = "█";
  static int spinpos = 0;

  double frac = (double) pos / size;
  char msg[80];
  size_t n, i;

  if (frac < 0) frac = 0; else if (frac > 1) frac = 1;

  if (frac == 1) {
    snprintf (msg, sizeof msg,
              "%s 100%% [****************************************]\n",
              spinner_100);
    progress = false; /* Don't print any more progress bar messages. */
  } else {
    snprintf (msg, sizeof msg,
              "%s %3d%% [----------------------------------------]\r",
              spinner[spinpos], (int)(100*frac));
    n = strcspn (msg, "-");
    for (i = 0; i < 40*frac; ++i)
      msg[n+i] = '*';
    spinpos = (spinpos+1) % ARRAY_SIZE (spinner);
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
  write (fileno (stderr), msg, strlen (msg));
#pragma GCC diagnostic pop
}

/* Machine-readable progress bar used with --progress-fd. */
static void
do_progress_bar_fd (off_t pos, int64_t size)
{
  double frac = (double) pos / size;
  char msg[80];

  if (frac < 0) frac = 0; else if (frac > 1) frac = 1;
  if (frac == 1)
    snprintf (msg, sizeof msg, "100/100\n");
  else
    snprintf (msg, sizeof msg, "%d/100\n", (int)(100*frac));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
  write (progress_fd, msg, strlen (msg));
#pragma GCC diagnostic pop
}

void
progress_bar (off_t pos, int64_t size)
{
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  if (!progress)
    return;
  if (size == 0)
    return;

  pthread_mutex_lock (&lock);
  if (progress_fd == -1)
    do_progress_bar (pos, size);
  else
    do_progress_bar_fd (pos, size);
  pthread_mutex_unlock (&lock);
}
