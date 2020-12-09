=========
Task list
=========

:author: Huang Hua <hua.huang@seagate.com>

.. list-table::
   :header-rows: 1

   * - **task name**
     - **description**
   * - re-construct data when some data units are not availabe (for object).
     - For object reading, if some error happens (network error, bad block, checksum erro,
       OOM, etc.) Motr server should return error. Motr client should read data or parity units
       from other exsting servers, and re-construct data.
   * - reconstruct key/val when some ctg is not avaiable (for DIX).
     - query another CTG in its replicas.
   * - DTM infrastructure
     - This is the task for implementation of DTM.
   * - Using DTM in object create/write/delete.
     - 
   * - Using DTM in index create/put/delete.
     -
   * - Handle error in object update (create/write/delete) operations.
     - If majority of its replicas have succeeded, then return success. Otherwise return error.
   * - Handle error in DIX update (create/put/delete) operations.
     - If majority of its replicas have succeeded, then return success. Otherwise return error.
   * - btree rewrite
     - This is a very big task including design, implementation, unit testing, and system testing.
   * - balloc rewrite
     - This is a very big task including design, implementation, unit testing, and system testing.
   * - libfabric network xprt
     - This is a big task including design, implementation, unit testing, and system testing.
   * - DTM
     - DTM design design, implementation, unit testing, and system testing.
   * - Galois replacement
     - Remove Galois and replace it with xxx.
   * - Basic 3-node deployment
     - Hare and Motr, to deploy a cluster with 3-node, each node with 2 data volumes, one metadata volume.
   * - Node addition
     - Hare and Motr, to add a storageset to existing and running cluster. This involves conf update in Hare,
       conf re-refresh and re-configure in Motr.
   * - Provisioning 3-node, 6-node, 9-node, 12-node.
     - Provisioning should be able to deploy the cluster.
   * - Move to CentOS 8.x
     - Porting Motr, and other components to CentOS 8.x
   * - Define and test the required 3rd party software versions.
     -
   * - Performance testing.
     - 
   * - Performance optimization.
     - Read: 1GB/sec, Write: 850MB/sec per node for 256KB objects
       Read: 3GB/sec; Write: 2.5GB/sec per node for 16MB objects
       Aggregated performance for the cluster must grow within 90% of linearly with the number of nodes
   * - Cluster Aging testing.
     - To fill nearly full, to test performance and corner cases, alerts.
   * - Test 100M objects per node.
     -
   * - Test and optimize TTFB to 150ms
     -
   * - Test network split scenarios
     -
   * - Test a node down in a storage set
     -
   * - Test (a node down in a storage set) * multiple storageset
     -
