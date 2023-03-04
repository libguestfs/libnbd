/* libnbd
 * Copyright Red Hat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Test the library can be loaded and unloaded using dlopen etc.
 *
 * We do this from a thread because of this problem identified in
 * libvirt:
 * https://lists.nongnu.org/archive/html/qemu-block/2021-04/msg00828.html
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>

/* Define these so we don't have to include <libnbd.h>. */
struct nbd_handle;

typedef struct nbd_handle *(*nbd_create_t) (void);
typedef void (*nbd_close_t) (struct nbd_handle *h);
typedef const char *(*nbd_get_error_t) (void);
typedef char *(*nbd_get_handle_name_t) (struct nbd_handle *h);
typedef int64_t (*nbd_get_size_t) (struct nbd_handle *h);

#ifndef LIBRARY
#error "-DLIBRARY was not defined"
#endif

static const char *progname;
static void *thread_start (void *arg);
static void *read_symbol (void *lib, const char *symbol);

int
main (int argc, char *argv[])
{
  pthread_t thread;
  int r;

  progname = argv[0];

  if (access (LIBRARY, X_OK) == -1) {
    fprintf (stderr, "%s: test skipped because %s cannot be accessed: %m\n",
             progname, LIBRARY);
    exit (77);
  }

  r = pthread_create (&thread, NULL, thread_start, NULL);
  if (r != 0) {
    errno = r;
    fprintf (stderr, "%s: pthread_create failed: %m\n", progname);
    exit (EXIT_FAILURE);
  }

  r = pthread_join (thread, NULL);
  if (r != 0) {
    errno = r;
    fprintf (stderr, "%s: pthread_join failed: %m\n", progname);
    exit (EXIT_FAILURE);
  }

  exit (EXIT_SUCCESS);
}

static void *
thread_start (void *arg)
{
  void *lib;
  nbd_create_t nbd_create;
  nbd_close_t nbd_close;
  nbd_get_error_t nbd_get_error;
  nbd_get_handle_name_t nbd_get_handle_name;
  nbd_get_size_t nbd_get_size;
  struct nbd_handle *h;
  char *name;

  lib = dlopen (LIBRARY, RTLD_LAZY);
  if (lib == NULL) {
    fprintf (stderr, "%s: could not open %s: %s",
             progname, LIBRARY, dlerror ());
    exit (EXIT_FAILURE);
  }

  nbd_create = read_symbol (lib, "nbd_create");
  nbd_close = read_symbol (lib, "nbd_close");
  nbd_get_error = read_symbol (lib, "nbd_get_error");
  nbd_get_handle_name = read_symbol (lib, "nbd_get_handle_name");
  nbd_get_size = read_symbol (lib, "nbd_get_size");

  /* Create a handle and try out some operations. */
  h = nbd_create ();
  if (h == NULL) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  name = nbd_get_handle_name (h);
  printf ("handle name = \"%s\"\n", name);
  free (name);

  /* This deliberately is an error because I want to check that we
   * don't try to free the thread-local error string by calling a
   * function in the dlclosed (unmapped) library.
   */
  if (nbd_get_size (h) != -1) {
    fprintf (stderr, "%s: expected nbd_get_size to return an error\n",
             progname);
    exit (EXIT_FAILURE);
  }
  printf ("expected error string = \"%s\"\n", nbd_get_error ());

  nbd_close (h);

  if (dlclose (lib) != 0) {
    fprintf (stderr, "%s: could not close %s: %s\n",
             progname, LIBRARY, dlerror ());
    exit (EXIT_FAILURE);
  }

  return NULL;
}

static void *
read_symbol (void *lib, const char *symbol)
{
  void *symval;
  const char *err;

  dlerror (); /* Clear error indicator. */
  symval = dlsym (lib, symbol);
  if ((err = dlerror ()) != NULL) {
    fprintf (stderr, "could not read symbol: %s: %s\n", symbol, err);
    exit (EXIT_FAILURE);
  }
  return symval;
}
