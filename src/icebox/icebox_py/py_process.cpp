#include "bindings.hpp"

namespace
{
    opt<flags_t> to_flags(PyObject* arg)
    {
        auto flags = flags_t{};
        auto attr  = PyObject_GetAttrString(arg, "is_x64");
        if(!attr || !PyBool_Check(attr))
            return py::fail_with(ext::nullopt, PyExc_RuntimeError, "missing or invalid is_x64 attribute");

        flags.is_x64 = PyLong_AsLong(attr);
        attr         = PyObject_GetAttrString(arg, "is_x86");
        if(!attr || !PyBool_Check(attr))
            return py::fail_with(ext::nullopt, PyExc_RuntimeError, "missing or invalid is_x86 attribute");

        flags.is_x86 = PyLong_AsLong(attr);
        return flags;
    }
}

PyObject* py::process::current(core::Core& core, PyObject* /*args*/)
{
    const auto opt_proc = ::process::current(core);
    if(!opt_proc)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to read current process");

    return py::to_bytes(*opt_proc);
}

PyObject* py::process::name(core::Core& core, PyObject* args)
{
    auto obj = static_cast<PyObject*>(nullptr);
    auto ok  = PyArg_ParseTuple(args, "S", &obj);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(obj);
    if(!opt_proc)
        return nullptr;

    const auto opt_name = ::process::name(core, *opt_proc);
    if(!opt_name)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to read process name");

    return PyUnicode_FromStringAndSize(opt_name->data(), opt_name->size());
}

PyObject* py::process::is_valid(core::Core& core, PyObject* args)
{
    auto obj = static_cast<PyObject*>(nullptr);
    auto ok  = PyArg_ParseTuple(args, "S", &obj);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(obj);
    if(!opt_proc)
        return nullptr;

    ok = ::process::is_valid(core, *opt_proc);
    if(!ok)
        Py_RETURN_FALSE;

    Py_RETURN_TRUE;
}

PyObject* py::process::pid(core::Core& core, PyObject* args)
{
    auto obj = static_cast<PyObject*>(nullptr);
    auto ok  = PyArg_ParseTuple(args, "S", &obj);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(obj);
    if(!opt_proc)
        return nullptr;

    const auto pid = ::process::pid(core, *opt_proc);
    return PyLong_FromUnsignedLongLong(pid);
}

PyObject* py::process::flags(core::Core& core, PyObject* args)
{
    auto obj = static_cast<PyObject*>(nullptr);
    auto ok  = PyArg_ParseTuple(args, "S", &obj);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(obj);
    if(!opt_proc)
        return nullptr;

    const auto flags = ::process::flags(core, *opt_proc);
    return Py_BuildValue("{s:O,s:O}",
                         "is_x86", flags.is_x86 ? Py_True : Py_False,
                         "is_x64", flags.is_x64 ? Py_True : Py_False);
}

PyObject* py::process::join(core::Core& core, PyObject* args)
{
    auto obj     = static_cast<PyObject*>(nullptr);
    auto strmode = static_cast<const char*>(nullptr);
    auto ok      = PyArg_ParseTuple(args, "Ss", &obj, &strmode);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(obj);
    if(!opt_proc)
        return nullptr;

    const auto mode = strmode && strmode == std::string_view{"kernel"} ? mode_e::kernel : mode_e::user;
    ::process::join(core, *opt_proc, mode);
    Py_RETURN_NONE;
}

PyObject* py::process::parent(core::Core& core, PyObject* args)
{
    auto obj = static_cast<PyObject*>(nullptr);
    auto ok  = PyArg_ParseTuple(args, "S", &obj);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(obj);
    if(!opt_proc)
        return nullptr;

    const auto opt_parent = ::process::parent(core, *opt_proc);
    if(!opt_parent)
        Py_RETURN_NONE;

    return py::to_bytes(*opt_parent);
}

PyObject* py::process::list(core::Core& core, PyObject* /*args*/)
{
    auto list = PyList_New(0);
    if(!list)
        return nullptr;

    PY_DEFER_DECREF(list);
    const auto ok = ::process::list(core, [&](proc_t proc)
    {
        auto item = py::to_bytes(proc);
        if(!item)
            return walk_e::stop;

        PY_DEFER_DECREF(item);
        const auto err = PyList_Append(list, item);
        if(err)
            return walk_e::stop;

        return walk_e::next;
    });
    if(!ok)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to list processes");

    Py_INCREF(list);
    return list;
}

PyObject* py::process::wait(core::Core& core, PyObject* args)
{
    auto name     = static_cast<const char*>(nullptr);
    auto py_flags = static_cast<PyObject*>(nullptr);
    const auto ok = PyArg_ParseTuple(args, "sO", &name, &py_flags);
    if(!ok)
        return nullptr;

    name                 = name ? name : "";
    const auto opt_flags = to_flags(py_flags);
    const auto opt_proc  = ::process::wait(core, name, *opt_flags);
    if(!opt_proc)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to wait for process");

    return py::to_bytes(*opt_proc);
}

namespace
{
    template <typename T>
    PyObject* on_listen(core::Core& core, PyObject* args, const T& operand)
    {
        auto py_func  = static_cast<PyObject*>(nullptr);
        const auto ok = PyArg_ParseTuple(args, "O", &py_func);
        if(!ok)
            return nullptr;

        if(!PyCallable_Check(py_func))
            return py::fail_with(nullptr, PyExc_TypeError, "arg must be callable");

        const auto opt_bpid = operand(core, [=](proc_t proc)
        {
            const auto py_proc = py::to_bytes(proc);
            const auto args    = Py_BuildValue("(O)", py_proc);
            if(!args)
                return;

            PY_DEFER_DECREF(args);
            const auto ret = PyEval_CallObject(py_func, args);
            if(ret)
                PY_DEFER_DECREF(ret);
        });
        if(!opt_bpid)
            return py::fail_with(nullptr, PyExc_RuntimeError, "unable to listen");

        return py::to_bytes(*opt_bpid);
    }
}

PyObject* py::process::listen_create(core::Core& core, PyObject* args)
{
    return on_listen(core, args, &::process::listen_create);
}

PyObject* py::process::listen_delete(core::Core& core, PyObject* args)
{
    return on_listen(core, args, &::process::listen_delete);
}
