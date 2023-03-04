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

#ifndef LIBNBD_VERSION_H
#define LIBNBD_VERSION_H

/* This function is used in the command line utilities to display the
 * version of the tool and the library.  It can be that the library
 * version is different (because of dynamic linking) but that would
 * usually indicate a packaging error.  program_name should be the
 * program name, eg. "nbdcopy".
 */
extern void display_version (const char *program_name);

#endif /* LIBNBD_INTERNAL_H */
