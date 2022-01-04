/* NBD client library in userspace
 * Copyright (C) 2013-2021 Red Hat Inc.
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

/* Check for a requirement or skip the test. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include "requires.h"

void
requires (const char *cmd)
{
  printf ("requires %s\n", cmd);
  fflush (stdout);
  if (system (cmd) != 0) {
    printf ("Test skipped because prerequisite is missing or not working.\n");
    exit (77);
  }
}

void
requires_not (const char *cmd)
{
  printf ("requires_not %s\n", cmd);
  fflush (stdout);
  if (system (cmd) == 0) {
    printf ("Test skipped because prerequisite is missing or not working.\n");
    exit (77);
  }
}

/* Check qemu-nbd was compiled with support for TLS. */
void
requires_qemu_nbd_tls_support (const char *qemu_nbd)
{
  char cmd[256];

  /* Note the qemu-nbd command will fail in some way.  We're only
   * interested in the error message that it prints.
   */
  snprintf (cmd, sizeof cmd,
            "if %s --object tls-creds-x509,id=tls0 |& grep -sq 'TLS credentials support requires GNUTLS'; then exit 1; else exit 0; fi",
            qemu_nbd);
  requires (cmd);
}

/* On some distros, nbd-server is built without support for syslog
 * which prevents use of inetd mode.  Instead nbd-server will exit with
 * this error:
 *
 *   Error: inetd mode requires syslog
 *   Exiting.
 *
 * https://listman.redhat.com/archives/libguestfs/2022-January/msg00003.html
 */
void
requires_nbd_server_supports_inetd (const char *nbd_server)
{
  char cmd[256];

  snprintf (cmd, sizeof cmd, "\"%s\" --version", nbd_server);
  requires (cmd);
  snprintf (cmd, sizeof cmd,
            "grep 'inetd mode requires syslog' \"$(command -v \"%s\")\"",
            nbd_server);
  requires_not (cmd);
}
