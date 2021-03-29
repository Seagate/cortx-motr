# How to run the test

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

## Hare usage

Hare is used for cluster bootstrap and shutdown. Firstly
it needs to be patched by the hare.patch in current directory
to make it able to generate clusted configuration that includes
DTM0 service. After that it should be built and installed into
the system (see README.md in the root of Hare repo).

## m0crate configuration

Before running the test the m0crate.yaml should be modified
according to local machine settings (IPs, service endpoints
and so on). To get the required information (and also to check
that Hare is able to bootstrap) the part related to m0crate run
and cluster shutdown can be commented, after that the test can
be run just typing the following:

$ sudo ./all2all

It bootstraps the cluster, does required checks and leaves m0d
processes running. Then the hctl status command can be run to
get the information about IPs and endpoints:

$ hctl status

After m0crate config file modififcations are done the cluster
can be stopped manually:

$ sudo hctl shutdown

Then uncomment the part related to m0crate and cluster shutdown.
Now the test is ready to be run:

$ sudo ./all2all
