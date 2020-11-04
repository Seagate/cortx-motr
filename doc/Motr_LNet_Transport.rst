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
