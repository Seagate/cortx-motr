# Motr Terminology Guide

<table>
  <tr>
    <th width=220p>Terminology</th>
    <th width=600px>Description</th>
    </tr>

  <tr>
    <td>ADDB</td>
    <td>Analysis and Diaganostics DataBase contains information describing ongoing activity of Motr system</td>
    </tr>

  <tr>
    <td>ATOM</td>
    <td>/archaic/ "ATtribute On Meta-data server" is a design idea inherited from Lustre (q.v.). Some object attributes, such as the amount of space used by the object, can be naturally maintained on each service that keeps parts of the object with the "total" value of the attribute being obtained by querying all such services and aggregating results (e.g., by adding together space used by the object on each service). According to ATOM, such attributes should be also accumulated in some centralised meta-data location, to avoid the overhead of sending and processing queries to large number of services.
    </td>    
  </tr>

  <tr>
    <td>Back-end (or simply "BE")</td>
    <td>Local meta-data back-end of motr. This component maintains transactional storage used by motr services to persistently store meta-data. BE used write-ahead-logging (WAL) to implement transactions. Main interfaces provided by BE are: transactions, allocator and b-tree. BE performance and stability are critical for overall motr performance, because all meta-data (internal and external) are stored and accessed via it.</td>
  </tr>

  <tr>
      <td>Background Scrub</td>
      <td> It Continuously scans the subsystem to identify and repair corrupted Motr data blocks. The worker created by scrub machine  serves a scrub request. A scrub-group is a group of relevant data blocks typically distributed across storage devices in a cluster, e.g. a parity group. A Scrub machine receives and run the scrub requests from DI and background scrub scanner. A Scrub request is submitted to repair the corrupt Motr daba block.
  </td>
  </tr>

  <tr>
      <td>BE Transaction</td>
      <td>A transaction is a collection of updates. User adds an update to a transaction by capturing the update's region. User explicitly closes a transaction. BE guarantees that a closed transaction is atomic with respect to process crashes that happen after transaction close call returns. That is, after such a crash, either all or none of transaction updates will be present in the segment memory when the segment is opened next time. If a process crashes before a transaction closes, BE guarantees that none of transaction updates will be present in the segment memory.
      </td>
  </tr>

  <tr>
      <td>Block</td>
      <td>The smallest Unit of allocation.</td>
  </tr>

  <tr>
      <td>Block Allocator (BALLOC)</td>
      <td>The data block allocator manages the free spaces in the container, and provides allocate and free blocks interfaces to other components and layers.</td>
  </tr>

  <tr>
      <td>Block Size</td>
      <td>The number of bytes of a Block.</td>
  </tr>

  <tr>
      <td>CAS</td>
      <td>Catalogue service (CAS) is a Motr service exporting key-value catalogues.</td>
  </tr>

  <tr>
      <td>Capability</td>
      <td>This is a permission assigned to an object, that allows some specific operations to be carried on the object from some specific user.</td>
  </tr>

  <tr>
      <td>Catalogue</td>
      <td>A container for records. A catalogue is explicitly created and deleted by a user and has an identifier, assigned by the user.</td>
  </tr>

  <tr>
      <td>COB</td>
      <td>A component object (cob) is a component of a file, referencing a single storage object and containing metadata describing the object.
      </td>
  </tr>

  <tr>
      <td>cobfid map </td>
      <td>A cobfid map is a persistent data structure that tracks the ID of cobs and their associated file fid, contained within other containers, such as a storage object.
      </td>
  </tr>

  <tr>
      <td>Consul</td>
      <td>Consul automates networking for simple and secure application delivery.
      </td>
  </tr>

  <tr>
      <td>Configuration database</td>
      <td>Configuration database is a central repository of cluster configuration and Motr configuration is a part of cluster meta-data.
      </td>
  </tr>

  <tr>
      <td>Configuration client</td>
      <td>Configuration client (confc) is a software module that manages node’s configuration cache.
      </td>
  </tr>

  <tr>
      <td>Configuration server</td>
      <td>Configuration server (confd) is a software module that mediates access to the configuration database. Also, the server node on which this module runs.
      </td>
  </tr>

  <tr>
      <td>Configuration consumer</td>
      <td>Configuration consumer is any software that uses confc API to access Motr configuration.
      </td>
  </tr>

  <tr>
      <td>Configuration cache</td>
      <td>Configuration cache is configuration data stored in node’s memory. Confc library maintains such a cache and provides configuration consumers with access to its data. Confd also uses configuration cache for faster retrieval of information requested by configuration clients.
      </td>
  </tr>

  <tr>
      <td>Configuration object</td>
      <td>Configuration object is a data structure that contains configuration information. There are several types of configuration objects: profile, service, node, etc.
      </td>
  </tr>  

  <tr>
      <td>Copy Machine (CM)</td>
      <td>A copy machine is a scalable distributed mechanism to restructure data in multiple ways (copying, moving, re-striping, reconstructing, encrypting, compressing, re-integrating, etc.). It can be used in variety of scenarios.
      </td>
  </tr>

  <tr>
      <td>Credit</td>
      <td>A credit is a measure of a group of updates. A credit is a pair (nr, size), where nr is the number of updates and size is total size in bytes of modified regions.
      </td>
  </tr>

  <tr>
      <td>Data unit</td>
      <td>A striping unit to which global object or component data are mapped.
      </td>
  </tr>

  <tr>
      <td>DI</td>
      <td>Motr data integrity component.
      </td>
  </tr>

  <tr>
      <td>DIX</td>
      <td>Distributed index or distributed index module.
      </td>
  </tr>

  <tr>
      <td>Distributable Function (or distributable computation)</td>
      <td>A function that conforms to a restricted model. This model allows the computation to be distributed and run in parallel on the storage cluster.
      </td>
  </tr>

  <tr>
      <td>Distributed Transaction Manager</td>
      <td>Distributed Transaction Manager is a component supporting distributed transactions, which are groups of file system updates that are guaranteed to be atomic w.r.t. failures.
      </td>
  </tr>

  <tr>
      <td>Extent</td>
      <td>Extent is used to describe a range of space, with "start" block number and "count" of blocks.
      </td>
  </tr>

  <tr>
      <td>FDMI</td>
      <td>File Data Manipulation Interface (FDMI) is a part of the Motr product and provides an interface for the Motr plugins. It horizontally extends the features and capabilities of the system.
      </td>
  </tr>

  <tr>
      <td>Fault Injection(FI)</td>
      <td>Fault injection API provides functions to set fault points inside the code, and functions to enable/disable an actuation of these points. It's aimed at increasing code coverage by enabling execution of error-handling code paths, which are not covered otherwise by unit tests.
      </td>
  </tr>

  <tr>
      <td>FID</td>
      <td>Any cluster entity including object, process, service, node, pools etc., are uniquely identified by an identifier.
      </td>
  </tr>

  <tr>
      <td>FOL</td>
      <td>File Operation Log. A FOL is a central Motr data-structure, maintained by every node where Motr core is deployed.
      </td>
  </tr>

  <tr>
      <td>FOL Record</td>
      <td>A FOL record describes a storage operation. A FOL record describes a complete storage operation, that is, a change to a storage system state that preserves state consistency.
      </td>
  </tr>

  <tr>
      <td>FOP</td>
      <td>File Operation Packet. FOP is description of file system state modification that can be passed across the network and stored in a data-base.
      </td>
  </tr>

  <tr>
      <td>FOP field instance</td>
      <td>A data value in a fop instance, corresponding to a fop field in instance's format is called a fop field instance.
      </td>
  </tr>

  <tr>
      <td>Fop State Machine (FOM)</td>
      <td>FOM is a state machine that represents current state of the FOP's execution on a node. FOM is associated with the particular FOP and implicitly includes this FOP as part of its state.
      </td>
  </tr>

  <tr>
      <td>Global object</td>
      <td>Components of a global object are normally located on the servers of the same pool. For example, during the migration, a global object can have a more complex layout with the components scattered across multiple pools.
      </td>
  </tr>

  <tr>
      <td>Global object data</td>
      <td>Object linear namespace called global object data.</td>
  </tr>

  <tr>
      <td>Global object layout</td>
      <td>A global object layout specifies a location of its data or redundancy information as a pair (component-id, component-offset). The component-id is the FID of a component stored in a certain container. (On the components FIDs, the layouts are introduced for the provided global object. But these are not important for the present specification.)
      </td>
  </tr>

  <tr>
      <td>HA</td>
      <td>CORTX-HA or High-Availability always ensures the availability of CORTX and prevents hardware component or software service failures. If any of your hardware components or software services are affected, CORTX-HA takes over the failover or failback control flow and stabilizes them across the CORTX cluster.
      </td>
  </tr>

  <tr>
      <td>HA interface</td>
      <td>HA interface API that allows Hare to control Motr and allows Motr to receive cluster state information from Hare.
      </td>
  </tr>

  <tr>
      <td>Hare</td>
      <td>HA component for Motr which: 1) generates configuration, 2) manages Motr processes startup, and 3) notifies Motr of device or processes failures. Uses Consul for consensus, key-value store and processes health-checking.
      </td>
  </tr>

  <tr>
      <td>Hierarchical Storage Management (HSM)</td>
      <td>Hierarchical Storage Management generalizes the concept of data caching. Different storage technologies are organized hierarchically in the tiers from fastest to the slowest.
      </td>
  </tr>

  <tr>
      <td>Index</td>
      <td>A distributed index, or simply index is an ordered container of key-value records.
      </td>
  </tr>

  <tr>
      <td>Key order</td>
      <td>Total order, defined on keys within a given container. Iterating through the container, returns keys in this order. The order is defined as lexicographical order of keys, interpreted as bit-strings.
      </td>
  </tr>

  <tr>
      <td>Layout</td>
      <td>A layout is a map determining where file data and meta-data are located. The layout is by itself a piece of meta-data and has to be stored somewhere.
      </td>
  </tr>

  <tr>
      <td>Layout identifier</td>
      <td>A layout is identified by layout identifier uniquely.
      </td>
  </tr>

  <tr>
      <td>Layout schema</td>
      <td>A layout schema is a way to store the layout information in data base. The schema describes the organization for the layout meta-data.
      </td>
  </tr>

  <tr>
      <td>Page size</td>
      <td>Page Size is a multiple of block size (it follows that stob size is a multiple of page size). At a given moment in time, some pages are up-to-date (their contents is the same as of the corresponding stob blocks) and some are dirty (their contents was modified relative to the stob blocks). In the initial implementation all pages are up-to-date, when the segment is opened. In the later versions, pages will be loaded dynamically on demand. The memory extent to which a segment is mapped is called segment memory.
      </td>
  </tr>

  <tr>
      <td>Parity group</td>
      <td>A parity group is a collection of data units and their parity units. We only consider layouts where data units of a parity group are contiguous in the source. We do consider layouts where units of a parity group are not contiguous in the target (parity declustering). Layouts of N+K pattern allow data in a parity group to be reconstructed when no more than K units of the parity group are missing.
      </td>
  </tr>

  <tr>
      <td>Parity unit</td>
      <td>A striping unit to which redundancy information is mapped is called a parity unit (this standard term will be used even though the redundancy information might be something different than parity).
      </td>
  </tr>

  <tr>
      <td>Pool</td>
      <td>A pool is a collection of storage, communication, and computational resources (server nodes, storage devices, and network interconnects) configured to provide IO services with certain fault-tolerance characteristics. Specifically, global objects are stored in the pool with striping layouts with such striping patterns that guarantee that data are accessible after a certain number of server and storage device failures. Additionally, pools guarantee (using the SNS repair described in this specification) that a failure is repaired in a certain time.
      </td>
  </tr>

  <tr>
      <td>Region</td>
      <td>A region is an extent within segment memory. A (meta-data) update is a modification of some region.
      </td>
  </tr>

  <tr>
      <td>Resource</td>
      <td>A resource is part of system or its environment for which a notion of ownership is well-defined.
      </td>
  </tr>

  <tr>
      <td>Resource Manager (rm)</td>
      <td>Resource Manager (rm) cooperates with request handler to determine which operations should be run locally and which should be delegated to remote Motr instances.
      </td>
  </tr>

  <tr>
      <td>RPC</td>
      <td>RPC is a container for FOPs and other auxiliary data. For example, ADDB records are placed in RPCs alongside with FOPs.
      </td>
  </tr>

  <tr>
      <td>SNS</td>
      <td>Server Network Striping
      </td>
  </tr>

  <tr>
      <td>Segment</td>
      <td>A segment is a stob mapped to an extent in process address space. Each address in the extent uniquely corresponds to the offset in the stob and vice versa. Stob is divided into blocks of fixed size.
      </td>
  </tr>

  <tr>
      <td>Storage object (stob)</td>
      <td>A storage object (stob) is a container for unstructured data, accessible through m0_stob interface. Back-End (BE) uses stobs to store meta-data on persistent store. BE accesses persistent store only through m0_stob interface and assumes that every completed stob write survives any node failure.
      </td>
  </tr>

  <tr>
      <td>Striping pattern</td>
      <td>A striping layout belongs to a striping pattern (N+K)/P; if it stores the K parity units with the redundancy information for every N data units and the units are stored in P containers. Typically P is equal to the number of storage devices in the pool. Where P is not important or clear from the context, one talks about N+K striping pattern (which coincides with the standard RAID terminology).
      </td>
  </tr>

  <tr>
      <td>Storage devices</td>
      <td>Attached to data servers.
      </td>
  </tr>

  <tr>
      <td>Xcode</td>
      <td>Xcode in Motr is the protocol to encode/decode (serialise) in-memory (binary) data structures.
      </td>
  </tr>

  <tr>
      <td>Unit Test (UT)</td>
      <td>UT is a way of testing a smallest piece of code that can be logically isolated in a system.
      </td>
  </tr>
</table>
