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

#ifndef LIBNBD_HUMAN_SIZE_H
#define LIBNBD_HUMAN_SIZE_H

#include <stdbool.h>
#include <stdint.h>

/* If you allocate a buffer of at least this length in bytes and pass
 * it as the first parameter to human_size, then it will not overrun.
 */
#define HUMAN_SIZE_LONGEST 64

/* Convert bytes to a human-readable string.
 *
 * This is roughly the opposite of nbdkit_parse_size.  It will convert
 * multiples of powers of 1024 to the appropriate human size with the
 * right extension like 'M' or 'G'.  Anything that cannot be converted
 * is returned as bytes.  The *human flag is set to true if the output
 * was abbreviated to a human-readable size, or false if it is just
 * bytes.
 *
 * If buf == NULL, a buffer is allocated and returned.  In this case
 * the returned buffer must be freed.
 *
 * buf may also be allocated by the caller, in which case it must be
 * at least HUMAN_SIZE_LONGEST bytes.
 *
 * On error the function returns an error and sets errno.
 */
extern char *human_size (char *buf, uint64_t bytes, bool *human);

#endif /* LIBNBD_HUMAN_SIZE_H */
