
go-Simple
=========

Simple is yet another golang bindings for motr

How to go simple 
================

Compile libsimple.so

```
gcc -I/home/502380/cortx-motr -DM0_EXTERN=extern -DM0_INTERNAL= -Wno-attributes -L/home/502380/cortx-motr/motr/.libs -fPIC -shared simple.c -o libsimple.so
```

Export library

```
export LD_LIBRARY_PATH=/home/502380/cortx-motr/bindings/go-simple/lib:/home/502380/cortx-motr/motr/.libs
```

Start motr, go to examples folder:
```
%MOTR_HOME%/motr/examples> ./setup_a_running_motr_system.sh
```

Make a change to the `client.go`, change the motr home directory for compile

`#cgo LDFLAGS: -L${SRCDIR}/lib -lsimple -L/your/path/cortx-motr/motr/.libs -lmotr`

Run go test

`go test`