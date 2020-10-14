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

var buf_size int

func init() {
    flag.Usage = usage
    flag.IntVar(&buf_size, "bsz", 32, "I/O buffer `size` (in Mbytes)")
    log.SetFlags(log.Ldate | log.Ltime | log.Lshortfile)
}

func main() {
    mio.Init()
    if flag.NArg() != 2 {
        usage()
        os.Exit(1)
    }

    src, dst := flag.Arg(0), flag.Arg(1)

    var mio_r, mio_w mio.Mio

    var reader io.Reader
    if _, err := mio.Scan_id(src); err == nil {
        if err = mio_r.Open(src); err != nil {
            log.Fatalf("failed to open object %v: %v", src, err)
        }
        defer mio_r.Close()
        reader = &mio_r
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
        mio.Obj_size = uint64(info.Size())
        reader = file
    }

    var writer io.Writer
    if _, err := mio.Scan_id(dst); err == nil {
        if err = mio_w.Open(dst); err != nil {
            if err = mio_w.Create(dst, mio.Obj_size); err != nil {
                log.Fatalf("failed to create object %v: %v", dst, err)
            }
        }
        defer mio_w.Close()
        writer = &mio_w
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

    buf := make([]byte, buf_size * 1024 * 1024)
    io.CopyBuffer(writer, reader, buf)
}
