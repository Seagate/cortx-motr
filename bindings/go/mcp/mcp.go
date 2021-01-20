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

package main

import (
    "os"
    "io"
    "fmt"
    "flag"
    "log"
    "../mio"
)

func usage() {
    fmt.Fprintf(flag.CommandLine.Output(),
`Usage: %s [options] src dst

 At least one of src and dst arguments must be object id.
 The other can be file path or '-' for stdin/stdout.

`, os.Args[0])
    flag.PrintDefaults()
}

var objSize uint64
var bufSize int
var pool    *string

func init() {
    log.SetFlags(log.Ldate | log.Ltime | log.Lshortfile)

    flag.Usage = usage
    flag.IntVar(&bufSize, "bsz", 32, "I/O buffer `size` (in Mbytes)")
    flag.Uint64Var(&objSize, "osz", 0, "object `size` (in Kbytes)")
    pool = flag.String("pool", "", "pool `fid` to create object at")
}

func main() {
    mio.Init()
    if flag.NArg() != 2 {
        usage()
        os.Exit(1)
    }
    objSize *= 1024

    src, dst := flag.Arg(0), flag.Arg(1)

    var mioR, mioW mio.Mio

    var reader io.Reader
    if _, err := mio.ScanID(src); err == nil {
        if err = mioR.Open(src, objSize); err != nil {
            log.Fatalf("failed to open object %v: %v", src, err)
        }
        defer mioR.Close()
        reader = &mioR
    } else if src == "-" {
        reader = os.Stdin
    } else {
        file, err := os.Open(src)
        if err != nil {
            log.Fatalf("failed to open file %v: %v", src, err)
        }
        defer file.Close()
        info, err := file.Stat()
        if err != nil {
            log.Fatalf("failed to get stat of file %v: %v", src, err)
        }
        if objSize == 0 {
            objSize = uint64(info.Size())
        }
        reader = file
    }

    var writer io.Writer
    if _, err := mio.ScanID(dst); err == nil {
        if err = mioW.Open(dst); err != nil {
            if err = mioW.Create(dst, objSize, *pool); err != nil {
                log.Fatalf("failed to create object %v: %v", dst, err)
            }
        }
        defer mioW.Close()
        writer = &mioW
    } else if dst == "-" {
        writer = os.Stdout
    } else {
        file, err := os.OpenFile(dst, os.O_WRONLY|os.O_CREATE, 0655)
        if err != nil {
            log.Fatalf("failed to open file %v: %v", dst, err)
        }
        defer file.Close()
        writer = file
    }

    buf := make([]byte, bufSize * 1024 * 1024)
    // Force Writer to always use buf.(Otherwise, the performance
    // might suffer. A tip from https://github.com/golang/go/issues/16474)
    io.CopyBuffer(struct{ io.Writer }{writer}, reader, buf)
}
