.. This is a sequence diagram for a typical cortx end-to-end data-flow. It shows
   processing of an S3 PUT request. Use Makefile or
   http://plantuml.com/plantuml/uml to build.

.. uml::

   skinparam ParticipantPadding 20
   skinparam BoxPadding 10

   participant "s3-client" as s3c
   box "s3 process"
   participant "s3-server" as s3
   participant "motr-client" as cl
   end box
   box "motr process 0"
   participant cas0
   participant "meta-device-0" as md0
   participant ios0
   participant "data-device-0" as dd0
   end box

   box "motr process 1"
   participant cas1
   participant "meta-device-1" as md1
   participant ios1
   participant "data-device-1" as dd1
   end box

   box "motr process 2"
   participant cas2
   participant "meta-device-2" as md2
   participant ios2
   participant "data-device-2" as dd2
   end box

   s3c   -> s3   ++ : http/put(header+data)

   note left of s3: fetch global s3 meta-data
   s3    -> cl   ++ : clovis-get(index, key)
   cl    -> cas0 ++ : GET(key)
   note right of md0: lookup b-tree in meta-b-tree
   cas0  -> md0  ++ : READ(meta-b-tree-node) (if not cached)
   cas0 <-- md0  -- : RET(node)
   cas0  -> md0  ++ : READ(meta-b-tree-node)
   cas0 <-- md0  -- : RET(node)
   cas0  -> md0  ++ : READ(meta-b-tree-node)
   cas0 <-- md0  -- : RET(btree)
   cas0  -> md0  ++ : READ(b-tree-node) (if not cached)
   note right of md0: lookup in catalogue b-tree
   cas0 <-- md0  -- : RET(node)
   cas0  -> md0  ++ : READ(b-tree-node)
   cas0 <-- md0  -- : RET(node)
   cas0  -> md0  ++ : READ(b-tree-node)
   cas0 <-- md0  -- : RET(node)
   cl   <-- cas0 -- : RET(val)
   s3   <-- cl   -- : RET(val)

   note left of s3: fetch other s3 meta-data
   s3    -> cl   ++ : clovis-get(index, key)
   cl    -> cas0 ++ : GET(key)
   note right of md0: lookup b-tree in meta-b-tree
   cas0  -> md0  ++ : READ(meta-b-tree-node) (if not cached)
   cas0 <-- md0  -- : RET(node)
   cas0  -> md0  ++ : READ(meta-b-tree-node)
   cas0 <-- md0  -- : RET(node)
   cas0  -> md0  ++ : READ(meta-b-tree-node)
   cas0 <-- md0  -- : RET(btree)
   cas0  -> md0  ++ : READ(b-tree-node)
   note right of md0: lookup in catalogue b-tree
   cas0 <-- md0  -- : RET(node)
   cas0  -> md0  ++ : READ(b-tree-node)
   cas0 <-- md0  -- : RET(node)
   cas0  -> md0  ++ : READ(b-tree-node)
   cas0 <-- md0  -- : RET(node)
   cl   <-- cas0 -- : RET(val)
   s3   <-- cl   -- : RET(val)

   note left of s3: insert meta-data for the new object
   s3    -> cl   ++ : clovis-put(index, key, val)
   cl    -> cas0 ++ : PUT(key,val)
   cas0  -> cas0    : tx_open(tx)
   cl    -> cas1 ++ : PUT(key,val)
   cas1  -> cas1    : tx_open(tx)

   cas1  -> md1  ++ : READ(meta-b-tree-node) (if not cached)
   note right of md1: lookup b-tree in meta-b-tree
   cas0  -> md0  ++ : READ(meta-b-tree-node) (if not cached)
   note right of md0: lookup b-tree in meta-b-tree
   cas0 <-- md0  -- : RET(node)
   cas1 <-- md1  -- : RET(node)
   cas0  -> md0  ++ : READ(meta-b-tree-node)
   cas0 <-- md0  -- : RET(node)
   cas1  -> md1  ++ : READ(meta-b-tree-node)
   cas0  -> md0  ++ : READ(meta-b-tree-node)
   cas1 <-- md1  -- : RET(node)
   cas1  -> md1  ++ : READ(meta-b-tree-node)
   cas1 <-- md1  -- : RET(btree)
   cas0 <-- md0  -- : RET(btree)
   cas1  -> md1  ++ : READ(b-tree-node)
   note right of md1: lookup in catalogue b-tree
   cas0  -> md0  ++ : READ(b-tree-node)
   note right of md0: lookup in catalogue b-tree
   cas0 <-- md0  -- : RET(node)
   cas1 <-- md1  -- : RET(node)
   cas0  -> md0  ++ : READ(b-tree-node)
   cas1  -> md1  ++ : READ(b-tree-node)
   cas1 <-- md1  -- : RET(node)
   cas0 <-- md0  -- : RET(node)
   cas0  -> md0  ++ : READ(b-tree-node)
   cas1  -> md1  ++ : READ(b-tree-node)
   cas0 <-- md0  -- : RET(node)
   cas0  -> cas0    : UPDATE(tx, index, key, val)
   cas0  -> cas0    : tx_close(tx)
   cl   <-- cas0 -- : RET
   cas1 <-- md1  -- : RET(node)
   cas1  -> md1  ++ : READ(node-alloc)
   cas1 <-- md1  -- : RET(new-node)
   cas1  -> cas1    : UPDATE(tx, key, val)
   cas1  -> cas1    : tx_close(tx)
   cl   <-- cas1 -- : RET
   s3   <-- cl   -- : RET

   cas1  -> md1  ++ : WRITE(tx, log)
   note left of cas1: asynchronously commit transaction
   cas1 <-- md1  -- : RET
   cas1  -> md1  ++ : WRITE(tx, seg)
   cas0  -> md0  ++ : WRITE(tx, log)
   note left of cas1: asynchronously commit transaction
   cas0 <-- md0  -- : RET
   cas0  -> md0  ++ : WRITE(tx, seg)
   cas0 <-- md0  -- : RET
   cas1 <-- md1  -- : RET
   
.. uml::
   
   skinparam ParticipantPadding 20
   skinparam BoxPadding 10

   participant "s3-client" as s3c
   box "s3 process"
   participant "s3-server" as s3
   participant "motr-client" as cl
   end box
   box "motr process 0"
   participant cas0
   participant "meta-device-0" as md0
   participant ios0
   participant "data-device-0" as dd0
   end box

   box "motr process 1"
   participant cas1
   participant "meta-device-1" as md1
   participant ios1
   participant "data-device-1" as dd1
   end box

   box "motr process 2"
   participant cas2
   participant "meta-device-2" as md2
   participant ios2
   participant "data-device-2" as dd2
   end box

   s3c   -> s3   ++ : http/put [continued]

   s3    -> s3      : split data into chunks

   s3    -> cl   ++ : WRITE(s3obj, 8MB)
   cl    -> cl      : compute parity
   
   cl    -> ios0 ++ : WRITE(cob0, 1MB)
   cl    -> ios1 ++ : WRITE(cob1, 1MB)
   cl    -> ios2 ++ : WRITE(cob2, 1MB)

   ios0  -> ios0    : tx_open(tx)
   ios0  -> md0  ++ : READ(cob-tree) (if not cached)
   note right of ios0: lookup object in cob domain
   ios0 <-- md0  -- : RET(node)
   ios0  -> md0  ++ : READ(cob-tree)
   ios0 <-- md0  -- : RET(node)
   ios0  -> md0  ++ : READ(cob-tree)
   ios1  -> ios1    : tx_open(tx)
   ios2  -> ios2    : tx_open(tx)
   ios2  -> md2  ++ : READ(cob-tree) (if not cached)
   note right of ios2: lookup object in cob domain
   ios2 <-- md2  -- : RET(node)
   ios1  -> md1  ++ : READ(cob-tree) (if not cached)
   note right of ios1: lookup object in cob domain
   ios1 <-- md1  -- : RET(node)
   ios1  -> md1  ++ : READ(cob-tree)
   ios1 <-- md1  -- : RET(node)
   ios2  -> md2  ++ : READ(cob-tree)
   ios2 <-- md2  -- : RET(node)
   ios2  -> md2  ++ : READ(cob-tree)
   ios2 <-- md2  -- : RET(cob-record)
   ios2  -> md2  ++ : READ(balloc-tree) (if not cached)
   note right of ios2: find free extent of blocks
   ios2 <-- md2  -- : RET(node)
   ios0 <-- md0  -- : RET(cob-record)
   ios1  -> md1  ++ : READ(cob-tree)
   ios1 <-- md1  -- : RET(cob-record)
   ios2  -> md2  ++ : READ(balloc-tree)
   ios2 <-- md2  -- : RET(node)
   ios1  -> md1  ++ : READ(balloc-tree) (if not cached)
   note right of ios1: find free extent of blocks
   ios1 <-- md1  -- : RET(node)
   ios0  -> md0  ++ : READ(balloc-tree) (if not cached)
   note right of ios0: find free extent of blocks
   ios2  -> md2  ++ : READ(balloc-tree)
   ios2 <-- md2  -- : RET(node)
   ios2  -> md2  ++ : READ(balloc-tree)
   ios2 <-- md2  -- : RET(free-extent)
   ios2  -> md2  ++ : READ(balloc-tree) (if not cached)
   note right of ios2: claim blocks for object
   ios2 <-- md2  -- : RET(node)
   ios1  -> md1  ++ : READ(balloc-tree)
   ios0 <-- md0  -- : RET(node)
   ios1 <-- md1  -- : RET(node)
   ios2  -> md2  ++ : READ(balloc-tree)
   ios2 <-- md2  -- : RET(node)
   ios2  -> md2  ++ : READ(balloc-tree)
   ios2 <-- md2  -- : RET(ext)
   ios2  -> ios2    : UPDATE(tx, balloc, extent)
   ios1  -> md1  ++ : READ(balloc-tree)
   ios0  -> md0  ++ : READ(balloc-tree)
   ios1 <-- md1  -- : RET(free-extent)
   ios1  -> md1  ++ : READ(balloc-tree) (if not cached)
   note right of ios1: claim blocks for object
   ios0 <-- md0  -- : RET(node)
   ios2  -> md2  ++ : READ(extmap-tree) (if not cached)
   note right of ios2: record extent in object map
   ios1 <-- md1  -- : RET(node)
   ios1  -> md1  ++ : READ(balloc-tree)
   ios2 <-- md2  -- : RET(node)
   ios2  -> md2  ++ : READ(extmap-tree)
   ios2 <-- md2  -- : RET(node)
   ios2  -> md2  ++ : READ(extmap-tree)
   ios2 <-- md2  -- : RET(extmap-rec)
   ios2  -> ios2    : tx_close(tx)
   ios2  -> cl   ++ : RDMA-GET
   note right of ios2: request RDMA from client
   ios0  -> md0  ++ : READ(balloc-tree)
   ios0 <-- md0  -- : RET(free-extent)
   ios0  -> md0  ++ : READ(balloc-tree) (if not cached)
   note right of ios0: claim blocks for object
   ios1 <-- md1  -- : RET(node)
   ios2 <-- cl      : RDMA-DATA
   ios2 <-- cl   -- : RDMA-DONE
   ios2  -> dd2  ++ : WRITE(1MB)
   ios2 <-- dd2  -- : DONE
   ios1  -> md1  ++ : READ(balloc-tree)
   ios1 <-- md1  -- : RET(ext)
   ios1  -> ios1    : UPDATE(tx, balloc, extent)
   ios1  -> md1  ++ : READ(extmap-tree) (if not cached)
   note right of ios1: record extent in object map
   ios0 <-- md0  -- : RET(node)
   ios0  -> md0  ++ : READ(balloc-tree)
   ios1 <-- md1  -- : RET(node)
   ios1  -> md1  ++ : READ(extmap-tree)
   ios0 <-- md0  -- : RET(node)
   ios0  -> md0  ++ : READ(balloc-tree)
   ios0 <-- md0  -- : RET(ext)
   ios0  -> ios0    : UPDATE(tx, balloc, extent)
   ios0  -> md0  ++ : READ(extmap-tree) (if not cached)
   note right of ios0: record extent in object map
   ios0 <-- md0  -- : RET(node)
   ios1 <-- md1  -- : RET(node)
   ios1  -> md1  ++ : READ(extmap-tree)
   ios0  -> md0  ++ : READ(extmap-tree)
   ios0 <-- md0  -- : RET(node)
   ios0  -> md0  ++ : READ(extmap-tree)
   ios0 <-- md0  -- : RET(extmap-rec)
   ios0  -> ios0    : UPDATE(tx, extmap-rec, extent)
   ios1 <-- md1  -- : RET(extmap-rec)
   ios1  -> ios1    : UPDATE(tx, extmap-rec, extent)
   ios1  -> ios1    : tx_close(tx)
   ios0  -> ios0    : tx_close(tx)

.. uml::

   skinparam ParticipantPadding 20
   skinparam BoxPadding 10

   participant "s3-client" as s3c
   box "s3 process"
   participant "s3-server" as s3
   participant "motr-client" as cl
   end box
   box "motr process 0"
   participant cas0
   participant "meta-device-0" as md0
   participant ios0
   participant "data-device-0" as dd0
   end box

   box "motr process 1"
   participant cas1
   participant "meta-device-1" as md1
   participant ios1
   participant "data-device-1" as dd1
   end box

   box "motr process 2"
   participant cas2
   participant "meta-device-2" as md2
   participant ios2
   participant "data-device-2" as dd2
   end box

   ios1  -> cl   ++ : RDMA-GET
   note right of ios1: request RDMA from client
   ios0  -> cl   ++ : RDMA-GET
   note right of ios0: request RDMA from client
   ios0 <-- cl      : RDMA-DATA
   ios1 <-- cl      : RDMA-DATA
   ios1 <-- cl   -- : RDMA-DONE
   ios1  -> dd1  ++ : WRITE(1MB)
   ios0 <-- cl   -- : RDMA-DONE
   ios1 <-- dd1  -- : DONE
   ios0  -> dd0  ++ : WRITE(1MB)
   ios0 <-- dd0  -- : DONE
   cl   <-- ios1 -- : RET
   cl   <-- ios2 -- : RET
   cl   <-- ios0 -- : RET
   s3   <-- cl   -- : RET

   s3c   -> s3   ++ : http/put [continue]
   
   s3    -> s3      : more chunks
   s3c  <-- s3   -- : RET

   ios0  -> md0  ++ : WRITE(tx, log)
   ios0 <-- md0  -- : RET
   ios0  -> md0  ++ : WRITE(tx, seg)
   ios0 <-- md0  -- : RET

   ios1  -> md1  ++ : WRITE(tx, log)
   ios1 <-- md1  -- : RET
   ios1  -> md1  ++ : WRITE(tx, seg)
   ios1 <-- md1  -- : RET

   ios2  -> md2  ++ : WRITE(tx, log)
   ios2 <-- md2  -- : RET
   ios2  -> md2  ++ : WRITE(tx, seg)
   ios2 <-- md2  -- : RET
