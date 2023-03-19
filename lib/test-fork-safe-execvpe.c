  /* nbd client library in userspace
 * Copyright (C) 2013-2023 Red Hat Inc.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

extern char **environ;

/* This is a perror() replacement that makes the error message more
 * machine-readable, for a select few error numbers. Do not use it as a general
 * error() replacement, only upon nbd_internal_execvpe_init() and
 * nbd_internal_fork_safe_execvpe() failure.
 */
static void
xperror (const char *s)
{
  const char *err;

  if (s != NULL && *s != '\0')
    (void)fprintf (stderr, "%s: ", s);

  switch (errno) {
  case EACCES:
    err = "EACCES";
    break;
  case ELOOP:
    err = "ELOOP";
    break;
  case ENOENT:
    err = "ENOENT";
    break;
  case ENOTDIR:
    err = "ENOTDIR";
    break;
  default:
    err = strerror (errno);
  }
  (void)fprintf (stderr, "%s\n", err);
}

int
main (int argc, char **argv)
{
  struct execvpe ctx;
  const char *prog_file;
  string_vector prog_argv;
  size_t i;

  if (argc < 3) {
    fprintf (stderr, "%1$s: usage: %1$s program-to-exec argv0 ...\n", argv[0]);
    return EXIT_FAILURE;
  }

  prog_file = argv[1];

  /* For the argv of the program to execute, we need to drop our argv[0] (= our
   * own name) and argv[1] (= the program we need to execute), and to tack on a
   * terminating null pointer. Note that "argc" does not include the terminating
   * NULL.
   */
  prog_argv = (string_vector)empty_vector;
  if (string_vector_reserve (&prog_argv, argc - 2 + 1) == -1) {
    perror ("string_vector_reserve");
    return EXIT_FAILURE;
  }

  for (i = 2; i < argc; ++i)
    (void)string_vector_append (&prog_argv, argv[i]);
  (void)string_vector_append (&prog_argv, NULL);

  if (nbd_internal_execvpe_init (&ctx, prog_file, prog_argv.len) == -1) {
    xperror ("nbd_internal_execvpe_init");
    goto reset_prog_argv;
  }

  /* Print out the generated candidates. */
  for (i = 0; i < ctx.pathnames.len; ++i)
    (void)fprintf (stdout, "%s\n", ctx.pathnames.ptr[i]);

  if (fflush (stdout) == EOF) {
    perror ("fflush");
    goto uninit_execvpe;
  }

  (void)nbd_internal_fork_safe_execvpe (&ctx, &prog_argv, environ);
  xperror ("nbd_internal_fork_safe_execvpe");
  /* fall through */

uninit_execvpe:
  nbd_internal_execvpe_uninit (&ctx);

reset_prog_argv:
  string_vector_reset (&prog_argv);

  return EXIT_FAILURE;
}
