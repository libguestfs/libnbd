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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include "nbdinfo.h"

void
print_json_string (const char *str)
{
  const unsigned char *s = (const unsigned char *)str;
  fputc ('"', fp);
  for (; *s; s++) {
    switch (*s) {
    case '\\':
    case '"':
      fputc ('\\', fp);
      fputc (*s, fp);
      break;
    default:
      if (*s < ' ')
        fprintf (fp, "\\u%04x", *s);
      else
        fputc (*s, fp);
    }
  }
  fputc ('"', fp);
}
