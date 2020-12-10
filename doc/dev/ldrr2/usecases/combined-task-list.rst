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

:id: [t.deploy-manual-3node]
:name: Deployement of 3 nodes with 5u84 with ADAPT
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: take into account: 4 + 2 + 0 for data and 1 + 2  for meta-data 
:justification:
:component: motr, s3
:req: HW10, SCALE10
:process: simple
:depends: availabilty of h/w
:resources:

-------

:id: [t.deploy-manual-6node]
:name: Deployement of 6 nodes with 5u84 with ADAPT
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: take into account: create a pool per storage set of 3 nodes
         and use pool selection policy for each new object
:justification:
:component: motr, s3
:req: HW10, SCALE10
:process: simple
:depends: 6-node h/w and t.pool-selection-policy
:resources:

-------

:id: [t.pool-selection-policy]
:name: Add a policy to select a pool per storage set for new objects
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: take into account: Policy can either be round-robin or based on
         available storage space in the storage set or skip a storage set
         if there is any failure or is under update
:justification:
:component: motr, s3, hare
:req: HW10, SCALE10
:process: simple
:depends: hare needs to support multiple pools
:resources:

-------

:id: [t.manual-deploy-vm-3-6-12-nodes]
:name: Manually deploy motr + s3 + hare in VM's with multiple pool per
       storage set.
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Do S3 io from client for 3 node setup and verify that pool from
         all the storage sets are used.
:justification:
:component: motr, s3, hare
:req: HW10, SCALE10
:process: simple
:depends: t.pool-selection-policy, S3 needs to scale above 3 nodes, until
          then it is run on first 3 nodes only.
:resources:

-------

:id: [t.fsstat]
:name: fsstat per pool as well as aggerated
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Provide support for per pool statistics and aggregated stats
:justification:
:component: motr, hare
:req: HW10, SCALE10
:process: simple
:depends: t.pool-selection-policy
:resources:

-------

:id: [t.multiple-nw-vendors-support]
:name: Deploy with different available vendors of RoCE nic and switch
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Deploy and do the performance analysis with different vendors or
         provide a framework to do such thing.
:justification:
:component: motr, perf
:req: HW30
:process: simple
:depends: avaialabilty of network hw from different vendors
:resources:

-------

:id: [t.validate-50gbs-NW-S3]
:name: Check 50Gbps is sufficient for S3 data or more is needed
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Check 50Gbps is sufficient for S3 data or more is needed
         Test with 6/9/12 node deployment as well.
:justification:
:component: motr, perf, s3
:req: HW30
:process: simple
:depends: hw
:resources:

-------

:id: [t.validate-50gbs-NW-motr]
:name: Check 50Gbps is sufficient for motr data or more is needed
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Check 50Gbps is sufficient for S3 data or more is needed
         Test with 6/9/12 node deployment as well.
:justification:
:component: motr, perf
:req: HW30
:process: simple
:depends: hw
:resources:

-------

:id: [t.libfabrics-m0nettest]
:name: Test the performance of libfabrics with m0nettest
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Test and compare m0nettest with libfabric and lnet transport between
         two nodes and if possible between three nodes as well.
:justification:
:component: motr, perf
:req: SW40
:process: simple
:depends: t.libfabrics-xprt
:resources:

-------

:id: [t.libfabrics-perf]
:name: Test the performance of libfabrics with 3-node setup
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: performance analysis with libfabic with LDR R1 setup
         and compare with Lnet results
         and then do the performance analysis with libfabic with
         LDR R2 setup and compare with Lnet results
:justification:
:component: motr, perf
:req: SW40
:process: simple
:depends: t.libfabrics-xprt
:resources:

-------

:id: [t.libfabrics-stability]
:name: Test the stability of libfabrics with 3-node setup
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Do all the long run and QA manual and automation tests with
         libfabric i.e QA automation and stability test suite must complete
         with it.
:justification:
:component: motr, perf
:req: SW40
:process: simple
:depends: t.libfabrics-xprt
:resources:

-------

:id: [t.galois-perf]
:name: galois to intel ISA perf analysis 
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Performance analysis with galois and intel ISA
         for 4+2 and 8+2 configs
:justification:
:component: motr, perf
:req: SW60
:process: simple
:depends: t.galois-intel-isa
:resources:

-------

:id: [t.balloc-perf]
:name: Balloc read/write/delete performance analysis
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Performance analysis of balloc on 3-node/6-node
         setups
:justification:
:component: motr, perf
:req: SW60
:process: simple
:depends: t.balloc-new
:resources:

-------

:id: [t.btree-perf]
:name: Btree concurrency/performance analysis
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Performance analysis of btree on 3-node/6-node
         setups
:justification:
:component: motr, perf
:req: SW60
:process: simple
:depends: t.btree-new
:resources:

-------

:id: [t.btree-stabilty]
:name: Btree concurrency/stability analysis
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Check the stability with new btree on 3-node/6-node
         setups
:justification:
:component: motr
:req: SW60
:process: simple
:depends: t.btree-new
:resources:

-------

:id: [t.net-perf]
:name: Performance analysis the stack with 10G,25G and 100G networks
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: use bonding for 10G and 25G n/w and do the performance anlysis with
         libfabrics and lnet
:justification:
:component: motr, perf
:req: NET10
:process: simple
:depends: t.libfabrics-xprt
:resources:

-------

:id: [t.net-sw-perf]
:name: Only RoCE supported switch vendors can be used for data
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Do the performance analysis with RoCE supported switches
:justification:
:component: motr, perf
:req: NET12
:process: simple
:depends: different nw switch vendors
:resources:

-------

:id: [t.perf-obj-100M]
:name: populate 100M objects per node
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: populate 100M objects per node with 3 node setup and do
         the performance analysis. Check with 256K, 1M, 16M and
         128M objects.
         Also do the performance analysis at different stages of
         storage 50%, 70%,80% and 90%
:justification:
:component: motr, perf
:req: SCALE70
:process: simple
:depends: t.3-node-deploy
:resources:

-------

:id: [t.perf-ttfb]
:name: check ttfb performance
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Check Time to first byte 150ms 99% of the time for different object
         size and also check TTFB at different stages of storage 50%, 70%, 80%
         and 90%.
:justification:
:component: motr, perf
:req: SCALE70
:process: simple
:depends: t.3-node-deploy
:resources:

-------

:id: [t.dg-failure-domain]
:name: Add support fro diskgroup failure domain
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Map controller to diskgroup failure domain
:justification:
:component: motr
:req: AD 10,20,30
:process: simple
:depends: 3 node hw
:resources:

-------

:id: [t.update-rpm-single-node]
:name: rpm update
:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
:detail: Remove a node from the 3-node or 6-node setup/cluster and update it
         to new rpm version and the add it back to the cluster.
         Test update of rpm's of a node in VM's with 3node deployment
:justification:
:component: motr, hare
:req: AD 10,20,30
:process: simple
:depends: t.hare-add-remove-node
:resources:

-------

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




------

:id: [t.io-perf-rw]
:name: io performance
:author: shashank 
:detail: support PRD performance numbers for 16MB and 256KB object sizes
:justification:
:component: motr, s3
:req: SCALE-40, SCALE-50
:process: check, DLD, DLDINSP, code, INSP, fix
:depends:
:resources:

------

:id: [t.io-perf-ttfb]
:name: io performance
:author: shashank
:detail: support Time-To-First-Byte of <150ms for any Object size
:justification:
:component: motr, s3
:req: SCALE-80
:process: check, DLD, DLDINSP, code, INSP, fix
:depends:
:resources:

------


------------
From Shankar
------------

:id: [t.support-different-drive]
:name: Benchmark and tune performance with different drive types
:detail: Different drive type can give different performance. Running standard benchmark workload profile and checking if there is any deviation from reference drives and capacity sizes. Check for any special handling for HAMR or SMR drive needs to ne enabled in PODS or 5u84  
:justification: Analyzing impact on performance will help in drive selection.
:component: motr, Performance evaluation team
:req: HW-10
:process: Test suite for performance evaluation
:depends:

------

:id: [t.hw-10.2]
:name: Handling Asssymetric Strorage Set in a Cluster
:author:
:detail:  Enclosure in a Storage Set will be symetric w.r.t capacity (validate with PLM), but across storage set they can be assymetric. Check impact of this and add support for its handling in motr.
:justification:
:component: motr
:req: HW-10
:process:
:depends:
:resources:

------



:id: [t.hw-10.3]
:name:
:author:
:detail: IO distribution based on sizes available/remaining
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [t.hw-20.1]
:name:
:author:
:detail: IO distribution based on sizes available/remaining
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------



:id: [t.hw-20.2]
:name:
:author:
:detail: Test perfromance numbers with different server and create benchmark table for customer reference
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [t.hw-30.1]
:name:
:author:
:detail: Benchmark performance with different newtork equipment 
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------


:id: [t.hw-30.2]
:name:
:author:
:detail:  Test number of active session supported with new hardware 
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------


:id: [t.sw-10.1]
:name:
:author:
:detail: Build motr with selected CentOS (8.x) and make sure all unit tests and system tests are passing.
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------


:id: [t.sw-10.2]
:name:
:author:
:detail: Change motr code as per changes to dependency package 
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------


:id: [t.sw-20.1]
:name:
:author:
:detail: Check latest verison of libfabric and Intel ISA is used. (Before final release to QA for testing, validate everything (motr) is working with latest version of software)
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------


:id: [t.sw-30.1]
:name:
:author:
:detail: libfabric: Add code to generate IEM for any unxpected error thrown by libfabric. Dependency : Notify SSPL and CSM for new IEM addition
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------


:id: [t.sw-40.2]
:name:
:author:
:detail: Remove the need for m0d to get UUID (UUID is received from Kernel) 
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------


:id: [t.net-10.1]
:name:
:author:
:detail:  Benchamark performance with netwrok of different speed
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------


:id: [t.net-10.1]
:name:
:author:
:detail: Evaluate Perfromance impact with multiple SS with low speed include 
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------


:id: [t.net-20.1]
:name:
:author:
:detail: Change in config file for motr for libfabric initialialization. Assuming upagrade will be disruptive
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------



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


------

:id: [t.global-md-serialize]
:name: serialize global meta-data create in the cluster
:author: shankar 
:detail: Create process to make sure one global metadata update is happening at a time in cluster
:justification:
:component: motr, s3
:req: SCALE10
:process: check, fix
:depends:
:resources:

------

:id: [t.startup-shutdown]
:name: Power UP/ Power DOWN the cluster gracefully.
:author: madhav 

:id: [t.startup-shutdown]
:name: Power UP/ Power DOWN the cluster gracefully.
:author: madhav 
:detail: make sure all the IOs complete before shutdown and data is available
         on next POWER UP.
:justification:
:component: all
:req: MGM-120, MGM-130
:process: check, fix
:depends:
:resources:

------

:id: [t.security-motr]
:name: Check Security vulnerability of Motr process and Motr data.
:author:  madhav
:detail: 
:justification:
:component: all
:req: SEC-130
:process: check, fix
:depends:
:resources:

------


:id: [t.hardware-maintenance]
:name: Replace any FRU within cluster.
:author: 
:detail: Motr process on the Node containing the FRU should shutdown gracefully
	before the replacement and after the replacement the Motr process should
	be able to start and continue IOs.
:justification:
:component: all
:req: OP-20
:process: check, fix
:depends:
:resources:

------

:id: [t.support-bundle]
:name: Debug logs in support bundle.
:author: 
:detail: Descriptive logs in Motr (especially ERRORS and WARNINGS) should help
         isolate the issue quickly.
:justification:
:component: all
:req: SUP-20
:process: check, fix
:depends:
:resources:

------


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



=====================================
m0tr tasks for scalability (Anatoliy)
=====================================

:id: [t.scale-m0tr-m0be]
:name:
:author: anatoliy
:detail: BE META TASK
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [t.scale-m0tr-txgr]
:name:
:author: anatoliy
:detail: BE GROUP META TASK
:justification:
:component: Motr
:req:
:process:
:depends: m0be
:resources:

------

:id: [t.scale-m0tr-txgr-mockG]
:name:
:author: anatoliy
:detail: Mock BE tx group with in-memory tx execution
:justification:
:component: Motr
:req:
:process:
:depends: txgr
:resources:

------

:id: [t.scale-m0tr-txgr-mockA]
:name:
:author: anatoliy
:detail: Mock BE allocator with sequential in-memory allocator
:justification:
:component: Motr
:req:
:process:
:depends: txgr
:resources:

------

:id: [t.scale-m0tr-txgr-5u84]
:name:
:author: anatoliy
:detail: Tune 5u84 w.r.t. the new configuration
:justification:
:component: Motr
:req:
:process:
:depends: txgr-mockA txgr-mockG
:resources:

------

:id: [t.scale-m0tr-txgr-A]
:name:
:author: anatoliy
:detail: Detailed design new block allocator w.r.t. to MRD performance requirements
:justification:
:component: Motr
:req:
:process:
:depends: txgr-5u84 txgr-G-optimistic
:resources:

------

:id: [t.scale-m0tr-txgr-G]
:name:
:author: anatoliy
:detail: Detailed design for new tx group logic
:justification:
:component: Motr
:req:
:process:
:depends: txgr-5u84
:resources:

------

:id: [t.scale-m0tr-txgr-G-fom]
:name:
:author: anatoliy
:detail: Update tx group FOM logic
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G
:resources:

------


:id: [t.scale-m0tr-txgr-G-log]
:name:
:author: anatoliy
:detail: Update BE log w.r.t. new group logic
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G-fom txgr-G-tx
:resources:

------

:id: [t.scale-m0tr-txgr-G-serialize]
:name:
:author: anatoliy
:detail: Provide new tx group serialisation algo
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G
:resources:

------

:id: [t.scale-m0tr-txgr-G-throttle]
:name:
:author: anatoliy
:detail: Provide new tx group serialisation algo throttling when there’re cyclic deps
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G-serialize
:resources:

------

:id: [t.scale-m0tr-txgr-G-optimistic]
:name:
:author: anatoliy
:detail: Update BE structures w.r.t. minimise cyclic dependencies on the data
:justification:
:component: Motr
:req:
:process:
:depends:
:resources: txgr-G

------


:id: [t.scale-m0tr-txgr-G-tx]
:name:
:author: anatoliy
:detail: Update TX SM w.r.t. new tx group logic
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G-fom
:resources:

------

:id: [t.scale-m0tr-txgr-G-tx-regarea]
:name:
:author: anatoliy
:detail: Update reg area w.r.t. new tx group logic
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G-tx
:resources:

------

:id: [t.scale-m0tr-txgr-G-recovery]
:name:
:author: anatoliy
:detail: Update recovery w.r.t. new log format
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G-serialize txgr-G-log
:resources:

------

:id: [t.scale-m0tr-txgr-G-5u84]
:name:
:author: anatoliy
:detail: Tune new algo w.r.t. 5u84 for different workloads and bss
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G txgr-A
:resources:

------


:id: [t.scale-m0tr-txgr-G-STAB]
:name:
:author: anatoliy
:detail: Stabilise new algo
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G-5u84 txgr-G-recovery
:resources:

------

:id: [t.scale-m0tr-parity]
:name:
:author: anatoliy
:detail: PARITY MATH META TASK
:justification:
:component: Motr
:req:
:process:
:depends:
:resources:

------

:id: [t.scale-m0tr-parity-degraded]
:name:
:author: anatoliy
:detail: Performance optimisation in degraded modes
:justification:
:component: Motr
:req:
:process:
:depends: parity
:resources:

------

:id: [t.scale-m0tr-parity-incremental]
:name:
:author: anatoliy
:detail: Incremental parity sums calc
:justification:
:component: Motr
:req:
:process:
:depends: parity
:resources:

------


:id: [t.scale-m0tr-parity-isa-int]
:name:
:author: anatoliy
:detail: ISA integration
:justification:
:component: Motr
:req:
:process:
:depends: parity
:resources:

------

:id: [t.scale-m0tr-parity-isa-tune]
:name:
:author: anatoliy
:detail: ISA tuning
:justification:
:component: Motr
:req:
:process:
:depends: parity
:resources:

------

:id: [t.scale-m0tr-parity-isa-n32log]
:name:
:author: anatoliy
:detail: n^3 -> n^2*log(n) linear system solver
:justification:
:component: Motr
:req:
:process:
:depends: parity
:resources:

------

:id: [t.scale-m0tr-parity-isa-reg]
:name:
:author: anatoliy
:detail: integrate region operations
:justification:
:component: Motr
:req:
:process:
:depends: parity
:resources:

------

:id: [t.scale-m0tr-parity-isa-vander]
:name:
:author: anatoliy
:detail: revise vandermonde matrix part of the algo w.r.t. ISA
:justification:
:component: Motr
:req:
:process:
:depends: parity
:resources:

------


:id: [t.scale-m0tr-parity-isa-NKS]
:name:
:author: anatoliy
:detail: optimisation for different layouts N+K+S
:justification:
:component: Motr
:req:
:process:
:depends: parity
:resources:

------


:id: [t.scale-m0tr-btree]
:name:
:author: anatoliy
:detail: NEW BTREE IMPLEMENTATION META TASK
:justification:
:component: Motr
:req:
:process:
:depends: m0be txgr-G-optimistic
:resources:

------


:id: [t.scale-m0tr-swap]
:name:
:author: anatoliy
:detail: SWAP META TASK
:justification:
:component: Motr
:req:
:process:
:depends: m0be
:resources:

------


:id: [t.scale-m0tr-stob]
:name:
:author: anatoliy
:detail: STOB META TASK
:justification:
:component: Motr
:req:
:process:
:depends: txgr-G-STAB
:resources:

------


:id: [t.scale-m0tr-stob-concurrency]
:name:
:author: anatoliy
:detail: limit concurrency w.r.t. different workloads
:justification:
:component: Motr
:req:
:process:
:depends: stob
:resources:

------


:id: [t.scale-m0tr-stob-work-small]
:name:
:author: anatoliy
:detail: small blocks
:justification:
:component: Motr
:req:
:process:
:depends: stob-concurrency
:resources:

------


:id: [t.scale-m0tr-stob-work-large]
:name:
:author: anatoliy
:detail: large blocks
:justification:
:component: Motr
:req:
:process:
:depends: stob-concurrency
:resources:

------


:id: [t.scale-m0tr-stob-metadata]
:name:
:author: anatoliy
:detail: metadata stobs
:justification:
:component: Motr
:req:
:process:
:depends: stob-concurrency
:resources:

------


:id: [t.scale-m0tr-stob-4-be-log]
:name:
:author: anatoliy
:detail: log stobs
:justification:
:component: Motr
:req:
:process:
:depends: stob-concurrency
:resources:

------


:id: [t.scale-m0tr-writeagg]
:name:
:author: anatoliy
:detail: WRITE AGGREGATION META TASK
:justification:
:component: Motr
:req:
:process:
:depends: m0be
:resources:

------


:id: [t.scale-m0tr-throttling]
:name:
:author: anatoliy
:detail: MERO LEVEL THROTTLING META TASK
:justification:
:component: Motr
:req:
:process:
:depends: 
:resources:

------


:id: [t.scale-m0tr-rpc]
:name:
:author: anatoliy
:detail: RPC META TASK
:justification:
:component: Motr
:req:
:process:
:depends: 
:resources:

------


:id: [t.scale-m0tr-rpc-formation]
:name:
:author: anatoliy
:detail: Formation tuning
:justification:
:component: Motr
:req:
:process:
:depends: rpc
:resources:

------


:id: [t.scale-m0tr-rpc-long-live]
:name:
:author: anatoliy
:detail: Tune “resends” for long living RPC
:justification:
:component: Motr
:req:
:process:
:depends: rpc
:resources:

------


:id: [t.scale-m0tr-perfinfra]
:name:
:author: anatoliy
:detail: Performance infrastructure for R2
:justification:
:component: Motr
:req:
:process:
:depends: 
:resources:

------


:id: [t.scale-m0tr-perfinfra-addb]
:name:
:author: anatoliy
:detail:  ADDB related work
:justification:
:component: Motr
:req:
:process:
:depends: perfinfra
:resources:

------


:id: [t.scale-m0tr-cas]
:name:
:author: anatoliy
:detail: CAS SERVICE META TASK
:justification:
:component: Motr
:req:
:process:
:depends: 
:resources:

------


:id: [t.scale-m0tr-cas-lock]
:name:
:author: anatoliy
:detail: CAS locking schema optimisation
:justification:
:component: Motr
:req:
:process:
:depends: cas
:resources:

------


:id: [t.scale-m0tr-reqh]
:name:
:author: anatoliy
:detail: REQUEST HANDLER META TASK
:justification:
:component: Motr
:req:
:process:
:depends: 
:resources:

------


:id: [t.scale-m0tr-reqh-long-lock]
:name:
:author: anatoliy
:detail: long lock fairness
:justification:
:component: Motr
:req:
:process:
:depends: reqh
:resources:

------

:id: [t.scale-m0tr-reqh-ast]
:name:
:author: anatoliy
:detail: AST profiling
:justification:
:component: Motr
:req:
:process:
:depends: reqh
:resources:

------

:id: [t.support-different-servers]
:name: Benchmark and tune performance with different server
:detail: Different server can give different performance. Running standard benchmark workload profile and checking if there is any deviation from reference server
:justification: Analyzing impact on performance will help customer in server selection.
:component: motr, Performance evaluation team
:req: HW-20
:process: Test suite for performance evaluation
:depends:
:resources:

-------

:id: [t.support-different-network-equipment]
:name: Benchmark and tune performance with different network equipment
:detail: Different networking equipment can give different performance. Running standard benchmark workload profile and checking if there is any deviation from reference networking equipment.
:justification: Analyzing impact on performance will help customer in server selection.
:component: motr, Performance evaluation team
:req: HW-30
:process: Test suite for performance evaluation
:depends:
:resources:

-------

:id: [t.small-object-performance]
:name: Increase parallelism in accessing b-tree EMAP
:detail: Create hash function which will take object ID and point to a b-tree.
         Store this hash table in metadata. Test perfromance with varying sizes 
         of hash entries e.g 128,256,512 and 1024 and conclude on size to use.
:justification: Will reduce lock contention and help improve performance.
:component: motr, motr.beck
:req: SCALE-50
:process: 
:depends:
:resources:

-------

:id: [t.small-object-performance]
:name: Increase parallelism in accessing b-tree e.g EMAP and CAS
:detail: Create hash function which will take object ID and point to a b-tree.
         Store this hash table in metadata. Test perfromance with varying sizes 
         of hash entries e.g 128,256,512 and 1024 and conclude on size to use.
         Note: Design of hash function for CAS will need some design work to
         arrive at structure.
:justification: Will reduce lock contention and help improve performance.
:component: motr, motr.beck
:req: SCALE-50
:process:
:depends:
:resources:

-------
