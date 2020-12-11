
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
:name: motr client gracefully recovers from errors or read
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
:name: s3 and motr client gracefully recovers from errors or write
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: if motr client cannot complete write in time (error or timeout) it
         reports this to s3. s3 allocates new fid and repeats last write in an
         alternative pool (2+2). s3 records starting offset and new fid in
         object meta-data. If 2+2 creation fails, return error to s3 client. See
         [q.object-cleanup]. See [t.s3.io-error-write].
:justification:
:component: motr.client, motr.pool, s3
:req: AD-10, AD-20
:process: HLD, HLDINSP, DLD, DLDINSP, CODE, INSP, ST
:depends:
:resources:

------



:id: [t.s3.io-error-write]
:name: s3 and motr client gracefully recovers from errors or write
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: support degraded 2+2 objects in s3, see [t.io-error-write]. Store fids
         and offsets of parts in s3 json.
:justification:
:component: motr, s3
:req: AD-10, AD-20
:process: HLD, HLDINSP, DLD, DLDINSP, CODE, INSP, ST
:depends:
:resources:

------



:id: [t.2+2.conf]
:name: s3 and motr client gracefully recovers from errors or write
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: prepare 2+2 pools needed by [t.io-error-write]
:justification:
:component: motr, provisioner
:req: AD-10, AD-20
:process: HLD, HLDINSP, DLD, DLDINSP, CODE, INSP, ST
:depends:
:resources:

------



:id: [t.md-error-read]
:name: motr client gracefully recovers from errors or meta-data lookup
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
:name: verify meta-data check-sums on read
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: verify be record checksum on access. Maybe this is already partially
         done.
:justification:
:component: motr.be
:req:
:depends:
:resources:

------



:id: [t.b-tree-rewrite]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: new implementation of b-tree. Must satisfy requirements for further rN
         releases. Support: prefix-compression, check-sums for keys and
         values. Large keys and values. Page daemon. Concurrency. Non-blocking
         implementation.
:justification:
:component: motr
:req:
:process:
:depends:
:resources: Lead: nikita

------



:id: [t.balloc-rewrite]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: re-implement block allocator. Design for object storage.
:justification:
:component: motr
:req:
:process:
:depends:
:resources: Lead: madhav

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
:resources: Lead: Huang Hua.

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

------



:id: [t.pools-policy-health]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: policy to prefer healthy pools (based on availability updates from
         hare)
:justification: optional?
:component: motr.client, provisioner, hare
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
:detail: md cobs are removed, so data cobs should store over and layout
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
:name: handle K != S in motr
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



:id: [t.perf-s3-cache]
:name:
:author: Nikita Danilov <nikita.danilov@seagate.com>
:detail: cache bucket and account global meta-data in memory, for o longer than
         X seconds. Create bucket (and auth update) should be delayed by X
         seconds.
:justification:
:component: s3
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



