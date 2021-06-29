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
 * Original creation date: 28-Apr-2021
 */

package main

import (
    "os"
    "fmt"
    "flag"
    "log"
    "motr/mio"
)

func usage() {
    fmt.Fprintf(flag.CommandLine.Output(),
`Usage: %s [options] index_id key [value]

 With value present it will be PUT operation.
 Without value it will be GET operation.

`, os.Args[0])
    flag.PrintDefaults()
}

var createFlag bool
var updateFlag bool
var deleteFlag bool

func init() {
    log.SetFlags(log.Ldate | log.Ltime | log.Lshortfile)
    flag.Usage = usage
    flag.BoolVar(&createFlag, "c", false, "create index if not present")
    flag.BoolVar(&updateFlag, "u", false, "update value at the existing key")
    flag.BoolVar(&deleteFlag, "d", false, "delete the record by the key")
}

func main() {
    mio.Init()
    if flag.NArg() != 2 && flag.NArg() != 3 {
        usage()
        os.Exit(1)
    }

    indexID := flag.Arg(0)

    var mkv mio.Mkv
    if err := mkv.Open(indexID, createFlag); err != nil {
        log.Fatalf("failed to open index %v: %v", indexID, err)
    }
    defer mkv.Close()

    if flag.NArg() == 3 {
        if deleteFlag {
            log.Printf("cannot delete and put at the same time")
            usage()
            os.Exit(1)
        }
        err := mkv.Put([]byte(flag.Arg(1)), []byte(flag.Arg(2)), updateFlag)
        if err != nil {
            log.Fatalf("failed to put: %v", err)
        }
    } else {
        if deleteFlag {
            err := mkv.Delete([]byte(flag.Arg(1)))
            if err != nil {
                log.Fatalf("failed to delete: %v", err)
            }
        } else {
            value, err := mkv.Get([]byte(flag.Arg(1)))
            if err != nil {
                log.Fatalf("failed to get: %v", err)
            }
            fmt.Printf("%s\n", value)
        }
    }
}

// vi: sw=4 ts=4 expandtab ai
