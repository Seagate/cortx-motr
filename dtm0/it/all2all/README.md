# How to run the test

## Prerequisit
lnet service should be running check with following command
```
sudo systemctl status lnet.service
```

If not running, please refer troubleshooting section in [README.md](https://github.com/Seagate/cortx-hare/blob/main/README.md)

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
```
sudo sh ./all2all
```

## Hare usage

Hare is used for cluster bootstrap and shutdown. Firstly
it needs to be patched by the hare.patch in current directory
to make it able to generate clusted configuration that includes
DTM0 service. After that it should be built and installed into
the system (see [README.md](https://github.com/Seagate/cortx-hare/blob/main/README.md) in the root of Hare repo).
