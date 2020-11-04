/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

// Package mio implements io.Reader/io.Writer interface over
// Motr client I/O API. This allows writing Motr client applications
// quickly and efficiently in the Go language.
//
// mio automatically determines the optimal unit (stripe) size for the
// newly created object (based on the object size provided by user
// in the mio.Create(obj, sz) call), as well as the optimal block size
// for Motr I/O based on the cluster configuration. So users don't have
// to bother about tuning these Motr-specific parameters for each specific
// object to reach maximum I/O performance on it and yet don't waste space
// (in case of a small objects).
//
// mio allows to read/write the blocks to Motr in parallel threads (see
// -threads option) provided the buffer size (len(p)) is big enough to
// accomodate several of such blocks in one Read(p)/Write(p) request.
//
// For the usage example, refer to mcp utility.
package mio

// #cgo CFLAGS: -I/usr/include/motr
// #cgo CFLAGS: -I../../.. -I../../../extra-libs/galois/include
// #cgo CFLAGS: -DM0_EXTERN=extern -DM0_INTERNAL=
// #cgo CFLAGS: -Wno-attributes
// #cgo LDFLAGS: -L../../../motr/.libs -lmotr
// #include <stdlib.h>
// #include "lib/types.h"
// #include "lib/trace.h"
// #include "motr/client.h"
// #include "motr/layout.h" /* m0c_pools_common */
//
// struct m0_client    *instance = NULL;
// struct m0_container container;
// struct m0_realm     uber_realm;
// struct m0_config    conf = {};
// struct m0_idx_dix_config dix_conf = {};
//
// int m0_open_entity(struct m0_entity *entity)
// {
//         int           rc;
//         struct m0_op *op = NULL;
//
//         rc = m0_entity_open(entity, &op);
//         if (rc != 0)
//                 goto cleanup;
//
//         m0_op_launch(&op, 1);
//         rc = m0_op_wait(op, M0_BITS(M0_OS_FAILED,
//                                     M0_OS_STABLE),
//                         M0_TIME_NEVER) ?: m0_rc(op);
// cleanup:
//         m0_op_fini(op);
//         m0_op_free(op);
//
//         return rc;
// }
import "C"

import (
    "fmt"
    "flag"
    "log"
    "errors"
    "io"
    "reflect"
    "os"
    "time"
    "sync"
    "unsafe"
)

type slot struct {
    idx int
    err error
}

// Mio implements io.Reader / io.Writer interfaces for Motr.
type Mio struct {
    objID   C.struct_m0_uint128
    obj    *C.struct_m0_obj
    objSz   uint64
    objLid  uint
    off     uint64
    buf     []C.struct_m0_bufvec
    ext     []C.struct_m0_indexvec
    attr    []C.struct_m0_bufvec
    minBuf  []byte
    ch      chan slot
    wg      sync.WaitGroup
}

func checkArg(arg *string, name string) {
    if *arg == "" {
        fmt.Printf("%s: %s must be specified\n\n", os.Args[0], name)
        flag.Usage()
        os.Exit(1)
    }
}

var verbose bool
var threadsN int

// Init initialises mio module.
// All the usual Motr's init stuff is done here.
func Init() {
    // Mandatory
    localEP  := flag.String("ep", "", "my `endpoint` address")
    haxEP    := flag.String("hax", "", "local hax `endpoint` address")
    profile  := flag.String("prof", "", "cluster profile `fid`")
    procFid  := flag.String("proc", "", "my process `fid`")
    // Optional
    traceOn  := flag.Bool("trace", false, "generate m0trace.pid file")
    flag.BoolVar(&verbose, "v", false, "be more verbose")
    flag.IntVar(&threadsN, "threads", 1, "`number` of threads to use")

    flag.Parse()

    checkArg(localEP, "my endpoint (-ep)")
    checkArg(haxEP,   "hax endpoint (-hax)")
    checkArg(profile, "profile fid (-prof)")
    checkArg(procFid, "my process fid (-proc)")

    if !*traceOn {
        C.m0_trace_set_mmapped_buffer(false)
    }

    C.conf.mc_is_oostore     = true
    C.conf.mc_local_addr     = C.CString(*localEP)
    C.conf.mc_ha_addr        = C.CString(*haxEP)
    C.conf.mc_profile        = C.CString(*profile)
    C.conf.mc_process_fid    = C.CString(*procFid)
    C.conf.mc_tm_recv_queue_min_len =    64
    C.conf.mc_max_rpc_msg_size      = 65536
    C.conf.mc_idx_service_id  = C.M0_IDX_DIX;
    C.dix_conf.kc_create_meta = false;
    C.conf.mc_idx_service_conf = unsafe.Pointer(&C.dix_conf)

    rc := C.m0_client_init(&C.instance, &C.conf, true)
    if rc != 0 {
        log.Panicf("m0_client_init() failed: %v", rc)
    }

    C.m0_container_init(&C.container, nil, &C.M0_UBER_REALM, C.instance)
    rc = C.container.co_realm.re_entity.en_sm.sm_rc;
    if rc != 0 {
        log.Panicf("C.m0_container_init() failed: %v", rc)
    }
}

// ScanID scans object id from string.
func ScanID(s string) (fid C.struct_m0_uint128, err error) {
    cs := C.CString(s)
    rc := C.m0_uint128_sscanf(cs, &fid)
    C.free(unsafe.Pointer(cs))
    if rc != 0 {
        return fid, fmt.Errorf("failed to parse fid: %d", rc)
    }
    return fid, nil
}

func (mio *Mio) objNew(id string) (err error) {
    mio.objID, err = ScanID(id)
    if err != nil {
        return err
    }
    mio.obj = (*C.struct_m0_obj)(C.calloc(1, C.sizeof_struct_m0_obj))
    return nil
}

func (mio *Mio) finishOpen(sz uint64) {
    mio.buf = make([]C.struct_m0_bufvec, threadsN)
    mio.ext = make([]C.struct_m0_indexvec, threadsN)
    mio.attr = make([]C.struct_m0_bufvec, threadsN)
    mio.ch = make(chan slot, threadsN)
    // fill the pool with slots
    for i := 0; i < threadsN; i++ {
        mio.ch <- slot{i, nil}
    }
    mio.objLid = uint(mio.obj.ob_attr.oa_layout_id)
    mio.off = 0
    mio.objSz = sz
}

// Open opens object for reading ant/or writing. The size
// must be specified when openning object for reading. Otherwise,
// nothing will be read. (Motr doesn't store objects metadata
// along with the objects.)
func (mio *Mio) Open(id string, anySz ...uint64) (err error) {
    if mio.obj != nil {
        return errors.New("object is already opened")
    }
    err = mio.objNew(id)
    if err != nil {
        return err
    }

    C.m0_obj_init(mio.obj, &C.container.co_realm, &mio.objID, 1)
    rc := C.m0_open_entity(&mio.obj.ob_entity);
    if rc != 0 {
        mio.Close()
        return fmt.Errorf("failed to open object entity: %d", rc)
    }

    sz := uint64(0)
    for _, v := range anySz {
        sz = v
    }
    mio.finishOpen(sz)

    return nil
}

// Close closes the object and releases all the resources that were
// allocated while working with it.
func (mio *Mio) Close() {
    if mio.obj == nil {
        return
    }
    C.m0_obj_fini(mio.obj)
    C.free(unsafe.Pointer(mio.obj))
    mio.obj = nil

    for i := 0; i < len(mio.buf); i++ {
        if mio.buf[i].ov_buf == nil {
            C.m0_bufvec_free2(&mio.buf[i])
                C.m0_bufvec_free(&mio.attr[i])
                C.m0_indexvec_free(&mio.ext[i])
        }
    }
    if mio.minBuf != nil {
        C.free(unsafe.Pointer(&mio.minBuf[0]))
        mio.minBuf = nil
    }
}

func bits(values ...C.ulong) (res C.ulong) {
    for _, v := range values {
        res |= (1 << v)
    }
    return res
}

func getOptimalUnitSz(sz uint64) (C.ulong, error) {
    var pver *C.struct_m0_pool_version
    rc := C.m0_pool_version_get(&C.instance.m0c_pools_common, nil, &pver)
    if rc != 0 {
        return 0, fmt.Errorf("m0_pool_version_get() failed: %v", rc)
    }
    lid := C.m0_layout_find_by_buffsize(&C.instance.m0c_reqh.rh_ldom,
                                        &pver.pv_id, C.ulong(sz))
    return lid, nil
}

func checkPool(pools []string) (res *C.struct_m0_fid, err error) {
    for _, pool := range pools {
        if pool == "" {
            return nil, nil // default pool to be used
        }
        id, err := ScanID(pool)
        if err != nil {
            return nil, fmt.Errorf("invalid pool: %v", pool)
        }
        res = new(C.struct_m0_fid)
        *res = C.struct_m0_fid{id.u_hi, id.u_lo}
        break
    }
    return res, nil
}

// Create creates object. Estimated object size must be specified
// so that the optimal object unit size and block size for the best
// I/O performance on the object could be calculated. Optionally,
// the pool fid can be provided, if the object to be created on a
// non-default pool.
func (mio *Mio) Create(id string, sz uint64, anyPool ...string) error {
    if mio.obj != nil {
        return errors.New("object is already opened")
    }
    if err := mio.objNew(id); err != nil {
        return err
    }
    pool, err := checkPool(anyPool)
    if err != nil {
        return err
    }

    lid, err := getOptimalUnitSz(sz)
    if err != nil {
        return fmt.Errorf("failed to figure out object unit size: %v", err)
    }
    C.m0_obj_init(mio.obj, &C.container.co_realm, &mio.objID, lid)

    var op *C.struct_m0_op
    rc := C.m0_entity_create(pool, &mio.obj.ob_entity, &op)
    if rc != 0 {
        return fmt.Errorf("failed to create object: %d", rc)
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
        return fmt.Errorf("create op failed: %d", rc)
    }
    mio.finishOpen(sz)

    return nil
}

func roundupPower2(x int) (power int) {
    for power = 1; power < x; power *= 2 {}
    return power
}

const maxM0BufSz = 128 * 1024 * 1024

// Estimate the optimal (block, group) sizes for the I/O
func (mio *Mio) getOptimalBlockSz(bufSz int) (bsz, gsz int) {
    if bufSz > maxM0BufSz {
            bufSz = maxM0BufSz
    }
    pver := C.m0_pool_version_find(&C.instance.m0c_pools_common,
                                   &mio.obj.ob_attr.oa_pver)
    if pver == nil {
            log.Panic("cannot find the object's pool version")
    }
    usz := int(C.m0_obj_layout_id_to_unit_size(C.ulong(mio.objLid)))
    pa := &pver.pv_attr
    gsz = usz * int(pa.pa_N)
    /* max 2-times pool-width deep, otherwise we may get -E2BIG */
    maxBs := int(C.uint(usz) * 2 * pa.pa_P * pa.pa_N / (pa.pa_N + 2 * pa.pa_K))

    if bufSz >= maxBs {
            return maxBs, gsz
    } else if bufSz <= gsz {
            return gsz, gsz
    } else {
            return roundupPower2(bufSz), gsz
    }
}

func pointer2slice(p unsafe.Pointer, n int) []byte {
    var res []byte

    slice := (*reflect.SliceHeader)(unsafe.Pointer(&res))
    slice.Data = uintptr(p)
    slice.Len = n
    slice.Cap = n

    return res
}

func (mio *Mio) prepareBuf(p []byte, i, bs, gs, off int,
                           offMio uint64) error {
    buf := p[off:]
    if rem := bs % gs; rem != 0 {
        bs += (gs - rem)
        // Must be zero-ed, so we always allocate it.
        // gs does not divide bs only at the end of object
        // so it should not happen very often.
        mio.minBuf = pointer2slice(C.calloc(1, C.ulong(bs)), bs)
        buf = mio.minBuf[:]
    } else if mio.minBuf != nil {
        C.free(unsafe.Pointer(&mio.minBuf[0]))
        mio.minBuf = nil
    }
    if mio.buf[i].ov_buf == nil {
        if C.m0_bufvec_empty_alloc(&mio.buf[i], 1) != 0 {
            return errors.New("mio.buf allocation failed")
        }
        if C.m0_bufvec_alloc(&mio.attr[i], 1, 1) != 0 {
            return errors.New("mio.attr allocation failed")
        }
        if C.m0_indexvec_alloc(&mio.ext[i], 1) != 0 {
            return errors.New("mio.ext allocation failed")
        }
    }
    *mio.buf[i].ov_buf = unsafe.Pointer(&buf[0])
    *mio.buf[i].ov_vec.v_count = C.ulong(bs)
    *mio.ext[i].iv_index = C.ulong(offMio)
    *mio.ext[i].iv_vec.v_count = C.ulong(bs)
    *mio.attr[i].ov_vec.v_count = 0

    return nil
}

func (mio *Mio) doIO(i int, opcode uint32) {
    defer mio.wg.Done()
    var op *C.struct_m0_op
    C.m0_obj_op(mio.obj, opcode,
                &mio.ext[i], &mio.buf[i], &mio.attr[i], 0, 0, &op)
    C.m0_op_launch(&op, 1)
    rc := C.m0_op_wait(op, bits(C.M0_OS_FAILED,
                                C.M0_OS_STABLE), C.M0_TIME_NEVER)
    if rc == 0 {
        rc = C.m0_rc(op)
    }
    C.m0_op_fini(op)
    C.m0_op_free(op)
    // put the slot back to the pool
    if rc != 0 {
        mio.ch <- slot{i, fmt.Errorf("io op (%d) failed: %d", opcode, rc)}
    }
    mio.ch <- slot{i, nil}
}

func getBW(n int, d time.Duration) (int, string) {
    bw := n / int(d.Milliseconds()) * 1000 / 1024 / 1024
    if bw > 9 {
        return bw, "Mbytes/sec"
    }
    bw = n / int(d.Milliseconds()) * 1000 / 1024
    if bw > 9 {
        return bw, "Kbytes/sec"
    }
    bw = n / int(d.Milliseconds()) * 1000
    return bw, "Bytes/sec"
}

func (mio *Mio) Write(p []byte) (n int, err error) {
    if mio.obj == nil {
        return 0, errors.New("object is not opened")
    }
    left, off := len(p), 0
    bs, gs := mio.getOptimalBlockSz(left)
    start, offSaved, bsSaved := time.Now(), mio.off, bs
    for ; left > 0; left -= bs {
        if left < bs {
            bs = left
        }
        slot := <-mio.ch // get next available from the pool
        if slot.err != nil {
            break
        }
        err = mio.prepareBuf(p, slot.idx, bs, gs, off, mio.off)
        if err != nil {
            return off, err
        }
        if mio.minBuf != nil {
            copy(mio.minBuf, p[off:])
        }
        mio.wg.Add(1)
        go mio.doIO(slot.idx, C.M0_OC_WRITE)
        off += bs
        mio.off += uint64(bs)
    }
    mio.wg.Wait()

    if verbose {
        elapsed := time.Now().Sub(start)
        n := int(mio.off - offSaved)
        bw, units := getBW(n, elapsed)
        log.Printf("W: off=%v len=%v bs=%v gs=%v speed=%v (%v)",
		   offSaved, n, bsSaved, gs, bw, units)
    }

    return off, err
}

func (mio *Mio) Read(p []byte) (n int, err error) {
    if mio.obj == nil {
        return 0, errors.New("object is not opened")
    }
    left, off := len(p), 0
    if mio.off + uint64(left) > mio.objSz {
        left = int(mio.objSz - mio.off)
        if left <= 0 {
            return 0, io.EOF
        }
    }
    bs, gs := mio.getOptimalBlockSz(left)
    start, offSaved, bsSaved := time.Now(), mio.off, bs
    for ; left > 0; left -= bs {
        if left < bs {
            bs = left
        }
        slot := <-mio.ch // get next available
        if slot.err != nil {
            break
        }
        err = mio.prepareBuf(p, slot.idx, bs, gs, off, mio.off)
        if err != nil {
            return off, err
        }
        mio.wg.Add(1)
        go mio.doIO(slot.idx, C.M0_OC_READ)
        if mio.minBuf != nil {
            mio.wg.Wait() // last one anyway
            copy(p[off:], mio.minBuf)
        }
        off += bs
        mio.off += uint64(bs)
    }
    mio.wg.Wait()

    if verbose {
        elapsed := time.Now().Sub(start)
        n := int(mio.off - offSaved)
        bw, units := getBW(n, elapsed)
        log.Printf("R: off=%v len=%v bs=%v gs=%v speed=%v (%v)",
		   offSaved, n, bsSaved, gs, bw, units)
    }

    return off, err
}
