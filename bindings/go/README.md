# Go bindings for Motr - mio

`mio` Go package implements Reader/Writer interface over Motr client I/O API.
This allows writing Motr client applications quickly and efficiently in the Go language.

`mio` automatically determines the optimal unit (stripe) size for the newly created object
(based on the object size provided by user in the mio.Create(obj, sz) call), as well as
the optimal block size for Motr I/O based on the cluster configuration. So users don't have
to bother about tuning these Motr-specific parameters for each specific object to reach
maximum I/O performance on it and yet don't waste space (in case of a small objects).

`mio` allows to read/write the blocks to Motr in parallel threads (see `-threads` option)
provided there is enough buffer size to accomodate several of such blocks in one
Read()/Write() request. (For example, see the source code of `mcp` utility and its `-bsz`
option.)

`mcp` (Motr cp) utility is a client application example written in pure Go which uses
`mio` package and has only 97 lines of code (as of 30 Oct 2020):

```Text
Usage: mcp [options] src dst

 At least one of src and dst arguments must be object id.
 The other can be file path or '-' for stdin/stdout.

  -bsz size
    	I/O buffer size (in Mbytes) (default 32)
  -ep endpoint
    	my endpoint address
  -hax endpoint
    	local hax endpoint address
  -osz size
    	object size (in Kbytes)
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

In order to build, [build Motr](../../doc/Quick-Start-Guide.rst) first
(or install motr-devel package). Then run:

```sh
cd motr/bindings/go/mcp && go build
```
