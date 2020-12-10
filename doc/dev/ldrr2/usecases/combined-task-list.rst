=====
Tasks
=====

:id: [t.md-overhead]
:name: estimate meta-data overhead for r2
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: take into account: swap, 1+2 replication, 1+N replication of global
         metadata. 2+2 in degraded mode
:justification: affects usable storage capacity
:component: motr, input from s3
:req: Possible data protection schemes
:process: simple
:depends:
:resources:

-------
   
:id: [t.io-error-data]
:name: handle io errors on data device
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: m0d reports io error to ha (iem) and hare. Returns EIO to
         client. This is mostly already here.
:justification:
:component: motr.ios
:req: AD-10, AD-20
:process: check, fix
:depends:
:resources:

------



:id: [t.io-error-meta-data]
:name: handle io errors on meta-data device
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: If motr/be gets io error when doing io to a segment or a log, it should
         report it (iem, hare) and them shutdown gracefully.
:justification:
:component: motr.be
:req: AD-10, AD-20
:process: DLD, DLDINSP, CODE, INSP, ST
:depends:
:resources:

------



:id: [t.io-error-read]
:name: motr client gracefully recovers from errors of read
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: If motr client cannot get some units from one of the servers in time
         (error, or timeout), it reconstructs the missing units from
         redundancy. Late units might still arrive. Buffer ownership and
         lifetime should be defined.
:justification:
:component: motr.client
:req: AD-10, AD-20
:process: HLD, HLDINSP, DLD, DLDINSP, CODE, INSP, ST
:depends:
:resources:

------



:id: [t.io-error-write]
:name: s3 and motr client gracefully recovers from errors of write
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: if motr client cannot complete write in time (error or timeout) it
         reports this to s3. s3 allocates new fid and repeats object creation in
         an alternative pool (2+2). If 2+2 creation fails, return error to s3
         client. See [q.object-cleanup].
:justification:
:component: motr.client, motr.pool, s3
:req: AD-10, AD-20
:process: HLD, HLDINSP, DLD, DLDINSP, CODE, INSP, ST
:depends:
:resources:
:**question**: In a 4+2 pool, if (at most) two of the data/parity units write fail,
           can we claim the write as success?

------



:id: [t.md-error-read]
:name: motr client gracefully recovers from errors of meta-data lookup
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: If motr client cannot get some units from one of the servers in time
         (error, or timeout), it reconstructs the missing units from other
         replicas. Late units might still arrive. Buffer ownership and lifetime
         should be defined.
:justification:
:component: motr.client
:req:
:process: HLD, HLDINSP, DLD, DLDINSP, CODE, INSP, ST
:depends:
:resources:
:**question**: Is this a DIX operation?

------



:id: [t.md-error-write]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: if motr client cannot complete index put in time (error or timeout) it
         can return success to client when it has confirmation from 1 cas
         service. CLARIFY.
:justification:
:component: motr.client, motr.dtm
:req:
:process:
:depends:
:resources:

------



:id: [t.md-checksum]
:name: verify meta-data checksums on read
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: verify be record checksum on access
:justification:
:component: motr.be
:req:
:depends:
:resources:

------



:id: [t.b-tree-rewrite]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.balloc-rewrite]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.lnet-libfabric]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.galois-isa]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.multiple-pools]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: support multiple active pools, select alternative pool version with
         2+2. Some code is already there was used in A200. Maybe m0t1fs only?
:justification:
:component: motr.client, provisioner
:req:
:process:
:depends:
:resources:
:**question**: I think the Mero in SAGE cluster (some old version of Motr) already
               has multiple-pool support.
------



:id: [t.multiple-pools-policy]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: support policy to select among active pools. Pool is selected by the
         policy for each object creation. Similarly for meta-data and bucket
         creation. Default policy: round-robin
:justification:
:component: motr.client, provisioner
:req:
:process:
:depends:
:resources:
:**question**: If pool is not specified, Motr client should make the decision. If Motr client (here S3 server)
               has already specified the pool, Motr will use that pool.

------



:id: [t.pools-policy-health]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: policy to prefer healthy pools (based on availability updates from
         hare)
:justification: optional?
:component: motr.client, provisioner
:req:
:process:
:depends:
:resources:

------



:id: [t.pools-policy-free-space]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: policy to prefer pools with most free space
:justification: optional?
:component: motr.client, provisioner
:req:
:process:
:depends:
:resources:

------



:id: [t.s3.use-dtm]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr.dtm, s3
:req:
:process:
:depends:
:resources:

------



:id: [t.s3-store-object-meta-data]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: store pool version (already there) and layout id (?) in s3 meta-data
         json. S3 should set pver and layout id when creating m0_obj structure.
:justification:
:component: s3, motr.client
:req:
:process:
:depends:
:resources:

------



:id: [t.avoid-md-cobs]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: avoid md-cob lookups when pver and layout id are set in the structure.
:justification:
:component: motr.client
:req:
:process:
:depends:
:resources:

------



:id: [t.s3-cache]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: cache bucket and account global meta-data in memory, for o longer than
         X seconds. Create bucket (and auth update) should be delayed by N
         seconds.
:justification:
:component: s3
:req:
:process:
:depends:
:resources:

------



:id: [t.beck]
:name: update beck tool to work with new meta-data layout
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: motr changes (no md cobs, new b-tree), s3 changes.
:justification:
:component: motr.beck
:req:
:process:
:depends:
:resources:

------



:id: [t.s3-no-replication]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: s3-level explicit replication of s3 top meta-data is no longer needed.
:justification:
:component: s3
:req:
:process:
:depends:
:resources:

------



:id: [t.dix-local-lookup]
:name: if possible to distributed index lookup locally
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: when doing lookup in a replicated index, dix client should, if possible
         select the network-closest node.
:justification:
:component: motr.client
:req:
:process:
:depends:
:resources:

------



:id: [t.cobs-loc_info]
:name: store pool version and layout identifiers in cobs
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: md cobs are removed, so data cobs should store pver and layout
         identifiers. This is needed for future SNS repair. And also for beck
         tool. Maybe this is done already?
:justification:
:component: motr.ios
:req:
:process:
:depends:
:resources:

------



:id: [t.s3-pending-list]
:name: clarify placement and use of pending list with s3 team
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: Is pending list global or local meta-data? Transactionality of updates.
:justification:
:component: s3
:req:
:process:
:depends:
:resources:

------



:id: [t.hare-notifications]
:name: hare delivers notification about process, node, device state changes
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: reliable notifications. Data and meta-data devices.
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.hare-partitions]
:name: handle network partitions in hare
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: CLARIFY
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.hare-split-brain]
:name: handle split brain situations in hare
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: CLARIFY
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.N+K+S]
:name: handle K != S in motr (S can be ZERO)
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: identify and fix code that assumes K == S
:justification:
:component: motr.client, motr.ios, motr.sns, dix, cas
:req:
:process:
:depends:
:resources:

------



:id: [t.resends]
:name: check that resend number is set for infinity everywhere
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.dix-global-replication-check]
:name: check that fix supports 1+N replication
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: check that dix can replicate global indices with 1+N, where N is the
         number of nodes
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.dix-global-replication-check]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: setup global replication of top-level s3 indices. Setup global
         meta-data pool. S3 should create global indices in this pool.
:justification:
:component: motr.dix, provisioner, s3
:req:
:process:
:depends:
:resources:

------



:id: [t.dtm-throttling]
:name: throttle incoming requests during dtm catchup
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: to guarantee overall dtm recovery progress, incoming requests should be
         throttled while recovery is going on. Maybe they will be throttled by
         recovery itself?
:justification:
:component: motr.dtm
:req:
:process:
:depends:
:resources:

------



:id: [t.hare-dtm-recovery]
:name: hare should participate in dtm recovery
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr.dtm, hare
:req:
:process:
:depends:
:resources:

------



:id: [t.perf-tx-group]
:name: Re-implement transaction groups
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr.be
:req:
:process:
:depends:
:resources:

------



:id: [t.perf-ldap-auth-caching]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.perf-tls-overhead-measure]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.hare-restart-notification]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: hare should arrange for a notification from systemd when a process
         dies.
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.dtm-recovery-1]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: when a motr instance learns that other instance is in recovery, the
         former sends to the latter at least 1 recovery message. This is needed
         to detect recovery completion.
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------



:id: [t.]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------




=========
Questions
=========

:id: [q.object-cleanup]
:name: when object is discarded and re-created in 2+2, should the old one be
       cleaned up?
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:to: Dan
:component:
:req:
:depends: t.io-error-write
:resources:

------



:id: [q.concurrent-PUT]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:to: Dan
:component:
:req:
:depends:
:resources:

------



:id: [q.concurrent-bucket-operation]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:to: Dan
:component:
:req:
:depends:
:resources:

------



:id: [q.service dependencies]
:name: who is tracking service dependencies?
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: Who re-starts s3 when motr is restarted? pacemaker?
:to: Dan
:component:
:req:
:depends:
:resources:

------



===========
Assumptions
===========

:id: [a.no-repair]
:name: no {SNS, DIX} repair is needed for P0
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail:
:justification: Gregory, Dan
:component:
:req:
:depends:
:resources:

------




-------------
From Shashank
-------------

------

:id: [t.io-perf-rw]
:name: io performance
:author: 
:detail: support PRD performance numbers for 16MB and 256KB object sizes
:justification:
:component: motr, s3
:req: SCALE-40, SCALE-50
:process: check, DLD, DLDINSP, code, INSP, fix
:depends: SW-50, SW-60
:resources:

------

:id: [t.io-perf-ttfb]
:name: io performance
:author: 
:detail: support Time-To-First-Byte of <150ms for any Object size
:justification:
:component: motr, s3
:req: SCALE-80
:process: check, DLD, DLDINSP, code, INSP, fix
:depends: SW-50, SW-60
:resources:

------


------------
From Shankar
------------

HW - 10  Support Largest Drive

  - 1. Any special handling for HAMR or SMR drive needs to ne enabled in PODS or 5u84
  - 2. Handling Asssymetric Strorage Set Part of Cluster
    - Assuming enclosure in SS are symetric w.r.t capacity
  - 3. IO distribution based on sizes available/remaining


HW-20 LR cluster must support Supermicro 1U COTS servers
  - 1. Test number of active session supported with new hardware
  - 2. Test perfromance numbers with different server and create benchmark table for customer reference


HW30: LR cluster may include optional certified networking equipment for private interconnect
  - 1. Benchmark performance with different newtork equipment
  - 2. Test number of active session supported with new hardware


SW-10 Lyve Rack must be running on the supported CentOS version
  - 1. Build motr with selected CentOS (8.x) and make sure all unit tests and system tests are passing
  - 2. Change motr code as per changes to dependency package


SW-20 All 3rd party applications must be running recent, maintained versions
  - 1. Check latest verison of libfabric and Intel ISA is used.
    (Before final release to QA for testing, validate everything (motr) is working with latest version of software)


SW-30 Any failure of any 3rd party SW component must be detected by CORTX
  - 1. libfabric: Add code to generate IEM for any unxpected error thrown by libfabric
    - Dependency : Notify SSPL and CSM for new IEM addition


SW-40 CORTX should have no kernel dependencies
  - 2. Remove the need for m0d to get UUID (UUID is received from Kernel)


NET-10 It should be possible to connect LR data network to 10G, 25G and 100G networks
  - 1. Benchamark performance with netwrok of different speed
  - 2. Evaluate Perfromance impact with multiple SS with low speed include


NET-20 LR must allow static IPs configuration for all interfaces
  - 1. Change in config file for motr for libfabric initialialization
    - Assuming upagrade will be disruptive


SCALE-10 LR cluster should support between 3 and 36 nodes. For P0 test up to 12 nodes config – but it should be possible to deploy a larger cluster. Scale requirements specified below must be tested at least up to 12 nodes
  - 4. Create process to make sure one global metadata update is happening at a time in cluster (for exterme corner scenario)
  - 6. Checksum for key and value. Key and Value to be stored together?
  - 7. Switch to 2+2 parity scheme for data in case of node failire (confirm with PLM)?
    - Will avoid need for Data DTM
  - 9. Any error in data write to be ignored with the number of failure is within limits
  - 13. Display metadata used and go to write protect if MD is all used


SCALE-50 : Read: 1GB/sec, Write: 850MB/sec per node for 256KB objects
  - 1. Small object performance: Create Hash list for emap btree access will help to speedup emap access
  - 2. Small object performance: Evaluate and add Hash list for CAS btree access


-----------
From Madhav
-----------


HW-10
SCALE-10
  1) Deployement of 3 nodes with 5u84 with ADAPT
  2) Deployemnt of 6 nodes i.e two storage sets
  3) Deployment of 12 nodes i.e four storage sets
  4) Design for 36 nodes i.e 9 storages sets
     How many nodes per rack ?
  5) 3,6 and 12 node deployment in VM
  6) configure pool per storage sets
  7) fsstat per pool as well as aggerated
  8) design pool selection policy and use it during object creation

HW-20  super micro COTS
HW-30  multiple vendors

  9) Deploy with different available vendors of RoCE nic and swicth and do the
     performance analysis.
  10) Check 50Gbps is sufficient for S3 data or more is needed (Test with 12 node deployment)
  11) Check 50Gbps is sufficient for motr-motr data or more is needed ((Test with 12 node deployment)

SW-10
  12) centos 8 support gccxml to other alternatives may be cast xml
  13) check with lustre on centos 8
  14.0) build on centos 8 and deploy on 3 node VM's without Hare
  14.1) build on centos 8 and deploy on 3 node VM's once Hare is ready
  15) deploy on centos 8 h/w setup once motr + hare + s3 are ready


SW-40
  17) m0nettest with libfabric
  18) performance analysis with libfabic with LDR R1 setup and
      compare with Lnet results
  19) Do all the long run and QA manual and automation tests once libfabric
      is ready with LR R1 setup
  20) performance analysis with libfabic with LDR R2 setup configs
       3-node, 6-node, 12-node


SW-60
  22) Performance analysis with galosi and intel ISA
  24) Btree concurrency/performance analysis
  26) Balloc read/write/delete performance analysis



NET-10
  27) Configure bonding to add the support for 10G and 25G networks
  28) Performance analysis the stack with 10G,25G and 100G networks
  29) Evaluate with both Lnet and Libfabrics


NET-12
  30) Only RoCE supported switch vendors can be used for data

SCALE-70
  45) populate 100M objects per node with 3 node setup and do the performance analysis
	    Check with 256K, 1M, 16M and 128 M objects
  46)  Also do the performance analysis at different stages of storage 50%, 70%,80% and 90%


SCALE-80
  47) Check Time to first byte 150ms 99% of the time for different object sizes
  48) Check TTFB at different stages of storage 50%, 70%,80% and 90%

AD-10,20,30
  49) Remove a node from the 3-node or 6-node setup/cluster and update it to new rpm
	    version and the add it back to the cluster
  50) Test update of rpm's of a node in VM's with 3node deployment

  51) Disk group failure domain needs to be supported


MGM-60
  58) Return aggr performance, may be s3 only task

MGM-120
  59) Need to shutdown and restart a node
MGM-130
  60) Need to stop the cluster and start again
      All the ongoing IO should be completed and new IO will get 500 error.

MGM-220
  61) Check with Switch or FW update(should be non-disruptive) and see that cluster
	    is still online

SEC-130
  62) Security vulnerabilty handling for motr

OP-20
  63) check the cluster and IO after server is replaced for a node

OP-70
  64) Support motr setup for automatic deployment with provisioner

VM-10
  65) VM support

SUP-20
  66) support bundle analysis


-------------
From Huanghua
-------------


:id: [t.cluster-aging-testing]
:name: Cluster Aging testing.
:author: hua.huang@seagate.com
:detail: To fill nearly full, to test performance and corner cases, alerts.
:justification:
:component: motr
:req:
:process:
:depends:
:resources:

------


-------------
From Anatoliy
-------------


:id: [t.dtm-all2all]
:name:
:author: anatoliy
:detail: During start of the cluster establish rpc connections between each m0d service and others m0ds
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [t.dtm-dtx-fop]
:name:
:author: anatoliy
:detail: Register DTM0 FOP types which are quite enough to send dtxes and service specific payloads (CAS_PUT CAS_DEL here)
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [t.dtm-cb-fop]
:name:
:author: anatoliy
:detail: Register DTM0 FOP types to deliver executed, persistent and redo callbacks to different parties
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [t.dtm-dtm0-srv]
:name:
:author: anatoliy
:detail: Create a clovis utility which is able to send dtx-related FOPs to DTM0 service
:justification:
:component: Motr
:req:
:process:
:depends: dtx-fop cb-fop
:resources:

------

:id: [t.dtm-dtxsm-cli]
:name:
:author: anatoliy
:detail: Define DTX state machine for the client side
:justification:
:component: Motr
:req:
:process:
:depends: deploy-vm
:resources:

------

:id: [t.dtm-fop-tool]
:name:
:author: anatoliy
:detail: Implement dummy dtm0 service which is able to accept DTM0 FOPs and log them.
:justification:
:component: Motr
:req:
:process:
:depends: dtm0-srv
:resources:

------

:id: [t.dtm-epoch]
:name:
:author: anatoliy
:detail: Implement versioning timestamping in a single originator configuration (PoC0).
:justification:
:component: Motr
:req:
:process:
:depends: deploy-vm
:resources:

------

:id: [t.dtm-11]
:name:
:author: anatoliy
:detail: Propagate DTX SM transitions to clovis OP trasitions
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli fop-tool
:resources:

------

:id: [t.dtm-12]
:name:
:author: anatoliy
:detail: Update clovis launch logic w.r.t. ~dtx==NULL~ and ~dtx!=NULL~
:justification:
:component: Motr
:req:
:process:
:depends: 11
:resources:

------

:id: [t.dtm-13]
:name:
:author: anatoliy
:detail: Provide dtx state logic near by ~clovis_op_launch()~ -> ~op->launch()~
:justification:
:component: Motr
:req:
:process:
:depends: 12
:resources:

------

:id: [t.dtm-dtxsm-cli-wait]
:name:
:author: anatoliy
:detail: Provide dtx state wait logic
:justification:
:component: Motr
:req:
:process:
:depends: 13 observ
:resources:

------

:id: [t.dtm-15]
:name:
:author: anatoliy
:detail:  Provide c0mt-alike test to emulate load patterns with a high level of parallelism for DIX PUT and DEL operations.
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli-wait
:resources:

------

:id: [t.dtm-16]
:name:
:author: anatoliy
:detail: Provide a way to emulate transient failures all over the stack deterministically and with the help of FI, crash to emulate such failure.
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli-wait
:resources:

------

:id: [t.dtm-17]
:name:
:author: anatoliy
:detail: Emulate transient failure of m0d during PUT after DEL workload.
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli-wait
:resources:

------

:id: [t.dtm-18]
:name:
:author: anatoliy
:detail: Emulate transient failure of m0d during DEL after PUT workload.
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli-wait
:resources:

------

:id: [t.dtm-plog]
:name:
:author: anatoliy
:detail: Implement DTM0 local persistent log structure on top of BE.
:justification:
:component: Motr
:req:
:process:
:depends: 10
:resources:

------

:id: [t.dtm-nplog]
:name:
:author: anatoliy
:detail: Implement DTM0 local non-persistent log structure for originators.
:justification:
:component: Motr
:req:
:process:
:depends: 10
:resources:

------

:id: [t.dtm-log-txr]
:name:
:author: anatoliy
:detail: Implement DTM0 local txr (log element) structure on top of BE.
:justification:
:component: Motr
:req:
:process:
:depends: deploy-vm
:resources:

------

:id: [t.dtm-22]
:name:
:author: anatoliy
:detail: Implement txr execution logic during specific service request execution.
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli-wait
:resources:

------

:id: [t.dtm-23]
:name:
:author: anatoliy
:detail: Implement a special strucutre to store versions for keys stored in CAS.
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli-wait
:resources:

------

:id: [t.dtm-24]
:name:
:author: anatoliy
:detail: Implement a logic which covers a proper key and value selection accordingly to versions for DELs after PUTs
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli-wait
:resources:

------

:id: [t.dtm-25 ]
:name:
:author: anatoliy
:detail: Implement a logic which covers a proper key and value selection accordingly to versions for PUTs after DELs
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli-wait
:resources:

------

:id: [t.dtm-26]
:name:
:author: anatoliy
:detail: Tombstones management, keys will not be overwritten by the objects with older versions.
:justification:
:component: Motr
:req:
:process:
:depends: dtxsm-cli-wait
:resources:

------

:id: [t.dtm-27]
:name:
:author: anatoliy
:detail: Redo       callback logic
:justification:
:component: Motr
:req:
:process:
:depends: 26
:resources:

------

:id: [t.dtm-28]
:name:
:author: anatoliy
:detail: Persistent callback logic
:justification:
:component: Motr
:req:
:process:
:depends: 26
:resources:

------

:id: [t.dtm-29]
:name:
:author: anatoliy
:detail: Executed   callback logic
:justification:
:component: Motr
:req:
:process:
:depends: 26
:resources:

------

:id: [t.dtm-30]
:name:
:author: anatoliy
:detail: Recovery logic iterating over DTM0 logs and sending corresponding redo messages to participants; triggered by HA.
:justification:
:component: Motr
:req:
:process:
:depends: 26
:resources:

------

:id: [t.dtm-31]
:name:
:author: anatoliy
:detail: Integrate txr execution logic into CAS serice including proper tx credit calculation, should be executed as a part of local transaction.
:justification:
:component: Motr
:req:
:process:
:depends: 26
:resources:

------

:id: [t.dtm-32]
:name:
:author: anatoliy
:detail: A Tool for an initial DTM0 log analysis
:justification:
:component: Motr
:req:
:process:
:depends: 31
:resources:

------

:id: [t.dtm-33]
:name:
:author: anatoliy
:detail: A Replay tool which will be able to save current dtm0 log and replay it again, useful for debugging
:justification:
:component: Motr
:req:
:process:
:depends: 31
:resources:

------

:id: [t.dtm-proto-vis]
:name:
:author: anatoliy
:detail: Tool for the DTM0 protocol visualisation
:justification:
:component: Motr
:req:
:process:
:depends: deploy-vm
:resources:

------

:id: [t.dtm-magic-bulk]
:name:
:author: anatoliy
:detail: Make RPC bulk to follow magic link semantics
:justification:
:component: Motr
:req:
:process:
:depends: 1
:resources:

------

:id: [t.dtm-observ]
:name:
:author: anatoliy
:detail: Provide observability and debuggability for the development cycle (not a fine-grained task)
:justification:
:component: Motr
:req:
:process:
:depends: deploy-vm
:resources:

------

:id: [t.dtm-ha-int ]
:name:
:author: anatoliy
:detail: Provide HA integration with Motr instances including design of the interraction protocol (not a fine-grained task)
:justification:
:component: Motr
:req:
:process:
:depends: observ
:resources:

------

:id: [t.dtm-s3-int]
:name:
:author: anatoliy
:detail: Provide S3 level integarion on new clovis interface with embedded dtx transactions (not a fine-grained task)
:justification:
:component: Motr
:req:
:process:
:depends: observ
:resources:

------

:id: [t.dtm-over-test]
:name:
:author: anatoliy
:detail: Provide a test infra to cover major failure cases in 1-node and n-node environments (not a fine-grained task)
:justification:
:component: Motr
:req:
:process:
:depends: ha-int s3-int
:resources:

------


==========
Assumption
==========

:id: [a.dtm-new-people]
:name:
:author: anatoliy
:detail: involvement of new people will reduce my bw down to 60%
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [a.dtm-anil-bw]
:name:
:author: anatoliy
:detail: Inital bw of Anil will be accounted as 30%
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [a.dtm-Mehul-bw]
:name:
:author: anatoliy
:detail: Inital bw of Mehul will be accounted as 60%
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [a.dtm-total-bw]
:name:
:author: anatoliy
:detail: total time measureed in person weeks in the next 6 months will be accounted as TT = sum(Est) / days per week / peoples involvement
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

