/* NBD client library in userspace
 * Copyright (C) 2013-2022 Red Hat Inc.
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
#include <fcntl.h>

#include "minmax.h"

#include "internal.h"

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

/* Replace a string_vector with a deep copy of in (including final NULL). */
int
nbd_internal_copy_string_list (string_vector *v, char **in)
{
  size_t i;

  assert (in);
  assert (v->ptr == NULL);

  for (i = 0; in[i] != NULL; ++i) {
    char *copy = strdup (in[i]);
    if (copy == NULL)
      return -1;
    if (string_vector_append (v, copy) == -1) {
      free (copy);
      return -1;
    }
  }

  return string_vector_append (v, NULL);
}

/* Store argv into h, or diagnose an error on failure. */
int
nbd_internal_set_argv (struct nbd_handle *h, char **argv)
{
  /* This should never be NULL.  The generator adds code to each
   * StringList call in lib/api.c to check this and return an error.
   */
  assert (argv);

  /* Because this function is only called from functions that take
   * argv-style lists of strings (such as nbd_connect_command) we can
   * check here that the command name is present.
   */
  if (argv[0] == NULL) {
    set_error (EINVAL, "missing command name in argv list");
    return -1;
  }

  string_vector_empty (&h->argv);

  if (nbd_internal_copy_string_list (&h->argv, argv) == -1) {
    set_error (errno, "realloc");
    return -1;
  }

  return 0;
}

/* Copy queries (defaulting to h->request_meta_contexts) into h->querylist.
 * Set an error on failure.
 */
int
nbd_internal_set_querylist (struct nbd_handle *h, char **queries)
{
  string_vector_empty (&h->querylist);

  if (queries) {
    if (nbd_internal_copy_string_list (&h->querylist, queries) == -1) {
      set_error (errno, "realloc");
      return -1;
    }
    /* Drop trailing NULL */
    assert (h->querylist.len > 0);
    string_vector_remove (&h->querylist, h->querylist.len - 1);
  }
  else {
    size_t i;

    for (i = 0; i < h->request_meta_contexts.len; ++i) {
      char *copy = strdup (h->request_meta_contexts.ptr[i]);
      if (copy == NULL) {
        set_error (errno, "strdup");
        return -1;
      }
      if (string_vector_append (&h->querylist, copy) == -1) {
        set_error (errno, "realloc");
        free (copy);
        return -1;
      }
    }
  }

  return 0;
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

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

/* Fork-safe version of perror.  ONLY use this after fork and before
 * exec, the rest of the time use set_error().
 */
void
nbd_internal_fork_safe_perror (const char *s)
{
  const int err = errno;
  const char *m = NULL;
  char buf[32];

  write (2, s, strlen (s));
  write (2, ": ", 2);
#ifdef HAVE_STRERRORDESC_NP
  m = strerrordesc_np (errno);
#else
#if HAVE_SYS_ERRLIST /* NB Don't use #ifdef */
  m = errno >= 0 && errno < sys_nerr ? sys_errlist[errno] : NULL;
#endif
#endif
  if (!m)
    m = nbd_internal_fork_safe_itoa ((long) errno, buf, sizeof buf);
  write (2, m, strlen (m));
  write (2, "\n", 1);

  /* Restore original errno in case it was disturbed by the system
   * calls above.
   */
  errno = err;
}

#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

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

  if (str == NULL) {
    fprintf (fp, "NULL");
    return;
  }

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
  size_t len = 0;
  FILE *fp;

  fp = open_memstream (&s, &len);
  if (fp == NULL)
    return NULL;

  if (list == NULL)
    fprintf (fp, "NULL");
  else {
    size_t i;

    fprintf (fp, "[");
    for (i = 0; list[i] != NULL; ++i) {
      if (i > 0)
        fprintf (fp, ", ");
      printable_string (list[i], fp);
    }
    fprintf (fp, "]");
  }
  fclose (fp);

  return s;

}

int nbd_internal_socket (int domain,
                         int type,
                         int protocol,
                         bool nonblock)
{
  int fd;

  /* So far we do not know about any platform that has SOCK_CLOEXEC and
   * lacks SOCK_NONBLOCK at the same time.
   *
   * The workaround for missing SOCK_CLOEXEC introduces a race which
   * cannot be fixed until support for SOCK_CLOEXEC is added (or other
   * fix is implemented).
   */
#ifndef SOCK_CLOEXEC
  int flags;
#else
  type |= SOCK_CLOEXEC;
  if (nonblock)
    type |= SOCK_NONBLOCK;
#endif

  fd = socket (domain, type, protocol);

#ifndef SOCK_CLOEXEC
  if (fd == -1)
    return -1;

  if (fcntl (fd, F_SETFD, FD_CLOEXEC) == -1) {
    close (fd);
    return -1;
  }

  if (nonblock) {
    flags = fcntl (fd, F_GETFL, 0);
    if (flags == -1 ||
        fcntl (fd, F_SETFL, flags|O_NONBLOCK) == -1) {
      close (fd);
      return -1;
    }
  }
#endif

  return fd;
}

int
nbd_internal_socketpair (int domain, int type, int protocol, int *fds)
{
  int ret;

  /*
   * Same as with nbd_internal_socket() this workaround for missing
   * SOCK_CLOEXEC introduces a race which cannot be fixed until support
   * for SOCK_CLOEXEC is added (or other fix is implemented).
   */
#ifndef SOCK_CLOEXEC
  size_t i;
#else
  type |= SOCK_CLOEXEC;
#endif

  ret = socketpair (domain, type, protocol, fds);

#ifndef SOCK_CLOEXEC
  if (ret == 0) {
    for (i = 0; i < 2; i++) {
      if (fcntl (fds[i], F_SETFD, FD_CLOEXEC) == -1) {
        close (fds[0]);
        close (fds[1]);
        return -1;
      }
    }
  }
#endif

  return ret;
}
