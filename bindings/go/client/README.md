High level Client for Motr
==========================

To run this sample, first start standalone motr by running command below:

```sh
> sudo su root
> cd cortx-motr/motr/examples/
>./setup_a_running_motr_system.sh
```

Go to directory: `cortx-motr/bindings/go`

Modify `mio/mio.go` find the variable and adding value as below. 
Because these variables are package private, that was no way to modify in `client.go`.

```golang
var verbose bool = true
var threadsN int = 1
```

Then go to directory: `cortx-motr/bindings/go/client`, open `client_test.go`
and change which motr server to point to ( which the value you could obtain when start motr ) :

```golang
func TestInit() client option:

	client, err = NewClient(&Options{
		HAAddr:     "192.168.63.52@tcp:12345:34:1",
		LocalAddr:  "192.168.63.52@tcp:12345:33:1000",
		Profile:    "0x7000000000000001:0",
		ProcessFId: "0x7200000000000001:64",
		TraceOn:    false,
	})
```

Then run command below to load test dependency library
```sh
> go mod tidy
```

lastly, run
```sh
> go test
```

You should see the result and the time it take to run the operation.

If you received this error :
```sh
ERROR  [dix/req.c:801:dix_idxop_meta_update_ast_cb]  All items are failed
```
It mean index already exist, you might want to comment TestCreate().

If the test case is hang, you probably not setting threadsN in mio.go.