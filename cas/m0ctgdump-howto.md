# Motr Catalogue Dump Tool

**m0ctgdump** is an offline Motr tool to dump a specified catalogue as Key/Val pairs in HEX encoded format or in plain string.
This is an offline tool, so it can only be used when Motr cluster is already stopped but its data is still there.

## How to use m0ctgdump

1.  Follow the [motr QSG guide](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst) in cortx-motr repo to build Motr.

2.  Follow the [hare QSG guide](https://github.com/Seagate/cortx-hare/blob/main/README.md) in the cortx-hare repo to get a Motr cluster up and running.

    2.1. Run `hctl status` to verify your cluster is up. See the following examples.
    ```Text
	bash-4.4$
	bash-4.4$
	bash-4.4$ cat dtm0_cdf.yaml
	nodes:
	  - hostname: localhost
	    data_iface: eth0
	    data_iface_type: tcp
	    transport_type: libfab
	    m0_servers:
	      - runs_confd: true
	        io_disks:
		          data: []
		      - io_disks:
		          data:
		            - path: /dev/loop0
		            - path: /dev/loop1
		      - io_disks:
		          data:
		            - path: /dev/loop2
		            - path: /dev/loop3
		      - io_disks:
		          data:
		            - path: /dev/loop4
		            - path: /dev/loop5
		    m0_clients:
		        s3: 0
		        other: 1
		
		pools:
		  - name: SNS pool
		    type: sns  # optional; supported values: "sns" (default), "dix", "md"
		    data_units: 1
		    parity_units: 1
		    allowed_failures: { site: 0, rack: 0, encl: 0, ctrl: 1, disk: 1 }
		
		  - name: DIX pool
		    type: dix  # optional; supported values: "sns" (default), "dix", "md"
		    data_units: 1
		    parity_units: 2
		    spare_units: 0
		    allowed_failures: { site: 0, rack: 0, encl: 0, ctrl: 1, disk: 1 }
		
	bash-4.4$
	bash-4.4$
	bash-4.4$
	bash-4.4$ cat ~/mkdisk.sh
	sudo mkdir -p /var/motr
	for i in {0..9}; do
	    sudo dd if=/dev/zero of=/var/motr/disk$i.img bs=1M seek=9999 count=1
	    sudo losetup /dev/loop$i /var/motr/disk$i.img
	done
	bash-4.4$ sudo sh ~/mkdisk.sh
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.00240074 s, 437 MB/s
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.00191779 s, 547 MB/s
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.00253795 s, 413 MB/s
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.00215872 s, 486 MB/s
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.0026171 s, 401 MB/s
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.00256998 s, 408 MB/s
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.00198717 s, 528 MB/s
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.00254844 s, 411 MB/s
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.00211003 s, 497 MB/s
	1+0 records in
	1+0 records out
	1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.0022449 s, 467 MB/s
	bash-4.4$
	bash-4.4$
	bash-4.4$
	bash-4.4$ sudo hctl bootstrap --mkfs dtm0_cdf.yaml
	[sudo] password for 520428: 
	2022-02-17 22:14:24: Generating cluster configuration... OK
	2022-02-17 22:14:25: Starting Consul server on this node........... OK
	2022-02-17 22:14:34: Importing configuration into the KV store... OK
	2022-02-17 22:14:35: Starting Consul on other nodes...Consul ready on all nodes
	2022-02-17 22:14:35: Updating Consul configuraton from the KV store... OK
	2022-02-17 22:14:35: Waiting for the RC Leader to get elected..... OK
	2022-02-17 22:14:38: Starting Motr (phase1, mkfs)... OK
	2022-02-17 22:14:44: Starting Motr (phase1, m0d)... OK
	2022-02-17 22:14:46: Starting Motr (phase2, mkfs)... OK
	2022-02-17 22:15:04: Starting Motr (phase2, m0d)... OK
	2022-02-17 22:15:11: Checking health of services... OK
	bash-4.4$
	bash-4.4$
	bash-4.4$
	bash-4.4$ sudo hctl status
	Byte_count:
	    critical_byte_count : 0
	    damaged_byte_count : 0
	    degraded_byte_count : 0
	    healthy_byte_count : 0
	Data pool:
	    # fid name
	    0x6f00000000000001:0x3e 'SNS pool'
	Profile:
	    # fid name: pool(s)
	    0x7000000000000001:0x65 'default': 'SNS pool' 'DIX pool' None
	Services:
	    localhost  (RC)
	    [started]  hax        0x7200000000000001:0x8   inet:tcp:10.230.240.203@22001
	    [started]  confd      0x7200000000000001:0xb   inet:tcp:10.230.240.203@21001
	    [started]  ioservice  0x7200000000000001:0xe   inet:tcp:10.230.240.203@21002
	    [started]  ioservice  0x7200000000000001:0x1b  inet:tcp:10.230.240.203@21003
	    [started]  ioservice  0x7200000000000001:0x28  inet:tcp:10.230.240.203@21004
	    [unknown]  m0_client  0x7200000000000001:0x35  inet:tcp:10.230.240.203@21501
	bash-4.4$ ps ax | grep m0d
	2819727 ?        Ssl    0:00 /home/520428/work/cortx-motr/motr/.libs/lt-m0d -e libfab:inet:tcp:10.230.240.203@21001 -A linuxstob:addb-stobs -f <0x7200000000000001:0xb> -T linux -S stobs -D db -m 524288 -q 16 -w 8 -c /etc/motr/confd.xc -H inet:tcp:10.230.240.203@22001 -U -r 134217728
	2820822 ?        Ssl    0:00 /home/520428/work/cortx-motr/motr/.libs/lt-m0d -e libfab:inet:tcp:10.230.240.203@21002 -A linuxstob:addb-stobs -f <0x7200000000000001:0xe> -T ad -S stobs -D db -m 524288 -q 16 -w 8 -H inet:tcp:10.230.240.203@22001 -U -r 134217728
	2821003 ?        Ssl    0:00 /home/520428/work/cortx-motr/motr/.libs/lt-m0d -e libfab:inet:tcp:10.230.240.203@21003 -A linuxstob:addb-stobs -f <0x7200000000000001:0x1b> -T ad -S stobs -D db -m 524288 -q 16 -w 8 -H inet:tcp:10.230.240.203@22001 -U -r 134217728
	2821185 ?        Ssl    0:00 /home/520428/work/cortx-motr/motr/.libs/lt-m0d -e libfab:inet:tcp:10.230.240.203@21004 -A linuxstob:addb-stobs -f <0x7200000000000001:0x28> -T ad -S stobs -D db -m 524288 -q 16 -w 8 -H inet:tcp:10.230.240.203@22001 -U -r 134217728
	2821909 pts/0    S+     0:00 grep m0d
	bash-4.4$
	bash-4.4$
    ```
	 
3.  Create an index and put some key/val pairs.
    Here is the example to create an index, with fid 12345:12345, and put 20 key/val pairs.
    ```Text
	    bash-4.4$
	    bash-4.4$ ./utils/m0kv -l inet:tcp:10.230.240.203@21501 -h inet:tcp:10.230.240.203@22001 -p 0x7000000000000001:0x65 -f 0x7200000000000001:0x35 -s index create 12345:12345
	    operation rc: 0
	    create done, rc: 0
	    Done, rc:  0
	    bash-4.4$
	    bash-4.4$
	    bash-4.4$ for ((i=0;i<20;i++)) ; do ./utils/m0kv -l inet:tcp:10.230.240.203@21501 -h inet:tcp:10.230.240.203@22001 -p 0x7000000000000001:0x65 -f 0x7200000000000001:0x35 -s index put 12345:12345 "key-$i" "some str value-$i" ; done
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    operation rc: 0
	    put done, rc: 0
	    Done, rc:  0
	    bash-4.4$
	    bash-4.4$
    ```
	         
4.  Shutdown the Motr cluster
    Now shutdown the cluster.
    ```Text
	 bash-4.4$ sudo hctl shutdown
	 [sudo] password for 520428:
	 Stopping m0d@0x7200000000000001:0xe (ios) at localhost...
	 Stopping m0d@0x7200000000000001:0x1b (ios) at localhost...
	 Stopping m0d@0x7200000000000001:0x28 (ios) at localhost...
	 Stopped m0d@0x7200000000000001:0xe (ios) at localhost
	 Stopped m0d@0x7200000000000001:0x28 (ios) at localhost
	 Stopped m0d@0x7200000000000001:0x1b (ios) at localhost
	 Stopping m0d@0x7200000000000001:0xb (confd) at localhost...
	 Stopped m0d@0x7200000000000001:0xb (confd) at localhost
	 Stopping hare-hax at localhost...
	 Stopped hare-hax at localhost
	 Making sure that RC leader can be re-elected next time
	 Stopping hare-consul-agent at localhost...
	 Stopped hare-consul-agent at localhost
	 Shutting down RC Leader at localhost...
	 bash-4.4$
	 bash-4.4$
	 bash-4.4$
	 bash-4.4$
    ```
	
5.  Run the m0ctgdump

    Here is the script to run m-ctgdump and its output. **m0ctgdump** takes the same arguments
    as m0mkfs and m0d. These arguments can be retrieved from the output of `hctl status`.
    The most important arguments are listed.
    ```Text
    -d: In a Motr cluster, use the default value: /etc/motr/disks-ios.conf
    -D: Use the prefix "/var/motr/" + process_id + "/db"
    -S: Use the prefix "/var/motr/" + process_id + "/stobs"
    -c: In a Motr cluster, use the default value: /etc/motr/confd.xc
    -e: Use the ios network endpoint
    -f: Use the ios process_id
    str: output key/val pair in string format
    dix_fid: user provided index FID
    ```
    Keep the other arguments as default.

    Here is the example output.
    ```Text
	bash-4.4$
	bash-4.4$
	bash-4.4$ cat /tmp/dump.sh
	#bash-4.4$ sudo hctl status
	#Byte_count:
	#    critical_byte_count : 0
	#    damaged_byte_count : 0
	#    degraded_byte_count : 0
	#    healthy_byte_count : 0
	#Data pool:
	#    # fid name
	#    0x6f00000000000001:0x3e 'SNS pool'
	#Profile:
	#    # fid name: pool(s)
	#    0x7000000000000001:0x65 'default': 'SNS pool' 'DIX pool' None
	#Services:
	#    localhost  (RC)
	#    [started]  hax        0x7200000000000001:0x8   inet:tcp:10.230.240.203@22001
	#    [started]  confd      0x7200000000000001:0xb   inet:tcp:10.230.240.203@21001
	#    [started]  ioservice  0x7200000000000001:0xe   inet:tcp:10.230.240.203@21002
	#    [started]  ioservice  0x7200000000000001:0x1b  inet:tcp:10.230.240.203@21003
	#    [started]  ioservice  0x7200000000000001:0x28  inet:tcp:10.230.240.203@21004
	#    [unknown]  m0_client  0x7200000000000001:0x35  inet:tcp:10.230.240.203@21501
	
	
	echo "Dumping ios1"
	sudo /home/520428/work/cortx-motr/cas/m0ctgdump  -T ad -d /etc/motr/disks-ios.conf -D /var/motr/m0d-0x7200000000000001\:0xe/db -S /var/motr/m0d-0x7200000000000001\:0xe/stobs \
	                                                 -A linuxstob:addb-stobs -w 4 -m 65536 -q 16 -N 100663296 -C 262144 -K 100663296 \
	                                                 -k 262144 -c /etc/motr/confd.xc -e libfab:inet:tcp:10.230.240.203@21002 -A linuxstob:addb-stobs -f 0x7200000000000001:0xe \
	                                                str 12345:12345
	
	
	echo "Dumping ios2"
	sudo /home/520428/work/cortx-motr/cas/m0ctgdump  -T ad -d /etc/motr/disks-ios.conf -D /var/motr/m0d-0x7200000000000001\:0x1b/db -S /var/motr/m0d-0x7200000000000001\:0x1b/stobs \
	                                                 -A linuxstob:addb-stobs -w 4 -m 65536 -q 16 -N 100663296 -C 262144 -K 100663296 \
	                                                 -k 262144 -c /etc/motr/confd.xc -e libfab:inet:tcp:10.230.240.203@21003 -A linuxstob:addb-stobs -f 0x7200000000000001:0x1b \
	                                                str 12345:12345
	
	
	echo "Dumping ios3"
	sudo /home/520428/work/cortx-motr/cas/m0ctgdump  -T ad -d /etc/motr/disks-ios.conf -D /var/motr/m0d-0x7200000000000001\:0x28/db -S /var/motr/m0d-0x7200000000000001\:0x28/stobs \
	                                                 -A linuxstob:addb-stobs -w 4 -m 65536 -q 16 -N 100663296 -C 262144 -K 100663296 \
	                                                 -k 262144 -c /etc/motr/confd.xc -e libfab:inet:tcp:10.230.240.203@21004 -A linuxstob:addb-stobs -f 0x7200000000000001:0x28 \
	                                                str 12345:12345
	bash-4.4$
	bash-4.4$
	bash-4.4$
	bash-4.4$
	bash-4.4$ sh /tmp/dump.sh
	Dumping ios1
	[sudo] password for 520428:
	{key: key-0}, {val: some str value-0}
	{key: key-1}, {val: some str value-1}
	{key: key-10}, {val: some str value-10}
	{key: key-11}, {val: some str value-11}
	{key: key-12}, {val: some str value-12}
	{key: key-13}, {val: some str value-13}
	{key: key-14}, {val: some str value-14}
	{key: key-15}, {val: some str value-15}
	{key: key-16}, {val: some str value-16}
	{key: key-17}, {val: some str value-17}
	{key: key-18}, {val: some str value-18}
	{key: key-19}, {val: some str value-19}
	{key: key-2}, {val: some str value-2}
	{key: key-3}, {val: some str value-3}
	{key: key-4}, {val: some str value-4}
	{key: key-5}, {val: some str value-5}
	{key: key-6}, {val: some str value-6}
	{key: key-7}, {val: some str value-7}
	{key: key-8}, {val: some str value-8}
	{key: key-9}, {val: some str value-9}
	Dumping ios2
	{key: key-0}, {val: some str value-0}
	{key: key-1}, {val: some str value-1}
	{key: key-10}, {val: some str value-10}
	{key: key-11}, {val: some str value-11}
	{key: key-12}, {val: some str value-12}
	{key: key-13}, {val: some str value-13}
	{key: key-14}, {val: some str value-14}
	{key: key-15}, {val: some str value-15}
	{key: key-16}, {val: some str value-16}
	{key: key-17}, {val: some str value-17}
	{key: key-18}, {val: some str value-18}
	{key: key-19}, {val: some str value-19}
	{key: key-2}, {val: some str value-2}
	{key: key-3}, {val: some str value-3}
	{key: key-4}, {val: some str value-4}
	{key: key-5}, {val: some str value-5}
	{key: key-6}, {val: some str value-6}
	{key: key-7}, {val: some str value-7}
	{key: key-8}, {val: some str value-8}
	{key: key-9}, {val: some str value-9}
	Dumping ios3
	{key: key-0}, {val: some str value-0}
	{key: key-1}, {val: some str value-1}
	{key: key-10}, {val: some str value-10}
	{key: key-11}, {val: some str value-11}
	{key: key-12}, {val: some str value-12}
	{key: key-13}, {val: some str value-13}
	{key: key-14}, {val: some str value-14}
	{key: key-15}, {val: some str value-15}
	{key: key-16}, {val: some str value-16}
	{key: key-17}, {val: some str value-17}
	{key: key-18}, {val: some str value-18}
	{key: key-19}, {val: some str value-19}
	{key: key-2}, {val: some str value-2}
	{key: key-3}, {val: some str value-3}
	{key: key-4}, {val: some str value-4}
	{key: key-5}, {val: some str value-5}
	{key: key-6}, {val: some str value-6}
	{key: key-7}, {val: some str value-7}
	{key: key-8}, {val: some str value-8}
	{key: key-9}, {val: some str value-9}
	bash-4.4$
	bash-4.4$
	
    ```
