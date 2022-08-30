
## Motr REST API server

Motr REST API Server is a simple REST API Server which based on motr go bindings. 

It included a REST API server and a http client.

The API implemented as below:

```
kv/put
kv/get
kv/delete
object/read
object/write
```

#### Server

To start REST API server, first you need to run motr server.

Follow motr [quick start guide](/doc/Quick-Start-Guide.rst) to build motr.

To start motr in development mode, open a new terminal, go to `%cortx_motr_dir%/motr/examples` at `motr` root directory.

```sh
cd %cortx_home%/motr/examples
./setup_a_running_motr_system.sh
```

After motr server start, you should see the connection information at the end of terminal.

At this directory, open `motr.toml`, this is where `motr` connection configure, modify the value accordingly and save the file.

Open another terminal and run the REST API server :
```
> go mod tidy
> export MOTR_HOME=motr_root_diretory
> CGO_CFLAGS="-I/$MOTR_HOME -I/usr/include/motr -DM0_EXTERN=extern -DM0_INTERNAL= -Wno-attributes" CGO_LDFLAGS="-L$MOTR_HOME/motr/.libs -Wl,-rpath=$MOTR_HOME/motr/.libs -lmotr" go run .
```

You should see the server start and running at port `8081`.


#### Client

To interect with the REST API server, you could use http client under `http_client` folder:

```go
client := NewClient("localhost", 8081)

// read from file and write to motr
response, err := client.Write(IndexID, "black_hole.png")

// read from motr and write to file
err := client.Read(IndexID, objectSize, "out.png")
```

Open another terminal and run the tests to see if the http client work as expected.
```
> cd http_client
> go test -v 
```

You could refer to test cases for more sample of client usage.


