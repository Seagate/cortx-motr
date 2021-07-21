/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
 * Original author: Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 29-Apr-2021
 */

package mio

// #cgo CFLAGS: -I/usr/include/motr
// #cgo CFLAGS: -I../../.. -I../../../extra-libs/galois/include
// #cgo CFLAGS: -DM0_EXTERN=extern -DM0_INTERNAL=
// #include <errno.h> /* EEXIST */
// #include "motr/client.h"
// #include "motr/layout.h" /* m0c_pools_common */
//
// extern struct m0_container container;
//
import "C"

import (
    "errors"
    "fmt"
    "unsafe"
)

// Mkv provides key-value API to Motr
type Mkv struct {
    idxID   C.struct_m0_uint128
    idx    *C.struct_m0_idx
}

func uint128fid(u C.struct_m0_uint128) (f C.struct_m0_fid) {
    f.f_container = u.u_hi
    f.f_key       = u.u_lo
    return f
}

func (mkv *Mkv) idxNew(id string) (err error) {
    mkv.idxID, err = ScanID(id)
    if err != nil {
        return err
    }
    fid := uint128fid(mkv.idxID)
    if C.m0_fid_tget(&fid) != C.m0_dix_fid_type.ft_id {
        return fmt.Errorf("index fid must start with 0x%x in MSByte" +
                          ", for example: 0x%x00000000000123:0x...",
                          C.m0_dix_fid_type.ft_id, C.m0_dix_fid_type.ft_id)
    }
    mkv.idx = (*C.struct_m0_idx)(C.calloc(1, C.sizeof_struct_m0_idx))
    return nil
}

// Open opens Mkv index for key-value operations.
func (mkv *Mkv) Open(id string, create bool) error {
    if mkv.idx != nil {
        return errors.New("index is already opened")
    }

    err := mkv.idxNew(id)
    if err != nil {
        return err
    }

    C.m0_idx_init(mkv.idx, &C.container.co_realm, &mkv.idxID)

    if create { // Make sure it's created
        var op *C.struct_m0_op
        rc := C.m0_entity_create(nil, &mkv.idx.in_entity, &op)
        if rc != 0 {
            mkv.Close()
            return fmt.Errorf("failed to set create op: %d", rc)
        }
        C.m0_op_launch(&op, 1)
        rc = C.m0_op_wait(op, bits(C.M0_OS_FAILED,
                                   C.M0_OS_STABLE), C.M0_TIME_NEVER)
        if rc == 0 {
            rc = C.m0_rc(op)
        }
        C.m0_op_fini(op)
        C.m0_op_free(op)

        if rc != 0 && rc != -C.EEXIST {
            return fmt.Errorf("index create failed: %d", rc)
        }
    }

    return nil
}

// Close closes Mkv index releasing all the resources
// that were allocated for it.
func (mkv *Mkv) Close() error {
    if mkv.idx == nil {
        return errors.New("index is not opened")
    }
    C.m0_idx_fini(mkv.idx)
    C.free(unsafe.Pointer(mkv.idx))
    mkv.idx = nil

    return nil
}

func (mkv *Mkv) doIdxOp(opcode uint32, key []byte, value []byte,
                        update bool) ([]byte, error) {
    if mkv.idx == nil {
        return nil, errors.New("index is not opened")
    }

    var k, v C.struct_m0_bufvec
    if C.m0_bufvec_empty_alloc(&k, 1) != 0 {
        return nil, errors.New("failed to allocate key bufvec")
    }
    defer C.m0_bufvec_free2(&k)

    if opcode == C.M0_IC_PUT || opcode == C.M0_IC_GET {
        if C.m0_bufvec_empty_alloc(&v, 1) != 0 {
            return nil, errors.New("failed to allocate value bufvec")
        }
        if opcode == C.M0_IC_GET {
            defer C.m0_bufvec_free(&v) // cleanup buffer after GET
        } else {
            defer C.m0_bufvec_free2(&v)
        }
    }

    *k.ov_buf = unsafe.Pointer(&key[0])
    *k.ov_vec.v_count = C.ulong(len(key))
    if opcode == C.M0_IC_PUT {
        *v.ov_buf = unsafe.Pointer(&value[0])
        *v.ov_vec.v_count = C.ulong(len(value))
    }

    vPtr := &v
    if opcode == C.M0_IC_DEL {
        vPtr = nil
    }

    flags := C.uint(0)
    if opcode == C.M0_IC_PUT && update {
        flags = C.M0_OIF_OVERWRITE
    }

    var rcI C.int32_t
    var op *C.struct_m0_op
    rc := C.m0_idx_op(mkv.idx, opcode, &k, vPtr, &rcI, flags, &op)
    if rc != 0 {
        return nil, fmt.Errorf("failed to init index op: %d", rc)
    }

    C.m0_op_launch(&op, 1)
    rc = C.m0_op_wait(op, bits(C.M0_OS_FAILED,
                               C.M0_OS_STABLE), C.M0_TIME_NEVER)
    if rc == 0 {
        rc = C.m0_rc(op)
    }
    C.m0_op_fini(op)
    C.m0_op_free(op)

    if rc != 0 {
        return nil, fmt.Errorf("op failed: %d", rc)
    }
    if rcI != 0 {
        return nil, fmt.Errorf("index op failed: %d", rcI)
    }

    if opcode == C.M0_IC_GET {
        value = make([]byte, *v.ov_vec.v_count)
        copy(value, pointer2slice(*v.ov_buf, int(*v.ov_vec.v_count)))
    }

    return value, nil
}

// Put puts key-value into the index.
func (mkv *Mkv) Put(key []byte, value []byte, update bool) error {
    _, err := mkv.doIdxOp(C.M0_IC_PUT, key, value, update)
    return err
}

// Get gets value from the index by key.
func (mkv *Mkv) Get(key []byte) ([]byte, error) {
    value, err := mkv.doIdxOp(C.M0_IC_GET, key, nil, false)
    return value, err
}

// Delete deletes the record by key.
func (mkv *Mkv) Delete(key []byte) error {
    _, err := mkv.doIdxOp(C.M0_IC_DEL, key, nil, false)
    return err
}

// vi: sw=4 ts=4 expandtab ai
