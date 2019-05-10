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

/* These are the hand-written Python module bindings.  The majority of
 * bindings are generated, see python/methods.c
 */

#include <config.h>

#define PY_SSIZE_T_CLEAN 1
#include <Python.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <libnbd.h>

#include "methods.h"

static inline PyObject *
put_handle (struct nbd_handle *h)
{
  assert (h);
  return PyCapsule_New ((void *) h, "nbd_handle", NULL);
}

static inline PyObject *
put_connection (struct nbd_connection *conn)
{
  assert (conn);
  return PyCapsule_New ((void *) conn, "nbd_connection", NULL);
}

PyObject *
nbd_internal_py_create (PyObject *self, PyObject *args)
{
  struct nbd_handle *h;

  if (!PyArg_ParseTuple (args, (char *) ":nbd_create"))
    return NULL;
  h = nbd_create ();
  if (h == NULL) {
    PyErr_SetString (PyExc_RuntimeError, nbd_get_error ());
    return NULL;
  }

  return put_handle (h);
}

PyObject *
nbd_internal_py_close (PyObject *self, PyObject *args)
{
  PyObject *py_h;
  struct nbd_handle *h;

  if (!PyArg_ParseTuple (args, (char *) "O:nbd_close", &py_h))
    return NULL;
  h = get_handle (py_h);

  nbd_close (h);

  Py_INCREF (Py_None);
  return Py_None;
}

PyObject *
nbd_internal_py_get_connection (PyObject *self, PyObject *args)
{
  PyObject *py_h;
  unsigned i;
  struct nbd_handle *h;
  struct nbd_connection *conn;

  if (!PyArg_ParseTuple (args, (char *) "OI:nbd_get_connection", &py_h, &i))
    return NULL;
  h = get_handle (py_h);

  conn = nbd_get_connection (h, i);
  if (!conn) {
    PyErr_SetString (PyExc_RuntimeError, nbd_get_error ());
    return NULL;
  }

  /* This is going to create a new PyObject even for the same
   * connection.  Unclear if that matters, but things should be fine
   * as long as no one cares about physical equality.
   */
  return put_connection (conn);
}

PyObject *
nbd_internal_py_connection_close (PyObject *self, PyObject *args)
{
  PyObject *py_conn;
  struct nbd_connection *conn;

  if (!PyArg_ParseTuple (args, (char *) "O:nbd_connection_close", &py_conn))
    return NULL;
  conn = get_connection (py_conn);

  nbd_connection_close (conn);

  Py_INCREF (Py_None);
  return Py_None;
}
