# Go bindings for Motr - go/mio

`go/mio` Go package implements
[Reader](https://pkg.go.dev/io#Reader) /
[Writer](https://pkg.go.dev/io#Writer)
interface over Motr client I/O API.
This allows writing Motr client applications quickly and efficiently in the Go language.

`go/mio` automatically determines the optimal unit (stripe) size for the newly created object
(based on the object size provided by user in the mio.Create(obj, sz) call), as well as
the optimal block size for Motr I/O based on the cluster configuration. So users don't have
to bother about tuning these Motr-specific parameters for each specific object to reach
maximum I/O performance on it and yet don't waste space (in case of a small objects).

`go/mio` allows to read/write the blocks to Motr in parallel threads (see `-threads` option)
provided there is enough buffer size to accomodate several of such blocks in one
Read()/Write() request. (For example, see the source code of `mcp` utility and its `-bsz`
option.)

To use `go/mio` in your app 1) install motr-devel pkg and 2):

```Go
import github.com/Seagate/cortx-motr/bindings/go/mio
```

## mcp

`mcp` (Motr cp) utility is a client application example written in pure Go which uses
`go/mio` package and has only 97 lines of code (as of 30 Oct 2020). It allows to copy
Motr objects to/from a file or between themselves:

```Text
Usage: mcp [options] src dst

 At least one of src and dst arguments must be object id.
 The other can be file path or '-' for stdin/stdout.

  -bsz size
    	i/o buffer size (in MiB) (default 32)
  -ep endpoint
    	my endpoint address
  -hax endpoint
    	local hax endpoint address
  -off offset
    	start object i/o at offset (in KiB)
  -osz size
    	object size (in KiB)
  -pool fid
    	pool fid to create object at
  -proc fid
    	my process fid
  -prof fid
    	cluster profile fid
  -threads number
    	number of threads to use (default 1)
  -trace
    	generate m0trace.pid file
  -v	be more verbose
```

In order to build:

 1. [Build Motr](../../doc/Quick-Start-Guide.rst) first
   (or install pre-built motr-devel package).
 2. Install Go: `sudo yum install go`.

Then run:

```sh
cd motr/bindings/go/mcp && go build && go install
```

The binary will be installed to your `GOBIN` directory
(check with `go env GOBIN` or `echo $(go env GOPATH)/bin`).

See the usage example at this discussion thread -
https://github.com/Seagate/cortx-motr/discussions/285.

## mkv

`mkv` is a trivial reference utility which can do two basic operations
over Motr distributed Key-Value Store (aka Index): PUT and GET.

```Text
Usage: mkv [options] index_id key [value]

 With value present it will be PUT operation.
 Without value it will be GET operation.

  -c	create index if not present
  -d	delete the record by the key
  -ep endpoint
    	my endpoint address
  -hax endpoint
    	local hax endpoint address
  -proc fid
    	my process fid
  -prof fid
    	cluster profile fid
  -threads number
    	number of threads to use (default 1)
  -trace
    	generate m0trace.pid file
  -u	update value at the existing key
  -v	be more verbose
```

The steps to build and install are similar to `mcp` (see above).
