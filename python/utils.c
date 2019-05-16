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

/* Miscellaneous helper functions for Python. */

#include <config.h>

#define PY_SSIZE_T_CLEAN 1
#include <Python.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <libnbd.h>

#include "methods.h"

/* These two functions are used when parsing argv parameters. */
char **
nbd_internal_py_get_string_list (PyObject *obj)
{
  size_t i, len;
  char **r;

  assert (obj);

  if (!PyList_Check (obj)) {
    PyErr_SetString (PyExc_TypeError, "expecting a list parameter");
    return NULL;
  }

  Py_ssize_t slen = PyList_Size (obj);
  if (slen == -1) {
    PyErr_SetString (PyExc_RuntimeError,
                     "get_string_list: PyList_Size failure");
    return NULL;
  }
  len = (size_t) slen;
  r = malloc (sizeof (char *) * (len+1));
  if (r == NULL) {
    PyErr_NoMemory ();
    return NULL;
  }

  for (i = 0; i < len; ++i) {
    PyObject *bytes = PyUnicode_AsUTF8String (PyList_GetItem (obj, i));
    r[i] = strdup (PyBytes_AS_STRING (bytes));
    if (r[i] == NULL) {
      PyErr_NoMemory ();
      return NULL;
    }
  }
  r[len] = NULL;

  return r;
}

void
nbd_internal_py_free_string_list (char **argv)
{
  size_t i;

  if (!argv)
    return;

  for (i = 0; argv[i] != NULL; ++i)
    free (argv[i]);
  free (argv);
}
