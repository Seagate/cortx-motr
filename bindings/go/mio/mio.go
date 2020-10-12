package main

// #cgo CFLAGS: -I../../.. -DM0_EXTERN=extern -DM0_INTERNAL=
// #cgo CFLAGS: -Wno-attributes
// #cgo LDFLAGS: -L../../../motr/.libs -lmotr
// #include "lib/trace.h"
// #include "motr/client.h"
// #include "motr/idx.h"
//
// struct m0_client    *instance = NULL;
// struct m0_container container;
// struct m0_realm     uber_realm;
// struct m0_config    conf = {};
// struct m0_idx_dix_config dix_conf = {};
import "C"

import (
    "os"
    "unsafe"
    "fmt"
    "flag"
    "log"
)

type mio struct {
    conf C.struct_m0_config
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

func init() {
    flag.Usage = usage
    log.SetPrefix(os.Args[0] + ": ")
    log.SetFlags(log.Lmsgprefix | log.Ldate | log.Ltime)

    // Mandatory
    local_ep := flag.String("ep", "", "my `endpoint` address")
    hax_ep   := flag.String("hax", "", "local hax `endpoint` address")
    profile  := flag.String("prof", "", "cluster profile `fid`")
    proc_fid := flag.String("proc", "", "my process `fid`")
    // Optional
    trace_on := flag.Bool("trace", false, "generate m0trace.pid file")

    flag.Parse()

    check_arg(local_ep, "my endpoint (-ep)")
    check_arg(hax_ep, "hax endpoint (-hax)")
    check_arg(profile, "profile fid (-prof)")
    check_arg(proc_fid, "my process fid (-proc)")

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
}

func main() {
    rc := C.m0_client_init(&C.instance, &C.conf, true)
    if rc != 0 {
        log.Panicf("m0_client_init() failed: %v", rc)
    }
    defer C.m0_client_fini(C.instance, true)
}
