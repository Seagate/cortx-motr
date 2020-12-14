# Table of contents
1. [Overview](#Overview)
   - [CORTX-Cluster](#CORTX-Cluster)
2. [Function-Requirement](#Function-Requirement)
    1. [Sub paragraph](#subparagraph1)
3. [Another paragraph](#paragraph2)

# Overview
This document will be describe LDR R2 motr architecture and design
## CORTX-Cluster
Following are the hierarchy of Cortx Cluster
1. Cortx Cluster 
   - Top Level cluster will have multiple storage set
   - Each storage set will be connected at network layer
1. Storage Set 
   - Storage set will have multiple nodes
   - Number of nodes can vary with storage set (e.g N1 != N2) 
   - The capacity of storge set can differ even if number of nodes are same (N1 == N2)
1. Nodes
   - Node consist of server and enclosure pair. 
   - For R2, enclosure is isolated and only node can access its enclosure.

![CORTX Cluster with Storage Set](images/CortexV2StorageSet.JPG)

### R2-CORTX-Cluster
CORTX R2 will have following characteristics
1. Storage set will communicate with each other at network layer. 
   - e.g In above figure Storage Set 1 and 2 are shown connected at network layer
1. **IOs will be confined to Storage Set.**
   - Data will only be striped across nodes in a Storage Set.
1. Any node in cluster can server S3 request for any storage set.
   - e.g In above figure Node 1 from Storage Set 1 can server read request for object stored in Storage Set 2

### Storage-Set
Storage set

# Function-Requirement
This section will list all the functional requirement for motr scaleout architecture

## Basic Assumptions
Following are the basic assumptions for creating the sequence flow for various error scenarios
- Global Bucket list is replicated across stroage set.
-- Lookup into global metadata will give Stroage Set where the bucket list and data reside
- Bucket Object List Table is replcated within storage set
-- Bucket Object List Table if we make this global than size of global data inreases lot? So this should be within storage set, but objects can be part of different storage set but Object data does not span across storage set ?

## Initial Condition : 
* Two Storage Set (SS) in cluster
* Each storage set with 3 nodes
* Single failure in a node is allowed
* Global bucket is replicated across all nodes of cluster (N):
   - This is small amount of global bucket data
   - Frequency of creation of this bucket data is low
   - Replication to all nodes allows each storage set to easily support one node failure and will also help with case of Storage Set addition.
   -- Full replication will help in performance, mimimum replication needed in a storage set is #num_of_node_failure_in_storage_set + 1 
* Local S3 metadata should be replicated across atleast 3 nodes of a storage set
   - Replication of data across 3 nodes of storage set, will helps in node failure     
* Data is striped in 4+2 parity config

## DTM Usage
This section will analyze the DTM usage in various scenario with assumption that


### Scenario
Following scenario will be analyzed w.r.t. DTM role to restore storage system to consistent state w.r.t metadata and data.

- IO Failure: Node or IO Service going down temporarily
- Software Upgrade: Node going through SW upgrade.
- Storage Set Addition: A new storage set becoming part of cluster

#### IO Failure
A node or storage unavailable for some time can cause following issues
1. Node will not be able to update following metadata and it will get out of sync with cluster
   1. Global Bucket List : Across Storage Set
   1. Bucket Object List Table : Within Storage Set
1. Data unit directed to the node will get dropped and object will lose units of data (1-MAX_DISK_GROUP_IN_SS) 

#### Software (SW) Upgrade
Without DTM, cluster can service Read Object request during SW upgrade but for write object and bucket creation metadata will need DTM. 

#### Storage Set Addition
A new storage set addition will need to sync Global Bucket List and DTM will be needed for that.

As for P0 we can live with object in degraded state, we can skip DTM for Data.
But the DTM is mandatory for keeping the cluster distributed metadata in consistent state.

# I. Node failure During IO
## Error Scenario : 
* One node from each storage set (SS) fails during IO.
* Any error in reading metadata should be retried with metadata available with replicated node

## 1. Simple Object Get/Read
* Read path should detect error in retriving data and should use parity units to get missing data.

```plantuml
@startuml
participant "S3 Client" as client
box StorageSet1 (SS1) #FEFEFE
participant "S3 Server/N1" as N1
participant "ctrl-A/CL-1" as CL_1
participant "ctrl-B/CR-1" as CR_1
participant "N2" as N2 #red
participant "CL-2" as CL_2 #orange
participant "CR-2" as CR_2 #orange
participant "N3" as N3
participant "CL-3" as CL_3
participant "CR-3" as CR_3
end box
box StorageSet2 (SS2) #EEEEEE
participant "N4" as N4 #red
participant "CL-4" as CL_4 #orange
participant "CR-4" as CR_4 #orange
participant "N5" as N5
participant "CL-5" as CL_5
participant "CR-5" as CR_5
participant "N6" as N6
participant "CL-6" as CL_6
participant "CR-6" as CR_6
end box

client -> N1: GET /bucket_name/object_name

== Lookup 1 : Global bucket ==
N1 -> N2: get_keyval(global_bucket_index,\n key = "bucket_name")
N2 --X N1: Error 
N1 -> N5: get_keyval(global_bucket_index,\n key = "bucket_name")
N5 --> N1: value = account_id of bucket owner

== Lookup 2 : Global Bucket ==
N1 -> N2: get_keyval(global_bucket_md_index,\n key = "account_id/bucket_name")
N2 --X N1: Error 
N1 -> N3: get_keyval(global_bucket_md_index,\n key = "account_id/bucket_name")
N3 --> N1: value = bucket metadata JSON

note left
   * Global lookup will have 1 + N-1 replication 
   * N total node in the system with __**Node2**__ in error
   * __**Any error in global lookup will needs to be retried with other nodes**__
end note

== Lookup 3 : Local Storage Set  ==
N1 -> N4: get_keyval(BUCKET_nnn_obj_index,\n key = "object_name");
N4 --X N1 : Error
N1 -> N5: get_keyval(BUCKET_nnn_obj_index,\n key = "object_name");
note left
   * KV in Local Storage Set will have 1 + M-1 replication 
   * M total node in the Storage System, with __**Node4**__ in error
   * __**Any error in local lookup will needs to be retried with other nodes in SS**__
end note
N5 --> N1: Found
note left
This lookup returns: Layout ID, Pool Version, Object ID, which helps to identify SS
end note

== Read Operation ==

loop until all data is read
  N1 -> "N4": Read data 1
note left
   Assuming 4+2 parity
end note
  "N4" -> CL_4
  N1 -> "N4": Read data 2
  "N4" -> CR_4

  N1 -> "N5": Read data 3
  "N5" -> CL_5
  N1 -> "N5": Read data 4
  "N5" -> CR_5

  N1 -> "N6": Read data P1
  "N6" -> CL_6
  N1 -> "N6": Read data P2
  "N6" -> CR_6

  N5 --> N1: Data (Read 3 & 4)
  N6 --> N1: Data (Read P1 & P2)
  N4 --X N1: ERROR (Read 1 & 2)
note left
   * Two of the Read Data returns error (Node 4)
   * As number of error unit <= number of parity units follow decode step
   * __**Decode Data using P1 & P2, 3 & 4**__
end note
  "N1" -> N1 : Decode (Read 1 & 2)    
end

N1 --> client: 200 OK
@enduml
```

## 2. Simple Object Put/Write
* Write path should detect error in writing data and should return success if the failed number of write unit is less than or equal to number of parity unit

```plantuml
@startuml

participant "S3 Client" as client
box StorageSet1 (SS1) #FEFEFE
participant "S3 Server/N1" as N1
participant "ctrl-A/CL-1" as CL_1
participant "ctrl-B/CR-1" as CR_1
participant "N2" as N2 #red
participant "CL-2" as CL_2 #orange
participant "CR-2" as CR_2 #orange
participant "N3" as N3
participant "CL-3" as CL_3
participant "CR-3" as CR_3
end box
box StorageSet2 (SS2) #EEEEEE
participant "N4" as N4 #red
participant "CL-4" as CL_4 #orange
participant "CR-4" as CR_4 #orange
participant "N5" as N5
participant "CL-5" as CL_5
participant "CR-5" as CR_5
participant "N6" as N6
participant "CL-6" as CL_6
participant "CR-6" as CR_6
end box

client -> N1: PUT /bucket_name/object_name

== Lookup 1 : Global bucket ==
N1 -> N2: get_keyval(global_bucket_index,\n key = "bucket_name")
N2 --X N1: Error 
N1 -> N5: get_keyval(global_bucket_index,\n key = "bucket_name")
N5 --> N1: value = account_id of bucket owner

== Lookup 2 : Global Bucket ==
N1 -> N2: get_keyval(global_bucket_md_index,\n key = "account_id/bucket_name")
N2 --X N1: Error 
N1 -> N3: get_keyval(global_bucket_md_index,\n key = "account_id/bucket_name")
N3 --> N1: value = bucket metadata JSON

note left
   * Global lookup will have 1 + N-1 replication 
   * N total node in the system with __**Node2**__ in error
   * __**Any error in global lookup will needs to be retried with other nodes**__
end note

== Lookup 3 : Local Storage Set  ==
N1 -> N4: get_keyval(BUCKET_nnn_obj_index,\n key = "object_name");
N4 --X N1 : Error
N1 -> N5: get_keyval(BUCKET_nnn_obj_index,\n key = "object_name");
note left
   * KV in Local Storage Set will have 1 + M-1 replication 
   * M total node in the Storage System, with __**Node4**__ in error
   * __**Any error in local lookup will needs to be retried with other nodes in SS**__
end note
N5 --> N1: not found

== Write Operation ==
N1 -> N1: create_object
N1 --> N1: success (completed)

loop until all data is written
  N1 -> "N4": Write data 1
note left
   Assuming 4+2 parity
end note
  "N4" -> CL_4
  N1 -> "N4": Write data 2
  "N4" -> CR_4

  N1 -> "N5": Write data 3
  "N5" -> CL_5
  N1 -> "N5": Write data 4
  "N5" -> CR_5

  N1 -> "N6": Write data P1
  "N6" -> CL_6
  N1 -> "N6": Write data P2
  "N6" -> CR_6

  N5 --> N1: Success (write 3 & 4)
  N6 --> N1: Success (write P1 & P2)
  N4 --X N1: ERROR (write 1 & 2)
note left
   * Two of the Write Data Units returns error (Node 4)
   * __**Return Success if Total-Error <= #Parity-Unit **__
end note

end

== Local SS metadata write ==
N1 -> N4: put_keyval(BUCKET_nnn_obj_index,\n key = object_name, val = object_metadata)
N4 --X N1: ERROR

N1 -> N5: put_keyval(BUCKET_nnn_obj_index,\n key = object_name, val = object_metadata)
N5 --> N1: success (completed)

N1 -> N6: put_keyval(BUCKET_nnn_obj_index,\n key = object_name, val = object_metadata)
N6 --> N1: success (completed)

N1 --> client: 200 OK

@enduml
```

# II. Detected Node failed Scenario
## Error Scenario : 
* One node from each storage set (SS) has failed.
* Motr is aware of failure and Hare has notified to motr about the node failure

## 1. Simple Object Get/Read
* Read path error handling avoids communicating with failed node for metadata and data operations.

```plantuml
@startuml
participant "S3 Client" as client
box StorageSet1 (SS1) #FEFEFE
participant "S3 Server/N1" as N1
participant "ctrl-A/CL-1" as CL_1
participant "ctrl-B/CR-1" as CR_1
participant "N2" as N2 #red
participant "CL-2" as CL_2 #red
participant "CR-2" as CR_2 #red
participant "N3" as N3
participant "CL-3" as CL_3
participant "CR-3" as CR_3
end box
box StorageSet2 (SS2) #EEEEEE
participant "N4" as N4 #red
participant "CL-4" as CL_4 #red
participant "CR-4" as CR_4 #red
participant "N5" as N5
participant "CL-5" as CL_5
participant "CR-5" as CR_5
participant "N6" as N6
participant "CL-6" as CL_6
participant "CR-6" as CR_6
end box

client -> N1: GET /bucket_name/object_name

== Lookup 1 : Global bucket ==
N1 -> N5: get_keyval(global_bucket_index,\n key = "bucket_name")
N5 --> N1: value = account_id of bucket owner

note left
   __** Communication with Node4 in SS2 is avoided **__
end note

== Lookup 2 : Global Bucket ==
N1 -> N3: get_keyval(global_bucket_md_index,\n key = "account_id/bucket_name")
N3 --> N1: value = bucket metadata JSON

note left
   __** Communication with Node2 in SS1 is avoided **__
end note

== Lookup 3 : Local Storage Set  ==
N1 -> N6: get_keyval(BUCKET_nnn_obj_index,\n key = "object_name");
N6 --> N1: not found

== Read Operation ==

loop until all data is read
  N1 -> "N5": Read data 3
  "N5" -> CL_5
  N1 -> "N5": Read data 4
  "N5" -> CR_5

  N1 -> "N6": Read data P1
  "N6" -> CL_6
  N1 -> "N6": Read data P2
  "N6" -> CR_6

  N5 --> N1: Data (Read 3 & 4)
  N6 --> N1: Data (Read P1 & P2)
note left
   * Two of the Read Data is missing 1 & 2
   * __**Decode Data using P1 & P2, 3 & 4**__
end note
  "N1" -> N1 : Decode (Read 1 & 2)    
end

N1 --> client: 200 OK

@enduml
```

## 2. Simple Object Put/Write
* Write path error handling avoids communicating with failed node for metadata and data operations.

```plantuml
@startuml

participant "S3 Client" as client
box StorageSet1 (SS1) #FEFEFE
participant "S3 Server/N1" as N1
participant "ctrl-A/CL-1" as CL_1
participant "ctrl-B/CR-1" as CR_1
participant "N2" as N2 #red
participant "CL-2" as CL_2 #red
participant "CR-2" as CR_2 #red
participant "N3" as N3
participant "CL-3" as CL_3
participant "CR-3" as CR_3
end box
box StorageSet2 (SS2) #EEEEEE
participant "N4" as N4 #red
participant "CL-4" as CL_4 #red
participant "CR-4" as CR_4 #red
participant "N5" as N5
participant "CL-5" as CL_5
participant "CR-5" as CR_5
participant "N6" as N6
participant "CL-6" as CL_6
participant "CR-6" as CR_6
end box

client -> N1: PUT /bucket_name/object_name

== Lookup 1 : Global bucket ==
N1 -> N5: get_keyval(global_bucket_index,\n key = "bucket_name")
N5 --> N1: value = account_id of bucket owner

note left
   __** Communication with Node4 in SS2 is avoided **__
end note

== Lookup 2 : Global Bucket ==
N1 -> N3: get_keyval(global_bucket_md_index,\n key = "account_id/bucket_name")
N3 --> N1: value = bucket metadata JSON

note left
   __** Communication with Node2 in SS1 is avoided **__
end note

== Lookup 3 : Local Storage Set  ==
N1 -> N6: get_keyval(BUCKET_nnn_obj_index,\n key = "object_name");
N6 --> N1: not found

== Write Operation ==
N1 -> N1: create_object
N1 --> N1: success (completed)

loop until all data is written
  N1 -> "N5": Write data 3
note left
   * Original Data Config: 4+2 parity
   * __**Due to failure of node, switching to pool with 2+2 parity in storage set**__
end note
  "N5" -> CL_5
  N1 -> "N5": Write data 4
  "N5" -> CR_5
  N1 -> "N6": Write data P1
  "N6" -> CL_6
  N1 -> "N6": Write data P2
  "N6" -> CR_6

  N5 --> N1: Success (write 3 & 4)
  N6 --> N1: Success (write P1 & P2)
end

== Local SS metadata write ==

N1 -> N5: put_keyval(BUCKET_nnn_obj_index,\n key = object_name, val = object_metadata)
N5 --> N1: success (completed)
note left
   * Original Metadata Config: 1+2 replication
   * __**Due to failure of node, switching to 1+1 replication in storage set**__
end note

N1 -> N6: put_keyval(BUCKET_nnn_obj_index,\n key = object_name, val = object_metadata)
N6 --> N1: success (completed)

N1 --> client: 200 OK
@enduml
```