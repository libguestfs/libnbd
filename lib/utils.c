/* NBD client library in userspace
 * Copyright (C) 2013-2019 Red Hat Inc.
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
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "internal.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

void
nbd_internal_hexdump (const void *data, size_t len, FILE *fp)
{
  size_t i, j;

  for (i = 0; i < len; i += 16) {
    fprintf (fp, "%04zx: ", i);
    for (j = i; j < MIN (i+16, len); ++j)
      fprintf (fp, "%02x ", ((const unsigned char *)data)[j]);
    for (; j < i+16; ++j)
      fprintf (fp, "   ");
    fprintf (fp, "|");
    for (j = i; j < MIN (i+16, len); ++j)
      if (isprint (((const char *)data)[j]))
        fprintf (fp, "%c", ((const char *)data)[j]);
      else
        fprintf (fp, ".");
    for (; j < i+16; ++j)
      fprintf (fp, " ");
    fprintf (fp, "|\n");
  }
}

size_t
nbd_internal_string_list_length (char **argv)
{
  size_t ret;

  for (ret = 0; argv[ret] != NULL; ++ret)
    ;
  return ret;
}

char **
nbd_internal_copy_string_list (char **argv)
{
  size_t i, j, n;
  char **ret;

  n = nbd_internal_string_list_length (argv);
  ret = calloc (n+1, sizeof (char *));
  if (!ret)
    return NULL;

  for (i = 0; i < n; ++i) {
    ret[i] = strdup (argv[i]);
    if (ret[i] == NULL) {
      for (j = 0; j < i; ++j)
        free (ret[j]);
      free (ret);
      return NULL;
    }
  }
  ret[n] = NULL;

  return ret;
}

void
nbd_internal_free_string_list (char **argv)
{
  size_t i;

  if (!argv)
    return;

  for (i = 0; argv[i] != NULL; ++i)
    free (argv[i]);
  free (argv);
}

/* Like sprintf ("%ld", v), but safe to use between fork and exec.  Do
 * not use this function in any other context.
 *
 * The caller must supply a scratch buffer which is at least 32 bytes
 * long (else the function will call abort()).  Note that the returned
 * string does not point to the start of this buffer.
 */
const char *
nbd_internal_fork_safe_itoa (long v, char *buf, size_t bufsize)
{
  unsigned long uv = (unsigned long) v;
  size_t i = bufsize - 1;
  bool neg = false;

  if (bufsize < 32) abort ();

  buf[i--] = '\0';
  if (v < 0) {
    neg = true;
    uv = -uv;
  }
  if (uv == 0)
    buf[i--] = '0';
  else {
    while (uv) {
      buf[i--] = '0' + (uv % 10);
      uv /= 10;
    }
  }
  if (neg)
    buf[i--] = '-';

  i++;
  return &buf[i];
}

/* Fork-safe version of perror.  ONLY use this after fork and before
 * exec, the rest of the time use set_error().
 */
void
nbd_internal_fork_safe_perror (const char *s)
{
  const int err = errno;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif
  write (2, s, strlen (s));
  write (2, ": ", 2);
#if HAVE_SYS_ERRLIST
  write (2, sys_errlist[errno], strlen (sys_errlist[errno]));
#else
  char buf[32];
  const char *v = nbd_internal_fork_safe_itoa ((long) errno, buf, sizeof buf);
  write (2, v, strlen (v));
#endif
  write (2, "\n", 1);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  /* Restore original errno in case it was disturbed by the system
   * calls above.
   */
  errno = err;
}

/* nbd_internal_printable_* functions are used by the API code to
 * print debug messages when we trace calls in and out of libnbd.  The
 * calls should attempt to convert the parameter into something
 * printable.
 *
 * They cannot fail, but it's OK if they return NULL.
 *
 * Caller frees the result.
 */

char *
nbd_internal_printable_buffer (const void *buf, size_t count)
{
  char *s = NULL;
  size_t len = 0, truncated;
  FILE *fp;

  fp = open_memstream (&s, &len);
  if (fp == NULL)
    return NULL;

  /* If the buffer is very long, truncate it to 1 sector. */
  if (count > 512) {
    truncated = count - 512;
    count = 512;
  }
  else
    truncated = 0;

  fprintf (fp, "\n");
  nbd_internal_hexdump (buf, count, fp);

  if (truncated)
    fprintf (fp, "[... %zu more bytes truncated ...]\n", truncated);
  fclose (fp);

  return s;
}

static void
printable_string (const char *str, FILE *fp)
{
  size_t i, n, truncated;

  n = strlen (str);
  if (n > 512) {
    truncated = n - 512;
    n = 512;
  }
  else
    truncated = 0;

  fprintf (fp, "\"");
  for (i = 0; i < n; ++i) {
    if (isprint (str[i]))
      fputc (str[i], fp);
    else
      fprintf (fp, "\\x%02x", str[i]);
  }

  if (truncated)
    fprintf (fp, "[... %zu more bytes truncated ...]", truncated);
  fprintf (fp, "\"");
}

char *
nbd_internal_printable_string (const char *str)
{
  char *s = NULL;
  size_t len = 0;
  FILE *fp;

  fp = open_memstream (&s, &len);
  if (fp == NULL)
    return NULL;

  printable_string (str, fp);
  fclose (fp);

  return s;
}

char *
nbd_internal_printable_string_list (char **list)
{
  char *s = NULL;
  size_t len = 0, i;
  FILE *fp;

  fp = open_memstream (&s, &len);
  if (fp == NULL)
    return NULL;

  fprintf (fp, "[");
  for (i = 0; list[i] != NULL; ++i) {
    if (i > 0)
      fprintf (fp, ", ");
    printable_string (list[i], fp);
  }
  fprintf (fp, "]");
  fclose (fp);

  return s;

}
