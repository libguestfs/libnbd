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

#if PY_MAJOR_VERSION == 2
#error "These bindings will not work with Python 2.  Recompile using Python 3 or use ./configure --disable-python."
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <libnbd.h>

#include "iszero.h"

#include "methods.h"

static inline PyObject *
put_handle (struct nbd_handle *h)
{
  assert (h);
  return PyCapsule_New ((void *) h, "nbd_handle", NULL);
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

static const char aio_buffer_name[] = "nbd.Buffer";

struct py_aio_buffer *
nbd_internal_py_get_aio_buffer (PyObject *capsule)
{
  return PyCapsule_GetPointer (capsule, aio_buffer_name);
}

static void
free_aio_buffer (PyObject *capsule)
{
  struct py_aio_buffer *buf = PyCapsule_GetPointer (capsule, aio_buffer_name);

  free (buf->data);
  free (buf);
}

/* Allocate a persistent buffer used for nbd_aio_pread. */
PyObject *
nbd_internal_py_alloc_aio_buffer (PyObject *self, PyObject *args)
{
  struct py_aio_buffer *buf;
  PyObject *ret;

  buf = malloc (sizeof *buf);
  if (buf == NULL) {
    PyErr_NoMemory ();
    return NULL;
  }

  if (!PyArg_ParseTuple (args, (char *) "n:nbd_internal_py_alloc_aio_buffer",
                         &buf->len)) {
    free (buf);
    return NULL;
  }

  if (buf->len < 0) {
    PyErr_SetString (PyExc_RuntimeError, "length < 0");
    free (buf);
    return NULL;
  }
  buf->data = malloc (buf->len);
  if (buf->data == NULL) {
    PyErr_NoMemory ();
    free (buf);
    return NULL;
  }

  ret = PyCapsule_New (buf, aio_buffer_name, free_aio_buffer);
  if (ret == NULL) {
    free (buf->data);
    free (buf);
    return NULL;
  }

  return ret;
}

PyObject *
nbd_internal_py_aio_buffer_from_bytearray (PyObject *self, PyObject *args)
{
  PyObject *obj;
  Py_ssize_t len;
  void *data;
  struct py_aio_buffer *buf;
  PyObject *ret;

  if (!PyArg_ParseTuple (args,
                         (char *) "O:nbd_internal_py_aio_buffer_from_bytearray",
                         &obj))
    return NULL;

  data = PyByteArray_AsString (obj);
  if (!data) {
    PyErr_SetString (PyExc_RuntimeError, "parameter is not a bytearray");
    return NULL;
  }
  len = PyByteArray_Size (obj);

  buf = malloc (sizeof *buf);
  if (buf == NULL) {
    PyErr_NoMemory ();
    return NULL;
  }

  buf->len = len;
  buf->data = malloc (len);
  if (buf->data == NULL) {
    PyErr_NoMemory ();
    free (buf);
    return NULL;
  }
  memcpy (buf->data, data, len);

  ret = PyCapsule_New (buf, aio_buffer_name, free_aio_buffer);
  if (ret == NULL) {
    free (buf->data);
    free (buf);
    return NULL;
  }

  return ret;
}

PyObject *
nbd_internal_py_aio_buffer_to_bytearray (PyObject *self, PyObject *args)
{
  PyObject *obj;
  struct py_aio_buffer *buf;

  if (!PyArg_ParseTuple (args,
                         (char *) "O:nbd_internal_py_aio_buffer_to_bytearray",
                         &obj))
    return NULL;

  buf = nbd_internal_py_get_aio_buffer (obj);
  if (buf == NULL)
    return NULL;

  return PyByteArray_FromStringAndSize (buf->data, buf->len);
}

PyObject *
nbd_internal_py_aio_buffer_size (PyObject *self, PyObject *args)
{
  PyObject *obj;
  struct py_aio_buffer *buf;

  if (!PyArg_ParseTuple (args,
                         (char *) "O:nbd_internal_py_aio_buffer_size",
                         &obj))
    return NULL;

  buf = nbd_internal_py_get_aio_buffer (obj);
  if (buf == NULL)
    return NULL;

  return PyLong_FromSsize_t (buf->len);
}

PyObject *
nbd_internal_py_aio_buffer_is_zero (PyObject *self, PyObject *args)
{
  PyObject *obj;
  struct py_aio_buffer *buf;
  Py_ssize_t offset, size;

  if (!PyArg_ParseTuple (args,
                         (char *) "Onn:nbd_internal_py_aio_buffer_is_zero",
                         &obj, &offset, &size))
    return NULL;

  if (size == 0)
    Py_RETURN_TRUE;

  buf = nbd_internal_py_get_aio_buffer (obj);
  if (buf == NULL)
    return NULL;

  /* Check the bounds of the offset. */
  if (offset < 0 || offset > buf->len) {
    PyErr_SetString (PyExc_IndexError, "offset out of range");
    return NULL;
  }

  /* Compute or check the length. */
  if (size == -1)
    size = buf->len - offset;
  else if (size < 0) {
    PyErr_SetString (PyExc_IndexError,
                     "size cannot be negative, "
                     "except -1 to mean to the end of the buffer");
    return NULL;
  }
  else if ((size_t) offset + size > buf->len) {
    PyErr_SetString (PyExc_IndexError, "size out of range");
    return NULL;
  }

  if (is_zero (buf->data + offset, size))
    Py_RETURN_TRUE;
  else
    Py_RETURN_FALSE;
}
