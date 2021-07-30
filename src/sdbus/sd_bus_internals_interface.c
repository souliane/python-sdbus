// SPDX-License-Identifier: LGPL-2.1-or-later
/*
    Copyright (C) 2020, 2021 igo95862

    This file is part of python-sdbus

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/
#include "sd_bus_internals.h"

// TODO: adding interface to different buses, recalculating vtable

static int SdBusInterface_init(SdBusInterfaceObject* self, PyObject* Py_UNUSED(args), PyObject* Py_UNUSED(kwds)) {
        self->interface_slot = (SdBusSlotObject*)CALL_PYTHON_CHECK_RETURN_NEG1(PyObject_CallFunctionObjArgs((PyObject*)&SdBusSlotType, NULL));
        self->method_list = CALL_PYTHON_CHECK_RETURN_NEG1(PyList_New((Py_ssize_t)0));
        self->method_dict = CALL_PYTHON_CHECK_RETURN_NEG1(PyDict_New());
        self->property_list = CALL_PYTHON_CHECK_RETURN_NEG1(PyList_New((Py_ssize_t)0));
        self->property_get_dict = CALL_PYTHON_CHECK_RETURN_NEG1(PyDict_New());
        self->property_set_dict = CALL_PYTHON_CHECK_RETURN_NEG1(PyDict_New());
        self->signal_list = CALL_PYTHON_CHECK_RETURN_NEG1(PyList_New((Py_ssize_t)0));
        self->vtable = NULL;
        return 0;
}

static void SdBusInterface_free(SdBusInterfaceObject* self) {
        Py_XDECREF(self->interface_slot);
        Py_XDECREF(self->method_list);
        Py_XDECREF(self->method_dict);
        Py_XDECREF(self->property_list);
        Py_XDECREF(self->property_get_dict);
        Py_XDECREF(self->property_set_dict);
        Py_XDECREF(self->signal_list);
        if (self->vtable) {
                free(self->vtable);
        }
        PyObject_Free(self);
}

inline int _check_callable_or_none(PyObject* some_object) {
        return PyCallable_Check(some_object) || (Py_None == some_object);
}

static PyObject* SdBusInterface_add_property(SdBusInterfaceObject* self, PyObject* const* args, Py_ssize_t nargs) {
        // Arguments
        // Name, Signature, Get, Set, Flags
        SD_BUS_PY_CHECK_ARGS_NUMBER(5);
        SD_BUS_PY_CHECK_ARG_TYPE(0, PyUnicode_Type);
        SD_BUS_PY_CHECK_ARG_TYPE(1, PyUnicode_Type);
        SD_BUS_PY_CHECK_ARG_CHECK_FUNC(2, PyCallable_Check);
        SD_BUS_PY_CHECK_ARG_CHECK_FUNC(3, _check_callable_or_none);
        SD_BUS_PY_CHECK_ARG_CHECK_FUNC(4, PyLong_Check);

        PyObject* new_tuple CLEANUP_PY_OBJECT = CALL_PYTHON_AND_CHECK(PyTuple_Pack(4, args[0], args[1], args[4], args[3]));

        CALL_PYTHON_INT_CHECK(PyList_Append(self->property_list, new_tuple));
        CALL_PYTHON_INT_CHECK(PyDict_SetItem(self->property_get_dict, args[0], args[2]));
        CALL_PYTHON_INT_CHECK(PyDict_SetItem(self->property_set_dict, args[0], args[3]));

        Py_RETURN_NONE;
}

static PyObject* SdBusInterface_add_method(SdBusInterfaceObject* self, PyObject* const* args, Py_ssize_t nargs) {
        // Arguments
        // Member name, signature, names of input values, result signature,
        // names of result values, callback function or coroutine, flags
        SD_BUS_PY_CHECK_ARGS_NUMBER(7);
        SD_BUS_PY_CHECK_ARG_TYPE(0, PyUnicode_Type);
        SD_BUS_PY_CHECK_ARG_TYPE(1, PyUnicode_Type);
        SD_BUS_PY_CHECK_ARG_CHECK_FUNC(2, PySequence_Check);
        SD_BUS_PY_CHECK_ARG_TYPE(3, PyUnicode_Type);
        SD_BUS_PY_CHECK_ARG_CHECK_FUNC(4, PySequence_Check);
        SD_BUS_PY_CHECK_ARG_CHECK_FUNC(5, PyLong_Check);
        SD_BUS_PY_CHECK_ARG_CHECK_FUNC(6, PyCallable_Check);

        PyObject* argument_name_list CLEANUP_PY_OBJECT = CALL_PYTHON_AND_CHECK(PyList_New(0));
        CALL_PYTHON_EXPECT_NONE(PyObject_CallMethodObjArgs(argument_name_list, extend_str, args[2], NULL));
        CALL_PYTHON_EXPECT_NONE(PyObject_CallMethodObjArgs(argument_name_list, extend_str, args[4], NULL));
        // HACK: add a null separator to the end of the array
        CALL_PYTHON_EXPECT_NONE(PyObject_CallMethodObjArgs(argument_name_list, append_str, null_str, NULL));

        PyObject* argument_names_string CLEANUP_PY_OBJECT = CALL_PYTHON_AND_CHECK(PyUnicode_Join(null_str, argument_name_list));
        // Method name, input signature, return signature, arguments names,
        // flags
        PyObject* new_tuple CLEANUP_PY_OBJECT = CALL_PYTHON_AND_CHECK(PyTuple_Pack(5, args[0], args[1], args[3], argument_names_string, args[5]));

        CALL_PYTHON_INT_CHECK(PyList_Append(self->method_list, new_tuple));
        CALL_PYTHON_INT_CHECK(PyDict_SetItem(self->method_dict, args[0], args[6]));

        Py_RETURN_NONE;
}

static PyObject* SdBusInterface_add_signal(SdBusInterfaceObject* self, PyObject* const* args, Py_ssize_t nargs) {
        // Arguments
        // Signal name, signature, names of input values, flags
        SD_BUS_PY_CHECK_ARGS_NUMBER(4);
        SD_BUS_PY_CHECK_ARG_TYPE(0, PyUnicode_Type);
        SD_BUS_PY_CHECK_ARG_TYPE(1, PyUnicode_Type);
        SD_BUS_PY_CHECK_ARG_CHECK_FUNC(2, PySequence_Check);
        SD_BUS_PY_CHECK_ARG_CHECK_FUNC(3, PyLong_Check);

        PyObject* argument_name_list CLEANUP_PY_OBJECT = CALL_PYTHON_AND_CHECK(PyList_New(0));
        CALL_PYTHON_EXPECT_NONE(PyObject_CallMethodObjArgs(argument_name_list, extend_str, args[2], NULL));
        // HACK: add a null separator to the end of the array
        CALL_PYTHON_EXPECT_NONE(PyObject_CallMethodObjArgs(argument_name_list, append_str, null_str, NULL));

        PyObject* argument_names_string CLEANUP_PY_OBJECT = CALL_PYTHON_AND_CHECK(PyUnicode_Join(null_str, argument_name_list));
        // Signal name, signature, names of input values, flags
        PyObject* new_tuple CLEANUP_PY_OBJECT = CALL_PYTHON_AND_CHECK(PyTuple_Pack(4, args[0], args[1], argument_names_string, args[3]));

        CALL_PYTHON_INT_CHECK(PyList_Append(self->signal_list, new_tuple));

        Py_RETURN_NONE;
}

static int _SdBusInterface_callback(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);

static int _SdBusInterface_property_get_callback(sd_bus* bus,
                                                 const char* path,
                                                 const char* interface,
                                                 const char* property,
                                                 sd_bus_message* reply,
                                                 void* userdata,
                                                 sd_bus_error* ret_error);

static int _SdBusInterface_property_set_callback(sd_bus* bus,
                                                 const char* path,
                                                 const char* interface,
                                                 const char* property,
                                                 sd_bus_message* value,
                                                 void* userdata,
                                                 sd_bus_error* ret_error);

static PyObject* SdBusInterface_create_vtable(SdBusInterfaceObject* self, PyObject* const* Py_UNUSED(args), Py_ssize_t nargs) {
        SD_BUS_PY_CHECK_ARGS_NUMBER(0);

        if (self->vtable) {
                Py_RETURN_NONE;
        }

        Py_ssize_t num_of_methods = PyList_Size(self->method_list);
        Py_ssize_t num_of_properties = PyList_Size(self->property_list);
        Py_ssize_t num_of_signals = PyList_Size(self->signal_list);

        self->vtable = calloc(num_of_signals + num_of_properties + num_of_methods + 2, sizeof(sd_bus_vtable));
        if (self->vtable == NULL) {
                return PyErr_NoMemory();
        }

        sd_bus_vtable start_vtable = SD_BUS_VTABLE_START(0);
        self->vtable[0] = start_vtable;
        Py_ssize_t current_index = 1;
        // Iter method definitions
        for (Py_ssize_t i = 0; i < num_of_methods; ({
                     ++i;
                     ++current_index;
             })) {
                PyObject* method_tuple = CALL_PYTHON_AND_CHECK(PyList_GetItem(self->method_list, i));

                PyObject* method_name_object = CALL_PYTHON_AND_CHECK(PyTuple_GetItem(method_tuple, 0));
                const char* method_name_char_ptr = SD_BUS_PY_UNICODE_AS_CHAR_PTR(method_name_object);

                PyObject* input_signature_object = CALL_PYTHON_AND_CHECK(PyTuple_GetItem(method_tuple, 1));
                const char* input_signature_char_ptr = SD_BUS_PY_UNICODE_AS_CHAR_PTR(input_signature_object);

                PyObject* result_signature_object = CALL_PYTHON_AND_CHECK(PyTuple_GetItem(method_tuple, 2));
                const char* result_signature_char_ptr = SD_BUS_PY_UNICODE_AS_CHAR_PTR(result_signature_object);

                PyObject* argument_names_string = CALL_PYTHON_AND_CHECK(PyTuple_GetItem(method_tuple, 3));

                const char* argument_names_char_ptr = SD_BUS_PY_UNICODE_AS_CHAR_PTR(argument_names_string);

                PyObject* flags_object = CALL_PYTHON_AND_CHECK(PyTuple_GetItem(method_tuple, 4));
                unsigned long long flags_long = PyLong_AsUnsignedLongLong(flags_object);
                if (PyErr_Occurred()) {
                        return NULL;
                }

                sd_bus_vtable temp_vtable = SD_BUS_METHOD_WITH_NAMES_OFFSET(method_name_char_ptr, input_signature_char_ptr, argument_names_char_ptr,
                                                                            result_signature_char_ptr, , _SdBusInterface_callback, 0, flags_long);
                self->vtable[current_index] = temp_vtable;
        }

        for (Py_ssize_t i = 0; i < num_of_properties; ({
                     ++i;
                     ++current_index;
             })) {
                PyObject* property_tuple = PyList_GET_ITEM(self->property_list, i);

                PyObject* property_name_str = PyTuple_GET_ITEM(property_tuple, 0);
                PyObject* property_signature_str = PyTuple_GET_ITEM(property_tuple, 1);
                PyObject* property_flags = PyTuple_GET_ITEM(property_tuple, 2);
                PyObject* setter_or_none = PyTuple_GET_ITEM(property_tuple, 3);

                const char* property_name_char_ptr = SD_BUS_PY_UNICODE_AS_CHAR_PTR(property_name_str);
                const char* property_signature_const_char = SD_BUS_PY_UNICODE_AS_CHAR_PTR(property_signature_str);

                unsigned long long flags_long = PyLong_AsUnsignedLongLong(property_flags);
                if (PyErr_Occurred()) {
                        return NULL;
                }

                if (setter_or_none == Py_None) {
                        sd_bus_vtable temp_vtable = SD_BUS_PROPERTY(property_name_char_ptr,                 // Name
                                                                    property_signature_const_char,          // Signature
                                                                    _SdBusInterface_property_get_callback,  // Get
                                                                    0,                                      // Offset
                                                                    flags_long                              // Flags
                        );
                        self->vtable[current_index] = temp_vtable;
                } else {
                        sd_bus_vtable temp_vtable = SD_BUS_WRITABLE_PROPERTY(property_name_char_ptr,                 // Name
                                                                             property_signature_const_char,          // Signature
                                                                             _SdBusInterface_property_get_callback,  // Get
                                                                             _SdBusInterface_property_set_callback,  // Set
                                                                             0,                                      // Offset
                                                                             flags_long                              // Flags
                        );
                        self->vtable[current_index] = temp_vtable;
                }
        }

        for (Py_ssize_t i = 0; i < num_of_signals; ({
                     ++i;
                     ++current_index;
             })) {
                PyObject* signal_tuple = PyList_GET_ITEM(self->signal_list, i);

                PyObject* signal_name_str = PyTuple_GET_ITEM(signal_tuple, 0);
                PyObject* signal_signature_str = PyTuple_GET_ITEM(signal_tuple, 1);
                PyObject* signal_input_names = PyTuple_GET_ITEM(signal_tuple, 2);
                PyObject* signal_flags = PyTuple_GET_ITEM(signal_tuple, 3);

                const char* signal_name_char_ptr = SD_BUS_PY_UNICODE_AS_CHAR_PTR(signal_name_str);
                const char* signal_signature_char_ptr = SD_BUS_PY_UNICODE_AS_CHAR_PTR(signal_signature_str);
                const char* signal_args_names_char_ptr = SD_BUS_PY_UNICODE_AS_CHAR_PTR(signal_input_names);

                unsigned long long flags_long = PyLong_AsUnsignedLongLong(signal_flags);
                if (PyErr_Occurred()) {
                        return NULL;
                }

                sd_bus_vtable temp_vtable = SD_BUS_SIGNAL_WITH_NAMES(signal_name_char_ptr, signal_signature_char_ptr, signal_args_names_char_ptr, flags_long);
                self->vtable[current_index] = temp_vtable;
        }

        sd_bus_vtable end_vtable = SD_BUS_VTABLE_END;
        self->vtable[current_index] = end_vtable;

        Py_RETURN_NONE;
}

static PyMethodDef SdBusInterface_methods[] = {
    {"add_method", (void*)SdBusInterface_add_method, METH_FASTCALL, "Add method to the dbus interface"},
    {"add_property", (void*)SdBusInterface_add_property, METH_FASTCALL, "Add property to the dbus interface"},
    {"add_signal", (void*)SdBusInterface_add_signal, METH_FASTCALL, "Add signal to the dbus interface"},
    {"_create_vtable", (void*)SdBusInterface_create_vtable, METH_FASTCALL, "Creates the vtable"},
    {NULL, NULL, 0, NULL},
};

static PyMemberDef SdBusInterface_members[] = {{"method_list", T_OBJECT, offsetof(SdBusInterfaceObject, method_list), READONLY, NULL},
                                               {"method_dict", T_OBJECT, offsetof(SdBusInterfaceObject, method_dict), READONLY, NULL},
                                               {"property_list", T_OBJECT, offsetof(SdBusInterfaceObject, property_list), READONLY, NULL},
                                               {"property_get_dict", T_OBJECT, offsetof(SdBusInterfaceObject, property_get_dict), READONLY, NULL},
                                               {"property_set_dict", T_OBJECT, offsetof(SdBusInterfaceObject, property_set_dict), READONLY, NULL},
                                               {"signal_list", T_OBJECT, offsetof(SdBusInterfaceObject, signal_list), READONLY, NULL},
                                               {0}};

PyTypeObject SdBusInterfaceType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "sd_bus_internals.SdBusInterface",
    .tp_basicsize = sizeof(SdBusInterfaceObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)SdBusInterface_init,
    .tp_free = (freefunc)SdBusInterface_free,
    .tp_methods = SdBusInterface_methods,
    .tp_members = SdBusInterface_members,
};

static int _SdBusInterface_callback(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
        SdBusInterfaceObject* self = userdata;
        // Get the member name from the message
        const char* member_char_ptr = sd_bus_message_get_member(m);
        PyObject* callback_object = PyDict_GetItemString(self->method_dict, member_char_ptr);
        if (callback_object == NULL) {
                sd_bus_error_set(ret_error, SD_BUS_ERROR_UNKNOWN_METHOD, "");
                return -1;
        };

        PyObject* running_loop CLEANUP_PY_OBJECT = PyObject_CallFunctionObjArgs(asyncio_get_running_loop, NULL);
        if (running_loop == NULL) {
                sd_bus_error_set(ret_error, SD_BUS_ERROR_FAILED, "");
                return -1;
        }

        PyObject* new_message CLEANUP_PY_OBJECT = PyObject_CallFunctionObjArgs((PyObject*)&SdBusMessageType, NULL);
        if (new_message == NULL) {
                sd_bus_error_set(ret_error, SD_BUS_ERROR_FAILED, "");
                return -1;
        }
        _SdBusMessage_set_messsage((SdBusMessageObject*)new_message, m);

        PyObject* is_coroutine_test_object CLEANUP_PY_OBJECT = PyObject_CallFunctionObjArgs(is_coroutine_function, callback_object, NULL);
        if (is_coroutine_test_object == NULL) {
                return -1;
        }

        if (Py_True == is_coroutine_test_object) {
                // Create coroutine
                PyObject* coroutine_activated CLEANUP_PY_OBJECT = PyObject_CallFunctionObjArgs(callback_object, new_message, NULL);
                if (coroutine_activated == NULL) {
                        return -1;
                }

                PyObject* task CLEANUP_PY_OBJECT = PyObject_CallMethodObjArgs(running_loop, create_task_str, coroutine_activated, NULL);
                if (task == NULL) {
                        sd_bus_error_set(ret_error, SD_BUS_ERROR_FAILED, "");
                        return -1;
                }
        } else {
                PyObject* handle CLEANUP_PY_OBJECT = PyObject_CallMethodObjArgs(running_loop, call_soon_str, callback_object, new_message, NULL);
                if (handle == NULL) {
                        sd_bus_error_set(ret_error, SD_BUS_ERROR_FAILED, "");
                        return -1;
                }
        }

        sd_bus_error_set(ret_error, NULL, NULL);

        return 1;
}

static int _SdBusInterface_property_get_callback(sd_bus* Py_UNUSED(bus),
                                                 const char* Py_UNUSED(path),
                                                 const char* Py_UNUSED(interface),
                                                 const char* property,
                                                 sd_bus_message* reply,
                                                 void* userdata,
                                                 sd_bus_error* Py_UNUSED(ret_error)) {
        SdBusInterfaceObject* self = userdata;
        PyObject* get_call = CALL_PYTHON_CHECK_RETURN_NEG1(PyDict_GetItemString(self->property_get_dict, property));

        PyObject* new_message CLEANUP_PY_OBJECT = CALL_PYTHON_CHECK_RETURN_NEG1(PyObject_CallFunctionObjArgs((PyObject*)&SdBusMessageType, NULL));
        _SdBusMessage_set_messsage((SdBusMessageObject*)new_message, reply);

        Py_XDECREF(CALL_PYTHON_CHECK_RETURN_NEG1(PyObject_CallFunctionObjArgs(get_call, new_message, NULL)));
        return 0;
}

static int _SdBusInterface_property_set_callback(sd_bus* Py_UNUSED(bus),
                                                 const char* Py_UNUSED(path),
                                                 const char* Py_UNUSED(interface),
                                                 const char* property,
                                                 sd_bus_message* value,
                                                 void* userdata,
                                                 sd_bus_error* Py_UNUSED(ret_error)) {
        SdBusInterfaceObject* self = userdata;

        PyObject* set_call = CALL_PYTHON_CHECK_RETURN_NEG1(PyDict_GetItemString(self->property_set_dict, property));

        PyObject* new_message CLEANUP_PY_OBJECT = CALL_PYTHON_CHECK_RETURN_NEG1(PyObject_CallFunctionObjArgs((PyObject*)&SdBusMessageType, NULL));
        _SdBusMessage_set_messsage((SdBusMessageObject*)new_message, value);

        Py_XDECREF(CALL_PYTHON_CHECK_RETURN_NEG1(PyObject_CallFunctionObjArgs(set_call, new_message, NULL)));
        return 0;
}
