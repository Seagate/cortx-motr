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
 * Original creation date: 28-Apr-2021
 */

package main

import (
    "os"
    "fmt"
    "flag"
    "log"
    "../mio"
)

func usage() {
    fmt.Fprintf(flag.CommandLine.Output(),
`Usage: %s [options] idx_id key [value]

 With value argument present it will be PUT operation.
 Without value argument it will be GET operation.
`, os.Args[0])
    flag.PrintDefaults()
}

var valueSize uint64

func init() {
    log.SetFlags(log.Ldate | log.Ltime | log.Lshortfile)
    flag.Usage = usage
    flag.Uint64Var(&valueSize, "vsz", 1, "value `size` (in KiB)")
}

func main() {
    mio.Init()
    if flag.NArg() != 2 || flag.NArg() != 3 {
        usage()
        os.Exit(1)
    }

    valueSize *= 1024

    index_id := flag.Arg(0)

    var mkv mio.Mkv
    if err := mkv.Open(index_id); err != nil {
        log.Fatalf("failed to open index %v: %v", index_id, err)
    }
    defer mkv.Close()

    if flag.NArg() == 3 {
        err := mkv.Put([]byte(flag.Arg(1)), []byte(flag.Arg(2)))
        if err != nil {
            log.Fatalf("failed to put: %v", err)
        }
    } else {
        value, err := mkv.Get([]byte(flag.Arg(1)), valueSize)
        if err != nil {
            log.Fatalf("failed to get: %v", err)
        }
        log.Printf("%s\n", value)
    }
}
