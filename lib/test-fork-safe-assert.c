/* nbd client library in userspace
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
#ifdef HAVE_PRCTL
#include <sys/prctl.h>
#endif
#include <sys/resource.h>

#undef NDEBUG

#include "internal.h"

/* Define these to verify that NBD_INTERNAL_FORK_SAFE_ASSERT() properly
 * stringifies the expression passed to it.
 */
#define TRUE  1
#define FALSE 0

int
main (void)
{
  struct rlimit rlimit;

  /* The standard approach for disabling core dumping. Has no effect on Linux if
   * /proc/sys/kernel/core_pattern starts with a pipe (|) symbol.
   */
  if (getrlimit (RLIMIT_CORE, &rlimit) == -1) {
    perror ("getrlimit");
    return EXIT_FAILURE;
  }
  rlimit.rlim_cur = 0;
  if (setrlimit (RLIMIT_CORE, &rlimit) == -1) {
    perror ("setrlimit");
    return EXIT_FAILURE;
  }

#ifdef HAVE_PRCTL
  if (prctl (PR_SET_DUMPABLE, 0, 0, 0, 0) == -1) {
    perror ("prctl");
    return EXIT_FAILURE;
  }
#endif

  NBD_INTERNAL_FORK_SAFE_ASSERT (TRUE);
  NBD_INTERNAL_FORK_SAFE_ASSERT (FALSE);
  return 0;
}
