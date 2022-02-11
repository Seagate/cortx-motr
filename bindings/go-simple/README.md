
go-simple
=========

Simple is a yet another golang bindings for motr. It is a thin wrapper to C motr client.

Build
=====

Make sure motr is build first.

Compile `libsimple.so`

```
cd lib
gcc -I/home/502380/cortx-motr -DM0_EXTERN=extern -DM0_INTERNAL= -Wno-attributes -L/home/502380/cortx-motr/motr/.libs -fPIC -shared simple.c -o libsimple.so
```

Go to  `client.go`, open and make a change at top of the line, replace `path_to_motr` to cortx-motr parent directory,
This is use by cgo for code compilation.

`#cgo LDFLAGS: -L${SRCDIR}/lib -lsimple -L/path_to_motr/cortx-motr/motr/.libs -lmotr`


Run
===

Go to `examples` folder and run sh below, it start a single instance of motr.
```
%MOTR_HOME%/motr/examples> ./setup_a_running_motr_system.sh
```

Export LD library to make sure when go-simple run, it could link to the library.

```
export LD_LIBRARY_PATH=/home/502380/cortx-motr/bindings/go-simple/lib:/home/502380/cortx-motr/motr/.libs
```

Go to `client_test.go`, replace the connection parameters.

Then run 

```
go test
```