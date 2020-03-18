(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: Python bindings
 * Copyright (C) 2013-2020 Red Hat Inc.
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
 *)

open Printf

open API
open Utils

let generate_python_methods_h () =
  generate_header CStyle;

  pr "#ifndef LIBNBD_METHODS_H\n";
  pr "#define LIBNBD_METHODS_H\n";
  pr "\n";
  pr "#define PY_SSIZE_T_CLEAN 1\n";
  pr "#include <Python.h>\n";
  pr "\n";
  pr "#include <assert.h>\n";
  pr "\n";
  pr "\
struct py_aio_buffer {
  Py_ssize_t len;
  void *data;
};

extern char **nbd_internal_py_get_string_list (PyObject *);
extern void nbd_internal_py_free_string_list (char **);
extern struct py_aio_buffer *nbd_internal_py_get_aio_buffer (PyObject *);

static inline struct nbd_handle *
get_handle (PyObject *obj)
{
  assert (obj);
  assert (obj != Py_None);
  return (struct nbd_handle *) PyCapsule_GetPointer(obj, \"nbd_handle\");
}

/* nbd.Error exception. */
extern PyObject *nbd_internal_py_Error;

static inline void
raise_exception ()
{
  PyObject *args = Py_BuildValue (\"si\", nbd_get_error (), nbd_get_errno ());

  if (args != NULL)
    PyErr_SetObject (nbd_internal_py_Error, args);
}

";

  List.iter (
    fun name ->
      pr "extern PyObject *nbd_internal_py_%s (PyObject *self, PyObject *args);\n"
         name;
  ) ([ "create"; "close";
       "alloc_aio_buffer";
       "aio_buffer_from_bytearray";
       "aio_buffer_to_bytearray";
       "aio_buffer_size";
       "aio_buffer_is_zero" ] @ List.map fst handle_calls);

  pr "\n";
  pr "#endif /* LIBNBD_METHODS_H */\n"

let generate_python_libnbdmod_c () =
  generate_header CStyle;

  pr "#include <config.h>\n";
  pr "\n";
  pr "#define PY_SSIZE_T_CLEAN 1\n";
  pr "#include <Python.h>\n";
  pr "\n";
  pr "#include <stdio.h>\n";
  pr "#include <stdlib.h>\n";
  pr "#include <assert.h>\n";
  pr "\n";
  pr "#include <libnbd.h>\n";
  pr "\n";
  pr "#include \"methods.h\"\n";
  pr "\n";
  pr "static PyMethodDef methods[] = {\n";
  List.iter (
    fun name ->
      pr "  { (char *) \"%s\", nbd_internal_py_%s, METH_VARARGS, NULL },\n"
         name name;
  ) ([ "create"; "close";
       "alloc_aio_buffer";
       "aio_buffer_from_bytearray";
       "aio_buffer_to_bytearray";
       "aio_buffer_size";
       "aio_buffer_is_zero" ] @ List.map fst handle_calls);
  pr "  { NULL, NULL, 0, NULL }\n";
  pr "};\n";
  pr "\n";
  pr "\
static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  \"libnbdmod\",           /* m_name */
  \"libnbd module\",       /* m_doc */
  -1,                    /* m_size */
  methods,               /* m_methods */
  NULL,                  /* m_reload */
  NULL,                  /* m_traverse */
  NULL,                  /* m_clear */
  NULL,                  /* m_free */
};

/* nbd.Error exception. */
PyObject *nbd_internal_py_Error;

extern PyMODINIT_FUNC PyInit_libnbdmod (void);

PyMODINIT_FUNC
PyInit_libnbdmod (void)
{
  PyObject *mod;

  mod = PyModule_Create (&moduledef);
  if (mod == NULL)
    return NULL;

  nbd_internal_py_Error = PyErr_NewException (\"nbd.Error\", NULL, NULL);
  if (nbd_internal_py_Error == NULL)
    return NULL;
  PyModule_AddObject (mod, \"Error\", nbd_internal_py_Error);

  return mod;
}
"

(* Functions with a Closure parameter are special because we
 * have to generate wrapper functions which translate the
 * callbacks back to Python.
 *)
let print_python_closure_wrapper { cbname; cbargs } =
  pr "/* Wrapper for %s callback. */\n" cbname;
  pr "static int\n";
  pr "%s_wrapper " cbname;
  C.print_cbarg_list ~wrap:true cbargs;
  pr "\n";
  pr "{\n";
  pr "  const struct user_data *data = user_data;\n";
  pr "  int ret = 0;\n";
  pr "\n";
  pr "  PyGILState_STATE py_save = PyGILState_UNLOCKED;\n";
  pr "  PyObject *py_args, *py_ret;\n";
  List.iter (
    function
    | CBArrayAndLen (UInt32 n, len) ->
       pr "  PyObject *py_%s = PyList_New (%s);\n" n len;
       pr "  for (size_t i = 0; i < %s; ++i)\n" len;
       pr "    PyList_SET_ITEM (py_%s, i, PyLong_FromUnsignedLong (%s[i]));\n" n n
    | CBBytesIn _
    | CBInt _
    | CBInt64 _ -> ()
    | CBMutable (Int n) ->
       pr "  PyObject *py_%s_modname = PyUnicode_FromString (\"ctypes\");\n" n;
       pr "  if (!py_%s_modname) { PyErr_PrintEx (0); return -1; }\n" n;
       pr "  PyObject *py_%s_mod = PyImport_Import (py_%s_modname);\n" n n;
       pr "  Py_DECREF (py_%s_modname);\n" n;
       pr "  if (!py_%s_mod) { PyErr_PrintEx (0); return -1; }\n" n;
       pr "  PyObject *py_%s = PyObject_CallMethod (py_%s_mod, \"c_int\", \"i\", *%s);\n" n n n;
       pr "  if (!py_%s) { PyErr_PrintEx (0); return -1; }\n" n;
    | CBString _
    | CBUInt _
    | CBUInt64 _ -> ()
    | CBArrayAndLen _ | CBMutable _ -> assert false
  ) cbargs;
  pr "\n";

  pr "  py_args = Py_BuildValue (\"(\"";
  List.iter (
    function
    | CBArrayAndLen (UInt32 n, len) -> pr " \"O\""
    | CBBytesIn (n, len) -> pr " \"y#\""
    | CBInt n -> pr " \"i\""
    | CBInt64 n -> pr " \"L\""
    | CBMutable (Int n) -> pr " \"O\""
    | CBString n -> pr " \"s\""
    | CBUInt n -> pr " \"I\""
    | CBUInt64 n -> pr " \"K\""
    | CBArrayAndLen _ | CBMutable _ -> assert false
  ) cbargs;
  pr " \")\"";
  List.iter (
    function
    | CBArrayAndLen (UInt32 n, _) -> pr ", py_%s" n
    | CBBytesIn (n, len) -> pr ", %s, (int) %s" n len
    | CBMutable (Int n) -> pr ", py_%s" n
    | CBInt n | CBInt64 n
    | CBString n
    | CBUInt n | CBUInt64 n -> pr ", %s" n
    | CBArrayAndLen _ | CBMutable _ -> assert false
  ) cbargs;
  pr ");\n";
  pr "  Py_INCREF (py_args);\n";
  pr "\n";
  pr "  if (PyEval_ThreadsInitialized ())\n";
  pr "    py_save = PyGILState_Ensure ();\n";
  pr "\n";
  pr "  py_ret = PyObject_CallObject (data->fn, py_args);\n";
  pr "\n";
  pr "  if (PyEval_ThreadsInitialized ())\n";
  pr "    PyGILState_Release (py_save);\n";
  pr "\n";
  pr "  Py_DECREF (py_args);\n";
  pr "\n";
  pr "  if (py_ret != NULL) {\n";
  pr "    if (PyLong_Check (py_ret))\n";
  pr "      ret = PyLong_AsLong (py_ret);\n";
  pr "    else\n";
  pr "      /* If it's not a long, just assume it's 0. */\n";
  pr "      ret = 0;\n";
  pr "    Py_DECREF (py_ret);\n";
  pr "  }\n";
  pr "  else {\n";
  pr "    /* Special case failed assertions to be fatal. */\n";
  pr "    if (PyErr_ExceptionMatches (PyExc_AssertionError)) {\n";
  pr "      PyErr_Print ();\n";
  pr "      abort ();\n";
  pr "    }\n";
  pr "    ret = -1;\n";
  pr "    PyErr_PrintEx (0); /* print exception */\n";
  pr "  };\n";
  pr "\n";
  List.iter (
    function
    | CBArrayAndLen (UInt32 n, _) ->
       pr "  Py_DECREF (py_%s);\n" n
    | CBMutable (Int n) ->
       pr "  PyObject *py_%s_ret = PyObject_GetAttrString (py_%s, \"value\");\n" n n;
       pr "  *%s = PyLong_AsLong (py_%s_ret);\n" n n;
       pr "  Py_DECREF (py_%s_ret);\n" n;
       pr "  Py_DECREF (py_%s);\n" n
    | CBBytesIn _
    | CBInt _ | CBInt64 _
    | CBString _
    | CBUInt _ | CBUInt64 _ -> ()
    | CBArrayAndLen _ | CBMutable _ -> assert false
  ) cbargs;
  pr "  return ret;\n";
  pr "}\n";
  pr "\n"

(* Generate the Python binding. *)
let print_python_binding name { args; optargs; ret; may_set_error } =
  pr "PyObject *\n";
  pr "nbd_internal_py_%s (PyObject *self, PyObject *args)\n" name;
  pr "{\n";
  pr "  PyObject *py_h;\n";
  pr "  struct nbd_handle *h;\n";
  pr "  %s ret;\n" (C.type_of_ret ret);
  pr "  PyObject *py_ret;\n";
  List.iter (
    function
    | Bool n -> pr "  int %s;\n" n
    | BytesIn (n, _) ->
       pr "  Py_buffer %s;\n" n
    | BytesOut (n, count) ->
       pr "  char *%s;\n" n;
       pr "  Py_ssize_t %s;\n" count
    | BytesPersistIn (n, _)
    | BytesPersistOut (n, _) ->
       pr "  PyObject *%s; /* PyCapsule pointing to struct py_aio_buffer */\n"
          n;
       pr "  struct py_aio_buffer *%s_buf;\n" n
    | Closure { cbname } ->
       pr "  struct user_data *%s_user_data = alloc_user_data ();\n" cbname;
       pr "  if (%s_user_data == NULL) return NULL;\n" cbname;
       pr "  nbd_%s_callback %s = { .callback = %s_wrapper,\n"
         cbname cbname cbname;
       pr "                         .user_data = %s_user_data,\n" cbname;
       pr "                         .free = free_user_data };\n"
    | Enum (n, _) -> pr "  int %s;\n" n
    | Flags (n, _) ->
       pr "  uint32_t %s_u32;\n" n;
       pr "  unsigned int %s; /* really uint32_t */\n" n
    | Fd n | Int n -> pr "  int %s;\n" n
    | Int64 n ->
       pr "  int64_t %s_i64;\n" n;
       pr "  long long %s; /* really int64_t */\n" n
    | Path n ->
       pr "  PyObject *py_%s = NULL;\n" n;
       pr "  char *%s = NULL;\n" n
    | SockAddrAndLen (n, _) ->
       pr "  /* XXX Complicated - Python uses a tuple of different\n";
       pr "   * lengths for the different socket types.\n";
       pr "   */\n";
       pr "  PyObject *%s;\n" n
    | String n -> pr "  const char *%s;\n" n
    | StringList n ->
       pr "  PyObject *py_%s;\n" n;
       pr "  char **%s = NULL;\n" n
    | UInt n -> pr "  unsigned int %s;\n" n
    | UInt32 n ->
       pr "  uint32_t %s_u32;\n" n;
       pr "  unsigned int %s; /* really uint32_t */\n" n
    | UInt64 n ->
       pr "  uint64_t %s_u64;\n" n;
       pr "  unsigned long long %s; /* really uint64_t */\n" n
  ) args;
  List.iter (
    function
    | OClosure { cbname } ->
       pr "  struct user_data *%s_user_data = alloc_user_data ();\n" cbname;
       pr "  if (%s_user_data == NULL) return NULL;\n" cbname;
       pr "  nbd_%s_callback %s = { .callback = %s_wrapper,\n"
         cbname cbname cbname;
       pr "                         .user_data = %s_user_data,\n" cbname;
       pr "                         .free = free_user_data };\n"
    | OFlags (n, _) ->
       pr "  uint32_t %s_u32;\n" n;
       pr "  unsigned int %s; /* really uint32_t */\n" n
  ) optargs;
  pr "\n";

  (* Parse the Python parameters. *)
  pr "  if (!PyArg_ParseTuple (args, (char *) \"O\"";
  List.iter (
    function
    | Bool n -> pr " \"b\""
    | BytesIn (n, _) -> pr " \"y*\""
    | BytesPersistIn (n, _) -> pr " \"O\""
    | BytesOut (_, count) -> pr " \"n\""
    | BytesPersistOut (_, count) -> pr " \"O\""
    | Closure _ -> pr " \"O\""
    | Enum _ -> pr " \"i\""
    | Flags _ -> pr " \"I\""
    | Fd n | Int n -> pr " \"i\""
    | Int64 n -> pr " \"L\""
    | Path n -> pr " \"O&\""
    | SockAddrAndLen (n, _) -> pr " \"O\""
    | String n -> pr " \"s\""
    | StringList n -> pr " \"O\""
    | UInt n -> pr " \"I\""
    | UInt32 n -> pr " \"I\""
    | UInt64 n -> pr " \"K\""
  ) args;
  List.iter (
    function
    | OClosure _ -> pr " \"O\""
    | OFlags _ -> pr " \"I\""
  ) optargs;
  pr "\n";
  pr "                         \":nbd_%s\",\n" name;
  pr "                         &py_h";
  List.iter (
    function
    | Bool n -> pr ", &%s" n
    | BytesIn (n, _) | BytesPersistIn (n, _)
    | BytesPersistOut (n, _) -> pr ", &%s" n
    | BytesOut (_, count) -> pr ", &%s" count
    | Closure { cbname } -> pr ", &%s_user_data->fn" cbname
    | Enum (n, _) -> pr ", &%s" n
    | Flags (n, _) -> pr ", &%s" n
    | Fd n | Int n -> pr ", &%s" n
    | Int64 n -> pr ", &%s" n
    | Path n -> pr ", PyUnicode_FSConverter, &py_%s" n
    | SockAddrAndLen (n, _) -> pr ", &%s" n
    | String n -> pr ", &%s" n
    | StringList n -> pr ", &py_%s" n
    | UInt n -> pr ", &%s" n
    | UInt32 n -> pr ", &%s" n
    | UInt64 n -> pr ", &%s" n
  ) args;
  List.iter (
    function
    | OClosure { cbname } -> pr ", &%s_user_data->fn" cbname
    | OFlags (n, _) -> pr ", &%s" n
  ) optargs;
  pr "))\n";
  pr "    return NULL;\n";

  pr "  h = get_handle (py_h);\n";
  List.iter (
    function
    | Bool _ -> ()
    | BytesIn _ -> ()
    | BytesOut (n, count) ->
       pr "  %s = malloc (%s);\n" n count
    | BytesPersistIn (n, _) | BytesPersistOut (n, _) ->
       pr "  %s_buf = nbd_internal_py_get_aio_buffer (%s);\n" n n
    | Closure { cbname } ->
       pr "  /* Increment refcount since pointer may be saved by libnbd. */\n";
       pr "  Py_INCREF (%s_user_data->fn);\n" cbname;
       pr "  if (!PyCallable_Check (%s_user_data->fn)) {\n" cbname;
       pr "    PyErr_SetString (PyExc_TypeError,\n";
       pr "                     \"callback parameter %s is not callable\");\n" cbname;
       pr "    return NULL;\n";
       pr "  }\n"
    | Enum _ -> ()
    | Flags (n, _) -> pr "  %s_u32 = %s;\n" n n
    | Fd _ | Int _ -> ()
    | Int64 n -> pr "  %s_i64 = %s;\n" n n
    | Path n ->
       pr "  %s = PyBytes_AS_STRING (py_%s);\n" n n;
       pr "  assert (%s != NULL);\n" n
    | SockAddrAndLen _ ->
       pr "  abort (); /* XXX SockAddrAndLen not implemented */\n";
    | String _ -> ()
    | StringList n ->
       pr "  %s = nbd_internal_py_get_string_list (py_%s);\n" n n;
       pr "  if (!%s) { py_ret = NULL; goto out; }\n" n
    | UInt _ -> ()
    | UInt32 n -> pr "  %s_u32 = %s;\n" n n
    | UInt64 n -> pr "  %s_u64 = %s;\n" n n
  ) args;
  List.iter (
    function
    | OClosure { cbname } ->
       pr "  if (%s_user_data->fn != Py_None) {\n" cbname;
       pr "    /* Increment refcount since pointer may be saved by libnbd. */\n";
       pr "    Py_INCREF (%s_user_data->fn);\n" cbname;
       pr "    if (!PyCallable_Check (%s_user_data->fn)) {\n" cbname;
       pr "      PyErr_SetString (PyExc_TypeError,\n";
       pr "                       \"callback parameter %s is not callable\");\n" cbname;
       pr "      return NULL;\n";
       pr "    }\n";
       pr "  }\n";
       pr "  else\n";
       pr "    %s.callback = NULL; /* we're not going to call it */\n" cbname
    | OFlags (n, _) -> pr "  %s_u32 = %s;\n" n n
  ) optargs;

  (* If there is a BytesPersistIn/Out parameter then we need to
   * increment the refcount and save the pointer into
   * completion_callback.user_data so we can decrement the
   * refcount on command completion.
   *)
  List.iter (
    function
    | BytesPersistIn (n, _) | BytesPersistOut (n, _) ->
       pr "  /* Increment refcount since buffer may be saved by libnbd. */\n";
       pr "  Py_INCREF (%s);\n" n;
       pr "  completion_user_data->buf = %s;\n" n;
    | _ -> ()
  ) args;

  (* Call the underlying C function. *)
  pr "  ret = nbd_%s (h" name;
  List.iter (
    function
    | Bool n -> pr ", %s" n
    | BytesIn (n, _) -> pr ", %s.buf, %s.len" n n
    | BytesOut (n, count) -> pr ", %s, %s" n count
    | BytesPersistIn (n, _)
    | BytesPersistOut (n, _) -> pr ", %s_buf->data, %s_buf->len" n n
    | Closure { cbname } -> pr ", %s" cbname
    | Enum (n, _) -> pr ", %s" n
    | Flags (n, _) -> pr ", %s_u32" n
    | Fd n | Int n -> pr ", %s" n
    | Int64 n -> pr ", %s_i64" n
    | Path n -> pr ", %s" n
    | SockAddrAndLen (n, _) -> pr ", /* XXX */ (void *) %s, 0" n
    | String n -> pr ", %s" n
    | StringList n -> pr ", %s" n
    | UInt n -> pr ", %s" n
    | UInt32 n -> pr ", %s_u32" n
    | UInt64 n -> pr ", %s_u64" n
  ) args;
  List.iter (
    function
    | OClosure { cbname } -> pr ", %s" cbname
    | OFlags (n, _) -> pr ", %s_u32" n
  ) optargs;
  pr ");\n";
  if may_set_error then (
    pr "  if (ret == %s) {\n"
      (match C.errcode_of_ret ret with Some s -> s | None -> assert false);
    pr "    raise_exception ();\n";
    pr "    py_ret = NULL;\n";
    pr "    goto out;\n";
    pr "  }\n"
  );

  (* Convert the result back to a Python object and return it. *)
  let use_ret = ref true in
  List.iter (
    function
    | BytesOut (n, count) ->
       pr "  py_ret = PyBytes_FromStringAndSize (%s, %s);\n" n count;
       use_ret := false
    | Bool _
    | BytesIn _
    | BytesPersistIn _ | BytesPersistOut _
    | Closure _
    | Enum _
    | Flags _
    | Fd _ | Int _
    | Int64 _
    | Path _
    | SockAddrAndLen _
    | String _
    | StringList _
    | UInt _
    | UInt32 _
    | UInt64 _ -> ()
  ) args;
  if !use_ret then (
    match ret with
    | RBool ->
       pr "  py_ret = ret ? Py_True : Py_False;\n";
       pr "  Py_INCREF (py_ret);\n"
    | RStaticString ->
       pr "  py_ret = PyUnicode_FromString (ret);\n"
    | RErr ->
       pr "  py_ret = Py_None;\n";
       pr "  Py_INCREF (py_ret);\n"
    | RFd
    | RInt
    | RUInt ->
       pr "  py_ret = PyLong_FromLong (ret);\n"
    | RInt64 | RCookie ->
       pr "  py_ret = PyLong_FromLongLong (ret);\n"
    | RString ->
       pr "  py_ret = PyUnicode_FromString (ret);\n";
       pr "  free (ret);\n"
  );

  pr "\n";
  if may_set_error then
    pr " out:\n";
  List.iter (
    function
    | Bool _ -> ()
    | BytesIn (n, _) -> pr "  PyBuffer_Release (&%s);\n" n
    | BytesPersistIn _ | BytesOut _ | BytesPersistOut _ -> ()
    | Closure _ -> ()
    | Enum _ -> ()
    | Flags _ -> ()
    | Fd _ | Int _ -> ()
    | Int64 _ -> ()
    | Path n ->
       pr "  Py_XDECREF (py_%s);\n" n
    | SockAddrAndLen _ -> ()
    | String n -> ()
    | StringList n -> pr "  nbd_internal_py_free_string_list (%s);\n" n
    | UInt _ -> ()
    | UInt32 _ -> ()
    | UInt64 _ -> ()
  ) args;
  pr "  return py_ret;\n";
  pr "}\n";
  pr "\n"

let generate_python_methods_c () =
  generate_header CStyle;

  pr "#define PY_SSIZE_T_CLEAN 1\n";
  pr "#include <Python.h>\n";
  pr "\n";
  pr "#include <stdio.h>\n";
  pr "#include <stdlib.h>\n";
  pr "#include <stdint.h>\n";
  pr "#include <stdbool.h>\n";
  pr "\n";
  pr "#include <libnbd.h>\n";
  pr "\n";
  pr "#include <methods.h>\n";
  pr "\n";

  pr "/* This is passed to *_wrapper as the user_data pointer\n";
  pr " * and freed in the free_user_data function below.\n";
  pr " */\n";
  pr "struct user_data {\n";
  pr "  PyObject *fn;    /* Optional pointer to Python function. */\n";
  pr "  PyObject *buf;   /* Optional pointer to persistent buffer. */\n";
  pr "};\n";
  pr "\n";
  pr "static struct user_data *\n";
  pr "alloc_user_data (void)\n";
  pr "{\n";
  pr "  struct user_data *data = calloc (1, sizeof *data);\n";
  pr "  if (data == NULL) {\n";
  pr "    PyErr_NoMemory ();\n";
  pr "    return NULL;\n";
  pr "  }\n";
  pr "  return data;\n";
  pr "}\n";
  pr "\n";
  pr "static void\n";
  pr "free_user_data (void *user_data)\n";
  pr "{\n";
  pr "  struct user_data *data = user_data;\n";
  pr "\n";
  pr "  if (data->fn != NULL)\n";
  pr "    Py_DECREF (data->fn);\n";
  pr "  if (data->buf != NULL)\n";
  pr "    Py_DECREF (data->buf);\n";
  pr "  free (data);\n";
  pr "}\n";
  pr "\n";

  List.iter print_python_closure_wrapper all_closures;
  List.iter (
    fun (name, fn) ->
      print_python_binding name fn
  ) handle_calls

let py_fn_rex = Str.regexp "L<nbd_\\([a-z0-9_]+\\)(3)>"
let py_const_rex = Str.regexp "C<LIBNBD_"

let generate_python_nbd_py () =
  generate_header HashStyle;

  pr "\
'''
Python bindings for libnbd

import nbd
h = nbd.NBD ()
h.connect_tcp (\"localhost\", \"nbd\")
buf = h.pread (512, 0)

Read the libnbd(3) man page to find out how to use the API.
'''

import libnbdmod

# Re-export Error exception as nbd.Error, adding some methods.
from libnbdmod import Error

Error.__doc__ = '''
Exception thrown when the underlying libnbd call fails.

This exception has three properties to query the error.  Use
the .string property to return a printable string containing
the error message.  Use the .errnum property for the associated
numeric error value (which may be 0 if the error did not
correspond to a system call failure), or the .errno property to
return a string containing the Python errno name if one is known
(which may be None if the numeric value does not correspond to
a known errno name).
'''

Error.string = property (lambda self: self.args[0])

def _errno (self):
    import errno
    try:
        return errno.errorcode[self.args[1]]
    except KeyError:
        return None
Error.errno = property (_errno)

Error.errnum = property (lambda self: self.args[1])

def _str (self):
    if self.errno:
        return (\"%%s (%%s)\" %% (self.string, self.errno))
    else:
        return (\"%%s\" %% self.string)
Error.__str__ = _str

";

  List.iter (
    fun { enum_prefix; enums } ->
      List.iter (
        fun (enum, i) ->
          let enum = sprintf "%s_%s" enum_prefix enum in
          pr "%-30s = %d\n" enum i
      ) enums;
      pr "\n"
  ) all_enums;
  List.iter (
    fun { flag_prefix; flags } ->
      List.iter (
        fun (flag, i) ->
          let flag = sprintf "%s_%s" flag_prefix flag in
          pr "%-30s = %d\n" flag i
      ) flags;
      pr "\n"
  ) all_flags;
  List.iter (fun (n, i) -> pr "%-30s = %d\n" n i) constants;
  List.iter (
    fun (ns, ctxts) ->
      let ns_upper = String.uppercase_ascii ns in
      pr "NAMESPACE_%-20s = \"%s:\"\n" ns_upper ns;
      List.iter (
        fun (ctxt, consts) ->
          let ctxt_upper = String.uppercase_ascii ctxt in
          pr "%-30s = \"%s:%s\"\n"
             (sprintf "CONTEXT_%s_%s" ns_upper ctxt_upper) ns ctxt;
          List.iter (fun (n, i) -> pr "%-30s = %d\n" n i) consts
      ) ctxts;
  ) metadata_namespaces;

  pr "\

class Buffer (object):
    '''Asynchronous I/O persistent buffer'''

    def __init__ (self, len):
        '''allocate an uninitialized AIO buffer used for nbd.aio_pread'''
        self._o = libnbdmod.alloc_aio_buffer (len)

    @classmethod
    def from_bytearray (cls, ba):
        '''create an AIO buffer from a bytearray'''
        o = libnbdmod.aio_buffer_from_bytearray (ba)
        self = cls (0)
        self._o = o
        return self

    def to_bytearray (self):
        '''copy an AIO buffer into a bytearray'''
        return libnbdmod.aio_buffer_to_bytearray (self._o)

    def size (self):
        '''return the size of an AIO buffer'''
        return libnbdmod.aio_buffer_size (self._o)

    def is_zero (self, offset=0, size=-1):
        '''
        Returns true if and only if all bytes in the buffer are zeroes.
        Note that a freshly allocated buffer is uninitialized, not zero.

        By default this tests the whole buffer, but you can restrict
        the test to a sub-range of the buffer using the optional
        offset and size parameters.  If size = -1 then we check from
        offset to the end of the buffer.  If size = 0, the function
        always returns true.  If size > 0, we check the interval
        [offset..offset+size-1].
        '''
        return libnbdmod.aio_buffer_is_zero (self._o, offset, size)

class NBD (object):
    '''NBD handle'''

    def __init__ (self):
        '''create a new NBD handle'''
        self._o = libnbdmod.create ()

    def __del__ (self):
        '''close the NBD handle and underlying connection'''
        libnbdmod.close (self._o)

";

  List.iter (
    fun (name, { args; optargs; shortdesc; longdesc }) ->
      let args =
        List.map (
          function
          | Bool n -> n, None, None
          | BytesIn (n, _) -> n, None, None
          | BytesOut (_, count) -> count, None, None
          | BytesPersistIn (n, _) -> n, None, Some (sprintf "%s._o" n)
          | BytesPersistOut (n, _) -> n, None, Some (sprintf "%s._o" n)
          | Closure { cbname } -> cbname, None, None
          | Enum (n, _) -> n, None, None
          | Flags (n, _) -> n, None, None
          | Fd n | Int n -> n, None, None
          | Int64 n -> n, None, None
          | Path n -> n, None, None
          | SockAddrAndLen (n, _) -> n, None, None
          | String n -> n, None, None
          | StringList n -> n, None, None
          | UInt n -> n, None, None
          | UInt32 n -> n, None, None
          | UInt64 n -> n, None, None
        ) args in
      let optargs =
        List.map (
          function
          | OClosure { cbname } -> cbname, Some "None", None
          | OFlags (n, _) -> n, Some "0", None
        ) optargs in
      let args = args @ optargs in
      pr "    def %s (self" name;
      List.iter (
        function
        | n, None, _ -> pr ", %s" n
        | n, Some default, _ -> pr ", %s=%s" n default
      ) args;
      pr "):\n";
      let longdesc = Str.global_replace py_fn_rex "C<nbd.\\1>" longdesc in
      let longdesc = Str.global_replace py_const_rex "C<" longdesc in
      let longdesc = pod2text longdesc in
      pr "        '''▶ %s\n\n%s'''\n" shortdesc (String.concat "\n" longdesc);
      pr "        return libnbdmod.%s (self._o" name;
      List.iter (
        function
        | _, _, Some getter -> pr ", %s" getter
        | n, _, None -> pr ", %s" n
      ) args;
      pr ")\n";
      pr "\n"
  ) handle_calls;

  (* For nbdsh. *)
  pr "\
package_name = NBD().get_package_name()
__version__ = NBD().get_version()

if __name__ == \"__main__\":
    import nbdsh

    nbdsh.shell()
"
