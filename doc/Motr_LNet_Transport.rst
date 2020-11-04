==============================
HLD OF Motr LNet Transport
==============================

This document presents a high level design (HLD) of the Motr LNet Transport. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

***************
Introduction
***************

The scope of this HLD includes the net.lnet-user and net.lnet-kernel tasks described in [1]. Portions of the design are influenced by [4].

***************
Definitions
***************

- Network Buffer: This term is used to refer to a struct M0_net_buffer. The word “buffer”, if used by itself, will be qualified by its context - it may not always refer to a network buffer.

- Network Buffer Vector: This term is used to refer to the struct M0_bufvec that is embedded in a network buffer. The related term, “I/O vector” if used, will be qualified by its context - it may not always refer to a network buffer vector.

- Event queue, EQ LNet: A data structure used to receive LNet events. Associated with an MD. The Lustre Networking module. It implements version 3.2 of the Portals Message Passing Interface, and provides access to a number of different transport protocols including InfiniBand and TCP over Ethernet.

- LNet address: This is composed of (NID, PID, Portal Number, Match bits, offset). The NID specifies a network interface end point on a host, the PID identifies a process on that host, the Portal Number identifies an opening in the address space of that process, the Match Bits identify a memory region in that opening, and the offset identifies a relative position in that memory region.

- LNet API: The LNet Application Programming Interface. This API is provided in the kernel and implicitly defines a PID with value LUSTRE_SRV_LNET_PID, representing the kernel. Also see ULA.

- LNetNetworkIdentifierString: The external string representation of an LNet Network Identifier (NID). It is typically expressed as a string of the form “Address@InterfaceType[Number]” where the number is used if there are multiple instances of the type, or plans to configure more than one interface in the future. e.g. “10.67.75.100@o2ib0”.

- Match bits: An unsigned 64 bit integer used to identify a memory region within the address space defined by a Portal. Every local memory region that will be remotely accessed through a Portal must either be matched exactly by the remote request, or wildcard matched after masking off specific bits specified on the local side when configuring the memory region.

- Memory Descriptor, MD: An LNet data structure identifying a memory region and an EQ.

- Match Entry, ME: An LNet data structure identifying an MD and a set of match criteria, including Match bits. Associated with a portal.

- NID, lnet_nid_t: Network Identifier portion of an LNet address, identifying a network end point. There can be multiple NIDs defined for a host, one per network interface on the host that is used by LNet. A NID is represented internally by an unsigned 64 bit integer, with the upper 32 bits identifying the network and the lower 32 bits the address. The network portion itself is composed of an upper 16 bit network interface type and the lower 16 bits identify an instance of that type. See LNetNetworkIdentifierString for the external representation of a NID.

- PID, lnet_pid_t: Process identifier portion of an LNet address. This is represented internally by a 32 bit unsigned integer. LNet assigns the kernel a PID of LUSTRE_SRV_LNET_PID (12345) when the module gets configured. This should not be confused with the operating system process identifier space which is unrelated.

- Portal Number: This is an unsigned integer that identifies an opening in a process address space. The process can associate multiple memory regions with the portal, each region identified by a unique set of Match bits. LNet allows up to MAX_PORTALS portals per process (64 with the Lustre 2.0 release)

- Portals Messages Passing Interface: An RDMA based specification that supports direct access to application memory. LNet adheres to version 3.2 of the specification.

- RDMA ULA: A port of the LNet API to user space, that communicates with LNet in the kernel using a private device driver. User space processes still share the same portal number space with the kernel, though their PIDs can be different. Event processing using the user space library is relatively expensive compared to direct kernel use of the LNet API, as an ioctl call is required to transfer each LNet event to user space. The user space LNet library is protected by the GNU Public License. ULA makes modifications to the LNet module in the kernel that have not yet, at the time of Lustre 2.0, been merged into the mainstream Lustre source repository. The changes are fully compatible with existing usage. The ULA code is currently in a Motr repository module.

- LNet Transport End Point Address: The design defines an LNet transport end point address to be a 4-tuple string in the format “LNETNetworkIdentifierString : PID : PortalNumber : TransferMachineIdentifier”. The TransferMachineIdentifier serves to distinguish between transfer machines sharing the same NID, PID and PortalNumber. The LNet Transport End Point Addresses concurrently in use on a host are distinct.

- Mapped memory page A memory page (struct page): that has been pinned in memory using the get_user_pages subroutine.

- Receive Network Buffer Pool: This is a pool of network buffers, shared between several transfer machines. This common pool reduces the fragmentation of the cache of receive buffers in a network domain that would arise were each transfer machine to be individually provisioned with receive buffers. The actual staging and management of network buffers in the pool is provided through the [r.M0.net.network-buffer-pool] dependency.

- Transfer Machine Identifier: This is an unsigned integer that is a component of the end point address of a transfer machine. The number identifies a unique instance of a transfer machine in the set of addresses that use the same 3-tuple of NID, PID and Portal Number. The transfer machine identifier is related to a portion of the Match bits address space in an LNet address - i.e. it is used in the ME associated with the receive queue of the transfer machine.

Refer to [3], [5] and to net/net.h in the Motr source tree, for additional terms and definitions.

***************
Requirements
***************

- [r.M0.net.rdma] Remote DMA is supported. [2]

- [r.M0.net.ib] Infiniband is supported. [2] 

- [r.M0.net.xprt.lnet.kernel] Create an LNET transport in the kernel. [1] 

- [r.M0.net.xprt.lnet.user] Create an LNET transport for user space. [1]

- [r.M0.net.xprt.lnet.user.multi-process] Multiple user space processes can concurrently use the LNet transport. [1]

- [r.M0.net.xprt.lnet.user.no-gpl] Do not get tainted with the use of GPL interfaces in the user space implementation. [1]

- [r.M0.net.xprt.lnet.user.min-syscalls] Minimize the number of system calls required by the user space transport. [1]

- [r.M0.net.xprt.lnet.min-buffer-vm-setup] Minimize the amount of virtual memory setup required for network buffers in the user space transport. [1]

- [r.M0.net.xprt.lnet.processor-affinity] Provide optimizations based on processor affinity.

- [r.M0.net.buffer-event-delivery-control] Provide control over the detection and delivery of network buffer events.

- [r.M0.net.xprt.lnet.buffer-registration] Provide support for hardware optimization through buffer pre-registration.

- [r.M0.net.xprt.auto-provisioned-receive-buffer-pool] Provide support for a pool of network buffers from which transfer machines can automatically be provisioned with receive buffers. Multiple transfer machines can share the same pool, but each transfer machine is only associated with a single pool. There can be multiple pools in a network domain, but a pool cannot span multiple network domains.

******************
Design Highlights
******************

The following figure shows the components of the proposed design and usage relationships between it and other related components:

.. image:: Images/LNET.PNG

- The design provides an LNet based transport for the Motr Network Layer, that co-exists with the concurrent use of LNet by Lustre. In the figure, the transport is labelled M0_lnet_u in user space and M0_lnet_k in the kernel.

- The user space transport does not use ULA to avoid GPL tainting. Instead it uses a proprietary device driver, labelled M0_lnet_dd in the figure, to communicate with the kernel transport module through private interfaces.

- Each transfer machine is assigned an end point address that directly identifies the NID, PID and Portal Number portion of an LNet address, and a transfer machine identifier. The design will support multiple transfer machines for a given 3-tuple of NID, PID and Portal Number. It is the responsibility of higher level software to make network address assignments to Motr components such as servers and command line utilities, and how clients are provided these addresses.

- The design provides transport independent support to automatically provision the receive queues of transfer machines on demand, from pools of unused, registered, network buffers. This results in greater utilization of receive buffers, as fragmentation of the available buffer space is reduced by delaying the commitment of attaching a buffer to specific transfer machines.

- The design supports the reception of multiple messages into a single network buffer. Events will be delivered for each message serially.

- The design addresses the overhead of communication between user space and kernel space. In particular, shared memory is used as much as possible, and each context switch involves more than one operation or event if possible.

- The design allows an application to specify processor affinity for a transfer machine.

- The design allows an application to control how and when buffer event delivery takes place. This is of particular interest to the user space request handler.

****************************
Functional Specification
****************************

The design follows the existing specification of the Motr Network module described in net/net.h and [5] for the most part. See the Logical Specification for reasons behind the features described in the functional specification.

LNet Transfer Machine End Point Address
========================================

The Motr LNet transport defines the following 4-tuple end point address format for transfer machines:

- NetworkIdentifierString : PID : PortalNumber : TransferMachineIdentifier

where the NetworkIdentifierString (a NID string), the PID and the Portal Number are as defined in an LNet Address. The TransferMachineIdentifier is defined in the definition section.

Every Motr service request handler, client and utility program needs a set of unique end point addresses. This requirement is not unique to the LNet transport: an end point address is in general pattern

- TransportAddress : TransferMachineIdentifier

with the transfer machine identifier component further qualifying the transport address portion, resulting in a unique end point address per transfer machine. The existing bulk emulation transports use the same pattern, though they use a 2-tuple transport address and call the transfer machine identifier component a “service id” [5]. Furthermore, there is a strong relationship between a TransferMachineIdentifier and a FOP state machine locality [6] which needs further investigation. These issues are beyond the scope of this document and are captured in the [r.M0.net.xprt.lnet.address-assignment] dependency.

The TransferMachineIdentifier is represented in an LNet ME by a portion of the higher order Match bits that form a complete LNet address. See Mapping of Endpoint Address to LNet Address for details.

All fields in the end point address must be specified. For example:

- 10.72.49.14@o2ib0:12345:31:0

- 192.168.96.128@tcp1:12345:32:0

The implementation should provide support to make it easy to dynamically assign an available transfer machine identifier by specifying a * (asterisk) character as the transfer machine component of the end point addressed passed to the M0_net_tm_start subroutine:

- 10.72.49.14@o2ib0:12345:31:*

If the call succeeds, the real address assigned by be recovered from the transfer machine’s ntm_ep field. This is captured in refinement [r.M0.net.xprt.lnet.dynamic-address-assignment].

Transport Variable
------------------

The design requires the implementation to expose the following variable in user and kernel space through the header file net/lnet.h:

- extern struct M0_net_xprt M0_lnet_xprt;

The variable represents the LNet transport module, and its address should be passed to the M0_net_domain_init() subroutine to create a network domain that uses this transport. This is captured in the refinement [r.M0.net.xprt.lnet.transport-variable].

**Support for automatic provisioning from receive buffer pools**

The design includes support for the use of pools of network buffers that will be used to receive messages from one or more transfer machines associated with each pool. This results in greater utilization of receive buffers, as fragmentation is reduced by delaying the commitment of attaching a buffer to specific transfer machines. This results in transfer machines performing on-demand, minimal, policy-based provisioning of their receive queues. This support is transport independent, and hence, can apply to the earlier bulk emulation transports in addition to the LNet transport.

The design uses the struct M0_net_buffer_pool object to group network buffers into a pool. New APIs will be added to associate a network buffer pool with a transfer machine, to control the number of buffers the transfer machine will auto-provision from the pool, and additional fields will be added to the transfer machine and network buffer data structures.

The M0_net_tm_pool_attach() subroutine assigns the transfer machine a buffer pool in the same domain. A buffer pool can only be attached before the transfer machine is started. A given buffer pool can be attached to more than one transfer machine, but each transfer machine can only have an association with a single buffer pool. The life span of the buffer pool must exceed that of all associated transfer machines. Once a buffer pool has been attached to a transfer machine, the transfer machine implementation will obtain network buffers from the pool to populate its M0_NET_QT_ACTIVE_BULK_RECV queue on an as-needed basis [r.M0.net.xprt.support-for-auto-provisioned-receive-queue].

The application provided buffer operation completion callbacks are defined by the callbacks argument of the attach subroutine - only the receive queue callback is used in this case. When the application callback is invoked upon receipt of a message, it is up to the application callback to determine whether to return the network buffer to the pool (identified by the network buffer’s nb_pool field) or not. The application should make sure that network buffers with the M0_NET_BUF_QUEUED flag set are not released back to the pool - this flag would be set in situations where there is sufficient space left in the network buffer for additional messages. See Requesting multiple message delivery in a single network buffer for details.

When a transfer machine is stopped or fails, receive buffers that have been provisioned from a buffer pool will be put back into that pool by the time the state change event is delivered.

The M0_net_tm_pool_length_set() subroutine is used to set the policy for the number of buffers the that will automatically be added to a transfer machine’s receive queue. The default value of 2 (M0_NET_TM_RECV_QUEUE_DEF_LEN) should be raised only if the transfer machine concerned is expected to have a very high temporal density of messages; reducing the value to 1 runs the risk of dropping messages when the active network buffer gets filled; zero is disallowed. If the length is reduced, the transfer machine will not immediately de-queue buffers it has already queued, but will allow the queue to drain as buffers are used up; auto-provisioning will not recommence until the queue length drops below the new size.

The M0_net_domain_buffer_pool_not_empty() subroutine should be used, directly or indirectly, as the “not-empty” callback of a network buffer pool. We recommend direct use of this callback - i.e. the buffer pool is dedicated for receive buffers provisioning purposes only.

Mixing automatic provisioning and manual provisioning in a given transfer machine is not recommended, mainly because the application would have to support two buffer release mechanisms for the automatic and manually provisioned network buffers, which may get confusing. See Automatic provisioning of receive buffers for details on how automatic provisioning works.


