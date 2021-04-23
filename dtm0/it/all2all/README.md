# How to run the test

## Prerequisit
lnet service should be running check with following command
```sh
sudo systemctl status lnet.service
```

If not running, please refer troubleshooting section in [README.md](https://github.com/Seagate/cortx-hare/blob/main/README.md#data_iface)

## About

This integration test is used to check whether whole
chain including Hare, m0d processes and client process
works and client can write key/value pairs with DTX
enabled.

What the test does:
 - bootstraps the cluster of 3 m0d processes using Hare
 - checks that no any handling of HA messages is done by
   DTM0 service
 - sends HA TRANSIENT messages to m0d's to trigger HA messages
   handling by DTM0 services
 - checks that all DTM0 services are ready to handle HA
   messages
 - sends HA ONLINE messages to m0d's to trigger connections
   logic
 - checks that all m0d's are connected with each other
 - run the m0crate to write key/value pairs
 - sends HA client ONLINE messages to m0d's to trigger
   connections from m0d's to the client
 - waits for m0crate completion

## Command to run this test
```sh
sudo ./all2all
```

## Hare usage

Hare is used for cluster bootstrap and shutdown. Firstly
it needs to be patched by the hare.patch in current directory
to make it able to generate clusted configuration that includes
DTM0 service. After that it should be built and installed into
the system (see [README.md](https://github.com/Seagate/cortx-hare/blob/main/README.md) in the root of Hare repo).

## Troubleshooting
### 1. Test fails for starting motr processes
If test fails at the stage where it start motr processes, please check if /etc/modprobe.d/lnet.conf has same interface as 
the lnet service is running on. If interfaces does not match then refer [this](https://github.com/Seagate/cortx-hare/blob/main/README.md#data_iface)

### 2. Test is stuck at starting motr process for first phase
If the test is stuck at "Starting Motr (phase1, mkfs)... " like shown below, then there is possbility that 
you have pulled latest code for cortx-motr/dtm0-main branch and rebuilt it, but not rebuilt the hare code again.

```sh
[root@ssc-vm-c-1966 all2all]# sudo ./all2all
INFO: Bootstrapping the cluster using Hare...
2021-04-22 00:12:06: Generating cluster configuration... OK
2021-04-22 00:12:07: Starting Consul server on this node......... OK
2021-04-22 00:12:14: Importing configuration into the KV store... OK
2021-04-22 00:12:14: Starting Consul on other nodes... OK
2021-04-22 00:12:14: Updating Consul configuraton from the KV store... OK
2021-04-22 00:12:15: Waiting for the RC Leader to get elected.... OK
2021-04-22 00:12:17: Starting Motr (phase1, mkfs)...

```
Solution:
In case you have pulled latest code, please rebuild motr and hare both. 
For building motr code again use following command
```sh
[root@ssc-vm-c-1966 cortx-motr]# time { MAKE_OPTS=-j32 CONFIGURE_OPTS=--disable-altogether-mode\ --enable-debug\ --enable-dtm0\ --with-trace-ubuf-size=32  ./scripts/m0 rebuild || echo FAIL; }
```
For building hare code use following command
```sh
[root@ssc-vm-c-1966 hare]# sudo make uninstall && sudo make clean && make && sudo make install
```
Now start the hare-consul-agent and reload the daemon using following command

```sh
[root@ssc-vm-c-1966 hare]# sudo service hare-consul-agent stop && sleep 5 && sudo systemctl daemon-reload
```
Try running the test again. Refer [this](https://github.com/Seagate/cortx-motr/blob/dtm0-main/dtm0/it/all2all/README.md#command-to-run-this-test)
