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

/* Test compilation with C++. */

#ifndef __cplusplus
#error "this test should be compiled with a C++ compiler"
#endif

#include <iostream>
#include <cstdlib>

#include <libnbd.h>

using namespace std;

int
main ()
{
  struct nbd_handle *nbd;

  nbd = nbd_create ();
  if (nbd == NULL) {
    cerr << nbd_get_error () << endl;
    exit (EXIT_FAILURE);
  }

  cout << nbd_get_package_name (nbd)
       << " "
       << nbd_get_version (nbd)
       << endl;

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
