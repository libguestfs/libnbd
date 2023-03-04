(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: Python bindings
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
extern char **nbd_internal_py_get_string_list (PyObject *);
extern void nbd_internal_py_free_string_list (char **);
extern int nbd_internal_py_get_sockaddr (PyObject *,
    struct sockaddr_storage *, socklen_t *);
extern PyObject *nbd_internal_py_get_aio_view (PyObject *, int);
extern int nbd_internal_py_init_aio_buffer (PyObject *);
extern PyObject *nbd_internal_py_get_nbd_buffer_type (void);
extern PyObject *nbd_internal_py_wrap_errptr (int);
extern PyObject *nbd_internal_py_get_subview (PyObject *, const char *, size_t);

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

  if (args != NULL) {
    PyErr_SetObject (nbd_internal_py_Error, args);
    Py_DECREF (args);
  }
}

";

  List.iter (
    fun name ->
      pr "extern PyObject *nbd_internal_py_%s (PyObject *self, PyObject *args);\n"
         name;
  ) ([ "create"; "close";
       "display_version";
       "alloc_aio_buffer";
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
       "display_version";
       "alloc_aio_buffer";
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
  if (PyModule_AddObject (mod, \"Error\", nbd_internal_py_Error) < 0) {
    Py_XDECREF (nbd_internal_py_Error);
    Py_DECREF (mod);
    return NULL;
  }

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
  pr "  int ret = -1;\n";
  pr "\n";
  pr "  PyGILState_STATE py_save = PyGILState_Ensure ();\n";
  pr "  PyObject *py_args, *py_ret;\n";
  List.iter (
    function
    | CBArrayAndLen (UInt32 n, _)
    | CBBytesIn (n, _)
    | CBMutable (Int n) ->
       pr "  PyObject *py_%s = NULL;\n" n
    | _ -> ()
  ) cbargs;
  pr "\n";
  List.iter (
    function
    | CBArrayAndLen (UInt32 n, len) ->
       pr "  py_%s = PyList_New (%s);\n" n len;
       pr "  if (!py_%s) { PyErr_PrintEx (0); goto out; }\n" n;
       pr "  size_t i_%s;\n" n;
       pr "  for (i_%s = 0; i_%s < %s; ++i_%s) {\n" n n len n;
       pr "    PyObject *py_e_%s = PyLong_FromUnsignedLong (%s[i_%s]);\n" n n n;
       pr "    if (!py_e_%s) { PyErr_PrintEx (0); goto out; }\n" n;
       pr "    PyList_SET_ITEM (py_%s, i_%s, py_e_%s);\n" n n n;
       pr "  }\n"
    | CBBytesIn (n, len) ->
       pr "  py_%s = nbd_internal_py_get_subview (data->view, %s, %s);\n" n n len;
       pr "  if (!py_%s) { PyErr_PrintEx (0); goto out; }\n" n
    | CBInt _
    | CBInt64 _ -> ()
    | CBMutable (Int n) ->
       pr "  py_%s = nbd_internal_py_wrap_errptr (*%s);\n" n n;
       pr "  if (!py_%s) { PyErr_PrintEx (0); goto out; }\n" n;
    | CBString _
    | CBUInt _
    | CBUInt64 _ -> ()
    | CBArrayAndLen _ | CBMutable _ -> assert false
  ) cbargs;
  pr "\n";

  let params =
    List.map (
      function
      | CBArrayAndLen (UInt32 n, _) -> "O", sprintf "py_%s" n
      | CBBytesIn (n, _) -> "O", sprintf "py_%s" n
      | CBInt n -> "i", n
      | CBInt64 n -> "L", n
      | CBMutable (Int n) -> "O", sprintf "py_%s" n
      | CBString n -> "s", n
      | CBUInt n -> "I", n
      | CBUInt64 n -> "K", n
      | CBArrayAndLen _ | CBMutable _ -> assert false
    ) cbargs in
  pr "  py_args = Py_BuildValue (";
  pr_wrap ',' (fun () ->
      pr "\"(";
      List.iter (
        function
        | n, _ -> pr "%s" n
        ) params;
      pr ")\"";
      List.iter (
        function
        | _, n -> pr ", %s" n
        ) params);
  pr ");\n";
  pr "  if (!py_args) { PyErr_PrintEx (0); goto out; }\n";
  pr "\n";
  pr "  py_ret = PyObject_CallObject (data->fn, py_args);\n";
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
  pr "    PyErr_PrintEx (0); /* print exception */\n";
  pr "  };\n";
  pr "\n";
  pr " out:\n";
  List.iter (
    function
    | CBArrayAndLen (UInt32 n, _) ->
       pr "  Py_XDECREF (py_%s);\n" n
    | CBBytesIn (n, _) ->
       pr "  Py_XDECREF (py_%s);\n" n
    | CBMutable (Int n) ->
       pr "  if (py_%s) {\n" n;
       pr "    PyObject *py_%s_ret = PyObject_GetAttrString (py_%s, \"value\");\n" n n;
       pr "    *%s = PyLong_AsLong (py_%s_ret);\n" n n;
       pr "    Py_DECREF (py_%s_ret);\n" n;
       pr "    Py_DECREF (py_%s);\n" n;
       pr "  }\n"
    | CBInt _ | CBInt64 _
    | CBString _
    | CBUInt _ | CBUInt64 _ -> ()
    | CBArrayAndLen _ | CBMutable _ -> assert false
  ) cbargs;
  pr "  PyGILState_Release (py_save);\n";
  pr "  return ret;\n";
  pr "}\n";
  pr "\n"

(* Generate the Python binding. *)
let print_python_binding name { args; optargs; ret; may_set_error } =
  pr "PyObject *\n";
  pr "nbd_internal_py_%s (" name;
  pr_wrap ',' (fun () -> pr "PyObject *self, PyObject *args");
  pr ")\n";
  pr "{\n";
  pr "  PyObject *py_h;\n";
  pr "  struct nbd_handle *h;\n";
  pr "  %s ret;\n" (C.type_of_ret ret);
  pr "  PyObject *py_ret = NULL;\n";
  List.iter (
    function
    | Bool n -> pr "  int %s;\n" n
    | BytesIn (n, _) ->
       pr "  Py_buffer %s = { .obj = NULL };\n" n
    | BytesOut (n, count) ->
       pr "  PyObject *%s = NULL;\n" n;
       pr "  Py_ssize_t %s;\n" count
    | BytesPersistIn (n, _)
    | BytesPersistOut (n, _) ->
       pr "  PyObject *%s; /* Buffer-like object or nbd.Buffer */\n" n;
       pr "  PyObject *%s_view = NULL; /* PyMemoryView of %s */\n" n n;
       pr "  Py_buffer *py_%s; /* buffer of %s_view */\n" n n
    | Closure { cbname } ->
       pr "  struct user_data *%s_user_data = NULL;\n" cbname;
       pr "  PyObject *py_%s_fn;\n" cbname;
       pr "  nbd_%s_callback %s = { " cbname cbname;
       pr_wrap ',' (fun () ->
           pr ".callback = %s_wrapper, .free = free_user_data" cbname);
       pr " };\n"
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
    | SizeT n ->
       pr "  Py_ssize_t %s;\n" n
    | SockAddrAndLen (n, _) ->
       pr "  PyObject *%s;\n" n;
       pr "  struct sockaddr_storage %s_sa;\n" n;
       pr "  socklen_t %s_len;\n" n;
    | String n -> pr "  const char *%s;\n" n
    | StringList n ->
       pr "  PyObject *py_%s;\n" n;
       pr "  char **%s = NULL;\n" n
    | UInt n | UIntPtr n -> pr "  unsigned int %s;\n" n
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
       pr "  struct user_data *%s_user_data = NULL;\n" cbname;
       pr "  PyObject *py_%s_fn;\n" cbname;
       pr "  nbd_%s_callback %s = { " cbname cbname;
       pr_wrap ',' (fun () ->
           pr ".callback = %s_wrapper, .free = free_user_data" cbname);
       pr " };\n"
    | OFlags (n, _, _) ->
       pr "  uint32_t %s_u32;\n" n;
       pr "  unsigned int %s; /* really uint32_t */\n" n
  ) optargs;
  pr "\n";

  (* Parse the Python parameters. *)
  let params =
    List.map (
      function
      | Bool n -> "p", sprintf "&%s" n, n
      | BytesIn (n, _) -> "y*", sprintf "&%s" n, sprintf "%s.buf, %s.len" n n
      | BytesOut (n, count) ->
         "n", sprintf "&%s" count,
         sprintf "PyByteArray_AS_STRING (%s), %s" n count
      | BytesPersistIn (n, _) | BytesPersistOut (n, _) ->
         "O", sprintf "&%s" n, sprintf "py_%s->buf, py_%s->len" n n
      | Closure { cbname } -> "O", sprintf "&py_%s_fn" cbname, cbname
      | Enum (n, _) -> "i", sprintf "&%s" n, n
      | Flags (n, _) -> "I", sprintf "&%s" n, sprintf "%s_u32" n
      | Fd n | Int n -> "i", sprintf "&%s" n, n
      | Int64 n -> "L", sprintf "&%s" n, sprintf "%s_i64" n
      | Path n -> "O&", sprintf "PyUnicode_FSConverter, &py_%s" n, n
      | SizeT n -> "n", sprintf "&%s" n, sprintf "(size_t)%s" n
      | SockAddrAndLen (n, _) ->
         "O", sprintf "&%s" n,
         sprintf "(struct sockaddr *) &%s_sa, %s_len" n n
      | String n -> "s", sprintf "&%s" n, n
      | StringList n -> "O", sprintf "&py_%s" n, n
      | UInt n | UIntPtr n -> "I", sprintf "&%s" n, n
      | UInt32 n -> "I", sprintf "&%s" n, sprintf "%s_u32" n
      | UInt64 n -> "K", sprintf "&%s" n, sprintf "%s_u64" n
    ) args in
  let optparams =
    List.map (
      function
      | OClosure { cbname } -> "O", sprintf "&py_%s_fn" cbname, cbname
      | OFlags (n, _, _) -> "I", sprintf "&%s" n, sprintf "%s_u32" n
    ) optargs in
  let params = params @ optparams in
  pr "  if (!PyArg_ParseTuple (";
  pr_wrap ',' (fun () ->
      pr "args, \"O";
      List.iter (
        function
        | n, _, _ -> pr "%s" n
        ) params;
      pr ":nbd_%s\", &py_h" name;
      List.iter (
        function
        | _, n, _ -> pr ", %s" n
        ) params);
  pr "))\n";
  pr "    goto out;\n";

  (* Two passes over parameters. Any 'goto err' must be in first pass. *)
  pr "  h = get_handle (py_h);\n";
  pr "  if (!h) goto out;\n";
  List.iter (
    function
    | OClosure { cbname } ->
       pr "  %s.user_data = %s_user_data = alloc_user_data ();\n" cbname cbname;
       pr "  if (%s_user_data == NULL) goto out;\n" cbname;
       pr "  if (py_%s_fn != Py_None) {\n" cbname;
       pr "    if (!PyCallable_Check (py_%s_fn)) {\n" cbname;
       pr "      PyErr_SetString (PyExc_TypeError,\n";
       pr "                       \"callback parameter %s is not callable\");\n" cbname;
       pr "      goto out;\n";
       pr "    }\n";
       pr "    /* Increment refcount since pointer may be saved by libnbd. */\n";
       pr "    Py_INCREF (py_%s_fn);\n" cbname;
       pr "    %s_user_data->fn = py_%s_fn;\n" cbname cbname;
       pr "  }\n";
       pr "  else\n";
       pr "    %s.callback = NULL; /* we're not going to call it */\n" cbname
    | OFlags (n, _, _) -> pr "  %s_u32 = %s;\n" n n
  ) optargs;
  List.iter (
    function
    | Bool _ -> ()
    | BytesIn _ -> ()
    | BytesOut (n, count) ->
       pr "  %s = PyByteArray_FromStringAndSize (NULL, %s);\n" n count;
       pr "  if (%s == NULL) goto out;\n" n
    | BytesPersistIn (n, _) ->
       pr "  %s_view = nbd_internal_py_get_aio_view (%s, PyBUF_READ);\n" n n;
       pr "  if (!%s_view) goto out;\n" n;
       pr "  py_%s = PyMemoryView_GET_BUFFER (%s_view);\n" n n;
       pr "  completion_user_data->view = %s_view;\n" n
    | BytesPersistOut (n, _) ->
       pr "  %s_view = nbd_internal_py_get_aio_view (%s, PyBUF_WRITE);\n" n n;
       pr "  if (!%s_view) goto out;\n" n;
       pr "  py_%s = PyMemoryView_GET_BUFFER (%s_view);\n" n n;
       pr "  completion_user_data->view = %s_view;\n" n
    | Closure { cbname } ->
       pr "  %s.user_data = %s_user_data = alloc_user_data ();\n" cbname cbname;
       pr "  if (%s_user_data == NULL) goto out;\n" cbname;
       pr "  if (!PyCallable_Check (py_%s_fn)) {\n" cbname;
       pr "    PyErr_SetString (PyExc_TypeError,\n";
       pr "                     \"callback parameter %s is not callable\");\n" cbname;
       pr "    goto out;\n";
       pr "  }\n";
       pr "  /* Increment refcount since pointer may be saved by libnbd. */\n";
       pr "  Py_INCREF (py_%s_fn);\n" cbname;
       pr "  %s_user_data->fn = py_%s_fn;\n" cbname cbname;
       if cbname = "chunk" then (
         pr "  chunk_user_data->view = nbd_internal_py_get_aio_view (buf, PyBUF_WRITE);\n";
         pr "  if (!chunk_user_data->view) goto out;\n"
       )
    | Enum _ -> ()
    | Flags (n, _) -> pr "  %s_u32 = %s;\n" n n
    | Fd _ | Int _ -> ()
    | Int64 n -> pr "  %s_i64 = %s;\n" n n
    | Path n ->
       pr "  %s = PyBytes_AS_STRING (py_%s);\n" n n;
       pr "  assert (%s != NULL);\n" n
    | SizeT n -> ()
    | SockAddrAndLen (n, _) ->
       pr "  if (nbd_internal_py_get_sockaddr (%s, &%s_sa, &%s_len) == -1)\n"
         n n n;
       pr "    goto out;\n"
    | String _ -> ()
    | StringList n ->
       pr "  %s = nbd_internal_py_get_string_list (py_%s);\n" n n;
       pr "  if (!%s) goto out;\n" n
    | UInt _ | UIntPtr _ -> ()
    | UInt32 n -> pr "  %s_u32 = %s;\n" n n
    | UInt64 n -> pr "  %s_u64 = %s;\n" n n
  ) args;
  pr "\n";

  (* Second pass, and call the underlying C function. *)
  List.iter (
    function
    | BytesPersistOut (n, _) ->
       pr "  if (nbd_internal_py_init_aio_buffer (%s) < 0) goto out;\n" n
    | _ -> ()
  ) args;
  pr "  Py_BEGIN_ALLOW_THREADS;\n";
  pr "  ret = nbd_%s (" name;
  pr_wrap ',' (fun () ->
      pr "h";
      List.iter (
        function
        | _, _, n -> pr ", %s" n
        ) params);
  pr ");\n";
  pr "  Py_END_ALLOW_THREADS;\n";
  List.iter (
    function
    | Closure { cbname } -> pr "  %s_user_data = NULL;\n" cbname
    | _ -> ()
  ) args;
  List.iter (
    function
    | OClosure { cbname } -> pr "  %s_user_data = NULL;\n" cbname
    | _ -> ()
  ) optargs;
  if may_set_error then (
    pr "  if (ret == %s) {\n"
      (match C.errcode_of_ret ret with Some s -> s | None -> assert false);
    pr "    raise_exception ();\n";
    pr "    goto out;\n";
    pr "  }\n"
  );

  (* Convert the result back to a Python object and return it. *)
  let use_ret = ref true in
  List.iter (
    function
    | BytesOut (n, _) ->
       pr "  py_ret = %s;\n" n;
       pr "  %s = NULL;\n" n;
       use_ret := false
    | _ -> ()
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
    | RFd | RInt | REnum _ | RFlags _ | RSizeT | RUInt | RUIntPtr ->
       pr "  py_ret = PyLong_FromLong (ret);\n"
    | RInt64 | RUInt64 | RCookie ->
       pr "  py_ret = PyLong_FromLongLong (ret);\n"
    | RString ->
       pr "  py_ret = PyUnicode_FromString (ret);\n";
       pr "  free (ret);\n"
  );

  pr "\n";
  pr " out:\n";
  List.iter (
    function
    | Bool _ -> ()
    | BytesIn (n, _) ->
       pr "  if (%s.obj)\n" n;
       pr "    PyBuffer_Release (&%s);\n" n
    | BytesOut (n, _) -> pr "  Py_XDECREF (%s);\n" n
    | BytesPersistIn (n, _) | BytesPersistOut (n, _) -> ()
    | Closure { cbname } ->
       pr "  free_user_data (%s_user_data);\n" cbname
    | Enum _ -> ()
    | Flags _ -> ()
    | Fd _ | Int _ -> ()
    | Int64 _ -> ()
    | Path n ->
       pr "  Py_XDECREF (py_%s);\n" n
    | SizeT _ -> ()
    | SockAddrAndLen _ -> ()
    | String n -> ()
    | StringList n -> pr "  nbd_internal_py_free_string_list (%s);\n" n
    | UInt _ -> ()
    | UInt32 _ -> ()
    | UInt64 _ -> ()
    | UIntPtr _ -> ()
  ) args;
  List.iter (
    function
    | OClosure { cbname } ->
       pr "  free_user_data (%s_user_data);\n" cbname
    | OFlags _ -> ()
  ) optargs;
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
  pr "  PyObject *view;  /* Optional PyMemoryView of persistent buffer. */\n";
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
  pr "  if (data) {\n";
  pr "    PyGILState_STATE py_save = PyGILState_Ensure ();\n";
  pr "    Py_XDECREF (data->fn);\n";
  pr "    Py_XDECREF (data->view);\n";
  pr "    PyGILState_Release (py_save);\n";
  pr "    free (data);\n";
  pr "  }\n";
  pr "}\n";
  pr "\n";

  List.iter print_python_closure_wrapper all_closures;
  List.iter (
    fun (name, fn) ->
      print_python_binding name fn
  ) handle_calls

let py_fn_rex = Str.regexp "L<nbd_\\([a-z0-9_]+\\)(3)>"
let py_const_rex = Str.regexp "C<LIBNBD_\\([A-Z0-9_]+\\)>"

let generate_python_nbd_py () =
  generate_header HashStyle;

  pr "\
'''
Python bindings for libnbd

import nbd
h = nbd.NBD()
h.connect_tcp(\"localhost\", \"nbd\")
buf = h.pread(512, 0)

Read the libnbd(3) man page to find out how to use the API.
'''

import contextlib
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

Error.string = property(lambda self: self.args[0])


def _errno(self):
    import errno
    try:
        return errno.errorcode[self.args[1]]
    except KeyError:
        return None


Error.errno = property(_errno)

Error.errnum = property(lambda self: self.args[1])


def _str(self):
    if self.errno:
        return \"%%s (%%s)\" %% (self.string, self.errno)
    else:
        return \"%%s\" %% self.string


Error.__str__ = _str


class ClosedHandle(ValueError):
    '''This exception is thrown when any method is called on an
    nbd handle after you have called h.close() on the same handle.'''
    pass


@contextlib.contextmanager
def nbd():
    '''
    This is a context manager function.  Python will close the handle
    automatically even if the body throws an exception:

    with nbd.nbd() as h:
        # use the handle 'h'
    '''
    h = NBD()
    yield h
    h.close()


";

  List.iter (
    fun { enum_prefix; enums } ->
      List.iter (
        fun (enum, i) ->
          let enum = sprintf "%s_%s" enum_prefix enum in
          pr "%s = %d\n" enum i
      ) enums;
      pr "\n"
  ) all_enums;
  List.iter (
    fun { flag_prefix; flags } ->
      let mask = ref 0 in
      List.iter (
        fun (flag, i) ->
          let flag = sprintf "%s_%s" flag_prefix flag in
          pr "%s = 0x%02x\n" flag i;
          mask := !mask lor i
      ) flags;
      pr "%s_MASK = 0x%02x\n" flag_prefix !mask;
      pr "\n"
  ) all_flags;
  List.iter (fun (n, i) -> pr "%s = %d\n" n i) constants;
  List.iter (
    fun (ns, ctxts) ->
      let ns_upper = String.uppercase_ascii ns in
      pr "NAMESPACE_%s = \"%s:\"\n" ns_upper ns;
      List.iter (
        fun (ctxt, consts) ->
          let ctxt_upper = String.uppercase_ascii ctxt in
          pr "%s = \"%s:%s\"\n"
             (sprintf "CONTEXT_%s_%s" ns_upper ctxt_upper) ns ctxt;
          List.iter (fun (n, i) -> pr "%s = %d\n" n i) consts
      ) ctxts;
  ) metadata_namespaces;

  pr "\


class Buffer(object):
    '''Asynchronous I/O persistent buffer'''

    def __init__(self, len):
        '''Allocate an uninitialized AIO buffer used for nbd.aio_pread.'''
        self._o = libnbdmod.alloc_aio_buffer(len)

    @classmethod
    def from_buffer(cls, buf):
        '''Create an AIO buffer that shares an existing buffer-like object.

        Because the buffer is shared, changes to the original are visible
        to nbd.aio_pwrite, and changes in nbd.aio_pread are visible to the
        original.
        '''
        self = cls(0)
        # Ensure that buf is already buffer-like
        with memoryview(buf):
            self._o = buf
        self._init = True
        return self

    @classmethod
    def from_bytearray(cls, ba):
        '''Create an AIO buffer from a bytearray or other buffer-like object.

        If ba is not a buffer, it is tried as the parameter to the
        bytearray constructor.  Otherwise, ba is copied.  Either way, the
        resulting AIO buffer is independent from the original.
        '''
        return cls.from_buffer(bytearray(ba))

    def to_buffer(self):
        '''Return a shared view of the AIO buffer contents.

        This exposes the underlying buffer; changes to the buffer are
        visible to nbd.aio_pwrite, and changes from nbd.aio_pread are
        visible in the buffer.
        '''
        if not hasattr(self, '_init'):
            self._o = bytearray(len(self._o))
            self._init = True
        return self._o

    def to_bytearray(self):
        '''Copy an AIO buffer into a bytearray.

        This copies the contents of an AIO buffer to a new bytearray, which
        remains independent from the original.
        '''
        if not hasattr(self, '_init'):
            return bytearray(len(self._o))
        return bytearray(self._o)

    def size(self):
        '''Return the size of an AIO buffer.'''
        return len(self)

    def __len__(self):
        '''Return the size of an AIO buffer.'''
        return len(self._o)

    def is_zero(self, offset=0, size=-1):
        '''Returns true if and only if all bytes in the buffer are zeroes.

        Note that although a freshly allocated buffer is uninitialized,
        this will report it as all zeroes, as it will be force-initialized
        to zero before any code that can access the buffer's contents.

        By default this tests the whole buffer, but you can restrict
        the test to a sub-range of the buffer using the optional
        offset and size parameters.  If size = -1 then we check from
        offset to the end of the buffer.  If size = 0, the function
        always returns true.  If size > 0, we check the interval
        [offset..offset+size-1].
        '''
        return libnbdmod.aio_buffer_is_zero(self._o, offset, size,
                                            hasattr(self, '_init'))


class NBD(object):
    '''NBD handle'''

    def __init__(self):
        '''Create a new NBD handle.'''
        self._o = libnbdmod.create()

    def __del__(self):
        '''Close the NBD handle and underlying connection.'''
        if self._o:
            libnbdmod.close(self._o)
            self._o = None

    def _check_not_closed(self):
        if not self._o:
            raise ClosedHandle(\"libnbd: method called on closed handle\")

    def close(self):
        '''Explicitly close the NBD handle and underlying connection.

        The handle is closed implicitly when its reference count goes
        to zero (eg. when it goes out of scope or the program ends).

        This call is only needed if you want to force the handle to
        close now.  After calling this, the program must not call
        any method on the handle (except the implicit call to
        __del__ which happens when the final reference is cleaned up).
        '''
        self._check_not_closed()
        libnbdmod.close(self._o)
        self._o = None

";

  List.iter (
    fun (name, { args; optargs; shortdesc; longdesc }) ->
      let args =
        List.map (
          function
          | Bool n -> n, None
          | BytesIn (n, _) -> n, None
          | BytesOut (_, count) -> count, None
          | BytesPersistIn (n, _) -> n, None
          | BytesPersistOut (n, _) -> n, None
          | Closure { cbname } -> cbname, None
          | Enum (n, _) -> n, None
          | Flags (n, _) -> n, None
          | Fd n | Int n -> n, None
          | Int64 n -> n, None
          | Path n -> n, None
          | SizeT n -> n, None
          | SockAddrAndLen (n, _) -> n, None
          | String n -> n, None
          | StringList n -> n, None
          | UInt n -> n, None
          | UInt32 n -> n, None
          | UInt64 n -> n, None
          | UIntPtr n -> n, None
        ) args in
      let optargs =
        List.map (
          function
          | OClosure { cbname } -> cbname, Some "None"
          | OFlags (n, _, _) -> n, Some "0"
        ) optargs in
      let args = args @ optargs in
      pr "    def %s(" name;
      pr_wrap ',' (fun () ->
          pr "self";
          List.iter (
            function
            | n, None -> pr ", %s" n
            | n, Some default -> pr ", %s=%s" n default
          ) args);
      pr "):\n";
      let longdesc = Str.global_replace py_fn_rex "C<nbd.\\1>" longdesc in
      let longdesc = Str.global_replace py_const_rex "C<\\1>" longdesc in
      let longdesc = pod2text longdesc in
      pr "        u'''▶ %s\n\n%s'''\n" shortdesc (String.concat "\n" longdesc);
      pr "        self._check_not_closed()\n";
      pr "        return libnbdmod.%s(" name;
      pr_wrap ',' (fun () ->
          pr "self._o";
          List.iter (
            function
            | n, _ -> pr ", %s" n
          ) args);
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
