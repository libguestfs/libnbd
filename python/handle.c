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
#include "version.h"

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

  if (!PyArg_ParseTuple (args, ":nbd_create"))
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

  if (!PyArg_ParseTuple (args, "O:nbd_close", &py_h))
    return NULL;
  h = get_handle (py_h);

  nbd_close (h);

  Py_INCREF (Py_None);
  return Py_None;
}

/* A wrapper around common/utils/version.c:display_version, used when
 * you do "nbdsh --version".
 */
PyObject *
nbd_internal_py_display_version (PyObject *self, PyObject *args)
{
  const char *program_name;

  if (!PyArg_ParseTuple (args, "s:display_version", &program_name))
    return NULL;

  display_version (program_name);

  Py_INCREF (Py_None);
  return Py_None;
}

/* Return new reference to MemoryView wrapping buffer or aio_buffer contents.
 * buffertype is PyBUF_READ or PyBUF_WRITE, for how the buffer will be used
 * (remember, aio_pwrite READS the buffer, aio_pread WRITES into the buffer).
 */
PyObject *
nbd_internal_py_get_aio_view (PyObject *object, int buffertype)
{
  PyObject *buffer = NULL;

  if (PyObject_CheckBuffer (object))
    buffer = object;
  else if (PyObject_IsInstance (object,
                                nbd_internal_py_get_nbd_buffer_type ())) {
    buffer = PyObject_GetAttrString (object, "_o");

    if (buffertype == PyBUF_READ &&
        ! PyObject_HasAttrString (object, "_init")) {
      assert (PyByteArray_Check (buffer));
      memset (PyByteArray_AS_STRING (buffer), 0,
              PyByteArray_GET_SIZE (buffer));
      if (PyObject_SetAttrString (object, "_init", Py_True) < 0)
        return NULL;
    }
  }

  if (buffer)
    return PyMemoryView_GetContiguous (buffer, buffertype, 'A');

  PyErr_SetString (PyExc_TypeError,
                   "aio_buffer: expecting buffer or nbd.Buffer instance");
  return NULL;
}

int
nbd_internal_py_init_aio_buffer (PyObject *object)
{
  if (PyObject_IsInstance (object, nbd_internal_py_get_nbd_buffer_type ()))
    return PyObject_SetAttrString (object, "_init", Py_True);
  return 0;
}

/* Allocate an uninitialized persistent buffer used for nbd_aio_pread. */
PyObject *
nbd_internal_py_alloc_aio_buffer (PyObject *self, PyObject *args)
{
  Py_ssize_t len;

  if (!PyArg_ParseTuple (args, "n:nbd_internal_py_alloc_aio_buffer",
                         &len))
    return NULL;

  /* Constructing bytearray(len) in python zeroes the memory; doing it this
   * way gives uninitialized memory.  This correctly flags negative len.
   */
  return PyByteArray_FromStringAndSize (NULL, len);
}

PyObject *
nbd_internal_py_aio_buffer_is_zero (PyObject *self, PyObject *args)
{
  Py_buffer buf;
  Py_ssize_t offset, size;
  int init;
  PyObject *ret = NULL;

  if (!PyArg_ParseTuple (args,
                         "y*nnp:nbd_internal_py_aio_buffer_is_zero",
                         &buf, &offset, &size, &init))
    return NULL;

  if (size == 0) {
    ret = Py_True;
    goto out;
  }

  /* Check the bounds of the offset. */
  if (offset < 0 || offset > buf.len) {
    PyErr_SetString (PyExc_IndexError, "offset out of range");
    goto out;
  }

  /* Compute or check the length. */
  if (size == -1)
    size = buf.len - offset;
  else if (size < 0) {
    PyErr_SetString (PyExc_IndexError,
                     "size cannot be negative, "
                     "except -1 to mean to the end of the buffer");
    goto out;
  }
  else if ((size_t) offset + size > buf.len) {
    PyErr_SetString (PyExc_IndexError, "size out of range");
    goto out;
  }

  if (!init || is_zero (buf.buf + offset, size))
    ret = Py_True;
  else
    ret = Py_False;
 out:
  PyBuffer_Release (&buf);
  Py_XINCREF (ret);
  return ret;
}
