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
 * Original author: Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 30-Oct-2020
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
// #cgo LDFLAGS: -L../../../motr/.libs -Wl,-rpath=../../../motr/.libs -lmotr
// #include <stdlib.h>
// #include "lib/types.h"
// #include "lib/trace.h"   /* m0_trace_set_mmapped_buffer */
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
//
// uint64_t m0_obj_layout_id(uint64_t lid)
// {
//         return M0_OBJ_LAYOUT_ID(lid);
// }
//
import "C"

import (
    "errors"
    "flag"
    "fmt"
    "io"
    "log"
    "os"
    "reflect"
    "sync"
    "time"
    "unsafe"
)

// Mio implements io.Reader / io.Writer interfaces for Motr.
type Mio struct {
    objID   C.struct_m0_uint128
    obj    *C.struct_m0_obj
    objSz   uint64
    objLid  C.ulong
    objPool C.struct_m0_fid
    off     int64
}

type slot struct {
    idx int
    err error
}

type iov struct {
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
    rc = C.container.co_realm.re_entity.en_sm.sm_rc
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

// GetPool returns the pool the object is located at.
func (mio *Mio) GetPool() string {
    if mio.obj == nil {
        return ""
    }
    p := mio.objPool

    return fmt.Sprintf("0x%x:0x%x", p.f_container, p.f_key)
}

// InPool checks whether the object is located at the pool.
func (mio *Mio) InPool(pool string) bool {
    if mio.obj == nil {
        return false
    }
    id1, err := ScanID(pool)
    if err != nil {
        return false
    }
    p := mio.objPool
    id2 := C.struct_m0_uint128{p.f_container, p.f_key}

    return C.m0_uint128_cmp(&id1, &id2) == 0
}

func (mio *Mio) open(sz uint64) error {
    pv := C.m0_pool_version_find(&C.instance.m0c_pools_common,
                                 &mio.obj.ob_attr.oa_pver)
    if pv == nil {
        return errors.New("cannot find pool version")
    }
    mio.objPool = pv.pv_pool.po_id

    mio.objSz = sz
    mio.objLid = C.m0_obj_layout_id(mio.obj.ob_attr.oa_layout_id)
    mio.off = 0

    return nil
}

// Open opens Mio object for reading ant/or writing. The size
// must be specified when openning object for reading. Otherwise,
// nothing will be read. (Motr doesn't store objects metadata
// along with the objects.)
func (mio *Mio) Open(id string, anySz ...uint64) error {
    if mio.obj != nil {
        return errors.New("object is already opened")
    }
    err := mio.objNew(id)
    if err != nil {
        return err
    }

    C.m0_obj_init(mio.obj, &C.container.co_realm, &mio.objID, 1)
    rc := C.m0_open_entity(&mio.obj.ob_entity)
    if rc != 0 {
        mio.Close()
        return fmt.Errorf("failed to open object entity: %d", rc)
    }

    for _, v := range anySz {
        mio.objSz = v
    }

    return mio.open(mio.objSz)
}

// Close closes Mio object and releases all the resources that were
// allocated for it. Implements io.Closer interface.
func (mio *Mio) Close() error {
    if mio.obj == nil {
        return errors.New("object is not opened")
    }
    C.m0_obj_fini(mio.obj)
    C.free(unsafe.Pointer(mio.obj))
    mio.obj = nil

    return nil
}

func bits(values ...C.ulong) (res C.ulong) {
    for _, v := range values {
        res |= (1 << v)
    }
    return res
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

    lid := C.m0_layout_find_by_objsz(C.instance, pool, C.ulong(sz))
    if lid <= 0 {
        return fmt.Errorf("could not find layout: rc=%v", lid)
    }
    C.m0_obj_init(mio.obj, &C.container.co_realm, &mio.objID, C.ulong(lid))

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

    return mio.open(sz)
}

func roundupPower2(x int) (power int) {
    for power = 1; power < x; power *= 2 {}
    return power
}

const maxM0BufSz = 512 * 1024 * 1024

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
    pa := &pver.pv_attr
    if pa.pa_P < pa.pa_N + pa.pa_K + pa.pa_S {
        log.Panic("pool width (%v) is less than the parity group size" +
                  " (%v + %v + %v == %v), check pool parity configuration",
                  pa.pa_P, pa.pa_N, pa.pa_K, pa.pa_S,
                           pa.pa_N + pa.pa_K + pa.pa_S)
    }
    usz := int(C.m0_obj_layout_id_to_unit_size(mio.objLid))
    gsz = usz * int(pa.pa_N) // group size in data units only
    // should be max 2-times pool-width deep, otherwise we may get -E2BIG
    maxBs := int(C.uint(usz) * 2 * pa.pa_P * pa.pa_N /
                    (pa.pa_N + pa.pa_K + pa.pa_S))
    maxBs = ((maxBs - 1) / gsz + 1) * gsz // multiple of group size

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

func (v *iov) freeVecs(n int) {
    for i := 0; i < n; i++ {
        C.m0_bufvec_free2(&v.buf[i])
        C.m0_bufvec_free(&v.attr[i])
        C.m0_indexvec_free(&v.ext[i])
    }
}
func (v *iov) alloc() error {
    v.buf = make([]C.struct_m0_bufvec, threadsN)
    v.ext = make([]C.struct_m0_indexvec, threadsN)
    v.attr = make([]C.struct_m0_bufvec, threadsN)
    v.ch = make(chan slot, threadsN) // pool of free slots

    var i int
    for i = 0; i < threadsN; i++ {
        v.ch <- slot{i, nil} // fill the pool in
        if C.m0_bufvec_empty_alloc(&v.buf[i], 1) != 0 {
            break
        }
        if C.m0_bufvec_alloc(&v.attr[i], 1, 1) != 0 {
            break
        }
        if C.m0_indexvec_alloc(&v.ext[i], 1) != 0 {
            break
        }
    }
    if i < threadsN {
        v.freeVecs(i)
        return errors.New("vecs allocation failed")
    }

    return nil
}

func (v *iov) free() {
    v.freeVecs(len(v.buf))
    if v.minBuf != nil {
        C.free(unsafe.Pointer(&v.minBuf[0]))
        v.minBuf = nil
    }
}

func (v *iov) prepareBuf(buf []byte, i, bs, gs int, off int64) error {
    if v.minBuf != nil {
        return errors.New("BUG IN THE CODE: minBuf must always be nil here")
    }
    if rem := bs % gs; rem != 0 {
        bs += (gs - rem)
        // minBuf must be zero-ed, so we always allocate it.
        // (That's apparently the easiest way to zero bufs in Go.)
        // gs does not divide bs only at the end of object
        // so it should not happen very often.
        v.minBuf = pointer2slice(C.calloc(1, C.ulong(bs)), bs)
        buf = v.minBuf[:]
    }
    *v.buf[i].ov_buf = unsafe.Pointer(&buf[0])
    *v.buf[i].ov_vec.v_count = C.ulong(bs)
    *v.ext[i].iv_index = C.ulong(off)
    *v.ext[i].iv_vec.v_count = C.ulong(bs)
    *v.attr[i].ov_vec.v_count = 0

    return nil
}

func (v *iov) doIO(i int, op *C.struct_m0_op) {
    defer v.wg.Done()
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
        v.ch <- slot{i, fmt.Errorf("io op (%d) failed: %d", op.op_code, rc)}
    }
    v.ch <- slot{i, nil}
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

func (mio *Mio) write(p []byte, off *int64) (n int, err error) {
    if mio.obj == nil {
        return 0, errors.New("object is not opened")
    }

    var v iov
    if err = v.alloc(); err != nil {
        return 0, err
    }
    defer v.free()

    left := len(p)
    bs, gs := mio.getOptimalBlockSz(left)
    start, offSaved, bsSaved := time.Now(), *off, bs
    for ; left > 0; left -= bs {
        if left < bs {
            bs = left
        }
        slot := <-v.ch // get next available from the pool
        if slot.err != nil {
            err = slot.err
            break
        }
        err = v.prepareBuf(p[n:], slot.idx, bs, gs, *off)
        if err != nil {
            break
        }
        if v.minBuf != nil { // last block, not aligned
            copy(v.minBuf, p[n:])
        }
        var op *C.struct_m0_op
        rc := C.m0_obj_op(mio.obj, C.M0_OC_WRITE,
                          &v.ext[slot.idx],
                          &v.buf[slot.idx],
                          &v.attr[slot.idx], 0, 0, &op)
        if rc != 0 {
            err = fmt.Errorf("creating m0_op failed: rc=%v", rc)
            break
        }
        v.wg.Add(1)
        go v.doIO(slot.idx, op)
        n += bs
        *off += int64(bs)
    }
    v.wg.Wait()

    if verbose {
        elapsed := time.Now().Sub(start)
        bw, units := getBW(n, elapsed)
        log.Printf("W: off=%v len=%v bs=%v gs=%v speed=%v (%v)",
                   offSaved, n, bsSaved, gs, bw, units)
    }

    return n, err
}

func (mio *Mio) Write(p []byte) (n int, err error) {
    return mio.write(p, &mio.off)
}

// WriteAt implements io.WriterAt interface
func (mio *Mio) WriteAt(p []byte, off int64) (n int, err error) {
    return mio.write(p, &off)
}

func (mio *Mio) read(p []byte, off *int64) (n int, err error) {
    if mio.obj == nil {
        return 0, errors.New("object is not opened")
    }

    var v iov
    if err = v.alloc(); err != nil {
        return 0, err
    }
    defer v.free()

    left := len(p)
    if uint64(*off) + uint64(left) > mio.objSz {
        left = int(mio.objSz - uint64(*off))
        if left <= 0 {
            return 0, io.EOF
        }
    }
    bs, gs := mio.getOptimalBlockSz(left)
    start, offSaved, bsSaved := time.Now(), *off, bs
    for ; left > 0; left -= bs {
        if left < bs {
            bs = left
        }
        slot := <-v.ch // get next available
        if slot.err != nil {
            err = slot.err
            break
        }
        err = v.prepareBuf(p[n:], slot.idx, bs, gs, *off)
        if err != nil {
            break
        }
        var op *C.struct_m0_op
        rc := C.m0_obj_op(mio.obj, C.M0_OC_READ,
                          &v.ext[slot.idx],
                          &v.buf[slot.idx],
                          &v.attr[slot.idx], 0, 0, &op)
        if rc != 0 {
            err = fmt.Errorf("creating m0_op failed: rc=%v", rc)
            break
        }
        v.wg.Add(1)
        go v.doIO(slot.idx, op)
        if v.minBuf != nil {
            v.wg.Wait() // last one anyway
            copy(p[n:], v.minBuf)
        }
        n += bs
        *off += int64(bs)
    }
    v.wg.Wait()

    if verbose {
        elapsed := time.Now().Sub(start)
        bw, units := getBW(n, elapsed)
        log.Printf("R: off=%v len=%v bs=%v gs=%v speed=%v (%v)",
                   offSaved, n, bsSaved, gs, bw, units)
    }

    return n, err
}

func (mio *Mio) Read(p []byte) (n int, err error) {
    return mio.read(p, &mio.off)
}

// ReadAt implements io.ReaderAt interface
func (mio *Mio) ReadAt(p []byte, off int64) (n int, err error) {
    return mio.read(p, &off)
}

// Seek implements io.Seeker interface
func (mio *Mio) Seek(offset int64, whence int) (int64, error) {
    if mio.obj == nil {
        return 0, errors.New("object is not opened")
    }

    switch whence {
    case io.SeekStart:
        if offset < 0 {
            return 0, errors.New("offset must be >= 0 for SeekStart")
        }
        mio.off = offset
    case io.SeekCurrent:
        if int64(mio.off) + offset < 0 {
            return 0, fmt.Errorf("curr+offset (%v+%v) must be >= 0",
                                 mio.off, offset)
        }
        mio.off += offset
    case io.SeekEnd:
        return 0, errors.New("Motr object is size-less, its end is unknown")
    default:
        return 0, fmt.Errorf("Invalid / unknown whence argument: %v", whence)
    }

    return int64(mio.off), nil
}

// vi: sw=4 ts=4 expandtab ai
