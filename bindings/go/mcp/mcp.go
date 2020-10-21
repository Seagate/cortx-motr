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

var bufSize int

func init() {
    flag.Usage = usage
    flag.IntVar(&bufSize, "bsz", 32, "I/O buffer `size` (in Mbytes)")
    log.SetFlags(log.Ldate | log.Ltime | log.Lshortfile)
}

func main() {
    mio.Init()
    if flag.NArg() != 2 {
        usage()
        os.Exit(1)
    }

    src, dst := flag.Arg(0), flag.Arg(1)

    var mioR, mioW mio.Mio

    var reader io.Reader
    if _, err := mio.ScanID(src); err == nil {
        if err = mioR.Open(src); err != nil {
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
        if mio.ObjSize == 0 {
            mio.ObjSize = uint64(info.Size())
        }
        reader = file
    }

    var writer io.Writer
    if _, err := mio.ScanID(dst); err == nil {
        if err = mioW.Open(dst); err != nil {
            if err = mioW.Create(dst, mio.ObjSize); err != nil {
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
    // always use buf, hack from https://github.com/golang/go/issues/16474
    io.CopyBuffer(struct{ io.Writer }{writer}, struct{ io.Reader }{reader}, buf)
}
