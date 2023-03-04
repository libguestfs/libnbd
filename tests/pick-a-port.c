/* NBD client library in userspace
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

/* Pick an unused TCP port at random.
 * This is inherently racy so it's only best effort.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "requires.h"
#include "pick-a-port.h"

static int
port_is_used (int port)
{
  char command[80];
  int r;

  snprintf (command, sizeof command, "ss -ltn | grep -sqE ':%d\\b'", port);
  r = system (command);
  if (WIFEXITED (r) && WEXITSTATUS (r) == 0 /* used */)
    return 1;
  else
    /* Anything else, assume unused. */
    return 0;
}

int
pick_a_port (void)
{
  int port;

  /* This requires the 'ss' utility, else we skip the whole test. */
  requires ("ss --version");

  /* Start from a random port number to make it less likely that two
   * parallel tests will conflict.
   */
  srand (time (NULL) + getpid ());
  port = 32768 + (rand () & 32767);

  /* Keep going until we find an unused port. */
  while (port_is_used (port)) {
    port++;
    if (port == 65536)
      port = 32768;
  }

  printf ("picked unused port %d\n", port);
  fflush (stdout);
  return port;
}
