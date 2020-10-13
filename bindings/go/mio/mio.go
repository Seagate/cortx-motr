package main

// #cgo CFLAGS: -I../../.. -I../../../extra-libs/galois/include
// #cgo CFLAGS: -DM0_EXTERN=extern -DM0_INTERNAL=
// #cgo CFLAGS: -Wno-attributes
// #cgo LDFLAGS: -L../../../motr/.libs -lmotr
// #include <stdlib.h>
// #include "lib/types.h"
// #include "lib/trace.h"
// #include "motr/client.h"
// #include "motr/layout.h" /* m0c_pools_common */
// #include "motr/idx.h"
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
    "errors"
    "os"
    "unsafe"
    "fmt"
    "flag"
    "log"
)

type Mio struct {
    obj_id  C.struct_m0_uint128
    obj    *C.struct_m0_obj
    obj_lid uint
}

func usage() {
    fmt.Fprintf(flag.CommandLine.Output(),
                "Usage: %s [options]\n",
                os.Args[0])
    flag.PrintDefaults()
}

func check_arg(arg *string, name string) {
    if *arg == "" {
        fmt.Printf("%s: %s must be specified\n", os.Args[0], name)
        usage()
        os.Exit(1)
    }
}

var obj_id string

func init() {
    flag.Usage = usage
    log.SetFlags(log.Ldate | log.Ltime | log.Lshortfile)

    // Mandatory
    local_ep := flag.String("ep", "", "my `endpoint` address")
    hax_ep   := flag.String("hax", "", "local hax `endpoint` address")
    profile  := flag.String("prof", "", "cluster profile `fid`")
    proc_fid := flag.String("proc", "", "my process `fid`")
    flag.StringVar(&obj_id, "obj", "", "object `id` to work with")
    // Optional
    trace_on := flag.Bool("trace", false, "generate m0trace.pid file")

    flag.Parse()

    check_arg(local_ep, "my endpoint (-ep)")
    check_arg(hax_ep, "hax endpoint (-hax)")
    check_arg(profile, "profile fid (-prof)")
    check_arg(proc_fid, "my process fid (-proc)")
    check_arg(&obj_id, "object id (-obj)")

    if !*trace_on {
        C.m0_trace_set_mmapped_buffer(false)
    }

    C.conf.mc_is_oostore     = true
    C.conf.mc_local_addr     = C.CString(*local_ep)
    C.conf.mc_ha_addr        = C.CString(*hax_ep)
    C.conf.mc_profile        = C.CString(*profile)
    C.conf.mc_process_fid    = C.CString(*proc_fid)
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

func scan_id(s string) (fid C.struct_m0_uint128, err error) {
    cs := C.CString(s)
    rc := C.m0_uint128_sscanf(cs, &fid)
    C.free(unsafe.Pointer(cs))
    if rc != 0 {
        return fid, errors.New(fmt.Sprintf("failed to parse fid: %d", rc))
    }
    return fid, nil
}

func (mio *Mio) obj_new(id string) (err error) {
    mio.obj_id, err = scan_id(id)
    if err != nil {
        return err
    }
    mio.obj = (*C.struct_m0_obj)(C.calloc(1, C.sizeof_struct_m0_obj))
    return nil
}

func (mio *Mio) Open(id string) (err error) {
    if mio.obj != nil {
        return errors.New("object is already opened")
    }
    err = mio.obj_new(id)
    if err != nil {
        return err
    }
    C.m0_obj_init(mio.obj, &C.container.co_realm, &mio.obj_id, 0)
    rc := C.m0_open_entity(&mio.obj.ob_entity);
    if rc != 0 {
        mio.Close()
        return errors.New(fmt.Sprintf("failed to open object entity: %d", rc))
    }
    mio.obj_lid = uint(mio.obj.ob_attr.oa_layout_id)
    return nil
}

func bits(values ...C.ulong) (res C.ulong) {
    for _, v := range values {
        res |= (1 << v)
    }
    return res
}

func (mio *Mio) Create(id string, sz uint64) (err error) {
    if mio.obj != nil {
        return errors.New("object is already opened")
    }
    err = mio.obj_new(id)
    if err != nil {
        return err
    }
    C.m0_obj_init(mio.obj, &C.container.co_realm, &mio.obj_id, 9)
    var op *C.struct_m0_op
    rc := C.m0_entity_create(nil, &mio.obj.ob_entity, &op)
    if rc != 0 {
        return errors.New(fmt.Sprintf("failed to create object: %d", rc))
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
        return errors.New(fmt.Sprintf("create op failed: %d", rc))
    }

    return nil
}

func roundup_power2(x uint64) (power uint64) {
    for power = 1; power < x; power *= 2 {}
    return power
}

const MAX_M0_BUFSZ = 128 * 1024 * 1024

// Calculate the optimal block size for the I/O
func (mio *Mio) get_optimal_bs(obj_sz uint64) uint64 {
    if obj_sz > MAX_M0_BUFSZ {
            obj_sz = MAX_M0_BUFSZ
    }
    pver := C.m0_pool_version_find(&C.instance.m0c_pools_common,
                                   &mio.obj.ob_attr.oa_pver)
    if pver == nil {
            log.Panic("cannot find the object's pool version")
    }
    usz := C.m0_obj_layout_id_to_unit_size(C.ulong(mio.obj_lid))
    pa := &pver.pv_attr
    gsz := C.uint(usz) * pa.pa_N
    /* max 2-times pool-width deep, otherwise we may get -E2BIG */
    max_bs := C.uint(usz) * 2 * pa.pa_P * pa.pa_N / (pa.pa_N + 2 * pa.pa_K)

    if obj_sz >= uint64(max_bs) {
            return uint64(max_bs)
    } else if obj_sz <= uint64(gsz) {
            return uint64(gsz)
    } else {
            return roundup_power2(obj_sz)
    }
}

func (mio *Mio) Write(p []byte) (n int, err error) {
    if mio.obj == nil {
        return 0, errors.New("object is not opened")
    }
    for left := len(p); left > 0; {
        
    }
    return 0, nil
}

func (mio *Mio) Close() {
    if mio.obj == nil {
        return
    }
    C.m0_obj_fini(mio.obj)
    C.free(unsafe.Pointer(mio.obj))
    mio.obj = nil
}

func main() {
    var mio Mio
    err := mio.Open(obj_id)
    if err == nil {
        fmt.Println("object already exists")
    }
    if err != nil {
        err = mio.Create(obj_id, 10000000)
        if err != nil {
            log.Panic(err)
        }
        fmt.Println("object created")
    }
    defer mio.Close()
}
