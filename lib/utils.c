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
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/uio.h>

#include "array-size.h"
#include "checked-overflow.h"
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

/* "Best effort" function for writing out a list of NUL-terminated strings to a
 * file descriptor (without the NUL-terminators). The list is terminated with
 * (char *)NULL. Partial writes, and EINTR and EAGAIN failures are handled
 * internally. No value is returned; only call this function for writing
 * diagnostic data on error paths, when giving up on a higher-level action
 * anyway.
 *
 * No more than 16 strings, excluding the NULL terminator, will be written. (As
 * of POSIX Issue 7 + TC2, _XOPEN_IOV_MAX is 16.)
 *
 * The function is supposed to remain async-signal-safe.
 *
 * (The va_*() macros, while not marked async-signal-safe in Issue 7 + TC2, are
 * considered such, per <https://www.austingroupbugs.net/view.php?id=711>, which
 * is binding for Issue 7 implementations via the Interpretations Track.
 *
 * Furthermore, writev(), while also not marked async-signal-safe in Issue 7 +
 * TC2, is considered such, per
 * <https://www.austingroupbugs.net/view.php?id=1455>, which is slated for
 * inclusion in Issue 7 TC3 (if there's going to be a TC3), and in Issue 8.)
 */
static void __attribute__ ((sentinel))
xwritel (int fildes, ...)
{
  /* Open-code the current value of _XOPEN_IOV_MAX, in order to contain stack
   * footprint, should _XOPEN_IOV_MAX grow in the future.
   */
  struct iovec iovec[16], *filled, *end, *pos;
  va_list ap;
  char *arg;

  /* Translate the variable argument list to IO vectors. Note that we cast away
   * const-ness intentionally.
   */
  filled = iovec;
  end = iovec + ARRAY_SIZE (iovec);
  va_start (ap, fildes);
  while (filled < end && (arg = va_arg (ap, char *)) != NULL)
    *filled++ = (struct iovec){ .iov_base = arg, .iov_len = strlen (arg) };
  va_end (ap);

  /* Write out the IO vectors. */
  pos = iovec;
  while (pos < filled) {
    ssize_t written;

    /* Skip any empty vectors at the front. */
    if (pos->iov_len == 0) {
      ++pos;
      continue;
    }

    /* Write out the vectors. */
    do
      written = writev (fildes, pos, filled - pos);
    while (written == -1 && (errno == EINTR || errno == EAGAIN));

    if (written == -1)
      return;

    /* Consume the vectors that have been written out (fully, or in part). Note
     * that "written" is positive here.
     */
    do {
      size_t advance;

      advance = MIN (written, pos->iov_len);
      /* Note that "advance" is positive here iff "pos->iov_len" is positive. */
      pos->iov_base = (char *)pos->iov_base + advance;
      pos->iov_len -= advance;
      written -= advance;

      /* At least one of "written" and "pos->iov_len" is zero here. */
      if (pos->iov_len == 0)
        ++pos;
    } while (written > 0);
  }
}

/* Fork-safe version of perror.  ONLY use this after fork and before
 * exec, the rest of the time use set_error().
 */
void
nbd_internal_fork_safe_perror (const char *s)
{
  const int err = errno;
  const char *m = NULL;
  char buf[32];

#ifdef HAVE_STRERRORDESC_NP
  m = strerrordesc_np (errno);
#else
#if HAVE_SYS_ERRLIST /* NB Don't use #ifdef */
  m = errno >= 0 && errno < sys_nerr ? sys_errlist[errno] : NULL;
#endif
#endif
  if (!m)
    m = nbd_internal_fork_safe_itoa ((long) errno, buf, sizeof buf);
  xwritel (STDERR_FILENO, s, ": ", m, "\n", (char *)NULL);

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

void
nbd_internal_fork_safe_assert (int result, const char *file, long line,
                               const char *func, const char *assertion)
{
  const char *line_out;
  char line_buf[32];

  if (result)
    return;

  line_out = nbd_internal_fork_safe_itoa (line, line_buf, sizeof line_buf);
  xwritel (STDERR_FILENO, file, ":", line_out, ": ", func, ": Assertion `",
           assertion, "' failed.\n", (char *)NULL);
  abort ();
}

/* Returns the value of the PATH environment variable -- falling back to
 * confstr(_CS_PATH) if PATH is absent -- as a dynamically allocated string. On
 * failure, sets "errno" and returns NULL.
 */
static char *
get_path (void)
  LIBNBD_ATTRIBUTE_ALLOC_DEALLOC (free)
{
  char *path;
  bool env_path_found;
  size_t path_size, path_size2;

  /* Note: per POSIX, here we should lock the environment, even just for
   * getenv(). However, glibc and any other high-quality libc will not be
   * modifying "environ" during getenv(), and no sane application should modify
   * the environment after launching threads.
   */
  path = getenv ("PATH");
  if ((env_path_found = (path != NULL)))
    path = strdup (path);
  /* This is where we'd unlock the environment. */

  if (env_path_found) {
    /* This handles out-of-memory as well. */
    return path;
  }

  errno = 0;
  path_size = confstr (_CS_PATH, NULL, 0);
  if (path_size == 0) {
    /* If _CS_PATH does not have a configuration-defined value, just store
     * ENOENT to "errno".
     */
    if (errno == 0)
      errno = ENOENT;

    return NULL;
  }

  path = malloc (path_size);
  if (path == NULL)
    return NULL;

  path_size2 = confstr (_CS_PATH, path, path_size);
  assert (path_size2 == path_size);
  return path;
}

/* nbd_internal_execvpe_init() and nbd_internal_fork_safe_execvpe() together
 * present an execvp() alternative that is async-signal-safe.
 *
 * nbd_internal_execvpe_init() may only be called before fork(), for filling in
 * the caller-allocated, uninitialized "ctx" structure. If
 * nbd_internal_execvpe_init() succeeds, then fork() may be called.
 * Subsequently, in the child process, nbd_internal_fork_safe_execvpe() may be
 * called with the inherited "ctx" structure, while in the parent process,
 * nbd_internal_execvpe_uninit() must be called to uninitialize (evacuate) the
 * "ctx" structure.
 *
 * On failure, "ctx" will not have been modified, "errno" is set, and -1 is
 * returned. Failures include:
 *
 * - Errors forwarded from underlying functions such as strdup(), confstr(),
 *   malloc(), string_vector_append().
 *
 * - ENOENT: "file" is an empty string.
 *
 * - ENOENT: "file" does not contain a <slash> "/" character, the PATH
 *           environment variable is not set, and confstr() doesn't associate a
 *           configuration-defined value with _CS_PATH.
 *
 * - ENOENT: "file" does not contain a <slash> "/" character, and: (a) the PATH
 *           environment variable is set to the empty string, or (b) PATH is not
 *           set, and confstr() outputs the empty string for _CS_PATH.
 *
 * - EOVERFLOW: the sizes or counts of necessary objects could not be expressed.
 *
 * - EINVAL: "num_args" is less than 2.
 *
 * On success, the "ctx" structure will have been filled in, and 0 is returned.
 *
 * - "pathnames" member:
 *
 *   - All strings pointed-to by elements of the "pathnames" string_vector
 *     member are owned by "pathnames".
 *
 *   - If "file" contains a <slash> "/" character, then the sole entry in
 *     "pathnames" is a copy of "file".
 *
 *   - If "file" does not contain a <slash> "/" character:
 *
 *     Let "system path" be defined as the value of the PATH environment
 *     variable, if the latter exists, and as the value output by confstr() for
 *     _CS_PATH otherwise. Per the ENOENT specifications above, "system path" is
 *     a non-empty string. Let "system path" further be of the form
 *
 *       <prefix_0> [n = 1]
 *
 *     or
 *
 *       <prefix_0>:<prefix_1>:...:<prefix_(n-1)> [n >= 2]
 *
 *     where for each 0 <= i < n, <prefix_i> does not contain the <colon> ":"
 *     character. In the (n = 1) case, <prefix_0> is never empty (see ENOENT
 *     above), while in the (n >= 2) case, any individual <prefix_i> may or may
 *     not be empty.
 *
 *     The "pathnames" string_vector member has n elements; for each 0 <= i < n,
 *     element i is of the form
 *
 *       suffix(curdir(<prefix_i>))file
 *
 *     where
 *
 *       curdir(x) := "."  if x = ""
 *       curdir(x) := x    otherwise
 *
 *     and
 *
 *       suffix(x) := x      if "x" ends with a <slash> "/"
 *       suffix(x) := x "/"  otherwise
 *
 *     This transformation implements the POSIX XBD / PATH environment variable
 *     semantics, creating candidate pathnames for execution by
 *     nbd_internal_fork_safe_execvpe(). nbd_internal_fork_safe_execvpe() will
 *     iterate over the candidate pathnames with execve() until execve()
 *     succeeds, or fails with an error that is due to neither pathname
 *     resolution, nor the candidate not being a regular file, nor the candidate
 *     lacking execution permission.
 *
 * - The "sh_argv" array member will have at least (num_args + 1) elements
 *   allocated, and none populated.
 *
 *   (The minimum value of "num_args" is 2 -- see EINVAL above. According to
 *   POSIX, "[t]he argument /arg0/ should point to a filename string that is
 *   associated with the process being started by one of the /exec/ functions",
 *   plus "num_args" includes the null pointer that terminates the argument
 *   list.)
 *
 *   This allocation is made in anticipation of execve() failing for a
 *   particular candidate inside nbd_internal_fork_safe_execvpe() with ENOEXEC
 *   ("[t]he new process image file has the appropriate access permission but
 *   has an unrecognized format"). While that failure terminates the iteration,
 *   the failed call
 *
 *     execve (pathnames[i],
 *             { argv[0], argv[1], ..., NULL }, // (num_args >= 2) elements
 *             { envp[0], envp[1], ..., NULL })
 *
 *   must be repeated as
 *
 *     execve (<shell-path>,
 *             { argv[0], pathnames[i],         // ((num_args + 1) >= 3)
                 argv[1], ..., NULL },          // elements
 *             { envp[0], envp[1], ..., NULL })
 *
 *   for emulating execvp(). The allocation in the "sh_argv" member makes it
 *   possible just to *link* the original "argv" elements and the "pathnames[i]"
 *   candidate into the right positions.
 *
 *   (POSIX leaves the shell pathname unspecified; "/bin/sh" should be good
 *   enough.)
 *
 *   The shell *binary* will see itself being executed under the name "argv[0]",
 *   will receive "pathnames[i]" as the pathname of the shell *script* to read
 *   and interpret ("command_file" in POSIX terminology), will expose
 *   "pathnames[i]" as the positional parameter $0 to the script, and will
 *   forward "argv[1]" and the rest to the script as positional parameters $1
 *   and onward.
 */
int
nbd_internal_execvpe_init (struct execvpe *ctx, const char *file,
                           size_t num_args)
{
  int rc;
  char *sys_path;
  string_vector pathnames;
  char *pathname;
  size_t num_sh_args;
  char **sh_argv;
  size_t sh_argv_bytes;

  rc = -1;

  if (file[0] == '\0') {
    errno = ENOENT;
    return rc;
  }

  /* First phase. */
  sys_path = NULL;
  pathnames = (string_vector)empty_vector;

  if (strchr (file, '/') == NULL) {
    size_t file_len;
    const char *sys_path_element, *scan;
    bool finish;

    sys_path = get_path ();
    if (sys_path == NULL)
      return rc;

    if (sys_path[0] == '\0') {
      errno = ENOENT;
      goto free_sys_path;
    }

    pathname = NULL;
    file_len = strlen (file);
    sys_path_element = sys_path;
    scan = sys_path;
    do {
      assert (sys_path_element <= scan);
      finish = (*scan == '\0');
      if (finish || *scan == ':') {
        const char *sys_path_copy_start;
        size_t sys_path_copy_size;
        size_t sep_copy_size;
        size_t pathname_size;
        char *p;

        if (scan == sys_path_element) {
          sys_path_copy_start = ".";
          sys_path_copy_size = 1;
        } else {
          sys_path_copy_start = sys_path_element;
          sys_path_copy_size = scan - sys_path_element;
        }

        assert (sys_path_copy_size >= 1);
        sep_copy_size = (sys_path_copy_start[sys_path_copy_size - 1] != '/');

        if (ADD_OVERFLOW (sys_path_copy_size, sep_copy_size, &pathname_size) ||
            ADD_OVERFLOW (pathname_size, file_len, &pathname_size) ||
            ADD_OVERFLOW (pathname_size, 1u, &pathname_size)) {
          errno = EOVERFLOW;
          goto empty_pathnames;
        }

        pathname = malloc (pathname_size);
        if (pathname == NULL)
          goto empty_pathnames;
        p = pathname;

        memcpy (p, sys_path_copy_start, sys_path_copy_size);
        p += sys_path_copy_size;

        memcpy (p, "/", sep_copy_size);
        p += sep_copy_size;

        memcpy (p, file, file_len);
        p += file_len;

        *p++ = '\0';

        if (string_vector_append (&pathnames, pathname) == -1)
          goto empty_pathnames;
        /* Ownership transferred. */
        pathname = NULL;

        sys_path_element = scan + 1;
      }

      ++scan;
    } while (!finish);
  } else {
    pathname = strdup (file);
    if (pathname == NULL)
      return rc;

    if (string_vector_append (&pathnames, pathname) == -1)
      goto empty_pathnames;
    /* Ownership transferred. */
    pathname = NULL;
  }

  /* Second phase. */
  if (num_args < 2) {
    errno = EINVAL;
    goto empty_pathnames;
  }
  if (ADD_OVERFLOW (num_args, 1u, &num_sh_args) ||
      MUL_OVERFLOW (num_sh_args, sizeof *sh_argv, &sh_argv_bytes)) {
    errno = EOVERFLOW;
    goto empty_pathnames;
  }
  sh_argv = malloc (sh_argv_bytes);
  if (sh_argv == NULL)
    goto empty_pathnames;

  /* Commit. */
  ctx->pathnames = pathnames;
  ctx->sh_argv = sh_argv;
  ctx->num_sh_args = num_sh_args;
  rc = 0;
  /* Fall through, for freeing temporaries. */

empty_pathnames:
  if (rc == -1) {
    free (pathname);
    string_vector_empty (&pathnames);
  }

free_sys_path:
  free (sys_path);

  return rc;
}

void
nbd_internal_execvpe_uninit (struct execvpe *ctx)
{
  free (ctx->sh_argv);
  ctx->num_sh_args = 0;
  string_vector_empty (&ctx->pathnames);
}

int
nbd_internal_fork_safe_execvpe (struct execvpe *ctx, const string_vector *argv,
                                char * const *envp)
{
  size_t pathname_idx;

  NBD_INTERNAL_FORK_SAFE_ASSERT (ctx->pathnames.len > 0);

  pathname_idx = 0;
  do {
    (void)execve (ctx->pathnames.ptr[pathname_idx], argv->ptr, envp);
    if (errno != EACCES && errno != ELOOP && errno != ENAMETOOLONG &&
        errno != ENOENT && errno != ENOTDIR)
      break;

    ++pathname_idx;
  } while (pathname_idx < ctx->pathnames.len);

  if (errno == ENOEXEC) {
    char **sh_argp;
    size_t argv_idx;

    NBD_INTERNAL_FORK_SAFE_ASSERT (ctx->num_sh_args >= argv->len);
    NBD_INTERNAL_FORK_SAFE_ASSERT (ctx->num_sh_args - argv->len == 1);

    sh_argp = ctx->sh_argv;
    *sh_argp++ = argv->ptr[0];
    *sh_argp++ = ctx->pathnames.ptr[pathname_idx];
    for (argv_idx = 1; argv_idx < argv->len; ++argv_idx)
      *sh_argp++ = argv->ptr[argv_idx];

    (void)execve ("/bin/sh", ctx->sh_argv, envp);
  }

  return -1;
}
