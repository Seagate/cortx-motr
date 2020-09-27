/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */


#include <Python.h>
#include "config.h"
#include "motr/ha.h"            /* m0_motr_ha */
#include "module/instance.h"
#include "net/net.h"
#include "net/buffer_pool.h"
#include "reqh/reqh.h"
#include "rpc/rpc_machine.h"
#include "spiel/spiel.h"

#define STRUCTS               \
	X(m0)                 \
	X(m0_net_domain)      \
	X(m0_net_buffer_pool) \
	X(m0_reqh)            \
	X(m0_reqh_init_args)  \
	X(m0_reqh_service)    \
	X(m0_rpc_machine)     \
	X(m0_motr_ha)         \
	X(m0_motr_ha_cfg)     \
	X(m0_spiel_tx)        \
	X(m0_spiel)

#define X(name)                                                 \
static PyObject *name ## __size(PyObject *self, PyObject *args) \
{                                                               \
	return PyLong_FromSize_t(sizeof(struct name));           \
}
STRUCTS
#undef X

static PyMethodDef methods[] = {
#define X(name) { #name "__size", name ## __size, METH_VARARGS, NULL },
STRUCTS
#undef X
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef motrmodule = {
	PyModuleDef_HEAD_INIT,
	"motr",   /* name of module */
	NULL, /* module documentation, may be NULL */
	-1,       /* size of per-interpreter state of the module,
	or -1 if the module keeps state in global variables. */
	methods
};

PyMODINIT_FUNC PyInit_motr(void)
{
	return PyModule_Create(&motrmodule);
}
