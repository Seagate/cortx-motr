package main

// #cgo CFLAGS: -I../../.. -DM0_EXTERN=extern -DM0_INTERNAL=
// #cgo CFLAGS: -Wno-attributes
// #cgo LDFLAGS: -L../../../motr/.libs -lmotr
// #include <stdlib.h>
// #include "lib/types.h"
// #include "lib/trace.h"
// #include "motr/client.h"
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

func (mio *Mio) Open(id string) (err error) {
    mio.obj_id, err = scan_id(id)
    if err != nil {
        return err
    }
    if mio.obj != nil {
        return errors.New("object is already opened")
    }
    mio.obj = (*C.struct_m0_obj)(C.calloc(1, C.sizeof_struct_m0_obj))
    C.m0_obj_init(mio.obj, &C.container.co_realm, &mio.obj_id, 0)
    rc := C.m0_open_entity(&mio.obj.ob_entity);
    if rc != 0 {
        return errors.New(fmt.Sprintf("failed to open object entity: %d", rc))
    }
    return nil
}

func bits(values ...C.ulong) (res C.ulong) {
    for _, v := range values {
        res |= (1 << v)
    }
    return res
}

func (mio *Mio) Create(id string) (err error) {
    if err = mio.Open(id); err == nil {
        return errors.New("the object already exists")
    }
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
        err = mio.Create(obj_id)
        if err != nil {
            log.Panic(err)
        }
        fmt.Println("object created")
    }
    defer mio.Close()
}
