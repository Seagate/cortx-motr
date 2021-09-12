/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */


/**
 * @addtogroup netsock
 *
 * Overview
 * --------
 *
 * net/sock.[ch] contains an implementation of the interfaces defined in
 * net/net.h. Together these interfaces define a network transport layer used by
 * motr. A network transport provides unreliable, unordered, asynchronous
 * point-to-point messaging interface.
 *
 * The rpc layer (rpc/) is the only user of network transport, it provides a
 * higher level interface that the rest of motr uses.
 *
 * The documentation uses "io" as a verb, e.g., "X bytes were ioed".
 *
 * Main abstractions provided by a network transport are:
 *
 *     - an end-point (m0_net_end_point) identifies a network peer. Messages are
 *       sent to and received from peers. An end-points has a string name. The
 *       name syntax depends on the transport, see addr_parse().
 *
 *     - a network buffer (m0_net_buffer) represents an operation of
 *       transferring data between a local memory buffer, specified by
 *       m0_net_buffer::nb_buffer bufvec, and a remote buffer located in the
 *       address space of a peer across network and associated with a network
 *       buffer on that peer.
 *
 *     - a network transfer machine (m0_net_transfer_mc) represents the local
 *       network end-point. It keeps track of buffers being sent to or received
 *       from other peers.
 *
 * A network buffer is "added" (also referred to as "queued") to a transfer
 * machine (m0_net_buffer_add()) to initiate the operation. When the transfer
 * operation completes a per-buffer completion call-back
 * (m0_net_buffer::nb_callbacks) specified by the user is invoked and the buffer
 * is removed from the transfer machine queue (there is a special mode, when
 * multiple transfer operations and multiple call-back can be served for the
 * same network buffer, see M0_NET_BUF_RETAIN). "Transfer operation completion"
 * here means that the network transfer is done with the operation and won't
 * touch the local buffer memory again. Completion doesn't guarantee that the
 * peer has seen the buffer data.
 *
 * There are multiple transfer operation types corresponding to queues within a
 * transfer machine (m0_net_transfer_mc::ntm_q[]) and identified by
 * m0_net_buffer::nb_qtype field taken from enum m0_net_queue_type.
 *
 * M0_NET_QT_MSG_RECV and M0_NET_QT_MSG_SEND operation types correspond to the
 * simplest form of messaging:
 *
 *     - to send a message, the network transport user places the message in the
 *       memory buffer associated with a network buffer and then adds the buffer
 *       to the M0_NET_QT_MSG_SEND queue;
 *
 *     - to receive messages, the user adds some number of buffers to the
 *       M0_NET_QT_MSG_RECV queue. When a data sent via M0_NET_QT_MSG_SEND
 *       operation are received, a buffer from the M0_NET_QT_MSG_RECV queue is
 *       selected and the data are placed in the associated memory buffer. If no
 *       receive buffers are available, the incoming data are dropped with a
 *       error message. See Provisioning section below.
 *
 * Other operation types are for so-called "bulk" operations which are efficient
 * for transfers of large amounts of data. They are based on "network buffer
 * descriptors" (m0_net_buf_desc). A network buffer descriptor is a
 * transport-specific datum (opaque to the upper layers) which uniquely
 * identifies a particular buffer within its local peer. Each bulk operation
 * involves 2 network buffers: one passive and one active. When the passive
 * buffer is added to a transfer machine, its buffer descriptor is generated and
 * stored in m0_net_buffer::nb_desc. The user then somehow (e.g., by means of
 * the simple messaging, described above) transmits this descriptor to the other
 * peer. At that peer an active buffer is prepared, the received descriptor is
 * stored in the buffer ->nb_desc field and the buffer is added to the active
 * queue. At that moment data transfer is initiated between these two
 * buffers. The direction of transfer is independent from which buffer is
 * active, which gives 4 queues (M0_NET_QT_PASSIVE_BULK_RECV,
 * M0_NET_QT_PASSIVE_BULK_SEND, M0_NET_QT_ACTIVE_BULK_RECV and
 * M0_NET_QT_ACTIVE_BULK_SEND), which can be used as following:
 *
 *     - buffer A: passive-recv, buffer B: active-send: the descriptor is moved
 *       from A to B, the data are moved from B to A;
 *
 *     - buffer A: passive-send, buffer B: active-recv: the descriptor is moved
 *       from A to B, the data are moved from A to B;
 *
 * Bulk transfers are exported to the upper layers (io client and io service)
 * via rpc-bulk interface (rpc/bulk.h).
 *
 * sock network transport is implemented on top of the standard bsd socket
 * interface. It is user-space only.
 *
 * Data structures
 * ---------------
 *
 * sock.c introduces data-structures corresponding to the abstractions in net.h:
 *
 *     - struct ep: end-point. It embeds generic m0_net_end_point as e::e_ep;
 *
 *     - struct buf: network buffer. It links to the corresponding m0_net_buffer
 *       through buf::b_buf. Generic buffer points back to buf via
 *       m0_net_buffer::nb_xprt_private;
 *
 *     - struct ma: transfer machine (because struct tm is defined in
 *       <time.h>). It links to the corresponding m0_net_transfer_mc through
 *       ma::t_ma. Generic transfer machine points back to ma via
 *       m0_net_transfer_mc::ntm_xprt_private;
 *
 * Additional data-structures:
 *
 *     - struct addr: network address;
 *
 *     - struct sock: represents a socket---a connection between two
 *       end-points. A sock is a state machine with states taken from enum
 *       sock_state;
 *
 *     - struct packet. Data are transferred through network in units called
 *       "packets". A packet has a header that identifies its source and target
 *       buffers, length, offset, etc.;
 *
 *     - struct mover: a state machine copying data between a socket and a
 *       buffer. A mover is a state machine.
 *
 * A transfer machine keeps a list of end-points
 * (m0_net_transfer_mc::ntm_end_points). An end-point keeps a list of sockets
 * connected to the peer represented by this end-point (ep::e_sock).
 *
 * A mover can be either a reader (reading data from a socket and writing them
 * to a buffer) or a writer (reading from a buffer and writing to a socket).
 *
 * A reader is associated with every socket (sock::s_reader). It reads incoming
 * packets and copies them to the appropriate buffers. This reader is
 * initialised and finalised together with the socket (sock_init(),
 * sock_fini()).
 *
 * A writer is associated with every M0_NET_QT_MSG_SEND and every
 * M0_NET_QT_ACTIVE_BULK_SEND buffer (buf::b_writer). This writer is initialised
 * when the buffer is queued (buf_add()) and finalised when operation completes.
 * As described in the Protocol section below, a writer is usually associated
 * with a M0_NET_QT_PASSIVE_BULK_SEND buffer as part of the bulk protocol.
 *
 * An end-point keeps a list of all writers writing data to its sockets
 * (ep::e_writer). Note that a writer is associated with an end-point rather
 * than a particular socket to this end-point. This allows, in principle, use of
 * multiple sockets to write the same buffer in parallel. While writing a
 * particular packet, the writer and the socket are "locked" together and the
 * socket cannot be used to write other packets (because doing so would make it
 * impossible to parse packets at the other end). While locked, the writer
 * mover::m_sock points to the socket (see sock_writer()).
 *
 * An address uniquely identifies an end-point in the network. An end-point
 * embeds its address (ep::e_a). An address has address family independent part
 * (address family, socket type, protocol and port, all in processor byte order)
 * and address family specific part (addr::a_data). The latter contains the
 * appropriate in{,6}_addr struct in network byte order. An addr is encoded to a
 * sockaddr (addr_encode()) before calling socket functions (like bind(2),
 * connect(2), inet_ntop(3)). An addr is recovered (addr_decode()) from sockaddr
 * returned by accept4(2).
 *
 * States and transitions
 * ----------------------
 *
 * sock uses non-blocking socket operations through linux epoll(2) interface (a
 * switch to poll(2) or select(2) can be made easily).
 *
 * There are two types of activity in sock module:
 *
 *     - "synchronous" that is initiated by net/net.h entry-points being called
 *       by a user and
 *
 *     - "asynchronous" initiated by network related events, such as incoming
 *       connections or sockets being ready for non-blocking io.
 *
 * Synchronous activities.
 *
 * A buffer is added to a transfer machine
 * (m0_net_buf_add()->...->buf_add()).
 *
 * If the buffer is added to M0_NET_QT_MSG_RECV, M0_NET_QT_PASSIVE_BULK_SEND or
 * M0_NET_QT_PASSIVE_BULK_RECV queue it is just placed on the queue, waiting for
 * the incoming data from a matching buffer.
 *
 * If the buffer is queued to M0_NET_QT_MSG_SEND or M0_NET_QT_ACTIVE_BULK_SEND,
 * its writer is initialised and this writer is added to the target end-point
 * (ep_add()). If necessary, this initiates the asynchronous creation of a
 * socket toward that end-point (non-blocking connect(2)):
 * ep_balance()->sock_init().
 *
 * The remaining queuing mode --M0_NET_QT_ACTIVE_BULK_RECV-- is curious: it
 * indicates that our side should initiate transfer of data from the remote
 * M0_NET_QT_PASSIVE_BULK_RECV buffer to our buffer. There is no way to do this
 * but by sending a special message to the remote side, see GET packet
 * description in the Protocol section. buf_add() initialises the buffer writer
 * to send a GET packet and adds it to the target end-point (ep_add()) as in the
 * previous case.
 *
 * Asynchronous activities.
 *
 * When a transfer machine is started, an end-point, representing the local
 * peer, is created. Then a socket to this end-point is created. This is a
 * "listening socket": listen(2) is called on it and it becomes readable (in
 * epoll sense) when a new incoming connection arrives. This socket forever
 * stays in S_LISTENING mode.
 *
 * The starting point of asynchronous activity associated with a sock transfer
 * machine is poller(). Currently, this function is ran as a separate thread,
 * but it can easily be adapted to be executed as a chore
 * (m0_locality_chore_init()) within a locality.
 *
 * poller() gets from epoll_wait(2) a list of readable and writable sockets and
 * calls sock_event(), which is socket state machine transition
 * function. sock_event() handles following cases:
 *
 *     - an incoming connection (the S_LISTENING socket becomes readable):
 *       accept the connection by calling accept4(2), create the end-point for
 *       the remote peer, create the sock for the connection and initialise its
 *       reader;
 *
 *     - outgoing connection is established (an S_CONNECTING socket becomes
 *       writable): this happens when a non-blocking connect(2) completes. Moves
 *       the socket to S_ACTIVE state;
 *
 *     - incoming data on an S_ACTIVE socket: call socket reader (sock_in()) to
 *       handle arrived data;
 *
 *     - a non-blocking write on an S_ACTIVE socket is possible (that is, some
 *       space in the socket in-kernel buffer is available): call writers
 *       associated with the socket end-point (sock_out()) to write to the
 *       socket;
 *
 *     - an incoming GET packet for an M0_NET_QT_PASSIVE_BULK_SEND buffer. The
 *       buffer writer is initialised and transfer of data to the target buffer
 *       starts (pk_header_done());
 *
 *     - an error event is raised for a socket, or a socket is closed by the
 *       peer (sock_close()): close the socket.
 *
 * What happens here is that there is a collection of movers (readers and
 * writers) associated with end-points, sockets and buffers, and their state
 * machines are advanced when non-blocking io is possible.
 *
 * Readers and writers are state machines and they have the same set of states
 * (enum rw_state) and the graph of state transitions (rw_conf_state[],
 * rw_conf_trans[]), because, after all, they are just data-copying loops:
 *
 *     - R_IDLE: initial state. Here readers wait for incoming data;
 *
 *     - R_PK: here processing (input or output) of the next packet starts;
 *
 *     - R_HEADER: in this state packet header is ioed. This state loops until
 *       the entire header is ioed;
 *
 *     - R_INTERVAL: once the header has been ioed, the packet payload is
 *       ioed. The payload is split in a number of "intervals", where each
 *       interval is ioed without blocking. This state loops until the entire
 *       payload is ioed;
 *
 *     - R_PK_DONE: packet completed. For a writer: switch to the next packet,
 *       if available and go to R_PK. If all packets have been written, complete
 *       the buffer (buf_done()) and go to R_DONE. For a reader, go back to
 *       R_IDLE.
 *
 *     - R_DONE: writer completes.
 *
 * The differences between various movers are represented by mover_op_vec
 * structure. It contains functions called when the mover reaches a particular
 * state, see mover_op(). All writers use the same op_vec: writer_op, readers
 * use stream_reader_op or dgram_reader_op depending on the socket type. Writers
 * for GET packets (see Protocol section) use get_op.
 *
 * Protocol
 * --------
 *
 * Network communication over sockets happen in the form of "packets". There are
 * 2 types of packets:
 *
 *     - PUT packet contains data to be copied from the source buffer to the
 *       target buffer. A PUT packet is sent from the peer containing the source
 *       buffer to the peer containing the target buffer;
 *
 *     - GET packet is a request to initiate transfer of the source buffer to
 *       the target buffer. It is sent to the peer containing the source buffer.
 *
 * A packet on the wire starts with the header (struct packet). The header
 * identifies the source buffer, the target buffer and, for a PUT packet,
 * identifies which part of the source buffer is transmitted in this packet.
 *
 * In a PUT packet, the header is followed by the payload data. The header
 * specifies the total number of packets used to transfer this buffeer, the
 * starting offset, size and index of this packet and the total buffer size. The
 * receiver uses these data to verify the header (pk_header_done(),
 * buf_accept()), to update the current state of the copy operation and to
 * detect operation completion (pk_done()). PUT packets for the same buffer can
 * be sent out of order and through different sockets.
 *
 * A GET packet has no payload. GET packets are used to initiate passive-send ->
 * active-recv bulk transfers (see above). Note that a GET packet is *not*
 * necessarily sent from the target buffer's peer: it can be sent from a 3rd
 * peer to command 2 peers to transfer data between their buffers.
 *
 * A buffer (either source of target) is identified in the packet by struct
 * bdesc structure, which contains peer address (struct addr) and buffer cookie
 * (see lib/cookie.h).
 *
 * Concurrency
 * -----------
 *
 * sock module uses a very simple locking model: all state transitions are
 * protected by a per-tm mutex: m0_net_transfer_mc::ntm_mutex. For synchronous
 * activity, this mutex is taken by the entry-point code in net/ and is not
 * released until the entry-point completes. For asynchronous activity, poller()
 * keeps the lock taken most of the time.
 *
 * A few items related to concurrency worth mentioning:
 *
 *     - the tm lock is not held while epoll_wait() is executed by poller(). On
 *       the other hand, the pointer to a sock structure is stored inside of
 *       epoll kernel-state (sock_ctl()). This means that sock structures cannot
 *       be freed in a synchronous context, lest the epoll-stored state points
 *       to an invalid memory region. To deal with this, a sock is not freed
 *       immediately. Instead it is moved to S_DELETED state and placed on a
 *       special per-tm list: ma::t_deathrow. Actual freeing is done by
 *       ma_prune() called from poller();
 *
 *     - buffer completion (buf_done()) includes removing the buffer from its
 *       queue and invoking a user-supplied call-back
 *       (m0_net_buffer::nb_callbacks). Completion can happen both
 *       asynchronously (when a transfer operation completes for the buffer or
 *       the buffer times out (ma_buf_timeout())) and synchronously (when the
 *       buffer is canceled by the user (m0_net_buf_del()->...->buf_del()). The
 *       difficulty is that net.h interface specifies that the transfer machine
 *       lock is to be released before invoking the call-back. This cannot be
 *       done in a synchronous context (to avoid breaking invariants), so in
 *       this case the buffer is queued to a special ma::t_done queue which is
 *       processed asynchronously by ma_buf_done().
 *
 * Socket interface use
 * --------------------
 *
 * sock module uses standard bsd socket interface, but
 *
 *     - accept4(2), a linux-specific system call, is used instead of accept(2)
 *       to set SOCK_NONBLOCK option on the new socket, eliminating the cost of
 *       a separate setsockopt(2) call, see sock_event(). Switch to accept(2) is
 *       trivial;
 *
 *     - similarly, a non-portable extension to socket(2) is used to set
 *       SOCK_NONBLOCK in socket(2), see sock_init_fd();
 *
 *     - bsd Reno "len" fields in socket address structures are optionally used;
 *
 *     - ipv4 and ipv6 protocol families are supported. Unix domain sockets are
 *       not supported.
 *
 * When a socket is created, it is added to the epoll instance monitored by
 * poller() (sock_init_fd()). All sockets are monitored for read events. Only
 * sockets to end-points with a non-empty list of writers are monitored for
 * writes (ep_balance()).
 *
 * @todo It is not clear how to manage write-monitoring in case of multiple
 * "parallel" sockets to the same end-point. If writeability of all such sockets
 * is monitored, epoll_wait() can busy-loop. If not all of them are monitored,
 * they are useless for concurrent writes.
 *
 * Buffer data are transmitted as a collection of PUT packets. For each packet,
 * first the header is transmitted, then the payload. The payload is transmitted
 * as a sequence of "intervals" (see above). This design is needed to
 * accommodate both stream (IPPROTO_TCP, SOCK_STREAM) and datagram (IPPROTO_UDP,
 * SOCK_DGRAM) sockets within the same mover state machine:
 *
 *     - for stream sockets, packet size is equal to the buffer data size
 *       (stream_pk_size()), that is, the entire buffer is transmitted as a
 *       single packet, consisting of multiple intervals. Note, that it is not
 *       required that the entire header is written in one write;
 *
 *     - for datagram sockets, packet size is equal to the maximal datagram size
 *       (dgram_pk_size()). The buffer is transmitted as a sequence of
 *       packets. Each packet (both header and payload) is transmitted as a
 *       single datagram (i.e., one interval per packet).
 *
 * In the write path, the differences stream of datagram sockets are hidden in
 * pk_io_prep() and pk_io() that track how much of the packet has been ioed.
 *
 * In the read path, the differences cannot be hidden, because the header must
 * be read and parsed to understand to which buffer the incoming data should be
 * placed. For a stream socket, a reader reads the header (stream_header())
 * parses it (pk_header_done()) and then reads payload directly into the target
 * buffer (stream_interval(), pk_io()).
 *
 * For a datagram socket, the packet header and payload are read together as a
 * single datagram, hence the reader cannot read the data directly in the (yet
 * unknown) buffer. Instead, a per-reader temporary area (mover::m_scratch) is
 * allocated (dgram_pk()). The packet payload is read in this area, the header
 * is parsed and the data are copied in the target buffer (all done in
 * dgram_header()).
 *
 * When a socket to a remote end-point is opened, the kernel automatically
 * assigns a source address to it (the local ip address and the selected
 * ephemeral port). This address is returned from accept4(2) at the remote
 * peer. The remote peer and creates an end-point for it (sock_event()). Each
 * packet header contains the "real" (not ephemeral) source address, used by the
 * remote end to establish a connection in the opposite direction. As a result,
 * 2 peers exchanging data will use 2 sockets, one in each direction:
 *
 * @verbatim
 *
 *   +-----------------------+               +-----------------------+
 *   | peer-A, ip: A         |               | peer-B, ip: B         |
 *   +-----------------------+               +-----------------------+
 *   | tm: src address: A:p0 |               | tm: src address: B:p1 |
 *   |                       | sock: A -> B  |                       |
 *   |       A:eph0 ---------+---------------+---> B:p1              |
 *   |                       | sock: B -> A  |                       |
 *   |         A:p0 <--------+---------------+---- B:eph1            |
 *   |                       |               |                       |
 *   +-----------------------+               +-----------------------+
 *
 * @endverbatim
 *
 * Here eph0 and eph1 are ephemeral source ports assigned to by kernel.
 *
 * Data structures life-cycles
 * ---------------------------
 *
 * Transfer machine life-cycle:
 *
 *     - m0_net_tm_init() -> ... -> ma_init(): allocate ma data structure;
 *
 *     - m0_net_tm_start() -> ... -> ma_start(): start poller and initialise
 *       epoll;
 *
 *     - transfer machine is active;
 *
 *     - m0_net_tm_stop() -> ... -> ma_stop(): stop poller;
 *
 *     - m0_net_tm_fini() -> ... -> ma_fini().
 *
 * Buffer life-cycle:
 *
 *     - m0_net_buffer_register() -> ... -> buf_register(): register the buffer
 *       with its network transport. Once registered, a buffer can be used for
 *       multiple operartions. Registration is used by some transports (but not
 *       by sock currently) to do things like registering the buffer with a
 *       hardware rdma engine;
 *
 *     - m0_net_buffer_add() -> ... -> buf_add(): queue the buffer. If
 *       necessary, initialise buffer writer, generate buffer descriptor;
 *
 *     - operation is in progress: data are transferred to or from the buffer;
 *
 *     - normal operation completion: buf_done(), ma_buf_done(). User-provided
 *       call-back is invoked;
 *
 *     - alternatively: timeout (ma_buf_timeout());
 *
 *     - alternatively: cancellation: m0_net_buffer_del() -> ... -> buf_del();
 *
 *     - at this point the buffer is removed from the queue, but still
 *       registered. It can be re-queued;
 *
 *     - de-registration: m0_net_buffer_deregister() -> ... ->
 *       buf_deregister(). The buffer is finalised.
 *
 * End-point life-cycle.
 *
 * End-points are reference counted. Users can take references to the end-points
 * (m0_net_end_point_create() -> ... -> end_point_create()). An additional
 * reference is taken by each sock to this end-point (sock_init(), sock_fini())
 * and each buffer writing to this end-point (ep_add(), ep_del()).
 *
 * Sock life-cycle.
 *
 * A sock is allocated (sock_init()) when a transfer machine starts
 * (ma_start()), when an outgoing connection is established (ep_add() ->
 * ep_balance()) or when an incoming connection is received (sock_event()).
 *
 * sock is finalised (file descriptor closed, etc.) by sock_done() when the
 * transfer machine stops, when an error condition is detected on the socket, or
 * when an error happens in a mover locked to the socket (stream_error(),
 * writer_error()). The sock has to be closed in the latter case, because the
 * remote end won't to able to correctly parse data from other writers
 * anyway. The sock structure is not freed by sock_done(), see Concurrency
 * section.
 *
 * Mover life-cycle.
 *
 * Movers are embedded in buffers (writers) or socks (readers) and have no
 * independent life-cycle.
 *
 * Provisioning
 * ------------
 *
 * Bulk operations happen between buffers known to the user and it is up to the
 * user to allocate and queue both active and passive buffers.
 *
 * For non-bulk messaging (M0_NET_QT_MSG_RECV, M0_NET_QT_MSG_SEND), on the other
 * hand, the user has no direct control over the buffers selected for receiving
 * messages and cannot guarantee that the receive queue is not empty. To deal
 * with this, provisioning (net/tm_provisioning.[ch]) module tries to keep some
 * number of buffers on the receive queue at all times. Unfortunately, this
 * mechanism is not suitable for sock transport as is, see the comment in
 * tm_provision_recv_q().
 *
 * sock has its own provisioning, see the comment in pk_header_done().
 *
 * Limitations and options
 * -----------------------
 *
 * Only TCP sockets have been tested so far.
 *
 * Only 1 socket to a particular end-point is supported, that is, multiple
 * parallel sockets are not opened. Whether they are needed at all is an
 * interesting question.
 *
 * Once opened, a socket is never closed until an error or tm
 * finalisation. Sockets should perhaps be garbage collected after a period of
 * inactivity.
 *
 * Packets for a buffer are sent sequentially.
 *
 * rdma (ROCE or iWARP) is not supported.
 *
 * Multiple incoming messages in one buffer (see M0_NET_QT_MSG_RECV,
 * m0_net_buffer::nb_min_receive_size, m0_net_buffer::nb_max_receive_msgs) are
 * not supported.
 *
 * Differences with lnet
 * ---------------------
 *
 * sock is a faithful implementation of net.h transport protocol. Moreover it
 * goes out of its way to be lnet compatible: sock understands and supports lnet
 * format of end-point addresses (addr_parse_lnet()).
 *
 * Still, sock is not a drop-in replacement for lnet. The main compatibility
 * problem is due to buffer fragmentation limitations. Memory associated with a
 * buffer (m0_net_buffer::nb_buffer) is a scatter gather list (m0_bufvec) of
 * "segments" (a segment is a continuous memory buffer). A network transport can
 * can place some restrictions on:
 *
 *     - the total size of a memory buffer (m0_net_domain_get_max_buffer_size(),
 *       get_max_buffer_size());
 *
 *     - the number of segments in a buffer
 *       (m0_net_domain_get_max_buffer_segments(), get_max_buffer_segments());
 *
 *     - the size of a segment (m0_net_domain_get_max_buffer_segment_size(),
 *       get_max_buffer_segment_size()).
 *
 * These restrictions are required by rdma hardware. Specifically, lnet sets the
 * maximal segment size to 4KB and large buffers submitted to lnet have a large
 * number of small 4KB segments. The actual values of lnet limits are assumed by
 * its users. Specifically, it is assumed that the maximal segment size is so
 * small that the size of any network io is its multiple. That is, it is assumed
 * that all segments, including the last one, of any network buffer have the
 * same size: 4KB and buffer size is simply 4KB times the number of segments.
 *
 * sock has no inherent fragmentation restrictions (because it internally does
 * all that splitting and assembling of packets anyway). Using a large number of
 * small segments is pure overhead and sock sets the maximal segment size to a
 * very large number (get_max_buffer_segment_size()), which breaks the
 * assumption that all io is done in multiples of segment size.
 *
 * Fixing the transport users to make no assumptions about fragmentation limits
 * (or using different allocation strategies depending on the transport) is a
 * work in progress.
 *
 * A brief history
 * ---------------
 *
 * Historically, the first motr network transport was based on sunrpc/xdr
 * library (and similar interfaces in the linux kernel). This was abandoned
 * because of the insurmountable problems with making in reliably multi-threaded
 * and performant.
 *
 * Next transport was based on lnet (net/lnet/, based on lustre lnet, which is
 * itself based on Sandia portals). lnet is a kernel module, which complicates
 * some deployments.
 *
 * sock transport (net/sock/) is based on the bsd sockets interface, which is
 * fairly portable. While lacking at the moment advanced lnet features like rdma
 * it is a useful basepoint and can be sufficiently performant for cloud
 * deployments (as opposed to hpc).
 *
 * @{
 */

#include "lib/vec.h"
#include <sys/epoll.h>                     /* epoll_create */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>                    /* epoll_create */
#include <netinet/in.h>                    /* INET_ADDRSTRLEN */
#include <netinet/ip.h>
#include <arpa/inet.h>                     /* inet_pton, htons */
#include <string.h>                        /* strchr */
#include <unistd.h>                        /* close */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"
#include "lib/errno.h"                     /* EINTR, ENOMEM */
#include "lib/thread.h"
#include "lib/misc.h"                      /* ARRAY_SIZE, IS_IN_ARRAY */
#include "lib/tlist.h"
#include "lib/types.h"
#include "lib/string.h"                    /* m0_strdup */
#include "lib/chan.h"
#include "lib/memory.h"
#include "lib/cookie.h"
#include "lib/bitmap.h"
#include "lib/refs.h"
#include "lib/time.h"
#include "lib/finject.h"
#include "sm/sm.h"
#include "motr/magic.h"
#include "net/net.h"
#include "net/buffer_pool.h"
#include "net/net_internal.h"              /* m0_net__tm_invariant */
#include "format/format.h"
#include "addb2/addb2.h"
#include "addb2/histogram.h"

#include "net/sock/addb2.h"
#include "net/sock/xcode.h"
#include "net/sock/xcode_xc.h"

#define EP_DEBUG (1)

#ifdef ENABLE_SOCK_MOCK_LNET
#define MOCK_LNET (1)
#else
#define MOCK_LNET (0)
#endif

#define USE_INVARIANTS (0)

struct sock;
struct mover;
struct addr;
struct ep;
struct buf;
struct ma;
struct bdesc;
struct packet;

/**
 * State of a sock state machine. Stored in sock::s_sm.sm_state.
 */
enum sock_state {
	/**
	 * sock has been initialised.
	 */
	S_INIT,
	/**
	 * sock is listening for incoming connections.
	 *
	 * Each transfer machine has a single listening socket, initialised in
	 * ma_start()->sock_init().
	 */
	S_LISTENING,
	/**
	 * Outgoing connection is being established through this sock.
	 *
	 * A non-blocking connect(2) has been executed (sock_init()) and its
	 * completion has not been reported (in sock_event()).
	 */
	S_CONNECTING,
	/**
	 * An outgoing or incoming connection has been established. Data can be
	 * transferred through this sock.
	 */
	S_OPEN,
	/**
	 * The sock has been finalised (sock_done()) and is now placed on
	 * ma::t_deathrow list. It will be collected and freed by ma_prune().
	 */
	S_DELETED
};

/**
 * Type of event that a mover state machine can handle.
 *
 * @see mover_op()
 */
enum mover_opcode {
	M_READ,
	M_WRITE,
	M_CLOSE,

	M_NR
};

/** Flags stored in sock::s_flags. */
enum sock_flags {
	/** sock has data available for non-blocking read. */
	HAS_READ   = M0_BITS(M_READ),
	/** Non blocking write is possible on the sock. */
	HAS_WRITE  = M0_BITS(M_WRITE),
	/** Non-blocking writes are monitored for this sock by epoll(2). */
	WRITE_POLL = M0_BITS(M_NR + 1),
	/**
	 * M0_NET_QT_MSG_SEND packet, received on this socket, requires receive
	 * queue re-provisioning.
	 */
	MORE_BUFS  = M0_BITS(M_NR + 2)
};

/**
 * State of a mover state machine, stored in mover::m_sm.sm_state.
 *
 * The state transition diagram (without failure transitions to R_FAIL) is the
 * following:
 *
 * @verbatim
 *
 *                      +-->IDLE     +-----+
 *                      |     |      |     |
 *                      |     V      V     |
 *                      |    PK--->HEADER--+
 *                      |     ^      |
 *                      |     |      |
 *          DONE<---PK_DONE---+      |
 *                      ^            |
 *                      |            V
 *                      +---------INTERVAL<--+
 *                                   |       |
 *                                   +-------+
 *
 * @endverbatim
 *
 */
enum rw_state {
	R_IDLE,
	R_PK,
	R_HEADER,
	R_INTERVAL,
	R_PK_DONE,
	R_FAIL,
	R_DONE,

	R_NR,
	STATE_NR = R_NR
};

/** A network end-point. */
struct ep {
	/** Generic end-point structure, linked into a per-tm list. */
	struct m0_net_end_point e_ep;
	/** Peer address. */
	struct addr             e_a;
	/** Sockets opened to this end-point. */
	struct m0_tl            e_sock;
	/** Writers sending data to this end-point. */
	struct m0_tl            e_writer;
#ifdef EP_DEBUG
	int e_r_mover;
	int e_r_sock;
	int e_r_buf;
	int e_r_find;
#endif
};

/** A network transfer machine */
struct ma {
	/** Generic transfer machine with buffer queues, etc. */
	struct m0_net_transfer_mc *t_ma;
	/**
	 * Poller thread.
	 *
	 * All asynchronous activity happens in this thread:
	 *
	 *     - notifications about incoming connections;
	 *
	 *     - notifications about possibility of non-blocking socket io;
	 *
	 *     - buffer completion events (ma_buf_done());
	 *
	 *     - buffer timeouts (ma_buf_timeout());
	 *
	 *     - freeing socket structures (ma_prune());
	 *
	 * Poller can easily be adapter to be a "chore" in a locality.
	 *
	 */
	struct m0_thread           t_poller;
	/** epoll(2) instance file descriptor. */
	int                        t_epollfd;
	bool                       t_shutdown;
	/** List of finalised sock structures. */
	struct m0_tl               t_deathrow;
	/** Channel to signal for non-automatic delivery. */
	struct m0_chan            *t_delivery_chan;
	/** List of completed buffers. */
	struct m0_tl               t_done;
	struct m0_addb2_hist       t_addb[M0_NET_QT_NR * M0_AVI_SOCK_Q_NR];
	struct m0_addb2_hist       t_epoll_wait;
	struct m0_addb2_hist       t_epoll_nr;
	struct m0_addb2_hist       t_frag[2]; /* [0] reads, [1] writes. */
};

/**
 * A state transition function for a mover.
 */
struct mover_op {
	/** State transition function, @see mover_op(). */
	int  (*o_op)(struct mover *self, struct sock *s);
	/**
	 * If this flag is true, the transition function is postponed until the
	 * socket has the corresponding HAS_{READ,WRITE} flag set.
	 *
	 * @see mover_op().
	 */
	bool   o_doesio;
};

/**
 * A vector of state transition functions for a mover.
 *
 * This implements specific actions on top of the generic mover state machine.
 *
 * @see writer_op, stream_op, dgram_op, get_op.
 */
struct mover_op_vec {
	/** Human-readable name. */
	const char     *v_name;
	struct mover_op v_op[STATE_NR][M_NR];
	/**
	 * A call-back called when an error is raised for a socket or an error
	 * happens during state transition.
	 */
	void          (*v_error)(struct mover *self, struct sock *s, int rc);
	/**
	 * A call-back called when the mover reaches R_DONE state.
	 */
	void          (*v_done) (struct mover *self, struct sock *s);
};

/**
 * A mover.
 *
 * A mover is a state machine (mover::m_sm) with non-blocking state-transitions,
 * moving data between a buffer (mover::m_buf) and an end-point (mover::m_ep).
 */
struct mover {
	uint64_t                   m_magix;
	/**
	 * For a reader, which is always embedded in a sock (sock::s_reader),
	 * this points back to the ambient sock.
	 *
	 * A writer is always embedded in a buffer (buf::b_writer) and is
	 * associated with a particular end-point (mover::m_ep), but can switch
	 * between different sockets to that end-point. This switch can happen
	 * only on a boundary between packets. When the writer is busy writing a
	 * packet (R_PK, R_HEADER, R_INTERVAL and R_PK_DONE states), it is
	 * locked to the socket to which mover::m_sock points.
	 */
	struct sock               *m_sock;
	/** The end-point. A writer takes a reference to it (ep_add()). */
	struct ep                 *m_ep;
	struct m0_sm               m_sm;
	const struct mover_op_vec *m_op;
	/** Linkage in the list of writers for an end-point (ep::e_writer). */
	struct m0_tlink            m_linkage;
	/** The buffer from or to which data are moved. */
	struct buf                *m_buf;
	/** The current packet. */
	struct packet              m_pk;
	/** The current packet in on-wire form. */
	char                       m_pkbuf[sizeof(struct packet)];
	/**
	 * How many bytes (header and payload) has been ioed in the current
	 * packet.
	 */
	m0_bcount_t                m_nob;
	/**
	 * An intermediate buffer into which a packet is read from a datagram
	 * socket.
	 */
	void                      *m_scratch;
	/**
	 * The buffer generation that data are trasnferred with.
	 *
	 * @see buf::b_gen.
	 */
	int                        m_gen;
};

/**
 * Sock structure for a network buffer.
 */
struct buf {
	uint64_t              b_magix;
	/** Cookie identifying the buffer. */
	uint64_t              b_cookie;
	/** Generic network buffer structure. */
	struct m0_net_buffer *b_buf;
	/** Writer moving the data from this buffer. */
	struct mover          b_writer;
	/**
	 * Buffers return code.
	 *
	 * This is 0 after the buffer was successfully added to a queue and
	 * before the buffer is terminated. Normal termination sets this to
	 * -EALREADY, cancellation to -ECANCELED, timeout to -ETIMEDOUT, any
	 * other error to the appropriate error code.
	 *
	 * When the buffer is registered, but not added, this field is -ENOENT.
	 *
	 * ->buf_rc is *not* cleared when the buffer completion call-back is
	 * delivered, so if a late incoming packet arrives after the call-back,
	 * it is rejected.
	 */
	int                   b_rc;
	/** Bitmap of received packets. */
	struct m0_bitmap      b_done;
	/** Descriptor of the other buffer in the transfer operation. */
	struct bdesc          b_peer;
	/**
	 * The other end-point for the transfer.
	 *
	 * This is set (by buf_accept()) when the first incoming packet is
	 * received for this buffer.
	 */
	struct ep            *b_other;
	/** Linkage in the list of completed buffers (ma::t_done). */
	struct m0_tlink       b_linkage;
	/** Not currently used. */
	m0_bindex_t           b_offset;
	/**
	 * The total size of data expected to be received, copied from
	 * packet::p_totalsize.
	 */
	m0_bindex_t           b_length;
	m0_time_t             b_last;
	/**
	 * Number of times the buffer was added to a queue.
	 *
	 * @see mover::m_gen.
	 */
	int                   b_gen;
};

/** A socket: connection to an end-point. */
struct sock {
	uint64_t        s_magix;
	/** File descriptor. */
	int             s_fd;
	/** Bit-field of flags taken from enum sock_flags. */
	uint64_t        s_flags;
	struct m0_sm    s_sm;
	/** The end-point to which this socket connects. */
	struct ep      *s_ep;
	/** The reader that handles packets incoming to this socket. */
	struct mover    s_reader;
	/** Linkage in the list of finalised sockets (ma::t_deathrow). */
	struct m0_tlink s_linkage;
	/** Not currently used. Will be used to garbage collect idle sockets. */
	m0_time_t       s_last;
};

/**
 * Socket type.
 *
 * This abstract the differences between stream and datagram sockets.
 */
struct socktype {
	const char                *st_name;
	/** Default reader opvec for this socket type. */
	const struct mover_op_vec *st_reader;
	/** Default packet size for this socket type. */
	m0_bcount_t              (*st_pk_size)(const struct mover *w,
					       const struct sock *s);
	/**
	 * Call-back invoked when an error is raised on the socket of this
	 * type.
	 */
	void                     (*st_error)(struct mover *m, struct sock *s);
	/** Default protocol for sockets of this type. See socket(2). */
	int                        st_proto;
};

/**
 * Protocol (and address) family.
 *
 * Abstracts the differences between ipv4 and ipv6.
 */
struct pfamily {
	const char *f_name;
	/**
	 * Encode sock address in sockaddr.
	 *
	 * @see ipv4_encode(), ipv6_encode().
	 */
	void      (*f_encode)(const struct addr *a, struct sockaddr *sa);
	/**
	 * Decode sock address from sockaddr.
	 *
	 * @see ipv4_decode(), ipv6_decode().
	 */
	void      (*f_decode)(struct addr *a, const struct sockaddr *sa);
};

/**
 * Vector of external entry points used by the code. UTs use this.
 */
struct sock_ops {
	int     (*so_socket)      (int family, int type, int proto);
	int     (*so_accept4)     (int fd, struct sockaddr *addr,
				   socklen_t *addrlen, int flags);
	int     (*so_listen)      (int fd, int backlog);
	int     (*so_bind)        (int fd, const struct sockaddr *addr,
				   socklen_t addrlen);
	int     (*so_connect)     (int fd, const struct sockaddr *addr,
				   socklen_t addrlen);
	ssize_t (*so_readv)       (int fd, const struct iovec *iov, int iovcnt);
	ssize_t (*so_writev)      (int fd, const struct iovec *iov, int iovcnt);
	int     (*so_close)       (int fd);
	int     (*so_shutdown)    (int fd, int how);
	int     (*so_setsockopt)  (int fd, int level, int optname,
				   const void *optval, socklen_t optlen);

	int     (*so_epoll_create)(int size);
	int     (*so_epoll_wait)  (int fd, struct epoll_event *events,
				   int maxevents, int timeout);
	int     (*so_epoll_ctl)   (int epfd, int op, int fd,
				   struct epoll_event *event);
};

/*
 * Tracing helpers
 */

#define SAFE(val, exp, fallback) ((val) != NULL ? (exp) : (fallback))

#define EP_F "%s"
#define EP_P(e) SAFE(e, (e)->e_ep.nep_addr, "null")
#define EP_FL EP_F "->" EP_F
#define EP_PL(e) EP_P(SAFE(e, ma_src(ep_ma(e)), NULL)), EP_P(e)

#define SOCK_F EP_FL "/%i/%" PRIx64 "/%i"
#define SOCK_P(s) EP_PL(SAFE(s, (s)->s_ep, NULL)),	\
	SAFE(s, (s)->s_sm.sm_state, 0),			\
	SAFE(s, (s)->s_flags, 0), SAFE(s, (s)->s_fd, 0)

#define B_F "[[%i]@%p:"EP_FL"]"
#define B_P(b) (b)->b_buf->nb_qtype, (b), EP_PL((b)->b_other)

#define TLOG(...) do { /* M0_LOG(M0_DEBUG, __VA_ARGS__) */; } while (0)

M0_TL_DESCR_DEFINE(s, "sockets",
		   static, struct sock, s_linkage, s_magix,
		   M0_NET_SOCK_SOCK_MAGIC, M0_NET_SOCK_SOCK_HEAD_MAGIC);
M0_TL_DEFINE(s, static, struct sock);

M0_TL_DESCR_DEFINE(m, "movers",
		   static, struct mover, m_linkage, m_magix,
		   M0_NET_SOCK_MOVER_MAGIC, M0_NET_SOCK_MOVER_HEAD_MAGIC);
M0_TL_DEFINE(m, static, struct mover);

M0_TL_DESCR_DEFINE(b, "buffers",
		   static, struct buf, b_linkage, b_magix,
		   M0_NET_SOCK_BUF_MAGIC, M0_NET_SOCK_BUF_HEAD_MAGIC);
M0_TL_DEFINE(b, static, struct buf);

static int  dom_init(const struct m0_net_xprt *xprt, struct m0_net_domain *dom);
static void dom_fini(struct m0_net_domain *dom);
static int  ma_init(struct m0_net_transfer_mc *ma);
static int  ma_confine(struct m0_net_transfer_mc *ma,
		       const struct m0_bitmap *processors);
static int  ma_start(struct m0_net_transfer_mc *ma, const char *name);
static int  ma_stop(struct m0_net_transfer_mc *ma, bool cancel);
static void ma_fini(struct m0_net_transfer_mc *ma);
static int  end_point_create(struct m0_net_end_point **epp,
			     struct m0_net_transfer_mc *ma, const char *name);
static int  buf_register(struct m0_net_buffer *nb);
static void buf_deregister(struct m0_net_buffer *nb);
static int  buf_add(struct m0_net_buffer *nb);
static void buf_del(struct m0_net_buffer *nb);
static int  bev_deliver_sync(struct m0_net_transfer_mc *ma);
static void bev_deliver_all(struct m0_net_transfer_mc *ma);
static bool bev_pending(struct m0_net_transfer_mc *ma);
static void bev_notify(struct m0_net_transfer_mc *ma, struct m0_chan *chan);
static m0_bcount_t get_max_buffer_size(const struct m0_net_domain *dom);
static m0_bcount_t get_max_buffer_segment_size(const struct m0_net_domain *);
static int32_t get_max_buffer_segments(const struct m0_net_domain *dom);
static m0_bcount_t get_max_buffer_desc_size(const struct m0_net_domain *);

static void poller   (struct ma *ma);
static void ma__fini (struct ma *ma);
static void ma_prune (struct ma *ma);
static void ma_lock  (struct ma *ma);
static void ma_unlock(struct ma *ma);
static bool ma_is_locked(const struct ma *ma);
static bool ma_invariant(const struct ma *ma);
static void ma_event_post (struct ma *ma, enum m0_net_tm_state state);
static void ma_buf_done   (struct ma *ma);
static void ma_buf_timeout(struct ma *ma);
static void ma_hist       (struct ma *ma, int qtype, int idx,
			   m0_time_t end, m0_time_t start);
static struct buf *ma_recv_buf(struct ma *ma, const struct packet *p);

static struct ep *ma_src(struct ma *ma);

static int  ep_find   (struct ma *ma, const char *name, struct ep **out);
static int  ep_create (struct ma *ma, struct addr *addr, const char *name,
		       struct ep **out);
static void ep_free   (struct ep *ep);
static void ep_put    (struct ep *ep);
static void ep_get    (struct ep *ep);
static bool ep_eq     (const struct ep *ep, const struct addr *addr);
static struct ma *ep_ma (struct ep *ep);
static struct ep *ep_net(struct m0_net_end_point *net);
static void ep_release(struct m0_ref *ref);
static bool ep_invariant(const struct ep *ep);
static int  ep_add(struct ep *ep, struct mover *w);
static void ep_del(struct mover *w);
static bool ep_is_listening(const struct ep *ep);
static int  ep_balance(struct ep *ep);

static int   addr_resolve     (struct addr *addr, const char *name);
static int   addr_parse       (struct addr *addr, const char *name);
static int   addr_parse_lnet  (struct addr *addr, const char *name);
static int   addr_parse_native(struct addr *addr, const char *name);
static void  addr_decode      (struct addr *addr, const struct sockaddr *sa);
static void  addr_encode      (const struct addr *addr, struct sockaddr *sa);
static char *addr_print       (const struct addr *addr);
static bool  addr_invariant   (const struct addr *addr);
static bool  addr_eq          (const struct addr *a0, const struct addr *a1);

static int  sock_in(struct sock *s);
static void sock_out(struct sock *s);
static void sock_close(struct sock *s);
static void sock_done(struct sock *s, bool balance);
static void sock_fini(struct sock *s);
static bool sock_event(struct sock *s, uint32_t ev);
static int  sock_ctl(struct sock *s, int op, uint32_t flags);
static int  sock_init_fd(int fd, struct sock *s, struct ep *ep, uint32_t flags);
static int  sock_init(int fd, struct ep *src, struct ep *tgt, uint32_t flags);
static struct mover *sock_writer(struct sock *s);
static bool sock_invariant(const struct sock *s);

static struct ma *buf_ma(struct buf *buf);
static bool buf_invariant(const struct buf *buf);
static int  buf_accept   (struct buf *buf, struct mover *m);
static void buf_done     (struct buf *buf, int rc);
static void buf_terminate(struct buf *buf, int rc);
static void buf_complete (struct buf *buf);
static void buf_hist     (struct buf *buf, int idx);

static int bdesc_create(struct addr *addr, struct buf *buf,
			struct m0_net_buf_desc *out);
static int bdesc_encode(const struct bdesc *bd, struct m0_net_buf_desc *out);
static int bdesc_decode(const struct m0_net_buf_desc *nbd, struct bdesc *out);

static void mover_init(struct mover *m, struct ma *ma,
		       const struct mover_op_vec *vop);
static void mover_fini(struct mover *m);
static int  mover_op  (struct mover *m, struct sock *s, int op);
static void mover_fail(struct mover *m, struct sock *s, int rc);
static bool mover_is_reader(const struct mover *m);
static bool mover_is_writer(const struct mover *m);
static int  mover_matches  (const struct mover *m);
static bool mover_invariant(const struct mover *m);

static m0_bcount_t pk_size (const struct mover *m, const struct sock *s);
static m0_bcount_t pk_tsize(const struct mover *m);
static m0_bcount_t pk_dnob (const struct mover *m);
static int pk_state(const struct mover *w);
static int pk_io(struct mover *w, struct sock *s,
		 uint64_t flag, struct m0_bufvec *bv, m0_bcount_t size);
static int pk_iov_prep(struct mover *m, struct iovec *iv, int nr,
		       struct m0_bufvec *bv, m0_bcount_t size, int *count);
static void pk_header_init(struct mover *m, struct sock *s);
static int  pk_header_done(struct mover *m);
static int  pk_done  (struct mover *m);
static void pk_encdec(struct mover *m, enum m0_xcode_what what);
static void pk_decode(struct mover *m);
static void pk_encode(struct mover *m);

static int stream_idle    (struct mover *self, struct sock *s);
static int stream_pk      (struct mover *self, struct sock *s);
static int stream_header  (struct mover *self, struct sock *s);
static int stream_interval(struct mover *self, struct sock *s);
static int stream_pk_done (struct mover *self, struct sock *s);
static int dgram_idle     (struct mover *self, struct sock *s);
static int dgram_pk       (struct mover *self, struct sock *s);
static int dgram_header   (struct mover *self, struct sock *s);
static int dgram_interval (struct mover *self, struct sock *s);
static int dgram_pk_done  (struct mover *self, struct sock *s);
static int writer_idle    (struct mover *self, struct sock *s);
static int writer_pk      (struct mover *self, struct sock *s);
static int writer_write   (struct mover *self, struct sock *s);
static int writer_pk_done (struct mover *self, struct sock *s);
static int get_idle       (struct mover *self, struct sock *s);
static int get_pk         (struct mover *self, struct sock *s);
static void writer_done   (struct mover *self, struct sock *s);
static void writer_error  (struct mover *w, struct sock *s, int rc);

static m0_bcount_t stream_pk_size(const struct mover *w, const struct sock *s);
static void        stream_error(struct mover *m, struct sock *s);

static m0_bcount_t dgram_pk_size(const struct mover *w, const struct sock *s);
static void        dgram_error(struct mover *m, struct sock *s);

static void ip4_encode(const struct addr *a, struct sockaddr *sa);
static void ip4_decode(struct addr *a, const struct sockaddr *sa);
static void ip6_encode(const struct addr *a, struct sockaddr *sa);
static void ip6_decode(struct addr *a, const struct sockaddr *sa);

static m0_time_t msec(m0_time_t a, m0_time_t b);
static bool in_ut(void);

struct m0_sm_conf sock_conf;
struct m0_sm_conf rw_conf;

static const struct mover_op_vec stream_reader_op;
static const struct mover_op_vec dgram_reader_op;
static const struct mover_op_vec writer_op;
static const struct mover_op_vec get_op;

static const struct pfamily  pf[];
static const struct socktype stype[];

static const struct m0_format_tag put_tag;
static const struct m0_format_tag get_tag;

static const struct pfamily pf[] = {
	[AF_UNIX]  = {
		.f_name = "unix"
	},
	[AF_INET]  = {
		.f_name   = "inet",
		.f_encode = &ip4_encode,
		.f_decode = &ip4_decode
	},
	[AF_INET6] = {
		.f_name = "inet6",
		.f_encode = &ip6_encode,
		.f_decode = &ip6_decode
	}
};

static const struct socktype stype[] = {
	[SOCK_STREAM] = {
		.st_name    = "stream",
		.st_reader  = &stream_reader_op,
		.st_pk_size = &stream_pk_size,
		.st_error   = &stream_error,
		.st_proto   = IPPROTO_TCP
	},
	[SOCK_DGRAM]  = {
		.st_name    = "dgram",
		.st_reader  = &dgram_reader_op,
		.st_pk_size = &dgram_pk_size,
		.st_error   = &dgram_error,
		.st_proto   = IPPROTO_UDP
	}
};

#ifdef EP_DEBUG
#define EP_GET(e, f) \
	({ struct ep *__ep = (e); ep_get(__ep); M0_CNT_INC(__ep->e_r_ ## f); })
#define EP_PUT(e, f) \
	({ struct ep *__ep = (e); M0_CNT_DEC(__ep->e_r_ ## f); ep_put(__ep); })
#else
#define EP_GET(e, f) ep_get(e)
#define EP_PUT(e, f) ep_put(e)
#endif

static const struct sock_ops *sops = NULL;

#define SOP(op, ...)							\
	({ in_ut() ? sops->so_ ## op(__VA_ARGS__) : op(__VA_ARGS__); })


#if USE_INVARIANTS
static bool ma_invariant(const struct ma *ma)
{
	const struct m0_net_transfer_mc *net = ma->t_ma;
	const struct m0_tl              *eps = &net->ntm_end_points;

	return  _0C(net != NULL) &&
		_0C(net->ntm_xprt_private == ma) &&
		m0_net__tm_invariant(net) &&
		s_tlist_invariant(&ma->t_deathrow) &&
		/* ma is either fully uninitialised or fully initialised. */
		_0C((ma->t_poller.t_func == NULL && ma->t_epollfd == -1 &&
		     m0_nep_tlist_is_empty(eps) &&
		     s_tlist_is_empty(&ma->t_deathrow)) ||
		    (ma->t_poller.t_func != NULL && ma->t_epollfd >= 0 &&
		     m0_tl_exists(m0_nep, nep, eps,
				  ep_is_listening(ep_net(nep)))) ||
		    ma->t_shutdown) &&
		/* In STARTED state ma is fully initialised. */
		_0C(ergo(net->ntm_state == M0_NET_TM_STARTED,
			 ma->t_epollfd >= 0)) &&
		_0C(m0_tl_forall(s, s, &ma->t_deathrow, sock_invariant(s))) &&
		/* Endpoints are unique. */
		_0C(m0_tl_forall(m0_nep, p, eps,
			m0_tl_forall(m0_nep, q, eps,
			     ep_eq(ep_net(p), &ep_net(q)->e_a) == (p == q)))) &&
		_0C(m0_tl_forall(m0_nep, p, eps, ep_ma(ep_net(p)) == ma &&
				 ep_invariant(ep_net(p)))) &&
		_0C(m0_forall(i, ARRAY_SIZE(net->ntm_q),
			m0_tl_forall(m0_net_tm, nb, &net->ntm_q[i],
				     buf_invariant(nb->nb_xprt_private)))) &&
		_0C(m0_tl_forall(b, buf, &ma->t_done,
				 buf_invariant(buf) && buf->b_rc != 0));
}

static bool sock_invariant(const struct sock *s)
{
	struct ma *ma = ep_ma(s->s_ep);

	return  _0C((s->s_sm.sm_state == S_DELETED) ==
		    s_tlist_contains(&ma->t_deathrow, s)) &&
		_0C((s->s_sm.sm_state != S_DELETED) ==
		    s_tlist_contains(&s->s_ep->e_sock, s));
}

static bool buf_invariant(const struct buf *buf)
{
	const struct m0_net_buffer *nb = buf->b_buf;
	/* Either the buffer is only added to the domain (not associated with a
	   transfer machine... */
	return  (nb->nb_flags == M0_NET_BUF_REGISTERED &&
		 nb->nb_tm == NULL) ^ /* or (exclusively) ... */
		/* it is queued to a machine. */
		(_0C(nb->nb_flags & (M0_NET_BUF_REGISTERED|M0_NET_BUF_QUEUED))&&
		 _0C(nb->nb_tm != NULL) &&
		 _0C(buf->b_rc <= 0) &&
		 _0C(ergo(buf->b_writer.m_sm.sm_conf != NULL,
			  mover_invariant(&buf->b_writer))) &&
		 _0C(m0_net__buffer_invariant(nb)));
}

static bool addr_invariant(const struct addr *a)
{
	return  _0C(IS_IN_ARRAY(a->a_family, pf)) &&
		_0C(pf[a->a_family].f_name != NULL) &&
		_0C(IS_IN_ARRAY(a->a_socktype, stype)) &&
		_0C(stype[a->a_socktype].st_name != NULL) &&
		_0C(M0_IN(a->a_family, (AF_INET, AF_INET6))) &&
		_0C(M0_IN(a->a_socktype, (SOCK_STREAM, SOCK_DGRAM))) &&
		_0C(M0_IN(a->a_protocol, (0, IPPROTO_TCP, IPPROTO_UDP)));
}

static bool ep_invariant(const struct ep *ep)
{
	const struct ma *ma = ep_ma((void *)ep);
	return  addr_invariant(&ep->e_a) &&
		m0_net__ep_invariant((void *)&ep->e_ep,
				     (void *)ma->t_ma, true) &&
		_0C(ep->e_ep.nep_addr != NULL) &&
#ifdef EP_DEBUG
		/*
		 * Reference counters consistency:
		 */

		/* Each writer got a reference... */
		_0C(ep->e_r_mover == m_tlist_length(&ep->e_writer)) &&
		/*
		 * and each socket (including ones lingering on ma->t_deathrow)
		 * got a reference.
		 */
		_0C(ep->e_r_sock  == s_tlist_length(&ep->e_sock) +
		    m0_tl_fold(s, s, dead, &ma->t_deathrow, 0,
			       dead + (s->s_ep == ep))) &&
#endif
		_0C(m0_tl_forall(s, s, &ep->e_sock,
				 s->s_ep == ep && sock_invariant(s))) &&
		_0C(m0_tl_forall(m, w, &ep->e_writer,
				 w->m_ep == ep &&
				 /*
				  * The writer locked to a socket is always at
				  * the head of the writers list.
				  */
				 ergo(w->m_sock != NULL,
				      w == m_tlist_head(&ep->e_writer))));
}

static bool mover_invariant(const struct mover *m)
{
	return  _0C(m_tlink_is_in(m) == (m->m_ep != NULL)) &&
		_0C(M0_IN(m->m_op, (&writer_op, &get_op)) ||
		    m0_exists(i, ARRAY_SIZE(stype),
			      stype[i].st_reader == m->m_op));
}

/* USE_INVARIANTS */
#else

static bool ma_invariant(const struct ma *ma)
{
	return true;
}

static bool sock_invariant(const struct sock *s)
{
	return true;
}

static bool buf_invariant(const struct buf *buf)
{
	return true;
}

static bool addr_invariant(const struct addr *a)
{
	return true;
}

static bool ep_invariant(const struct ep *ep)
{
	return true;
}

static bool mover_invariant(const struct mover *m)
{
	return true;
}

/* USE_INVARIANTS */
#endif

/** Used as m0_net_xprt_ops::xo_dom_init(). */
static int dom_init(const struct m0_net_xprt *xprt, struct m0_net_domain *dom)
{
	M0_ENTRY();
	return M0_RC(0);
}

/** Used as m0_net_xprt_ops::xo_dom_fini(). */
static void dom_fini(struct m0_net_domain *dom)
{
	M0_ENTRY();
	M0_LEAVE();
}

static void ma_lock(struct ma *ma)
{
	m0_mutex_lock(&ma->t_ma->ntm_mutex);
}

static void ma_unlock(struct ma *ma)
{
	m0_mutex_unlock(&ma->t_ma->ntm_mutex);
}

static bool ma_is_locked(const struct ma *ma)
{
	return m0_mutex_is_locked(&ma->t_ma->ntm_mutex);
}

/**
 * Main loop of a per-ma thread that polls sockets.
 */
static void poller(struct ma *ma)
{
	enum { EV_NR = 256 };
	struct epoll_event ev[EV_NR] = {};
	int                nr;
	int                i;
	/*
	 * Notify users that ma reached M0_NET_TM_STARTED state.
	 *
	 * This also sets ma->ntm_ep.
	 *
	 * This should be done once per tm, so if multiple poller threads are
	 * used, only 1 event should be posted.
	 *
	 * @todo there is a race condition here: an application (i.e., the rpc
	 * layer), might timeout waiting for the ma to start and call
	 * m0_net_tm_stop(), which moves ma into STOPPING state, but the event
	 * posted below moves the ma back into STARTED state.
	 *
	 * Because of this, we do not assert ma states here.
	 */
	ma_event_post(ma, M0_NET_TM_STARTED);
	M0_ADDB2_PUSH(M0_AVI_SOCK_POLLER, (uint64_t)ma);
	for (i = 0; i < ARRAY_SIZE(ma->t_addb); ++i) {
		m0_addb2_hist_add_auto(&ma->t_addb[i], 100,
				       M0_AVI_SOCK_Q_INIT + i, -1);
	}
	m0_addb2_hist_add_auto(&ma->t_epoll_wait, 100,
			       M0_AVI_SOCK_EPOLL_WAIT, -1);
	m0_addb2_hist_add_auto(&ma->t_epoll_nr, 100, M0_AVI_SOCK_EPOLL_NR, -1);
	m0_addb2_hist_add_auto(&ma->t_frag[0], 100, M0_AVI_SOCK_FRAG_READ, -1);
	m0_addb2_hist_add_auto(&ma->t_frag[1], 100, M0_AVI_SOCK_FRAG_WRITE, -1);
	while (1) {
		m0_time_t now = m0_time_now();
		if (ma->t_shutdown)
			break;
		nr = SOP(epoll_wait, ma->t_epollfd, ev, ARRAY_SIZE(ev), 1000);
		m0_addb2_hist_mod(&ma->t_epoll_wait, msec(m0_time_now(), now));
		if (nr == -1) {
			if (errno == EINTR)
				M0_LOG(M0_DEBUG, "epoll: %i.", -errno);
			else
				M0_LOG(M0_ERROR, "Strange errno: %i.", -errno);
			continue;
		}
		m0_addb2_hist_mod(&ma->t_epoll_nr, nr);
		/* Check again because epoll() may block for some time */
		if (ma->t_shutdown)
			break;
		M0_LOG(M0_DEBUG, "Got: %d.", nr);
		ma_lock(ma);
		M0_ASSERT(ma_is_locked(ma) && ma_invariant(ma));
		for (i = 0; i < nr; ++i) {
			struct sock *s = ev[i].data.ptr;

			if (s->s_sm.sm_state == S_DELETED)
				continue;
			if (sock_event(s, ev[i].events))
				/*
				 * Ran out of buffers on the receive queue,
				 * break out, deliver completion events,
				 * re-provision.
				 */
				break;
		}
		/* @todo close long-unused sockets. */
		ma_buf_timeout(ma);
		/*
		 * Deliver buffer completion events and re-provision receive
		 * queue if necessary.
		 */
		if (ma->t_ma->ntm_bev_auto_deliver)
			ma_buf_done(ma);
		M0_ASSERT(ma_invariant(ma));
		/*
		 * This is the only place, where sock structures are freed,
		 * except for ma finalisation.
		 */
		ma_prune(ma);
		M0_ASSERT(ma_invariant(ma));
		ma_unlock(ma);
	}
	m0_addb2_hist_del(&ma->t_frag[1]);
	m0_addb2_hist_del(&ma->t_frag[0]);
	m0_addb2_hist_del(&ma->t_epoll_nr);
	m0_addb2_hist_del(&ma->t_epoll_wait);
	for (i = 0; i < ARRAY_SIZE(ma->t_addb); ++i)
		m0_addb2_hist_del(&ma->t_addb[i]);
	m0_addb2_pop(M0_AVI_SOCK_POLLER);
	m0_addb2_force(0);
}

/**
 * Initialises transport-specific part of the transfer machine.
 *
 * Allocates sock ma and attaches it to the generic part of ma.
 *
 * Listening socket (ma::t_listen) cannot be initialised before the local
 * address to bind, which is supplied as a parameter to
 * m0_net_xprt_ops::xo_tm_start(), is known.
 *
 * Poller thread (ma::t_poller) cannot be started, because a call to
 * m0_net_tm_confine() can be done after initialisation.
 *
 * ->epollfd can be initialised here, but it is easier to initialise everything
 * in ma_start().
 *
 * Used as m0_net_xprt_ops::xo_tm_init().
 */
static int ma_init(struct m0_net_transfer_mc *net)
{
	struct ma *ma;
	int        result;

	M0_ASSERT(net->ntm_xprt_private == NULL);

	M0_ALLOC_PTR(ma);
	if (ma != NULL) {
		ma->t_epollfd = -1;
		ma->t_shutdown = false;
		net->ntm_xprt_private = ma;
		ma->t_ma = net;
		s_tlist_init(&ma->t_deathrow);
		b_tlist_init(&ma->t_done);
		result = 0;
	} else
		result = M0_ERR(-ENOMEM);
	return M0_RC(result);
}

/** Frees finalised sock structures. */
static void ma_prune(struct ma *ma)
{
	struct sock *sock;

	M0_PRE(ma_is_locked(ma));
	m0_tl_for(s, &ma->t_deathrow, sock) {
		sock_fini(sock);
	} m0_tl_endfor;
	M0_POST(s_tlist_is_empty(&ma->t_deathrow));
}

/**
 * Finalises the bulk of ma state.
 *
 * This is called from the normal finalisation path (ma_fini()) and in error
 * cleanup case during initialisation (ma_start()).
 */
static void ma__fini(struct ma *ma)
{
	struct m0_net_end_point *net;

	M0_PRE(ma_is_locked(ma));
	if (!ma->t_shutdown) {
		/* Set the shutdown flag.
		 * Release the lock and wait for poller(), so that the poller()
		 * will get a chance to detect this flag and exit.
		 */
		ma->t_shutdown = true;
		ma_unlock(ma);
		if (ma->t_poller.t_func != NULL) {
			m0_thread_join(&ma->t_poller);
			m0_thread_fini(&ma->t_poller);
		}
		/* Go on finalizing the ma */
		ma_lock(ma);
		m0_tl_for(m0_nep, &ma->t_ma->ntm_end_points, net) {
			struct ep   *ep = ep_net(net);
			struct sock *sock;
			m0_tl_for(s, &ep->e_sock, sock) {
				sock_done(sock, false);
			} m0_tl_endfor;
		} m0_tl_endfor;
		/*
		 * Finalise epoll after sockets, because sock_done() removes the
		 * socket from the poll set.
		 */
		if (ma->t_epollfd >= 0) {
			SOP(close, ma->t_epollfd);
			ma->t_epollfd = -1;
		}
		ma_buf_done(ma);
		ma_prune(ma);
		b_tlist_fini(&ma->t_done);
		s_tlist_fini(&ma->t_deathrow);
		M0_ASSERT(m0_nep_tlist_is_empty(&ma->t_ma->ntm_end_points));
		ma->t_ma->ntm_ep = NULL;
	}
	M0_POST(ma_invariant(ma));
}

/**
 * Used as m0_net_xprt_ops::xo_ma_fini().
 */
static void ma_fini(struct m0_net_transfer_mc *net)
{
	struct ma *ma = net->ntm_xprt_private;

	ma_lock(ma);
	M0_PRE(ma_is_locked(ma) && ma_invariant(ma));
	ma__fini(ma);
	ma_unlock(ma);
	net->ntm_xprt_private = NULL;
	m0_free(ma);
}

/**
 * Starts initialised ma.
 *
 * Initialises everything that ma_init() didn't. Note that ma is in
 * M0_NET_TM_STARTING state after this returns. Switch to M0_NET_TM_STARTED
 * happens when the poller thread posts special event.
 *
 * Used as m0_net_xprt_ops::xo_tm_start().
 */
static int ma_start(struct m0_net_transfer_mc *net, const char *name)
{
	struct ma *ma = net->ntm_xprt_private;
	int        result;

	M0_PRE(ma_is_locked(ma) && ma_invariant(ma));
	M0_PRE(net->ntm_state == M0_NET_TM_STARTING);

	/*
	 * - initialise epoll
	 *
	 * - parse the address and create the source endpoint
	 *
	 * - create the listening socket
	 *
	 * - start the poller thread.
	 *
	 * Should be done in this order, because the poller thread uses the
	 * listening socket to get the source endpoint to post a ma state change
	 * event (outside of ma lock).
	 */
	ma->t_epollfd = SOP(epoll_create, 1);
	if (ma->t_epollfd >= 0) {
		struct ep *ep;

		result = ep_find(ma, name, &ep);
		if (result == 0) {
			result = sock_init(-1, ep, NULL, EPOLLET);
			if (result == 0) {
				result = M0_THREAD_INIT(&ma->t_poller,
							struct ma *, NULL,
							&poller, ma, "socktm");
			}
			EP_PUT(ep, find);
		}
	} else
		result = -errno;
	if (result != 0)
		ma__fini(ma);
	M0_POST(ma_invariant(ma));
	return M0_RC(result);
}

/**
 * Stops a ma that has been started or is being started.
 *
 * Reverses the actions of ma_start(). Again, the ma stays in M0_NET_TM_STOPPING
 * state on return, a state change event is delivered separately by the poller
 * thread.
 *
 * Used as m0_net_xprt_ops::xo_tm_stop().
 */
static int ma_stop(struct m0_net_transfer_mc *net, bool cancel)
{
	struct ma *ma = net->ntm_xprt_private;

	M0_PRE(ma_is_locked(ma) && ma_invariant(ma));
	M0_PRE(net->ntm_state == M0_NET_TM_STOPPING);

	if (cancel)
		m0_net__tm_cancel(net);
	ma__fini(ma);
	ma_unlock(ma);
	ma_event_post(ma, M0_NET_TM_STOPPED);
	ma_lock(ma);
	return 0;
}

static int ma_confine(struct m0_net_transfer_mc *ma,
		      const struct m0_bitmap *processors)
{
	return -ENOSYS;
}

/**
 * Helper function that posts a ma state change event.
 */
static void ma_event_post(struct ma *ma, enum m0_net_tm_state state)
{
	struct m0_net_end_point *listen;

	/*
	 * Find "self" end-point. Cannot use ma_src(), because
	 * m0_net_transfer_mc::ntm_ep can be not yet or already not set.
	 */
	if (state == M0_NET_TM_STARTED) {
		listen = m0_tl_find(m0_nep, ne, &ma->t_ma->ntm_end_points,
				    ep_is_listening(ep_net(ne)));
		M0_ASSERT(listen != NULL);
	} else
		listen = NULL;
	m0_net_tm_event_post(&(struct m0_net_tm_event) {
			.nte_type       = M0_NET_TEV_STATE_CHANGE,
			.nte_next_state = state,
			.nte_time       = m0_time_now(),
			.nte_ep         = listen,
			.nte_tm         = ma->t_ma,
	});
}

/**
 * Finds queued buffers that timed out and completes them with prejudice^Wan
 * error.
 */
static void ma_buf_timeout(struct ma *ma)
{
	struct m0_net_transfer_mc *net = ma->t_ma;
	int                        i;
	m0_time_t                  now = m0_time_now();

	M0_PRE(ma_invariant(ma));
	for (i = 0; i < ARRAY_SIZE(net->ntm_q); ++i) {
		struct m0_net_buffer *nb;

		m0_tl_for(m0_net_tm, &ma->t_ma->ntm_q[i], nb) {
			if (nb->nb_timeout < now) {
				nb->nb_flags |= M0_NET_BUF_TIMED_OUT;
				buf_terminate(nb->nb_xprt_private, -ETIMEDOUT);
			}
		} m0_tl_endfor;
	}
	M0_POST(ma_invariant(ma));
}

/**
 * Finds buffers pending completion and completes them.
 *
 * A buffer is placed on ma::t_done queue when its operation is done, but the
 * completion call-back cannot be immediately invoked, for example, because
 * completion happened in a synchronous context.
 */
static void ma_buf_done(struct ma *ma)
{
	struct buf *buf;
	int         nr = 0;

	M0_PRE(ma_is_locked(ma) && ma_invariant(ma));
	m0_tl_for(b, &ma->t_done, buf) {
		b_tlist_del(buf);
		buf_complete(buf);
		nr++;
	} m0_tl_endfor;
	if (nr > 0 && ma->t_ma->ntm_callback_counter == 0)
		m0_chan_broadcast(&ma->t_ma->ntm_chan);
	M0_POST(ma_invariant(ma));
}

/**
 * Finds a buffer on M0_NET_QT_MSG_RECV queue, ready to receive this packet.
 */
static struct buf *ma_recv_buf(struct ma *ma, const struct packet *p)
{
	struct m0_net_buffer *nb;
	struct buf           *b;
	struct buf           *large = NULL;

	m0_tl_for(m0_net_tm, &ma->t_ma->ntm_q[M0_NET_QT_MSG_RECV], nb) {
		b = nb->nb_xprt_private;
		if (memcmp(&b->b_peer, &p->p_src, sizeof p->p_src) == 0)
			return b; /* Already receiving. */
		else if (large == NULL && b->b_done.b_nr == 0 &&
			 m0_vec_count(&nb->nb_buffer.ov_vec) >= p->p_totalsize)
			large = b; /* Idle and has enough space. */
	} m0_tl_endfor;
	return large;
}

/** Returns the "self" end-point of a transfer machine. */
static struct ep *ma_src(struct ma *ma)
{
	return ep_net(ma->t_ma->ntm_ep);
}

/**
 * Returns an end-point with the given name.
 *
 * Used as m0_net_xprt_ops::xo_end_point_create().
 *
 * @see m0_net_end_point_create().
 */
static int end_point_create(struct m0_net_end_point **epp,
			    struct m0_net_transfer_mc *net,
			    const char *name)
{
	struct ep *ep;
	struct ma *ma = net->ntm_xprt_private;
	int        result;

	M0_PRE(ma_is_locked(ma) && ma_invariant(ma));
	result = ep_find(ma, name, &ep);
	*epp = result == 0 ? &ep->e_ep : NULL;
	return M0_RC(result);
}

/**
 * Initialises a network buffer.
 *
 * Used as m0_net_xprt_ops::xo_buf_register().
 *
 * @see m0_net_buffer_register().
 */
static int buf_register(struct m0_net_buffer *nb)
{
	struct buf *b;

	M0_ALLOC_PTR(b);
	if (b != NULL) {
		nb->nb_xprt_private = b;
		b->b_buf = nb;
		b_tlink_init(b);
		b->b_rc = -ENOENT;
		return M0_RC(0);
	} else
		return M0_ERR(-ENOMEM);
}

/**
 * Finalises a network buffer.
 *
 * Used as m0_net_xprt_ops::xo_buf_deregister().
 *
 * @see m0_net_buffer_deregister().
 */
static void buf_deregister(struct m0_net_buffer *nb)
{
	struct buf *buf = nb->nb_xprt_private;

	M0_PRE(nb->nb_flags == M0_NET_BUF_REGISTERED && buf_invariant(buf));
	M0_PRE(buf->b_writer.m_sm.sm_conf == NULL);
	M0_PRE(buf->b_other == NULL);
	M0_PRE(M0_IS0(&buf->b_peer));
	M0_PRE(buf->b_offset == 0);
	M0_PRE(buf->b_length == 0);
	/* buf->b_rc can be still non-zero here. */
	m0_free(buf);
	nb->nb_xprt_private = NULL;
}

/**
 * Adds a network buffer to a ma queue.
 *
 * Used as m0_net_xprt_ops::xo_buf_add().
 *
 * @see m0_net_buffer_add().
 */
static int buf_add(struct m0_net_buffer *nb)
{
	struct buf   *buf  = nb->nb_xprt_private;
	struct ma    *ma   = buf_ma(buf);
	struct mover *w    = &buf->b_writer;
	struct bdesc *peer = &buf->b_peer;
	int           qt   = nb->nb_qtype;
	int           result;

	M0_PRE(ma_is_locked(ma) && ma_invariant(ma) && buf_invariant(buf));
	M0_PRE(nb->nb_offset == 0); /* Do not support an offset during add. */
	M0_PRE((nb->nb_flags & M0_NET_BUF_RETAIN) == 0);

	if (M0_IN(qt, (M0_NET_QT_MSG_SEND, M0_NET_QT_ACTIVE_BULK_SEND)))
		mover_init(w, ma, &writer_op);
	else if (qt == M0_NET_QT_ACTIVE_BULK_RECV)
		mover_init(w, ma, &get_op);
	w->m_buf = buf;
	switch (qt) {
	case M0_NET_QT_MSG_RECV:
		result = 0;
		break;
	case M0_NET_QT_MSG_SEND: {
		struct ep *ep = ep_net(nb->nb_ep);

		M0_ASSERT(nb->nb_length <= m0_vec_count(&nb->nb_buffer.ov_vec));
		peer->bd_addr = ep->e_a;
		result = ep_add(ep, w);
		break;
	}
	case M0_NET_QT_PASSIVE_BULK_RECV: /* For passive buffers, generate */
	case M0_NET_QT_PASSIVE_BULK_SEND: /* the buffer descriptor. */
		m0_cookie_new(&buf->b_cookie);
		result = bdesc_create(&ma_src(ma)->e_a, buf, &nb->nb_desc);
		break;
	case M0_NET_QT_ACTIVE_BULK_RECV: /* For active buffers, decode the */
	case M0_NET_QT_ACTIVE_BULK_SEND: /* passive buffer descriptor. */
		result = bdesc_decode(&nb->nb_desc, peer);
		if (result == 0) {
			struct ep *ep; /* Passive peer end-point. */
			result = ep_create(ma, &peer->bd_addr, NULL, &ep);
			if (result == 0) {
				result = ep_add(ep, w);
				EP_PUT(ep, find);
			}
		}
		break;
	default:
		M0_IMPOSSIBLE("invalid queue type: %x", qt);
		break;
	}
	if (result != 0) {
		mover_fini(w);
		M0_SET0(&buf->b_peer);
	} else {
		buf->b_rc = 0;
		buf->b_last = m0_time_now();
		buf->b_gen++;
	}
	M0_POST(ma_is_locked(ma) && ma_invariant(ma) && buf_invariant(buf));
	TLOG(B_F, B_P(buf));
	return M0_RC(result);
}

/**
 * Cancels a buffer operation..
 *
 * Used as m0_net_xprt_ops::xo_buf_del().
 *
 * @see m0_net_buffer_del().
 */
static void buf_del(struct m0_net_buffer *nb)
{
	struct buf  *buf = nb->nb_xprt_private;
	struct ma   *ma  = buf_ma(buf);

	M0_PRE(ma_is_locked(ma) && ma_invariant(ma) && buf_invariant(buf));
	TLOG(B_F, B_P(buf));
	nb->nb_flags |= M0_NET_BUF_CANCELLED;
	buf_terminate(buf, -ECANCELED);
}

static int bev_deliver_sync(struct m0_net_transfer_mc *tm)
{
	return 0;
}

static void bev_deliver_all(struct m0_net_transfer_mc *tm)
{
	struct ma *ma = tm->ntm_xprt_private;
	ma_buf_done(ma);
	ma->t_delivery_chan = NULL;
}

static bool bev_pending(struct m0_net_transfer_mc *tm)
{
	struct ma *ma = tm->ntm_xprt_private;
	M0_PRE(ma_is_locked(ma) && ma_invariant(ma));
	return !b_tlist_is_empty(&ma->t_done);
}

static void bev_notify(struct m0_net_transfer_mc *tm, struct m0_chan *chan)
{
	struct ma *ma = tm->ntm_xprt_private;
	M0_PRE(ma_is_locked(ma) && ma_invariant(ma));
	M0_PRE(ma->t_delivery_chan == NULL);
	ma->t_delivery_chan = chan;
}

/**
 * Maximal number of bytes in a buffer.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_size()
 *
 * @see m0_net_domain_get_max_buffer_size()
 */
static m0_bcount_t get_max_buffer_size(const struct m0_net_domain *dom)
{
	if (MOCK_LNET)
		return 1024*1024;
	else
		/* There is no real limit. Return an arbitrary large number. */
		return M0_BCOUNT_MAX / 2;
}

/**
 * Maximal number of bytes in a buffer segment.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_segment_size()
 *
 * @see m0_net_domain_get_max_buffer_segment_size()
 */
static m0_bcount_t get_max_buffer_segment_size(const struct m0_net_domain *dom)
{
	if (MOCK_LNET)
		return 4096;
	else
		/* There is no real limit. Return an arbitrary large number. */
		return M0_BCOUNT_MAX / 2;
}

/**
 * Maximal number of segments in a buffer
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_segments()
 *
 * @see m0_net_domain_get_max_buffer_segments()
 */
static int32_t get_max_buffer_segments(const struct m0_net_domain *dom)
{
	if (MOCK_LNET)
		return 512;
	else
		/* There is no real limit. Return an arbitrary large number. */
		return INT32_MAX / 2; /* Beat this, LNet! */
}

/**
 * Maximal number of bytes in a buffer descriptor.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_desc_size()
 *
 * @see m0_net_domain_get_max_buffer_desc_size()
 */
static m0_bcount_t get_max_buffer_desc_size(const struct m0_net_domain *dom)
{
	return sizeof(struct bdesc);
}

static m0_bcount_t get_rpc_max_seg_size(struct m0_net_domain *ndom)
{
	if (MOCK_LNET)
		return 4096;
	else
		return default_xo_rpc_max_seg_size(ndom);
}

static uint32_t get_rpc_max_segs_nr(struct m0_net_domain *ndom)
{
	if (MOCK_LNET)
		return 512;
	else
		return default_xo_rpc_max_segs_nr(ndom);
}

/** Processes a "readable" event for a socket. */
static int sock_in(struct sock *s)
{
	M0_PRE(sock_invariant(s) && s->s_sm.sm_state == S_OPEN);
	TLOG(SOCK_F, SOCK_P(s));
	s->s_flags |= HAS_READ;
	return mover_op(&s->s_reader, s, M_READ);
}

/** Processes a "writable" event for a socket. */
static void sock_out(struct sock *s)
{
	struct mover *w;
	int           state;

	M0_PRE(sock_invariant(s));
	TLOG(SOCK_F, SOCK_P(s));
	s->s_flags |= HAS_WRITE;
	/*
	 * @todo this can monopolise processor. Consider breaking out of this
	 * loop after some number of iterations.
	 */
	while ((s->s_flags & HAS_WRITE) &&
	       (w = m_tlist_head(&s->s_ep->e_writer)) != NULL) {
		state = mover_op(w, s, M_WRITE);
		if (state != R_DONE && w->m_sock != s)
			m_tlist_move_tail(&s->s_ep->e_writer, w);
	}
}

/** Processes an "error" event for a socket. */
static void sock_close(struct sock *s)
{
	M0_PRE(sock_invariant(s));
	TLOG(SOCK_F, SOCK_P(s));
	mover_op(&s->s_reader, s, M_CLOSE);
	if (sock_writer(s) != NULL)
		mover_op(sock_writer(s), s, M_CLOSE);
}

/** Returns the writer locked to the socket, if any. */
static struct mover *sock_writer(struct sock *s)
{
	struct mover *w = m_tlist_head(&s->s_ep->e_writer);

	return w != NULL && w->m_sock == s ? w : NULL;
}

/**
 * Frees the socket.
 *
 * @see sock_done().
 */
static void sock_fini(struct sock *s)
{
	struct ma *ma = ep_ma(s->s_ep);

	M0_PRE(ma_is_locked(ma));
	M0_PRE(s->s_ep != NULL);
	M0_PRE(s->s_reader.m_sm.sm_conf == NULL);
	M0_PRE(s->s_sm.sm_conf != NULL);
	M0_PRE(s->s_sm.sm_state == S_DELETED);
	M0_PRE(s_tlist_contains(&ma->t_deathrow, s));

	TLOG(SOCK_F, SOCK_P(s));
	EP_PUT(s->s_ep, sock);
	s->s_ep = NULL;
	m0_sm_fini(&s->s_sm);
	s_tlink_del_fini(s);
	m0_free(s);
}

/**
 * Finalises the socket.
 *
 * @see sock_fini().
 */
static void sock_done(struct sock *s, bool balance)
{
	struct ma *ma = ep_ma(s->s_ep);

	M0_PRE(ma_is_locked(ma));
	M0_PRE(s->s_ep != NULL);
	M0_PRE(s->s_sm.sm_conf != NULL);
	M0_PRE(sock_invariant(s));

	TLOG(SOCK_F, SOCK_P(s));
	/* This function can be called multiple times, should be idempotent. */
	if (s->s_reader.m_sm.sm_conf != NULL) {
		if (s->s_fd > 0)
			sock_close(s);
		if (s->s_sm.sm_state != S_DELETED) { /* sock_close()    */
			mover_fini(&s->s_reader);    /* might finalise. */
			M0_ASSERT(sock_writer(s) == NULL);
			if (s->s_fd > 0) {
				int result = sock_ctl(s, EPOLL_CTL_DEL, 0);
				if (result != 0 && result != -ENOENT)
					M0_LOG(M0_ERROR, "Strange errno: %i.",
					       result);
				SOP(shutdown, s->s_fd, SHUT_RDWR);
				/*
				 * What if epoll_ctl(CTL_DEL) and close(2) fail
				 * for some obscure reason? The kernel continues
				 * to deliver epoll(2) notifications for the
				 * socket, but the sock structure is no longer
				 * here. Bad. At least notify the user.
				 */
				result = SOP(close, s->s_fd);
				if (result != 0)
					M0_LOG(M0_ERROR, "Cannot close: %i.",
					       -errno);
				s->s_fd = -1;
			}
			m0_sm_state_set(&s->s_sm, S_DELETED);
			s_tlist_move(&ma->t_deathrow, s);
			if (balance)
				(void)ep_balance(s->s_ep);
		}
	}
}

/**
 * Allocates and initialises a new socket between given endpoints.
 *
 * "src" is the source end-point, the "self" end-point of the local transfer
 * machine.
 *
 * "tgt" is the end-point to which the connection is establishes. If "tgt" is
 * NULL, the socket listening for incoming connections is established.
 *
 * If "fd" is negative, a new socket is created and connected (without
 * blocking). Otherwise (fd >= 0), the socket already exists (returned from
 * accept4(2), see sock_event()), and a sock structure should be created for it.
 */
static int sock_init(int fd, struct ep *src, struct ep *tgt, uint32_t flags)
{
	struct ma   *ma = ep_ma(src);
	struct ep   *ep = tgt ?: src;
	struct sock *s;
	int          st = src->e_a.a_socktype;
	int          result;
	int          state = S_INIT;

	M0_PRE(ma_is_locked(ma));
	M0_PRE((flags & ~(EPOLLOUT|EPOLLET)) == 0);
	M0_PRE(src != NULL);
	M0_PRE(M0_IN(ma->t_ma->ntm_ep, (NULL, &src->e_ep)));
	M0_PRE(ergo(tgt != NULL, ma == ep_ma(tgt) &&
		    src->e_a.a_family   == tgt->e_a.a_family &&
		    src->e_a.a_socktype == tgt->e_a.a_socktype &&
		    src->e_a.a_protocol == tgt->e_a.a_protocol));
	M0_ALLOC_PTR(s);
	if (s == NULL)
		return M0_ERR(-ENOMEM);
	s->s_ep = ep;
	EP_GET(ep, sock);
	s_tlink_init_at(s, &ep->e_sock);
	m0_sm_init(&s->s_sm, &sock_conf, state, &ma->t_ma->ntm_group);
	m0_sm_addb2_counter_init(&s->s_sm);
	mover_init(&s->s_reader, ma, stype[ep->e_a.a_socktype].st_reader);
	s->s_reader.m_sock = s;
	result = sock_init_fd(fd, s, src, flags);
	if (result == 0) {
		if (fd >= 0) {
			state = S_OPEN;
		} else if (tgt == NULL) {
			/* Listening. */
			if (st == SOCK_STREAM) {
				result = SOP(listen, s->s_fd, 128);
				if (result == 0)
					/*
					 * Will be readable on an incoming
					 * connection.
					 */
					state = S_LISTENING;
				else
					result = M0_ERR(-errno);
			} else {
				M0_ASSERT(st == SOCK_DGRAM);
				state = S_OPEN;
			}
		} else {
			struct sockaddr_storage sa = {};
			M0_ASSERT(M0_IN(st, (SOCK_STREAM, SOCK_DGRAM)));
			/* Connecting. */
			addr_encode(&ep->e_a, (void *)&sa);
			result = SOP(connect, s->s_fd, (void *)&sa, sizeof sa);
			if (result == 0) {
				state = S_OPEN;
			} else if (errno == EINPROGRESS) {
				/* Datagram sockets connect instantly. */
				M0_ASSERT(st != SOCK_DGRAM);
				/*
				 * Will be writable on a successful connection,
				 * will be writable and readable on a failure.
				 */
				state = S_CONNECTING;
				result = 0;
			} else
				result = M0_ERR(-errno);
		}
		if (result == 0)
			m0_sm_state_set(&s->s_sm, state);
	}
	if (result != 0)
		sock_done(s, false);
	M0_POST(ergo(result == 0, s != NULL && sock_invariant(s)));
	return M0_RC(result);
}

/**
 * A helper for sock_init().
 *
 * Create the socket, set options, bind(2) if necessary.
 */
static int sock_init_fd(int fd, struct sock *s, struct ep *ep, uint32_t flags)
{
	int result = 0;

	if (fd < 0) {
		int flag = true;

		fd = SOP(socket, ep->e_a.a_family,
			 /* Linux: set NONBLOCK immediately. */
			 ep->e_a.a_socktype | SOCK_NONBLOCK,
			 ep->e_a.a_protocol);
		/*
		 * Perhaps set some other socket options here? SO_LINGER, etc.
		 */
		if (fd >= 0) {
			/* EPOLLET means the socket is for listening. */
			if (flags & EPOLLET) {
				struct sockaddr_storage sa = {};

				result = SOP(setsockopt, fd, SOL_SOCKET,
					     SO_REUSEADDR, &flag, sizeof flag);
				if (result == 0) {
					addr_encode(&ep->e_a, (void *)&sa);
					result = SOP(bind, fd, (void *)&sa,
						     sizeof sa);
				} else
					result = M0_ERR(-errno);
			} else
				result = 0;
		}
	}
	if (fd >= 0 && result == 0) {
		s->s_fd = fd;
		result = sock_ctl(s, EPOLL_CTL_ADD, flags & ~EPOLLET);
	}
	if (result != 0 || fd < 0)
		result = M0_ERR(-errno);
	return M0_RC(result);
}

/**
 * Implements a state transition in a socket state machine.
 *
 * Returns true iff the event wasn't processed because of buffer shortage. The
 * caller must re-try.
 */
static bool sock_event(struct sock *s, uint32_t ev)
{
	enum { EV_ERR = EPOLLRDHUP|EPOLLERR|EPOLLHUP };
	struct sockaddr_storage sa = {};
	struct addr             addr;
	int                     result;

	M0_PRE(sock_invariant(s));
	M0_LOG(M0_DEBUG, "State: %x, event: %x.", s->s_sm.sm_state, ev);
	TLOG(SOCK_F": %x", SOCK_P(s), ev);
	switch (s->s_sm.sm_state) {
	case S_INIT:
	case S_DELETED:
	default:
		M0_IMPOSSIBLE("Wrong state: %x.", s->s_sm.sm_state);
		break;
	case S_LISTENING:
		if (ev == EPOLLIN) { /* A new incoming connection. */
			int        fd;
			socklen_t  socklen = sizeof sa;
			struct ep *we = s->s_ep;

			addr = we->e_a; /* Copy family, socktype, proto. */
			fd = SOP(accept4, s->s_fd,
				 (void *)&sa, &socklen, SOCK_NONBLOCK);
			if (fd >= 0) {
				struct ep *ep = NULL;

				addr_decode(&addr, (void *)&sa);
				M0_ASSERT(addr_invariant(&addr));
				result = ep_create(ep_ma(we),
						   &addr, NULL, &ep) ?:
					/*
					 * Accept incoming connections
					 * unconditionally. Alternatively, it
					 * can be rejected by some admission
					 * policy.
					 */
					sock_init(fd, we, ep, 0);
				if (result != 0)
					/*
					 * Maybe already closed in sock_init()
					 * failure path, do not care.
					 */
					SOP(close, fd);
				if (ep != NULL)
					EP_PUT(ep, find);
			} else if (M0_IN(errno, (EWOULDBLOCK,  /* BSD */
						 ECONNABORTED, /* POSIX */
						 EPROTO)) ||   /* SVR4 */
				   M0_IN(errno,
	/*
	 * Linux accept() (and accept4()) passes already-pending network errors
	 * on the new socket as an error code from accept(). This behavior
	 * differs from other BSD socket implementations. For reliable operation
	 * the application should detect the network errors defined for the
	 * protocol after accept() and treat them like EAGAIN by retrying.  In
	 * the case of TCP/IP, these are... -- https://manpath.be/f14/2/accept4
	 */
						(ENETDOWN, EPROTO, ENOPROTOOPT,
						 EHOSTDOWN, ENONET,
						 EHOSTUNREACH, EOPNOTSUPP,
						 ENETUNREACH))) {
				M0_LOG(M0_DEBUG, "Got: %i.", errno);
			} else {
				M0_LOG(M0_ERROR, "Got: %i.", errno);
			}
		} else
			M0_LOG(M0_ERROR,
			       "Unexpected event while listening: %x.", ev);
		break;
	case S_CONNECTING:
		if ((ev & (EPOLLOUT|EPOLLIN)) == EPOLLOUT) {
			/* Successful connection. */
			m0_sm_state_set(&s->s_sm, S_OPEN);
		} else if ((ev & (EPOLLOUT|EPOLLIN)) == (EPOLLOUT|EPOLLIN) ||
			   /*
			    * Contrary to Messrs. G. R. Wright and W. R. Stevens
			    * (TCP/IP Illustrated, Volume 2, 1995, 27.7; UNIX
			    * Network Programming Volume 1, 2003, p. 547) and
			    * the rest of the world, Linux sometimes fails a
			    * connection by setting some of the error bits
			    * instead of EPOLLOUT.
			    */
			   (ev & EV_ERR) != 0) {
			/* Failed connection. */
			sock_done(s, false);
		} else {
			M0_LOG(M0_ERROR,
			       "Unexpected event while connecting: %x.", ev);
		}
		break;
	case S_OPEN:
		if (ev & EPOLLIN) {
			/* Ran out of buffer on the receive queue. */
			if (sock_in(s) == -ENOBUFS)
				return true;
		}
		if (s->s_sm.sm_state == S_OPEN && ev & EPOLLOUT)
			sock_out(s);
		if (s->s_sm.sm_state == S_OPEN && ev & EV_ERR)
			sock_close(s);
		break;
	}
	if (ev & ~(EPOLLIN|EPOLLOUT|EV_ERR))
		M0_LOG(M0_ERROR, "Unexpected event: %x.", ev);
	return false;
}

/**
 * Adds, removes a socket to the epoll instance, or modifies which events are
 * monitored.
 */
static int sock_ctl(struct sock *s, int op, uint32_t flags)
{
	int result;

	/* Always monitor errors. */
	flags |= EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
	result = SOP(epoll_ctl, ep_ma(s->s_ep)->t_epollfd, op, s->s_fd,
		     &(struct epoll_event){
			     .events = flags,
			     .data   = { .ptr = s }});
	if (result == 0) {
		if ((flags & EPOLLOUT) != 0)
			s->s_flags |= WRITE_POLL;
		else
			s->s_flags &= ~WRITE_POLL;
	} else
		result = M0_ERR(-errno);
	return result;
}

/**
 * Returns the end-point with a given address.
 *
 * Either finds and returns an existing end-point with elevated reference
 * count or creates a new end-point.
 *
 * "name" can be provided by the caller. If it is NULL, the canonical name
 * (addr_print()) is used.
 */
static int ep_create(struct ma *ma, struct addr *addr, const char *name,
		     struct ep **out)
{
	struct ep               *ep;
	struct m0_net_end_point *net;
	char                    *cname;

	M0_PRE(ma_is_locked(ma) && addr_invariant(addr));
	/*
	 * @todo this is duplicated in every transport. Should be factored out
	 * by exporting ->xo_ep_eq() method.
	 */
	m0_tl_for(m0_nep, &ma->t_ma->ntm_end_points, net) {
		struct ep *xep = ep_net(net);
		if (ep_eq(xep, addr)) {
			EP_GET(xep, find);
			*out = xep;
			return M0_RC(0);
		}
	} m0_tl_endfor;
	cname = name != NULL ? m0_strdup(name) : addr_print(addr);
	M0_ALLOC_PTR(ep);
	if (cname == NULL || ep == NULL) {
		m0_free(ep);
		m0_free(cname);
		return M0_ERR(-ENOMEM);
	}
	net = &ep->e_ep;
	m0_ref_init(&net->nep_ref, 1, &ep_release);
#ifdef EP_DEBUG
	ep->e_r_find = 1;
#endif
	net->nep_tm = ma->t_ma;
	m0_nep_tlink_init_at_tail(net, &ma->t_ma->ntm_end_points);
	s_tlist_init(&ep->e_sock);
	m_tlist_init(&ep->e_writer);
	net->nep_addr = cname;
	ep->e_a = *addr;
	*out = ep;
	M0_POST(*out != NULL && ep_invariant(*out));
	M0_POST(ma_is_locked(ma));
	TLOG(EP_FL, EP_PL(ep));
	return M0_RC(0);
}

/** Returns (finds or creates) the end-point with the given name. */
static int ep_find(struct ma *ma, const char *name, struct ep **out)
{
	struct addr addr = {};

	return addr_resolve(&addr, name) ?: ep_create(ma, &addr, name, out);
}

/** Returns end-point transfer machine. */
static struct ma *ep_ma(struct ep *ep)
{
	M0_PRE(ep != NULL);
	return ep->e_ep.nep_tm->ntm_xprt_private;
}

/** Converts generic end-point to its sock structure. */
static struct ep *ep_net(struct m0_net_end_point *net)
{
	return container_of(net, struct ep, e_ep);
}

/**
 * End-point finalisation call-back.
 *
 * Used as m0_net_end_point::nep_ref::release(). This call-back is called when
 * end-point reference count drops to 0.
 */
static void ep_release(struct m0_ref *ref)
{
	ep_free(container_of(ref, struct ep, e_ep.nep_ref));
}

/**
 * Adds a writer to an endpoint.
 *
 * If necessary creates a new socket to the endpoint.
 */
static int ep_add(struct ep *ep, struct mover *w)
{
	M0_PRE(ep_invariant(ep));
	M0_PRE(mover_invariant(w) && mover_is_writer(w));
	M0_PRE(w->m_ep == NULL);
	m_tlist_add_tail(&ep->e_writer, w);
	EP_GET(ep, mover);
	w->m_ep = ep;
	return M0_RC(ep_balance(ep));
}

/**
 * Removes a (completed) writer from its end-point.
 *
 * Stops monitoring end-point sockets for writes, when the last writer is
 * removed.
 */
static void ep_del(struct mover *w)
{
	struct ep *ep = w->m_ep;

	if (ep != NULL) {
		M0_PRE(ep_invariant(ep));
		M0_PRE(mover_invariant(w) && mover_is_writer(w));
		M0_PRE(M0_IN(w->m_sm.sm_state, (R_DONE, R_FAIL)));

		if (m_tlink_is_in(w)) {
			m_tlist_del(w);
			ep_balance(ep);
			EP_PUT(ep, mover);
		}
		w->m_ep = NULL;
	}
}

/** True if this end point accepts incoming connections. */
static bool ep_is_listening(const struct ep *ep)
{
	int state = ep->e_a.a_socktype == SOCK_STREAM ? S_LISTENING : S_OPEN;
	return m0_tl_exists(s, s, &ep->e_sock, s->s_sm.sm_state == state);
}
/**
 * Updates end-point when a writer is added or removed.
 *
 * If there are writers, but no sockets, open a socket.
 *
 * If there are sockets, but no writers, stop monitoring sockets for writer.
 */
static int ep_balance(struct ep *ep)
{
	int          result = 0;
	struct sock *s;

	if (m_tlist_is_empty(&ep->e_writer)) {
		/*
		 * No more writers.
		 *
		 * @todo Consider closing the sockets to this endpoint (after
		 * some time?).
		 */
		s = s_tlist_head(&ep->e_sock);
		if (s != NULL)
			result = sock_ctl(s, EPOLL_CTL_MOD, 0);
	} else {
		s = m0_tl_find(s, s, &ep->e_sock, M0_IN(s->s_sm.sm_state,
						(S_CONNECTING, S_OPEN)));
		if (s == NULL)
			result = sock_init(-1, ma_src(ep_ma(ep)), ep, EPOLLOUT);
		else {
			/*
			 * @todo Alternatively, more parallel sockets can be
			 * opened.
			 */
			/* Make sure that at least one socket is writable. */
			if (!(s->s_flags & WRITE_POLL))
				result = sock_ctl(s, EPOLL_CTL_MOD, EPOLLOUT);
		}
	}
	return result;
}

/** Finalises the end-point. */
static void ep_free(struct ep *ep)
{
	m0_nep_tlist_del(&ep->e_ep);
	m_tlist_fini(&ep->e_writer);
	s_tlist_fini(&ep->e_sock);
	m0_free((void *)ep->e_ep.nep_addr);
	m0_free(ep);
}

static void ep_put(struct ep *ep)
{
	m0_ref_put(&ep->e_ep.nep_ref);
}

static void ep_get(struct ep *ep)
{
	m0_ref_get(&ep->e_ep.nep_ref);
}

/**
 * Converts address name to address.
 *
 * Currently this involves only name parsing. In the future, things like dns
 * resolution can be added here.
 */
static int addr_resolve(struct addr *addr, const char *name)
{
	int result = addr_parse(addr, name);

	if (result == 0) {
		/*
		 * Currently only numberical ip addresses are supported (see
		 * addr_parse()). They do not require any resolving. In the
		 * future, use getaddrinfo(3).
		 */
		;
	}
	return M0_RC(result);
}

/**
 * Parses network address.
 *
 * The following address formats are supported:
 *
 *     - lnet compatible, see nlx_core_ep_addr_decode():
 *
 *           nid:pid:portal:tmid
 *
 *       for example: "10.0.2.15@tcp:12345:34:123" or
 *       "192.168.96.128@tcp1:12345:31:*"
 *
 *     - native sock format, see socket(2):
 *
 *           family:type:ipaddr[@port]
 *
 *       for example: "inet:stream:lanl.gov@23",
 *       "inet6:dgram:FE80::0202:B3FF:FE1E:8329@6663" or
 *       "unix:dgram:/tmp/socket".
 *
 */
static int addr_parse(struct addr *addr, const char *name)
{
	int result;

	if (name[0] == 0)
		result =  M0_ERR(-EPROTO);
	else if (name[0] < '0' || name[0] > '9')
		result = addr_parse_native(addr, name);
	else
		/* Lnet format. */
		result = addr_parse_lnet(addr, name);
	M0_POST(ergo(result == 0, addr_invariant(addr)));
	return M0_RC(result);
}

/**
 * Bitmap of used transfer machine identifiers.
 *
 * This is used to allocate unique transfer machine identifiers for LNet network
 * addresses with wildcard transfer machine identifier (like
 * "192.168.96.128@tcp1:12345:31:*").
 *
 * @todo Move it to m0 instance or make per-domain.
 */
static char autotm[1024] = {};

static int addr_parse_lnet(struct addr *addr, const char *name)
{
	struct sockaddr_in  sin;
	char               *at                  = strchr(name, '@');
	char                ip[INET_ADDRSTRLEN] = {};
	int                 nr;
	unsigned            pid;
	unsigned            portal;
	unsigned            tmid;

	if (strncmp(name, "0@lo", 4) == 0) {
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	} else {
		if (at == NULL || at - name >= sizeof ip)
			return M0_ERR(-EPROTO);
		memcpy(ip, name, at - name);
		if (inet_pton(AF_INET, ip, &sin.sin_addr) != 1)
			return M0_ERR(-EPROTO);
	}
	sin.sin_family = AF_INET;
	if ((at = strchr(at, ':')) == NULL) /* Skip 'tcp...:' bit. */
		return M0_ERR(-EPROTO);
	nr = sscanf(at + 1, "%u:%u:%u", &pid, &portal, &tmid);
	if (nr != 3) {
		nr = sscanf(at + 1, "%u:%u:*", &pid, &portal);
		if (nr != 2)
			return M0_ERR(-EPROTO);
		for (nr = 0; nr < ARRAY_SIZE(autotm); ++nr) {
			if (autotm[nr] == 0) {
				tmid = nr;
				break;
			}
		}
		if (nr == ARRAY_SIZE(autotm))
			return M0_ERR(-EADDRNOTAVAIL);
	}
	/*
	 * Hard-code LUSTRE_SRV_LNET_PID to avoid dependencies on the Lustre
	 * headers.
	 */
	if (pid != 12345)
		return M0_ERR(-EPROTO);
	/*
	 * Deterministically combine portal and tmid into a unique 16-bit port
	 * number (greater than 1024). Tricky.
	 *
	 * Port number is, in little-endian binary: tttttttttt1ppppp, that is,
	 * 10 bits of tmid (which must be less than 1024), followed by a set bit
	 * (guaranteeing that the port is not reserved), followed by 5 bits of
	 * (portal - 30), so that portal must be in the range 30..61.
	 */
	if (tmid >= 1024 || (portal - 30) >= 32)
		return M0_ERR_INFO(-EPROTO,
				   "portal: %u, tmid: %u", portal, tmid);
	sin.sin_port     = htons(tmid | (1 << 10) | ((portal - 30) << 11));
	addr->a_family   = PF_INET;
	addr->a_socktype = SOCK_STREAM;
	addr->a_protocol = IPPROTO_TCP;
	autotm[tmid] = 1;
	addr_decode(addr, (void *)&sin);
	return M0_RC(0);
}

static int addr_parse_native(struct addr *addr, const char *name)
{
	int   shift;
	int   result;
	int   f;
	int   s;
	long  port;
	char *at;
	char *end;
	char  ip[INET6_ADDRSTRLEN] = {};

	for (f = 0; f < ARRAY_SIZE(pf); ++f) {
		if (pf[f].f_name != NULL) {
			shift = strlen(pf[f].f_name);
			if (strncmp(name, pf[f].f_name, shift) == 0)
				break;
		}
	}
	if (f == ARRAY_SIZE(pf) || name[shift] != ':')
		return M0_ERR(-EINVAL);
	name += shift + 1;
	for (s = 0; s < ARRAY_SIZE(stype); ++s) {
		if (stype[s].st_name != NULL) {
			shift = strlen(stype[s].st_name);
			if (strncmp(name, stype[s].st_name, shift) == 0)
				break;
		}
	}
	if (s == ARRAY_SIZE(stype) || name[shift] != ':')
		return M0_ERR(-EINVAL);
	name += shift + 1;
	at = strchr(name, '@');
	if (at == NULL) {
		/* XXX @todo: default port? */
		return M0_ERR(-EINVAL);
	} else {
		port = strtol(at + 1, &end, 10);
		if (*end != 0)
			return M0_ERR(-EINVAL);
		if (port < 0 || port > USHRT_MAX)
			return M0_ERR(-ERANGE);
	}
	memcpy(ip, name, min64(at - name, ARRAY_SIZE(ip) - 1));
	result = inet_pton(f, ip, addr->a_data.v_data);
	if (result == 0)
		return M0_ERR(-EINVAL);
	if (result == -1)
		return M0_ERR(-errno);
	addr->a_family   = f;
	addr->a_socktype = s;
	addr->a_protocol = stype[s].st_proto;
	addr->a_port     = port;
	M0_POST(addr_invariant(addr));
	return 0;
}

/** Encodes an addr structure in an ipv4 sockaddr. */
static void ip4_encode(const struct addr *a, struct sockaddr *sa)
{
	struct sockaddr_in *sin = (void *)sa;

	M0_SET0(sin);
#if 0 /* BSD Reno. */
	sin->sin_len         = sizeof *sin;
#endif
	sin->sin_port        = htons(a->a_port);
	sin->sin_addr.s_addr = *(uint32_t *)&a->a_data.v_data[0];
}

/** Fills an addr struct from an ipv4 sockaddr. */
static void ip4_decode(struct addr *a, const struct sockaddr *sa)
{
	const struct sockaddr_in *sin = (void *)sa;

	a->a_port = ntohs(sin->sin_port);
	*((uint32_t *)&a->a_data.v_data[0]) = sin->sin_addr.s_addr;
}

/** Encodes an addr structure in an ipv6 sockaddr. */
static void ip6_encode(const struct addr *a, struct sockaddr *sa)
{
	struct sockaddr_in6 *sin6 = (void *)sa;

	M0_SET0(sin6);
#if 0 /* BSD Reno. */
	sin6->sin6_len         = sizeof *sin6;
#endif
	sin6->sin6_port        = htons(a->a_port);
	memcpy(sin6->sin6_addr.s6_addr, a->a_data.v_data,
	       sizeof sin6->sin6_addr.s6_addr);
}

/** Fills an addr struct from an ipv6 sockaddr. */
static void ip6_decode(struct addr *a, const struct sockaddr *sa)
{
	struct sockaddr_in6 *sin6 = (void *)sa;

	a->a_port = ntohs(sin6->sin6_port);
	memcpy(a->a_data.v_data, sin6->sin6_addr.s6_addr,
	       sizeof sin6->sin6_addr.s6_addr);
}

/** Returns the canonical name for an addr. */
static char *addr_print(const struct addr *a)
{
	char *name;
	int   nob;
	struct sockaddr_storage sa = {};

	enum { MAX_LEN = sizeof("inet6:stream:@65536") + INET6_ADDRSTRLEN };
	M0_PRE(addr_invariant(a));

	name = m0_alloc(MAX_LEN);
	if (name == NULL)
		return NULL;
	nob = snprintf(name, MAX_LEN, "%s:%s:",
		       pf[a->a_family].f_name, stype[a->a_socktype].st_name);
	M0_ASSERT(nob < MAX_LEN);
	addr_encode(a, (void *)&sa);
	switch (a->a_family) {
	case AF_INET: {
		struct sockaddr_in *sin = (void *)&sa;

		inet_ntop(AF_INET, &sin->sin_addr, name + nob, MAX_LEN - nob);
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 *sin = (void *)&sa;

		inet_ntop(AF_INET6, &sin->sin6_addr, name + nob, MAX_LEN - nob);
		break;
	}
	default:
		M0_IMPOSSIBLE("Wrong family: %i.", a->a_family);
	}
	nob = strlen(name);
	M0_ASSERT(nob < MAX_LEN);
	nob += snprintf(name + nob, MAX_LEN - nob, "@%i", a->a_port);
	M0_ASSERT(nob < MAX_LEN);
	return name;
}

/** Constructs an addr structure from a sockaddr. */
static void addr_decode(struct addr *addr, const struct sockaddr *sa)
{
	M0_PRE(IS_IN_ARRAY(addr->a_family, pf));
	M0_PRE(pf[addr->a_family].f_name != NULL);
	M0_SET0(&addr->a_data);
	pf[addr->a_family].f_decode(addr, sa);
}

/** Encodes an addr in a sockaddr. */
static void addr_encode(const struct addr *addr, struct sockaddr *sa)
{
	M0_PRE(IS_IN_ARRAY(addr->a_family, pf));
	M0_PRE(pf[addr->a_family].f_name != NULL);
	pf[addr->a_family].f_encode(addr, sa);
	sa->sa_family = addr->a_family;
}

/** Returns true iff 2 addresses are equal. */
static bool addr_eq(const struct addr *a0, const struct addr *a1)
{
	return  a0->a_family    == a1->a_family &&
		a0->a_protocol  == a1->a_protocol &&
		a0->a_socktype  == a1->a_socktype &&
		memcmp(a0->a_data.v_data, a1->a_data.v_data,
		       ARRAY_SIZE(a0->a_data.v_data)) == 0;
}

/** Returns true iff an end-point has a given addr. */
static bool ep_eq(const struct ep *ep, const struct addr *a0)
{
	const struct addr *a1 = &ep->e_a;

	return addr_eq(a0, a1) && a0->a_port == a1->a_port;
}

static struct ma *buf_ma(struct buf *buf)
{
	return buf->b_buf->nb_tm->ntm_xprt_private;
}

/**
 * Checks that a valid incoming packet (m->m_pk) is received for "buf".
 *
 * If first packet is received for the buffer, setup the buffer (initialise
 * bitmap, set peer, etc.). Otherwise, check that the packet matches already
 * ongoing transfer.
 */
static int buf_accept(struct buf *buf, struct mover *m)
{
	struct packet *p      = &m->m_pk;
	struct bdesc  *src    = &p->p_src;
	m0_bcount_t    length = m0_vec_count(&buf->b_buf->nb_buffer.ov_vec);
	int            result = 0;

	M0_PRE(mover_invariant(m) && mover_is_reader(m));

	if (p->p_offset + p->p_size > length)
		return M0_ERR(-EMSGSIZE);
	if (p->p_totalsize > length)
		return M0_ERR(-EMSGSIZE);
	if (buf->b_rc != 0)
		return M0_RC(buf->b_rc);
	if (buf->b_done.b_nr == 0) {
		result = m0_bitmap_init(&buf->b_done, p->p_nr);
		if (result != 0)
			return result;
		result = ep_create(buf_ma(buf),
				   &src->bd_addr, NULL, &buf->b_other);
		if (result == 0) {
			buf_hist(buf, M0_AVI_SOCK_Q_ACCEPT);
#ifdef EP_DEBUG
			M0_CNT_DEC(buf->b_other->e_r_find);
			M0_CNT_INC(buf->b_other->e_r_buf);
#endif
		}
		buf->b_peer   = *src;
		buf->b_length = p->p_totalsize;
	} else if (buf->b_done.b_nr != p->p_nr) {
		result = M0_ERR(-EPROTO);
	} else if (buf->b_length != p->p_totalsize) {
		result = M0_ERR(-EPROTO);
	} else if (buf->b_other != NULL && !ep_eq(buf->b_other, &src->bd_addr)){
		result = M0_ERR(-EPROTO);
	} else if (memcmp(&buf->b_peer, src, sizeof *src)) {
		result = M0_ERR(-EPROTO);
	} else if (m0_bitmap_get(&buf->b_done, p->p_idx)) {
		result = M0_ERR(-EPROTO);
	}
	if (result == 0) {
		m->m_buf = buf;
		m->m_gen = buf->b_gen;
	}
	return result;
}

/**
 * Terminates the buffer due to some abnormal condition: timeout,
 * cancellation. Socket errors and normal byffer completion are handled
 * separately in mover_op() and buf_done().
 *
 * After this function completes, all further incoming packets to this buffer
 * and all attempts to write from this buffer will be rejected, until the buffer
 * is re-queued.
 */
static void buf_terminate(struct buf *buf, int rc)
{
	struct sock *sock;

	M0_PRE(rc != 0);
	if (buf->b_rc == 0) {
		/*
		 * Close the writer. This triggers ->v_error() and ->st_error().
		 */
		mover_fail(&buf->b_writer, buf->b_writer.m_sock, rc);
		if (buf->b_other != NULL) {
			m0_tl_for(s, &buf->b_other->e_sock, sock) {
				if (sock->s_reader.m_buf == buf)
					mover_fail(&sock->s_reader, sock, rc);
			} m0_tl_endfor;
		}
		buf_done(buf, rc);
	}
}

/** Completes the buffer operation. */
static void buf_done(struct buf *buf, int rc)
{
	struct ma *ma = buf_ma(buf);

	M0_PRE(ma_is_locked(ma) && ma_invariant(ma) && buf_invariant(buf));
	M0_PRE(ergo(buf->b_other != NULL,
		    m0_tl_forall(s, sock, &buf->b_other->e_sock,
				 sock->s_reader.m_buf != buf)));

	/*printf("done: %p[%i] %" PRIi64 " %i\n", buf,
	       buf->b_buf != NULL ? buf->b_buf->nb_qtype : -1,
	       buf->b_buf != NULL ? buf->b_buf->nb_length : -1, rc); */
	if (buf->b_writer.m_sm.sm_rc == 0) /* Reuse this field for result. */
		buf->b_writer.m_sm.sm_rc = rc;
	if (buf->b_done.b_words > 0)
		m0_bitmap_fini(&buf->b_done);
	/*
	 * Multiple buf_done() calls on the same buffer are possible if the
	 * buffer is cancelled or times out.
	 */
	if (buf->b_rc == 0) {
		M0_ASSERT(!b_tlink_is_in(buf));
		buf->b_rc = rc;
		b_tlist_add_tail(&ma->t_done, buf);
		buf_hist(buf, M0_AVI_SOCK_Q_DONE);
		if (ma->t_delivery_chan != NULL)
			m0_chan_signal_lock(ma->t_delivery_chan);
	}
}

/** Invokes completion call-back (releasing tm lock). */
static void buf_complete(struct buf *buf)
{
	struct m0_net_buffer *nb    = buf->b_buf;
	struct ma            *ma    = buf_ma(buf);
	int                   qtype = nb->nb_qtype;
	m0_time_t             start;
	struct m0_net_buffer_event ev = {
		.nbe_buffer = nb,
		.nbe_status = buf->b_rc == -EALREADY ? 0 : buf->b_rc,
		.nbe_time   = m0_time_now()
	};
	if (M0_IN(qtype, (M0_NET_QT_MSG_RECV, M0_NET_QT_PASSIVE_BULK_RECV,
			  M0_NET_QT_ACTIVE_BULK_RECV))) {
		ev.nbe_length = buf->b_length;
	}
	if (qtype == M0_NET_QT_MSG_RECV) {
		if (ev.nbe_status == 0 && buf->b_other != NULL) {
			ev.nbe_ep = &buf->b_other->e_ep;
			EP_GET(buf->b_other, find);
		}
		ev.nbe_offset = 0; /* Starting offset not supported. */
	}
	ma->t_ma->ntm_callback_counter++;
	/*printf("DONE: %p[%i] %" PRIi64 " %i\n", buf,
	  buf->b_buf != NULL ? buf->b_buf->nb_qtype : -1,
	  buf->b_length, ev.nbe_status); */
	TLOG(B_F" nb: %p %i", B_P(buf), buf->b_buf, ev.nbe_status);
	/*
	 * It's ok to clear buf state, because sock doesn't support
	 * M0_NET_BUF_RETAIN flag and the buffer will be unqueued
	 * unconditionally.
	 */
	mover_fini(&buf->b_writer);
	if (buf->b_done.b_nr > 0)
		m0_bitmap_fini(&buf->b_done);
	b_tlink_fini(buf);
	if (buf->b_other != NULL) {
		EP_PUT(buf->b_other, buf);
		buf->b_other = NULL;
	}
	M0_SET0(&buf->b_peer);
	buf->b_offset = 0;
	buf->b_length = 0;
	M0_ASSERT(ma_invariant(ma));
	M0_ADDB2_ADD(M0_AVI_SOCK_BUF_EVENT, (uint64_t)buf, qtype,
		     msec(ev.nbe_time, nb->nb_add_time),
		     ev.nbe_status, ev.nbe_length);
	buf_hist(buf, M0_AVI_SOCK_Q_CB_START);
	start = buf->b_last;
	ma_unlock(ma);
	m0_net_buffer_event_post(&ev);
	ma_lock(ma);
	/*
	 * Update histogram manually---the buffer can be already de-allocated at
	 * this point.
	 */
	ma_hist(ma, qtype, M0_AVI_SOCK_Q_CB_END, m0_time_now(), start);
	M0_ASSERT(ma_invariant(ma));
	M0_ASSERT(M0_IN(ma->t_ma->ntm_state, (M0_NET_TM_STARTED,
					      M0_NET_TM_STOPPING)));
	ma->t_ma->ntm_callback_counter--;
}

/** Updates per-queue histogram for the buffer. */
static void buf_hist(struct buf *buf, int idx)
{
	m0_time_t now = m0_time_now();
	ma_hist(buf_ma(buf), buf->b_buf->nb_qtype, idx, now, buf->b_last);
	buf->b_last = now;
}

/** Updates per-queue histogram. */
static void ma_hist(struct ma *ma, int qtype, int idx,
		    m0_time_t end, m0_time_t start)
{
	int idx0 = qtype * M0_AVI_SOCK_Q_NR + idx - M0_AVI_SOCK_Q_INIT;

	M0_PRE(IS_IN_ARRAY(idx0, ma->t_addb));
	m0_addb2_hist_mod(&ma->t_addb[idx0], msec(end, start));
}

/** Creates the descriptor for a (passive) network buffer. */
static int bdesc_create(struct addr *addr, struct buf *buf,
			struct m0_net_buf_desc *out)
{
	struct bdesc bd = { .bd_addr = *addr };

	m0_cookie_init(&bd.bd_cookie, &buf->b_cookie);
	return bdesc_encode(&bd, out);
}

static int bdesc_encode(const struct bdesc *bd, struct m0_net_buf_desc *out)
{
	m0_bcount_t len;
	int         result;

	/* Cannot pass &out->nbd_len below, as it is 32 bits. */
	result = m0_xcode_obj_enc_to_buf(&M0_XCODE_OBJ(bdesc_xc, (void *)bd),
					 (void **)&out->nbd_data, &len);
	if (result == 0)
		out->nbd_len = len;
	else
		m0_free0(&out->nbd_data);
	return M0_RC(result);
}

static int bdesc_decode(const struct m0_net_buf_desc *nbd, struct bdesc *out)
{
	return m0_xcode_obj_dec_from_buf(&M0_XCODE_OBJ(bdesc_xc, out),
					 nbd->nbd_data, nbd->nbd_len);
}

static void mover_init(struct mover *m, struct ma *ma,
		       const struct mover_op_vec *vop)
{
	M0_PRE(m->m_sm.sm_conf == NULL);
	M0_SET0(m);
	m_tlink_init(m);
	m0_sm_init(&m->m_sm, &rw_conf, R_IDLE, &ma->t_ma->ntm_group);
	m0_sm_addb2_counter_init(&m->m_sm);
	m->m_op = vop;
	M0_POST(mover_invariant(m));
}

static void mover_fini(struct mover *m)
{
	if (m->m_sm.sm_conf != NULL) { /* Must be idempotent. */
		M0_ASSERT(mover_invariant(m));
		if (m->m_sm.sm_state != R_DONE)
			m0_sm_state_set(&m->m_sm, R_DONE);
		m0_sm_fini(&m->m_sm);
		if (m_tlink_is_in(m)) {
			m_tlist_del(m);
			EP_PUT(m->m_ep, mover);
			m->m_ep = NULL;
		}
		m_tlink_fini(m);
		M0_SET0(&m->m_pk);
		m0_free0(&m->m_scratch);
		m->m_sm.sm_conf = NULL;
	}
}

/**
 * Runs mover state transition(s).
 *
 * This is a rather complicated function, which tries to achieve multiple
 * intertwined things:
 *
 *     - common mover state machine infrastructure is used both for readers and
 *       writers. Readers come in multiple types depending on the socket type
 *       (stream vs. datagram), while writers send both GET and PUT
 *       packets. This is abstracted by keeping state transition functions in
 *       mover::m_op::v_op[][] array;
 *
 *     - some state transitions do no io (stream_idle(), stream_pk(), etc.),
 *       they only update mover fields. Some others need socket to be writable
 *       (writer_write(), etc.) or readable (stream_header(), etc.). This
 *       function attempts to execute as many state transitions as possible
 *       without blocking;
 *
 *     - avoid monopolising the processor while achieving the previous goal;
 *
 *     - handle unexpected events (e.g., readability of a socket for a writer),
 *       socket being closed by the peer, network errors and state transition
 *       errors;
 *
 *     - handle writer completion.
 *
 */
static int mover_op(struct mover *m, struct sock *s, int op)
{
	int state = m->m_sm.sm_state;

	M0_PRE(IS_IN_ARRAY(state, m->m_op->v_op));
	M0_PRE(IS_IN_ARRAY(op, m->m_op->v_op[state]));
	M0_PRE(m->m_sm.sm_conf != NULL);
	M0_PRE(mover_invariant(m));
	/*
	 * @todo This can monopolise processor doing state transitions for the
	 * same mover (i.e., continuous stream of data from the same remote
	 * end-point). Consider breaking out of this loop after some number of
	 * iterations or bytes ioed.
	 */
	while (s->s_flags & M0_BITS(op) || !m->m_op->v_op[state][op].o_doesio) {
		M0_LOG(M0_DEBUG, "Got %s: state: %x, op: %x.",
		       m->m_op->v_name, state, op);
		if (state == R_DONE) {
			if (m->m_op->v_done != NULL)
				m->m_op->v_done(m, s);
			else
				mover_fini(m);
			break;
		} else if (s->s_sm.sm_state != S_OPEN) {
			break;
		} else if (m->m_op->v_op[state][op].o_op == NULL) {
			state = -EPROTO;
			if (op != M_CLOSE) /* Ignore unexpected events. */
				break;
		} else
			state = m->m_op->v_op[state][op].o_op(m, s);
		if (state >= 0) {
			M0_ASSERT(IS_IN_ARRAY(state, m->m_op->v_op));
			m0_sm_state_set(&m->m_sm, state);
		} else {
			if (state == -ENOBUFS) /* Unwind and re-provision. */
				break;
			mover_fail(m, s, state);
		}
		state = m->m_sm.sm_state;
	}
	return state;
}

static bool mover_is_reader(const struct mover *m)
{
	return !mover_is_writer(m);
}

static bool mover_is_writer(const struct mover *m)
{
	return M0_IN(m->m_op, (&writer_op, &get_op));
}

/** True, iff the mover still matches its buffer. */
static int mover_matches(const struct mover *m)
{
	struct buf *buf = m->m_buf;

	M0_PRE(mover_is_reader(m));
	if (buf == NULL)
		/*
		 * The buffer might have been terminated (timeout,
		 * cancellation), and then deregistered.
		 */
		return M0_RC(-ECANCELED);
	if (buf->b_rc != 0) /* The buffer has already been terminated. */
		return M0_RC(buf->b_rc);
	if (buf->b_gen != m->m_gen)
		/* The buffer might have been re-queued after termination. */
		return M0_RC(-ENOENT);
	return 0;
}

static void mover_fail(struct mover *m, struct sock *s, int rc)
{
	if (m->m_sm.sm_conf != NULL) {
		m0_sm_state_set(&m->m_sm, R_FAIL);
		if (m->m_op->v_error != NULL)
			m->m_op->v_error(m, s, rc);
		if (m->m_sm.sm_conf != NULL) /* Can be zeroed by ->v_error(). */
			m0_sm_state_set(&m->m_sm, R_DONE);
		if (s != NULL)
			stype[s->s_ep->e_a.a_socktype].st_error(m, s);
	}
}

/**
 * Returns preferred packet payload size for a given sock.
 *
 * All, except for the last, packets for a given source buffer have the same
 * size, determined by this function.
 */
static m0_bcount_t pk_size(const struct mover *m, const struct sock *s)
{
	return stype[s->s_ep->e_a.a_socktype].st_pk_size(m, s);
}

/** Returns the size of the entire current packet (header plus payload). */
static m0_bcount_t pk_tsize(const struct mover *m)
{
	return sizeof m->m_pkbuf + m->m_pk.p_size;
}

/**
 * Returns how many payload bytes have been transferred in the current packet.
 */
static m0_bcount_t pk_dnob(const struct mover *m)
{
	return max64(m->m_nob - sizeof m->m_pkbuf, 0);
}

/**
 * Returns the new state, to which the writer moves after io.
 *
 * This function is not suitable for readers, because header (specifically,
 * mover::m_pk::p_size) is not initially known.
 */
static int pk_state(const struct mover *m)
{
	m0_bcount_t nob = m->m_nob;

	if (nob < sizeof m->m_pkbuf)
		return R_HEADER;
	else if (nob < pk_tsize(m))
		return R_INTERVAL;
	else {
		M0_ASSERT(nob == pk_tsize(m));
		return R_PK_DONE;
	}
}

/**
 * Prepares iovec for packet io.
 *
 * Fills a supplied iovec "iv", with "nr" elements, to launch vectorised io.
 *
 * Returns the number of elements filled. Returns in *count the total number of
 * bytes to be ioed.
 *
 * "bv" is a data buffer for payload.
 *
 * "tgt" is how many bytes (both header and payload) to try to io.
 */
static int pk_iov_prep(struct mover *m, struct iovec *iv, int nr,
		       struct m0_bufvec *bv, m0_bcount_t tgt, int *count)
{
	struct m0_bufvec_cursor cur;
	int                     idx;

	M0_PRE(nr > 0);
	M0_PRE(tgt >= m->m_nob);
	M0_PRE(tgt >= sizeof m->m_pkbuf); /* For simplicity assume the entire
					     header is always ioed. */
	if (m->m_nob < sizeof m->m_pkbuf) { /* Cannot use pk_state(): header can
					       be unintialised for reads. */
		iv[0].iov_base = &m->m_pkbuf[m->m_nob];
		*count = iv[0].iov_len = sizeof m->m_pkbuf - m->m_nob;
		idx = 1;
	} else {
		*count = 0;
		idx = 0;
	}
	if (tgt == sizeof m->m_pkbuf) /* Only header is ioed. */
		return idx;
	m0_bufvec_cursor_init(&cur, bv);
	m0_bufvec_cursor_move(&cur, m->m_pk.p_offset + pk_dnob(m));
	for (; idx < nr && !m0_bufvec_cursor_move(&cur, 0); ++idx) {
		m0_bcount_t frag = m0_bufvec_cursor_step(&cur);

		frag = min64u(frag, tgt - m->m_nob - *count);
		if (frag == 0)
			break;
		iv[idx].iov_base = m0_bufvec_cursor_addr(&cur);
		*count += (iv[idx].iov_len = frag);
		m0_bufvec_cursor_move(&cur, frag);
	}
	M0_POST(m0_reduce(i, idx, 0ULL, + iv[i].iov_len) == *count);
	M0_POST(idx > 0); /* Check that there is some progress. */
	return idx;
}

/**
 * Does packet io.
 *
 * "bv" is a data buffer for payload.
 *
 * "tgt" is how many bytes (both header and payload) to try to io.
 */
static int pk_io(struct mover *m, struct sock *s, uint64_t flag,
		 struct m0_bufvec *bv, m0_bcount_t tgt)
{
	struct iovec iv[256] = {};
	int          count;
	int          nr;
	int          rc;

	M0_PRE(M0_IN(flag, (HAS_READ, HAS_WRITE)));
	if (mover_is_reader(m) && m->m_buf != NULL) {
		rc = mover_matches(m);
		if (rc != 0)
			return rc;
	}
	nr = pk_iov_prep(m, iv, ARRAY_SIZE(iv),
			 bv ?: m->m_buf != NULL ?
			 &m->m_buf->b_buf->nb_buffer : NULL, tgt, &count);
	s->s_flags &= ~flag;
	rc = (flag == HAS_READ ? readv : writev)(s->s_fd, iv, nr);
	M0_LOG(M0_DEBUG, "flag: %" PRIi64 ", rc: %i, idx: %i, errno: %i.",
	       flag, rc, nr, errno);
	if (rc >= 0) {
		m->m_nob += rc;
		/*
		 * If everything was ioed, the socket might have more space in
		 * the buffer, try to io some more.
		 */
		s->s_flags |= (rc == count ? flag : 0);
		m0_addb2_hist_mod(&ep_ma(s->s_ep)->t_frag[flag==HAS_WRITE], rc);
	} else if (errno == EWOULDBLOCK) { /* Overshoot (see s_flags above). */
		rc = 0;
	} else if (errno == EINTR) { /* Nothing was ioed, repeat. */
		rc = 0;
	} else if (M0_IN(errno, (EPIPE, ECONNRESET)))
		rc = M0_RC(-errno);
	else
		rc = M0_ERR(-errno);
	/*
	 * printf("%s -> %s, %p: flag: %" PRIx64 ", tgt: %" PRIu64 ", nob %" PRIu64 ","
	 * " nr: %i, count: %i, rc: %i, sflags: %" PRIx64 "\n",
	 * ma_src(ep_ma(s->s_ep))->e_ep.nep_addr, s->s_ep->e_ep.nep_addr,
	 * m, flag, tgt, m->m_nob, nr, count, rc, s->s_flags);
	 */
	return rc;
}

/** Initialises the header for the current packet in a writer. */
static void pk_header_init(struct mover *m, struct sock *s)
{
	struct packet *p = &m->m_pk;

	M0_PRE(M0_IS0(&p->p_src.bd_cookie));
	M0_PRE(mover_is_writer(m));
	m0_cookie_init(&p->p_src.bd_cookie, &m->m_buf->b_cookie);
	p->p_src.bd_addr = ma_src(ep_ma(s->s_ep))->e_a;
	p->p_dst         = m->m_buf->b_peer;
	p->p_totalsize   = m->m_buf->b_buf->nb_length;
	M0_POST(!M0_IS0(&p->p_dst));
}

/**
 * Completes the processing of current packet header for a reader.
 *
 * This is called when the header has been completely read.
 *
 * Sanity checks the header, then tries to associate it with a buffer.
 *
 * Also handles GET packets (see "isget").
 */
static int pk_header_done(struct mover *m)
{
	struct packet       *p = &m->m_pk;
	struct m0_format_tag tag;
	struct ma           *ma = ep_ma(m->m_sock->s_ep);
	int                  result;
	bool                 isget;
	bool                 hassrc;
	bool                 hasdst;
	struct buf          *buf = NULL;
	uint64_t            *cookie;

	M0_PRE(m->m_nob >= sizeof *p);
	M0_PRE(mover_is_reader(m));

	pk_decode(m);
	result = m0_format_footer_verify_generic(&p->p_footer, p,
						 offsetof(struct packet,
							  p_footer), false);
	if (result != 0)
		return M0_ERR(result);
	m0_format_header_unpack(&tag, &p->p_header);
	isget = memcmp(&tag, &get_tag, sizeof tag) == 0;
	if (!isget && memcmp(&tag, &put_tag, sizeof tag) != 0)
		return M0_ERR(-EPROTO);
	if (p->p_idx >= p->p_nr)
		return M0_ERR(-EPROTO);
	if (p->p_offset + p->p_size < p->p_offset)
		return M0_ERR(-EFBIG);
	if (p->p_offset + p->p_size > p->p_totalsize)
		return M0_ERR(-EPROTO);
	if (p->p_idx == 0 && p->p_offset != 0)
		return M0_ERR(-EPROTO);
	if (p->p_idx == p->p_nr - 1 &&
	    p->p_offset + p->p_size != p->p_totalsize)
		return M0_ERR(-EPROTO);
	if (!ep_eq(ma_src(ma), &p->p_dst.bd_addr))
		return M0_ERR(-EPROTO);
	if (!addr_eq(&m->m_sock->s_ep->e_a, &p->p_src.bd_addr))
		return M0_ERR(-EPROTO);
	hassrc = !M0_IS0(&p->p_src.bd_cookie);
	hasdst = !M0_IS0(&p->p_dst.bd_cookie);
	if (!hassrc && !hasdst)         /* Go I know not whither */
		return M0_ERR(-EPROTO); /* and fetch I know not what? */
	if (hasdst) {
		enum { BFLAGS = M0_NET_BUF_QUEUED | M0_NET_BUF_REGISTERED };
		int qtype;
		result = m0_cookie_dereference(&p->p_dst.bd_cookie, &cookie);
		if (result != 0)
			return M0_ERR_INFO(result,
					   "Wrong cookie: %" PRIx64 ":%" PRIx64 "",
					   p->p_dst.bd_cookie.co_addr,
					   p->p_dst.bd_cookie.co_generation);
		buf = container_of(cookie, struct buf, b_cookie);
		if (buf_ma(buf) != ma)
			return M0_RC(-EPERM);
		if ((buf->b_buf->nb_flags & BFLAGS) != BFLAGS)
			return M0_RC(-EPERM);
		if (buf->b_rc != 0)
			return M0_RC(buf->b_rc);
		if (!M0_IS0(&buf->b_peer) &&
		    memcmp(&buf->b_peer, &p->p_src, sizeof p->p_src) != 0)
			return M0_RC(-EPERM);
		qtype = buf->b_buf->nb_qtype;
		if (isget && qtype != M0_NET_QT_PASSIVE_BULK_SEND)
			return M0_RC(-EPERM);
		if (!isget && !M0_IN(qtype, (M0_NET_QT_ACTIVE_BULK_RECV,
					     M0_NET_QT_PASSIVE_BULK_RECV)))
			return M0_RC(-EPERM);
	}
	if (isget) {
		struct mover *w = &buf->b_writer;
		if (p->p_idx != 0 || p->p_nr != 1 || p->p_size != 0 ||
		    p->p_offset != 0 || !hasdst || !hassrc)
			return M0_ERR(-EPROTO);
		buf->b_peer = p->p_src;
		mover_init(w, ma, &writer_op);
		w->m_buf = buf;
		/*
		 * Here a packet GET(src = ip0:port0:bd0, dst = ip1:port1:bd1)
		 * is received from ip0:ephemeral end-point. The data can be
		 * sent back either through the ephemeral address or to a new
		 * ip0:port0 end-point. For now, keep the number of sockets
		 * minimal.
		 */
		if (ep_eq(m->m_sock->s_ep, &p->p_src.bd_addr))
			result = ep_add(m->m_sock->s_ep, w);
		else {
			struct ep *ep;
			result = ep_create(ma, &p->p_src.bd_addr, NULL, &ep);
			if (result == 0) {
				result = ep_add(ep, w);
				EP_PUT(ep, find);
			}
		}
		if (result != 0)
			buf_terminate(buf, result);
		return R_IDLE;
	}
	if (!hasdst) {
		/* Select a buffer from the receive queue. */
		buf = ma_recv_buf(ma, p);
		if (buf == NULL) {
			struct m0_net_transfer_mc *tm   = ma->t_ma;
			struct m0_net_buffer_pool *pool = tm->ntm_recv_pool;
			/*
			 * A user of network transport (such as the rpc module)
			 * has no control over consumption of buffers on the
			 * receive queue. It might (and does) so happen that
			 * this queue becomes empty.
			 *
			 * After a buffer completion event has been delivered to
			 * the user, m0_net_buffer_event_post() tries to
			 * re-provision the receive queue by calling
			 * m0_net__tm_provision_recv_q(). This does not always
			 * work because:
			 *
			 *     - standard provisioning code does not deal with
			 *       the busy buffers and
			 *
			 *     - the loop in poller() processes multiple events
			 *       before buffer completions are invoked.
			 *
			 * First, try to provision the receive queue right here
			 * on the spot. This might fail because the try-lock
			 * below fails or because the pool is out of buffers. In
			 * case of failure, return -ENOBUFS all the way up to
			 * sock_event(), which returns true to instruct poller()
			 * to break out of the loop and to re-provision the
			 * queue.
			 */
			if (pool == NULL)
				return M0_RC(-ENODATA); /* Drop the packet. */
			if (m->m_sock->s_flags & MORE_BUFS)
				return M0_RC(-ENODATA); /* Already tried. */
			if (m0_mutex_trylock(&pool->nbp_mutex) == 0) {
				m0_net__tm_provision_buf(tm);
				/* Got the lock. Add 2 buffers. */
				m0_net__tm_provision_buf(tm);
				m0_net_buffer_pool_unlock(pool);
				buf = ma_recv_buf(ma, p);
			}
		}
		if (buf == NULL) {
			m->m_sock->s_flags |= MORE_BUFS;
			return -ENOBUFS; /* Not always a error. */
		}
	}
	return buf_accept(buf, m) ?: R_INTERVAL;
}

/**
 * Completes an incoming packet processing.
 *
 * If all packets for the buffer have been received, complete the
 * buffer. Disassociate the buffer from the reader.
 */
static int pk_done(struct mover *m)
{
	struct buf    *buf = m->m_buf;
	struct packet *pk  = &m->m_pk;
	int            result;

	M0_PRE(mover_is_reader(m));
	result = mover_matches(m);
	if (result != 0)
		return result;
	if (m0_bitmap_get(&buf->b_done, pk->p_idx))
		return M0_ERR(-EPROTO);
	m->m_buf = NULL;
	m0_bitmap_set(&buf->b_done, pk->p_idx, true);
	if (m0_bitmap_ffz(&buf->b_done) == -1)
		buf_done(buf, -EALREADY);
	return 0;
}

static void pk_encdec(struct mover *m, enum m0_xcode_what what)
{
	struct m0_bufvec_cursor cur;
	m0_bcount_t             len = sizeof m->m_pkbuf;
	void                   *buf = m->m_pkbuf;
	int                     rc;

	m0_bufvec_cursor_init(&cur, &M0_BUFVEC_INIT_BUF(&buf, &len));
	rc = m0_xcode_encdec(&M0_XCODE_OBJ(packet_xc, &m->m_pk), &cur, what);
	M0_ASSERT(rc == 0);
}

static void pk_decode(struct mover *m)
{
	pk_encdec(m, M0_XCODE_DECODE);
}

static void pk_encode(struct mover *m)
{
	m0_format_footer_update(&m->m_pk);
	pk_encdec(m, M0_XCODE_ENCODE);
}

/**
 * Input is available on the previously idle stream socket.
 *
 * Do nothing, fall through to the R_PK state.
 */
static int stream_idle(struct mover *self, struct sock *s)
{
	return R_PK;
}

/**
 * Starts incoming packet processing.
 */
static int stream_pk(struct mover *self, struct sock *s)
{
	s->s_flags &= ~MORE_BUFS;
	self->m_nob = 0;
	self->m_gen = 0;
	M0_ASSERT(self->m_buf == NULL);
	return R_HEADER;
}

/**
 * Reads packet header (or some part thereof).
 *
 * Loops in R_HEADER state until the entire header has been read.
 */
static int stream_header(struct mover *self, struct sock *s)
{
	int result = pk_io(self, s, HAS_READ, NULL, sizeof self->m_pkbuf);
	if (result < 0)
		return M0_RC(result);
	/* Cannot use pk_state() with unverified header. */
	else if (self->m_nob < sizeof self->m_pkbuf)
		return R_HEADER;
	else {
		M0_ASSERT(self->m_nob == sizeof self->m_pkbuf);
		return pk_header_done(self);
	}
}

/**
 * Reads an interval (i.e., part of the packet payload).
 *
 * Loops in R_INTERVAL until the entire packet is read.
 */
static int stream_interval(struct mover *self, struct sock *s)
{
	int result = pk_io(self, s, HAS_READ, NULL, pk_tsize(self));
	return result >= 0 ? pk_state(self) : result;
}

/** Completes processing of an incoming packet. */
static int stream_pk_done(struct mover *self, struct sock *s)
{
	return pk_done(self) ?: R_IDLE;
}

/**
 * Returns the maximal packet size for a stream socket.
 *
 * There is no reason to split a buffer into multiple packets over a stream
 * socket, so return a very large value here.
 */
static m0_bcount_t stream_pk_size(const struct mover *w, const struct sock *s)
{
	return M0_BSIGNED_MAX / 2;
}

/** Handles an error for a stream socket. */
static void stream_error(struct mover *m, struct sock *s)
{
	if (m->m_sock != NULL || s->s_reader.m_buf != NULL) {
		m->m_sock = NULL;
		s->s_reader.m_buf = NULL;
		sock_done(s, true);
	}
}

/**
 * Input is available on the previously idle datagram socket.
 *
 * Do nothing, fall through to the R_PK state.
 */
static int dgram_idle(struct mover *self, struct sock *s)
{
	return R_PK;
}

/**
 * Starts processing of an incoming packet over a datagram socket.
 *
 * Allocate, if still not allocated, a scratch area for the payload.
 */
static int dgram_pk(struct mover *self, struct sock *s)
{
	s->s_flags &= ~MORE_BUFS;
	self->m_nob = 0;
	self->m_gen = 0;
	self->m_buf = NULL;
	M0_SET0(&self->m_pk);
	if (self->m_scratch == NULL) {
		self->m_scratch = m0_alloc(pk_size(self, s));
		if (self->m_scratch == NULL)
			return M0_ERR(-ENOMEM);
	}
	return R_HEADER;
}

/**
 * Processes an incoming packet over an datagram socket.
 *
 * Read the entire packet (header and payload) in one go. The header is read in
 * self->m_pkbuf (see pk_io_prep()). The payload is read in the scratch area.
 *
 * Parse and verify the header (pk_header_done()), then copy the payload in the
 * associated buffer.
 *
 * If this fails for whatever reason, jump to R_IDLE.
 */
static int dgram_header(struct mover *self, struct sock *s)
{
	m0_bcount_t maxsize = pk_size(self, s);
	m0_bcount_t dsize;
	struct m0_bufvec bv = M0_BUFVEC_INIT_BUF(&self->m_scratch, &maxsize);
	struct m0_bufvec_cursor cur;

	M0_ASSERT(self->m_nob == 0);
	int result = pk_io(self, s, HAS_READ, &bv,
			   sizeof self->m_pkbuf + maxsize);
	if (result < 0)
		return R_IDLE;
	if (self->m_nob == 0)
		return R_IDLE; /* -EWOULDBLOCK. */
	result = pk_header_done(self);
	if (result < 0)
		return R_IDLE;
	if (self->m_buf == NULL)
		return R_IDLE; /* GET packet processed. */
	dsize = self->m_pk.p_size;
	if (self->m_nob != sizeof self->m_pkbuf + dsize) {
		M0_LOG(M0_ERROR, "Wrong packet size: %"PRId64" != %"PRId64".",
		       self->m_nob, sizeof self->m_pkbuf + dsize);
		return R_IDLE;
	}
	m0_bufvec_cursor_init(&cur, &self->m_buf->b_buf->nb_buffer);
	m0_bufvec_cursor_move(&cur, self->m_pk.p_offset);
	m0_data_to_bufvec_copy(&cur, self->m_scratch, dsize);
	return pk_state(self);
}

/**
 * Falls through: the entire packet was processed in dgram_header().
 */
static int dgram_interval(struct mover *self, struct sock *s)
{
	int result = mover_matches(self);

	if (result != 0)
		return result;
	return pk_state(self);
}

/**
 * Completes the processing of an incoming packet over a datagram socket.
 */
static int dgram_pk_done(struct mover *self, struct sock *s)
{
	return pk_done(self) ?: R_IDLE;
}

/**
 * Returns the maximal packet size for a datagram socket.
 *
 * A packet must fit in a single datagram.
 */
static m0_bcount_t dgram_pk_size(const struct mover *w, const struct sock *s)
{
	return    65535 /* 16 bit "total length" in ip header */
		-     8 /* UDP header */
		/* IPv4 or IPV6 header size */
		- (s->s_ep->e_a.a_family == AF_INET ? 20 : 40)
		- sizeof w->m_pkbuf;
}

static void dgram_error(struct mover *m, struct sock *s)
{
}

/** Initialises a writer. */
static int writer_idle(struct mover *w, struct sock *s)
{
	m0_bcount_t pksize = pk_size(w, s);
	m0_bcount_t size   = w->m_buf->b_buf->nb_length;

	M0_ASSERT(size > 0);
	pk_header_init(w, s);
	w->m_pk.p_nr = (size + pksize - 1) / pksize;
	m0_format_header_pack(&w->m_pk.p_header, &put_tag);
	return R_PK;
}

/**
 * Starts a packet write-out.
 *
 * Select the packet size, lock the writer to the socket.
 */
static int writer_pk(struct mover *w, struct sock *s)
{
	m0_bcount_t pksize = pk_size(w, s);
	m0_bcount_t size   = w->m_buf->b_buf->nb_length;

	w->m_nob = 0;
	w->m_pk.p_size = min64u(pksize, size - w->m_pk.p_offset);
	pk_encode(w);
	w->m_sock = s; /* Lock the socket and the writer together. */
	return R_HEADER;
}

/**
 * Writes a part of a packet.
 *
 * Write as much as the socket allows. This deals with both header and payload.
 */
static int writer_write(struct mover *w, struct sock *s)
{
	int result = pk_io(w, s, HAS_WRITE, NULL, pk_tsize(w));
	return result >= 0 ? pk_state(w) : result;
}

/**
 * Completes packet write-out.
 *
 * Unlock the writer from the socket. Is all packets for the buffer have been
 * written, complete the writer, otherwise switch to the next packet.
 */
static int writer_pk_done(struct mover *w, struct sock *s)
{
	w->m_sock = NULL;
	if (++w->m_pk.p_idx == w->m_pk.p_nr)
		return R_DONE;
	else {
		w->m_pk.p_offset += w->m_pk.p_size;
		return R_PK;
	}
}

/** Handles R_DONE state in a writer. */
static void writer_done(struct mover *w, struct sock *s)
{
	writer_error(w, s, -EALREADY);
}

/**
 * Completes a writer.
 *
 * This handles both normal (rc == -EALREADY) and error cases.
 *
 * Remove the writer from the socket and complete the buffer.
 */
static void writer_error(struct mover *w, struct sock *s, int rc)
{
	ep_del(w);
	buf_done(w->m_buf, rc);
}

/** Starts processing of a GET packet. */
static int get_idle(struct mover *self, struct sock *s)
{
	return R_PK;
}

/** Fills GET packet, prepares on-wire representation. */
static int get_pk(struct mover *cmd, struct sock *s)
{
	pk_header_init(cmd, s);
	m0_format_header_pack(&cmd->m_pk.p_header, &get_tag);
	cmd->m_nob = 0;
	cmd->m_pk.p_nr = 1;
	cmd->m_pk.p_totalsize = 0;
	cmd->m_pk.p_size = 0;
	pk_encode(cmd);
	cmd->m_sock = s;
	return R_HEADER;
}

/** Completes a GET packet. */
static void get_done(struct mover *w, struct sock *s)
{
	ep_del(w);
	mover_fini(w);
}

static m0_time_t msec(m0_time_t a, m0_time_t b)
{
	return m0_time_sub(a, b) >> 10;
}

static bool sock_ut(void)
{
	/*
	 * Define failure injection point here and not in in_ut(), so that
	 * function name is unique.
	 */
	return M0_FI_ENABLED("ut");
}

/** True, iff UT is going on. */
static bool in_ut(void)
{
	return sock_ut();
}

static struct m0_sm_state_descr sock_conf_state[] = {
	[S_INIT] = {
		.sd_name = "init",
		.sd_flags = M0_SDF_INITIAL,
		.sd_allowed = M0_BITS(S_LISTENING, S_CONNECTING,
				      S_OPEN, S_DELETED)
	},
	[S_LISTENING] = {
		.sd_name = "listening",
		.sd_allowed = M0_BITS(S_DELETED)
	},
	[S_CONNECTING] = {
		.sd_name = "connecting",
		.sd_allowed = M0_BITS(S_OPEN, S_DELETED)
	},
	[S_OPEN] = {
		.sd_name = "active",
		.sd_allowed = M0_BITS(S_DELETED)
	},
	[S_DELETED] = {
		.sd_name = "deleted",
		.sd_flags = M0_SDF_TERMINAL
	}
};

static struct m0_sm_trans_descr sock_conf_trans[] = {
	{ "listen",        S_INIT,       S_LISTENING },
	{ "connect",       S_INIT,       S_CONNECTING },
	{ "accept",        S_INIT,       S_OPEN },
	{ "init-deleted",  S_INIT,       S_DELETED },
	{ "listen-close",  S_LISTENING,  S_DELETED },
	{ "connected",     S_CONNECTING, S_OPEN },
	{ "connect-close", S_CONNECTING, S_DELETED },
	{ "close",         S_OPEN,       S_DELETED }
};

/* used in addb2/dump.c */
struct m0_sm_conf sock_conf = {
	.scf_name      = "sock",
	.scf_nr_states = ARRAY_SIZE(sock_conf_state),
	.scf_state     = sock_conf_state,
	.scf_trans_nr  = ARRAY_SIZE(sock_conf_trans),
	.scf_trans     = sock_conf_trans
};

static struct m0_sm_state_descr rw_conf_state[] = {
	[R_IDLE] = {
		.sd_name = "idle",
		.sd_flags = M0_SDF_INITIAL,
		.sd_allowed = M0_BITS(R_PK, R_DONE, R_FAIL)
	},
	[R_PK] = {
		.sd_name = "packet",
		.sd_allowed = M0_BITS(R_HEADER, R_FAIL)
	},
	[R_HEADER] = {
		.sd_name = "header",
		.sd_allowed = M0_BITS(R_IDLE, R_HEADER, R_INTERVAL,
				      R_PK_DONE, R_DONE, R_FAIL)
	},
	[R_INTERVAL] = {
		.sd_name = "interval",
		.sd_allowed = M0_BITS(R_INTERVAL, R_PK_DONE, R_FAIL)
	},
	[R_PK_DONE] = {
		.sd_name = "packet-done",
		.sd_allowed = M0_BITS(R_PK, R_IDLE, R_DONE, R_FAIL)
	},
	[R_FAIL] = {
		.sd_name = "fail",
		.sd_allowed = M0_BITS(R_DONE)
	},
	[R_DONE] = {
		.sd_name = "done",
		.sd_flags = M0_SDF_TERMINAL
	}
};

static struct m0_sm_trans_descr rw_conf_trans[] = {
	{ "pk-start",       R_IDLE,       R_PK },
	{ "close",          R_IDLE,       R_DONE },
	{ "error",          R_IDLE,       R_FAIL },
	{ "header-start",   R_PK,         R_HEADER },
	{ "pk-error",       R_PK,         R_FAIL },
	{ "header-cont",    R_HEADER,     R_HEADER },
	{ "get-send-done",  R_HEADER,     R_PK_DONE },
	{ "get-rcvd-done",  R_HEADER,     R_IDLE },
	{ "interval-start", R_HEADER,     R_INTERVAL },
	{ "header-error",   R_HEADER,     R_FAIL },
	/* This transition is only possible for a GET command. */
	{ "cmd-done",       R_HEADER,     R_DONE },
	{ "interval-cont",  R_INTERVAL,   R_INTERVAL },
	{ "interval-done",  R_INTERVAL,   R_PK_DONE },
	{ "interval-error", R_INTERVAL,   R_FAIL },
	{ "pk-next",        R_PK_DONE,    R_PK },
	/* This transition is not possible for a writer. */
	{ "pk-idle",        R_PK_DONE,    R_IDLE },
	{ "pk-error",       R_PK_DONE,    R_FAIL },
	{ "pk-close",       R_PK_DONE,    R_DONE },
	{ "done",           R_FAIL,       R_DONE },
};

/* used in addb2/dump.c */
struct m0_sm_conf rw_conf = {
	.scf_name      = "reader-writer",
	.scf_nr_states = ARRAY_SIZE(rw_conf_state),
	.scf_state     = rw_conf_state,
	.scf_trans_nr  = ARRAY_SIZE(rw_conf_trans),
	.scf_trans     = rw_conf_trans
};

static const struct mover_op_vec stream_reader_op = {
	.v_name = "stream-reader",
	.v_op   = {
		[R_IDLE]     = { [M_READ] = { &stream_idle, true } },
		[R_PK]       = { [M_READ] = { &stream_pk, false } },
		[R_HEADER]   = { [M_READ] = { &stream_header, true } },
		[R_INTERVAL] = { [M_READ] = { &stream_interval, true } },
		[R_PK_DONE]  = { [M_READ] = { &stream_pk_done, false } }
	}
};

static const struct mover_op_vec dgram_reader_op = {
	.v_name = "dgram-reader",
	.v_op   = {
		[R_IDLE]     = { [M_READ] = { &dgram_idle, true } },
		[R_PK]       = { [M_READ] = { &dgram_pk, false } },
		[R_HEADER]   = { [M_READ] = { &dgram_header, true } },
		[R_INTERVAL] = { [M_READ] = { &dgram_interval, false } },
		[R_PK_DONE]  = { [M_READ] = { &dgram_pk_done, false } }
	}
};

static const struct mover_op_vec writer_op = {
	.v_name  = "writer",
	.v_done  = &writer_done,
	.v_error = &writer_error,
	.v_op    = {
	      [R_IDLE]     = { [M_WRITE] = { &writer_idle, true } },
	      [R_PK]       = { [M_WRITE] = { &writer_pk, false } },
	      [R_HEADER]   = { [M_WRITE] = { &writer_write /* sic. */, true } },
	      [R_INTERVAL] = { [M_WRITE] = { &writer_write /* sic. */, true } },
	      [R_PK_DONE]  = { [M_WRITE] = { &writer_pk_done, false } }
	}
};

static const struct mover_op_vec get_op = {
	.v_name  = "get",
	.v_done  = &get_done,
	.v_error = &writer_error,
	.v_op    = {
		[R_IDLE]     = { [M_WRITE] = { &get_idle, true } },
		[R_PK]       = { [M_WRITE] = { &get_pk, false } },
		[R_HEADER]   = { [M_WRITE] = { &writer_write, true } },
		[R_PK_DONE]  = { [M_WRITE] = { &writer_pk_done, false } }
	}
};

enum {
	M0_NET_SOCK_PROTO_VERSION = 1,
	M0_NET_SOCK_PROTO_PUT     = 0x1,
	M0_NET_SOCK_PROTO_GET     = 0x2
};

static const struct m0_format_tag put_tag = {
	.ot_version       = M0_NET_SOCK_PROTO_VERSION,
	.ot_type          = M0_NET_SOCK_PROTO_PUT,
	.ot_footer_offset = offsetof(struct packet, p_footer)
};

static const struct m0_format_tag get_tag = {
	.ot_version       = M0_NET_SOCK_PROTO_VERSION,
	.ot_type          = M0_NET_SOCK_PROTO_GET,
	.ot_footer_offset = offsetof(struct packet, p_footer)
};

static const struct m0_net_xprt_ops xprt_ops = {
	.xo_dom_init                    = &dom_init,
	.xo_dom_fini                    = &dom_fini,
	.xo_tm_init                     = &ma_init,
	.xo_tm_confine                  = &ma_confine,
	.xo_tm_start                    = &ma_start,
	.xo_tm_stop                     = &ma_stop,
	.xo_tm_fini                     = &ma_fini,
	.xo_end_point_create            = &end_point_create,
	.xo_buf_register                = &buf_register,
	.xo_buf_deregister              = &buf_deregister,
	.xo_buf_add                     = &buf_add,
	.xo_buf_del                     = &buf_del,
	.xo_bev_deliver_sync            = &bev_deliver_sync,
	.xo_bev_deliver_all             = &bev_deliver_all,
	.xo_bev_pending                 = &bev_pending,
	.xo_bev_notify                  = &bev_notify,
	.xo_get_max_buffer_size         = &get_max_buffer_size,
	.xo_get_max_buffer_segment_size = &get_max_buffer_segment_size,
	.xo_get_max_buffer_segments     = &get_max_buffer_segments,
	.xo_get_max_buffer_desc_size    = &get_max_buffer_desc_size,

	.xo_rpc_max_seg_size            = &get_rpc_max_seg_size,
	.xo_rpc_max_segs_nr             = &get_rpc_max_segs_nr,
	.xo_rpc_max_msg_size            = default_xo_rpc_max_msg_size,
	.xo_rpc_max_recv_msgs           = default_xo_rpc_max_recv_msgs,

};

#if MOCK_LNET
const struct m0_net_xprt m0_net_lnet_xprt = {
	.nx_name = "lnet",
	.nx_ops  = &xprt_ops
};
M0_EXPORTED(m0_net_lnet_xprt);
#else
extern const struct m0_net_xprt m0_net_lnet_xprt;
#endif

const struct m0_net_xprt m0_net_sock_xprt = {
	.nx_name = "sock",
	.nx_ops  = &xprt_ops
};
M0_EXPORTED(m0_net_sock_xprt);

M0_INTERNAL int m0_net_sock_mod_init(void)
{
	int result;

	/*
	 * Ignore SIGPIPE that a write to socket gets when RST is received.
	 *
	 * A more elegant approach is to use sendmsg(2) with MSG_NOSIGNAL flag
	 * instead of writev(2).
	 */
	result = sigaction(SIGPIPE,
			   &(struct sigaction){ .sa_handler = SIG_IGN }, NULL);
	if (result != 0)
		return M0_ERR(-errno);
	if (MOCK_LNET) {
		m0_net_xprt_register(&m0_net_lnet_xprt);
		if (m0_streq(M0_DEFAULT_NETWORK, "LNET"))
			m0_net_xprt_default_set(&m0_net_lnet_xprt);
	} else {
		m0_net_xprt_register(&m0_net_sock_xprt);
		if (m0_streq(M0_DEFAULT_NETWORK, "SOCK"))
			m0_net_xprt_default_set(&m0_net_sock_xprt);
	}
	m0_sm_conf_init(&rw_conf);
	m0_sm_conf_init(&sock_conf);
	result = m0_sm_addb2_init(&rw_conf, M0_AVI_SOCK_MOVER,
				  M0_AVI_SOCK_MOVER_COUNTER);
	if (result == 0) {
		result = m0_sm_addb2_init(&sock_conf, M0_AVI_SOCK_SOCK,
					  M0_AVI_SOCK_SOCK_COUNTER);
		if (result != 0)
			m0_sm_addb2_fini(&rw_conf);
	}
	return 0;
}

M0_INTERNAL void m0_net_sock_mod_fini(void)
{
	if (MOCK_LNET)
		m0_net_xprt_deregister(&m0_net_lnet_xprt);
	else
		m0_net_xprt_deregister(&m0_net_sock_xprt);
	m0_sm_addb2_fini(&sock_conf);
	m0_sm_addb2_fini(&rw_conf);
	m0_sm_conf_fini(&sock_conf);
	m0_sm_conf_fini(&rw_conf);
}

M0_INTERNAL void mover__print(const struct mover *m)
{
	printf("\t%p: %s sock: %p state: %i buf: %p\n", m,
	       mover_is_reader(m) ? "R" : (m->m_op == &writer_op ? "W" : "G"),
	       m->m_sock, m->m_sm.sm_state, m->m_buf);
}

M0_INTERNAL void addr__print(const struct addr *addr)
{
	char *s = addr_print(addr);
	printf("\t%s\n", s);
	m0_free(s);
}

M0_INTERNAL void sock__print(const struct sock *sock)
{
	printf("\t\tfd: %i, flags: %" PRIx64 ", state: %i\n",
	       sock->s_fd, sock->s_flags, sock->s_sm.sm_state);
}

M0_INTERNAL void ep__print(const struct ep *ep)
{
	struct sock  *s;
	struct mover *w;
	if (ep == NULL)
		printf("NULL ep\n");
	else {
		printf("\t%p: ", ep);
		addr__print(&ep->e_a);
		m0_tl_for(s, &ep->e_sock, s) {
			sock__print(s);
		} m0_tl_endfor;
		m0_tl_for(m, &ep->e_writer, w) {
			mover__print(w);
		} m0_tl_endfor;
	}
}

M0_INTERNAL void buf__print(const struct buf *buf)
{
	printf("\t%p: %" PRIx64 " bitmap: %"PRIx64
	       " peer: %" PRIx64 ":%" PRIx64 "\n", buf,
	       buf->b_cookie,
	       buf->b_peer.bd_cookie.co_addr,
	       buf->b_peer.bd_cookie.co_generation);
	ep__print(buf->b_other);
}

M0_INTERNAL void ma__print(const struct ma *ma)
{
	struct m0_net_end_point *ne;
	struct m0_net_buffer *nb;
	struct m0_net_transfer_mc *tm = ma->t_ma;
	int i;

	printf("%p, state: %x\n", ma, ma->t_ma->ntm_state);
	m0_tl_for(m0_nep, &ma->t_ma->ntm_end_points, ne) {
		ep__print(ep_net(ne));
	} m0_tl_endfor;
	for (i = 0; i < ARRAY_SIZE(tm->ntm_q); ++i) {
		printf("\t ---%i[]---\n", i);
		m0_tl_for(m0_net_tm, &tm->ntm_q[i], nb) {
			buf__print(nb->nb_xprt_private);
		} m0_tl_endfor;
	}
}

#include <ctype.h>

M0_INTERNAL void bvec__print(const struct m0_bufvec *bv)
{
	m0_bcount_t             idx;
	struct m0_bufvec_cursor cur;
	bool        first = true;
	int         prev;
	int         next;
	m0_bcount_t rep = 1;

	m0_bufvec_cursor_init(&cur, bv);
	printf("[");
	for (idx = 0; !m0_bufvec_cursor_move(&cur, 0); ++idx, prev = next) {
		char *addr = m0_bufvec_cursor_addr(&cur);
		next = *addr;
		if (m0_bufvec_cursor_move(&cur, 1))
			next = 10001;
		if (first)
			first = false;
		else if (next != prev) {
			if (isprint(prev))
				printf("%c", prev & 0xff);
			else
				printf("\\%x", prev & 0xff);
			if (rep > 1)
				printf(" <*%"PRId64">", rep);
			rep = 1;
		} else
			rep++;
	}
	printf("]\n");
}

/*
 * @UT
 */

struct sock_ut_conf {
	int uc_maxfd;
	int uc_sockbuf_len;
	int uc_epoll_len;
	int uc_delay;
	int uc_err;
	int uc_socket_err;
	int uc_accept4_err;
	int uc_accept4_miss;
	int uc_listen_err;
	int uc_bind_err;
	int uc_connect_err;
	int uc_readv_err;
	int uc_readv_again;
	int uc_readv_0;
	int uc_readv_break;
	int uc_readv_skip;
	int uc_readv_flip;
	int uc_writev_err;
	int uc_writev_again;
	int uc_writev_0;
	int uc_writev_break;
	int uc_setsockopt_err;
	int uc_close_err;
	int uc_epoll_create_err;
	int uc_epoll_wait_err;
	int uc_epoll_raise_err;
	int uc_epoll_raise_in;
	int uc_epoll_raise_out;
	int uc_epoll_raise_garbage;
	int uc_epoll_break;
	int uc_epoll_ctl_err;
	int uc_alloc_err;
	int uc_delay_ns;
};

enum { M_EPHEMERAL_LO = 32768, M_EPHEMERAL_HI = 60999 };

static struct sock_ut_conf *m_conf;
static struct m0_mutex      m_ut_lock;
static struct mock_fd      *m_table;
static int                  m_ephemeral;
static int                  m_gen;
static uint64_t             m_seed = 0;
struct m0_net_domain        m_dom = {};
static struct m0_mutex      m_mock_lock;
static struct m0_semaphore  m_tm_state;

enum { MT_FREE, MT_SOCK, MT_EPOLL };

enum {
	MF_CLOSED_SRC = 1 << 0,
	MF_CLOSED_TGT = 1 << 1,
	MF_LISTEN     = 1 << 2,
	MF_CONNECTED  = 1 << 3
};

struct mock_epoll_entry {
	int                ee_fd;
	struct epoll_event ee_event;
};

struct dgram {
	int   d_size;
	void *d_payload;
};

struct mock_fd {
	int       md_type;
	int       md_gen;
	int       md_idx;
	int       md_src;
	int       md_tgt;
	uint64_t  md_flags;
	union {
		char                    *u_sock;
		struct mock_epoll_entry *u_epoll;

	}         md_buf;
	int       md_len;
	int       md_head;
	int       md_tail;
};

static void ut_init(const struct sock_ops *sop, struct sock_ut_conf *conf)
{
	int result;

	m_conf = conf;
	m0_mutex_init(&m_mock_lock);
	m0_mutex_init(&m_ut_lock);
	m0_semaphore_init(&m_tm_state, 0);
	M0_ALLOC_ARR(m_table, m_conf->uc_maxfd);
	M0_ASSERT(m_table != 0);
	m_ephemeral = M_EPHEMERAL_LO;
	if (m_seed == 0) /* Do this only once globally. */
		m_seed = time(NULL) + m0_pid();
	m_gen = 0;
	sops = sop;
	m0_fi_enable("sock_ut", "ut");
	if (conf->uc_alloc_err > 0)
		m0_fi_enable_each_nth_time("m0_alloc", "fail_allocation",
					   10000 / conf->uc_alloc_err);
	result = m0_net_domain_init(&m_dom, MOCK_LNET ?
				    &m0_net_lnet_xprt: &m0_net_sock_xprt);
	M0_ASSERT(result == 0);
}

static void ut_fini(void)
{
	M0_SET0(&m_dom); /* Cannot finalise: transport can be broken. */
	if (m_conf->uc_alloc_err > 0)
		m0_fi_disable("m0_alloc", "fail_allocation");
	m0_fi_disable("sock_ut", "ut");
	sops = NULL;
	m0_semaphore_fini(&m_tm_state);
	m0_mutex_fini(&m_mock_lock);
	m0_mutex_fini(&m_ut_lock);
	m0_free(m_table);
	m_conf = NULL;
}

/* @@MOCK */

static int mock_rnd(int max)
{
	return (m0_rnd64(&m_seed) >> 32) % max;
}

static bool mock_fd_invariant(const struct mock_fd *fd)
{
	int idx = fd - m_table;
	return  _0C(0 <= idx && idx < m_conf->uc_maxfd) &&
		_0C(fd->md_idx == idx) &&
		_0C(M0_IN(fd->md_type, (MT_FREE, MT_SOCK, MT_EPOLL)));
}

static bool mock_dice(int value)
{
	return value > 0 && mock_rnd(10000) < value;
}

#define MOCK_DICE(field) mock_dice(m_conf->uc_ ## field)

static int mock_prologue(void)
{
	m0_mutex_lock(&m_mock_lock);
	if (MOCK_DICE(delay)) {
		int delay = mock_rnd(m_conf->uc_delay_ns);
		m0_mutex_unlock(&m_mock_lock);
		m0_nanosleep(delay, NULL);
		m0_mutex_lock(&m_mock_lock);
	}
	return MOCK_DICE(err);
}

static int mock_ret(int rc)
{
	m0_mutex_unlock(&m_mock_lock);
	if (rc < 0) {
		errno = -rc;
		return -1;
	} else
		return rc;
}

static int mock_err(void)
{
	return -mock_rnd(100) - 1;
}

static struct mock_fd *get_fd(int ftype)
{
	int             i;
	struct mock_fd *fd;
	M0_PRE(M0_IN(ftype, (MT_SOCK, MT_EPOLL)));
	for (i = 0; i < m_conf->uc_maxfd; ++i) {
		fd = &m_table[i];
		if (fd->md_type == MT_FREE) {
			m0_free(fd->md_buf.u_sock);
			M0_SET0(fd);
			if (ftype == MT_SOCK) {
				fd->md_len = m_conf->uc_sockbuf_len;
				M0_ALLOC_ARR(fd->md_buf.u_sock, fd->md_len);
			} else {
				fd->md_len = m_conf->uc_epoll_len;
				M0_ALLOC_ARR(fd->md_buf.u_epoll, fd->md_len);
			}
			if (fd->md_buf.u_sock == NULL)
				return NULL;
			fd->md_type = ftype;
			fd->md_idx  = i;
			fd->md_gen  = m_gen++;
			M0_ASSERT(mock_fd_invariant(fd));
			return fd;
		}
	}
	return NULL;
}

static int src_fdnum(const struct mock_fd *fd)
{
	if (fd == NULL)
		return -ENOMEM;
	M0_PRE(fd->md_idx + 10 < (1U << 15));
	M0_PRE(fd->md_gen < (1U << 15));
	return  (fd->md_idx + 10) /* Avoid conflicts with std{in,out,err} */ |
		(fd->md_gen << 15);
}

enum { TGT_MASK = 1U << 30 };
static int tgt_fdnum(const struct mock_fd *fd)
{
	M0_PRE(fd->md_type == MT_SOCK);
	return TGT_MASK | src_fdnum(fd);
}

static struct mock_fd *userfd0(int fdnum)
{
	int idx;

	fdnum &= ~TGT_MASK;
	idx = (fdnum & 0x7fff) - 10;
	return &m_table[idx];
}

static struct mock_fd *userfd(int fdnum)
{
	struct mock_fd *fd = userfd0(fdnum);
	M0_ASSERT(mock_fd_invariant(fd) &&
		  fd->md_gen == (fdnum & ~TGT_MASK) >> 15);
	return fd;
}

static bool is_closed(int fdnum, struct mock_fd *fd)
{
	return fd->md_flags & ((fdnum & TGT_MASK) ?
			       MF_CLOSED_TGT : MF_CLOSED_SRC);
}

static int mock_ep(const struct sockaddr *addr)
{
	struct sockaddr_in *sin = (void *)addr;
	M0_ASSERT(sin->sin_port != 0);
	M0_ASSERT(sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK));
	return sin->sin_port;
}

static int mock_socket(int family, int type, int proto)
{
	if (mock_prologue() || MOCK_DICE(socket_err))
		return mock_ret(mock_err());
	return mock_ret(src_fdnum(get_fd(MT_SOCK)));
}

static int mock_accept4(int fdnum, struct sockaddr *addr,
			socklen_t *addrlen, int flags)
{
	int i;
	struct mock_fd *fd;
	struct sockaddr_in *sin = (void *)addr;
	M0_PRE(*addrlen >= sizeof *sin);
	if (mock_prologue() || MOCK_DICE(accept4_err))
		return mock_ret(mock_err());
	fd = userfd(fdnum);
	M0_PRE(fd->md_type == MT_SOCK);
	M0_PRE(fd->md_flags == MF_LISTEN);
	for (i = 0; i < m_conf->uc_maxfd; ++i) {
		struct mock_fd *scan = &m_table[i];
		if (scan->md_type == MT_SOCK && scan->md_tgt == fd->md_src &&
		    !(scan->md_flags & MF_CONNECTED)) {
			if (MOCK_DICE(accept4_miss))
				continue;
			sin->sin_port = scan->md_src;
			sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			*addrlen = sizeof *sin;
			scan->md_flags |= MF_CONNECTED;
			return mock_ret(tgt_fdnum(scan));
		}
	}
	return mock_ret(-EWOULDBLOCK);
}

static int mock_listen(int fdnum, int backlog)
{
	struct mock_fd *fd;
	if (mock_prologue() || MOCK_DICE(listen_err))
		return mock_ret(mock_err());
	fd = userfd(fdnum);
	M0_PRE(fd->md_type == MT_SOCK);
	M0_PRE(fd->md_flags == 0);
	M0_PRE(fd->md_src != 0);
	M0_PRE(backlog >= 0);
	fd->md_flags = MF_LISTEN;
	return mock_ret(0);
}

static int mock_bind(int fdnum, const struct sockaddr *addr, socklen_t addrlen)
{
	struct mock_fd *fd;
	if (mock_prologue() || MOCK_DICE(bind_err))
		return mock_ret(mock_err());
	fd = userfd(fdnum);
	M0_PRE(fd->md_type == MT_SOCK);
	M0_PRE(fd->md_flags == 0);
	fd->md_src = mock_ep(addr);
	return mock_ret(0);
}

static int mock_connect(int fdnum,
			const struct sockaddr *addr, socklen_t addrlen)
{
	struct mock_fd *fd;
	if (mock_prologue() || MOCK_DICE(connect_err))
		return mock_ret(mock_err());
	fd = userfd(fdnum);
	M0_PRE(fd->md_type == MT_SOCK);
	M0_PRE(fd->md_flags == 0);
	fd->md_tgt = mock_ep(addr);
	fd->md_src = m_ephemeral;
	if (++m_ephemeral == M_EPHEMERAL_HI)
		m_ephemeral = M_EPHEMERAL_LO;
	return mock_ret(0);
}

static ssize_t mock_readv(int fdnum, const struct iovec *iov, int iovcnt)
{
	int     i;
	int     j;
	ssize_t nob = 0;
	char    byte;
	struct mock_fd *fd;
	if (mock_prologue() || MOCK_DICE(readv_err))
		return mock_ret(mock_err());
	fd = userfd(fdnum);
	M0_PRE(fd->md_type == MT_SOCK);
	M0_PRE((fd->md_flags & MF_CONNECTED) && !is_closed(fdnum, fd));
	if (fd->md_head >= fd->md_tail || !(fdnum & TGT_MASK) ||
	    MOCK_DICE(readv_again))
		return mock_ret(-EWOULDBLOCK);
	if (MOCK_DICE(readv_0))
		return mock_ret(0);
	for (i = 0; i < iovcnt; ++i) {
		for (j = 0; j < iov[i].iov_len; ++j) {
			if (fd->md_head >= fd->md_tail ||
			    MOCK_DICE(readv_break))
				return mock_ret(nob);
			if (MOCK_DICE(readv_skip))
				continue;
			byte = fd->md_buf.u_sock[fd->md_head++ % fd->md_len];
			if (MOCK_DICE(readv_flip))
				byte ^= 8;
			((char *)iov[i].iov_base)[j] = byte;
			++nob;
		}
	}
	return mock_ret(nob);
}

static ssize_t mock_writev(int fdnum, const struct iovec *iov, int iovcnt)
{
	int     i;
	int     j;
	ssize_t nob = 0;
	struct mock_fd *fd;
	if (mock_prologue() || MOCK_DICE(writev_err))
		return mock_ret(mock_err());
	fd = userfd(fdnum);
	M0_PRE(fd->md_type == MT_SOCK);
	M0_PRE(!(fdnum & TGT_MASK));
	M0_PRE((fd->md_flags & MF_CONNECTED) && !is_closed(fdnum, fd));
	if (fd->md_tail - fd->md_head > fd->md_len || (fdnum & TGT_MASK) ||
	    MOCK_DICE(writev_again))
		return mock_ret(-EWOULDBLOCK);
	if (MOCK_DICE(writev_0))
		return mock_ret(0);
	for (i = 0; i < iovcnt; ++i) {
		for (j = 0; j < iov[i].iov_len; ++j) {
			if (fd->md_tail - fd->md_head >= fd->md_len ||
			    MOCK_DICE(writev_break))
				return mock_ret(nob);
			fd->md_buf.u_sock[fd->md_tail++ % fd->md_len] =
				((char *)iov[i].iov_base)[j];
			++nob;
		}
	}
	return mock_ret(nob);
}

static int mock_close(int fdnum)
{
	struct mock_fd *fd;
	/*
	 * Special case: close() has to close the descriptor even if it fails.
	 */
	int ret = mock_prologue() || MOCK_DICE(close_err) ? mock_err() : 0;
	fd = userfd0(fdnum);
	if (fd->md_gen != (fdnum & ~TGT_MASK) >> 15)
		return mock_ret(ret);  /* The slot can be closed and re-used. */
	if (fd->md_type == MT_SOCK) {
		int i;
		int j;
		fd->md_flags |= (fdnum & TGT_MASK) ?
			MF_CLOSED_TGT : MF_CLOSED_SRC;
		if ((fd->md_flags & (MF_CLOSED_TGT | MF_CLOSED_SRC)) ==
		    (MF_CLOSED_TGT | MF_CLOSED_SRC)) {
			/* Clean from all epoll tables. */
			for (i = 0; i < m_conf->uc_maxfd; ++i) {
				struct mock_fd *scan = &m_table[i];
				if (scan->md_type == MT_EPOLL) {
					for (j = 0; j < scan->md_tail; ++j) {
						struct mock_epoll_entry *ee =
						       &scan->md_buf.u_epoll[j];
						if (ee->ee_fd != 0 &&
						    userfd(ee->ee_fd) == fd)
							ee->ee_fd = 0;
					}
				}
			}
			fd->md_type = MT_FREE;
		}
	} else {
		fd->md_type = MT_FREE;
	}
	return mock_ret(ret);
}

static int mock_shutdown(int fdnum, int how)
{
	return mock_close(fdnum);
}

static int mock_setsockopt(int fd, int level, int optname,
			   const void *optval, socklen_t optlen)
{
	if (mock_prologue() || MOCK_DICE(setsockopt_err))
		return mock_ret(mock_err());
	else
		return mock_ret(0);
}

static int mock_epoll_create(int size)
{
	if (mock_prologue() || MOCK_DICE(epoll_create_err))
		return mock_ret(mock_err());
	return mock_ret(src_fdnum(get_fd(MT_EPOLL)));
}

static int mock_epoll_wait(int fdnum, struct epoll_event *events,
			   int maxevents, int timeout)
{
	int i;
	int used = 0;
	struct mock_fd *set;
	struct mock_fd *fd;
	if (mock_prologue() || MOCK_DICE(epoll_wait_err))
		return mock_ret(mock_err());
	set = userfd(fdnum);
	M0_PRE(set->md_type == MT_EPOLL);
	for (i = 0; i < set->md_tail; ++i) {
		struct mock_epoll_entry *ee = &set->md_buf.u_epoll[i];
		bool in  = ee->ee_event.events & EPOLLIN;
		bool out = ee->ee_event.events & EPOLLOUT;
		bool connected;
		uint32_t mask = 0;
		bool tgt;
		if (ee->ee_fd == 0)
			continue;
		tgt = ee->ee_fd & TGT_MASK;
		fd = userfd(ee->ee_fd);
		M0_ASSERT(fd->md_type == MT_SOCK);
		connected = (fd->md_flags & MF_CONNECTED) &&
			!is_closed(ee->ee_fd, fd);
		if (MOCK_DICE(epoll_raise_err))
			mask |= EPOLLERR;
		if (MOCK_DICE(epoll_raise_in))
			mask |= EPOLLIN;
		if (MOCK_DICE(epoll_raise_out))
			mask |= EPOLLOUT;
		if (MOCK_DICE(epoll_raise_garbage))
			mask |= ~EPOLLIN;
		if (in && (fd->md_flags & MF_LISTEN) &&
			   m0_exists(j, m_conf->uc_maxfd,
				     m_table[j].md_type == MT_SOCK &&
				     m_table[j].md_tgt == fd->md_src &&
				     !(m_table[j].md_flags & MF_CONNECTED))) {
			mask |= EPOLLIN;
		}
		if (in && tgt && connected && fd->md_head < fd->md_tail)
			mask |= EPOLLIN;
		if (out && !tgt && connected &&
		    fd->md_tail - fd->md_head < fd->md_len)
			mask |= EPOLLOUT;
		if (mask != 0) {
			events[used].data = ee->ee_event.data;
			events[used].events = mask;
			if (++used == maxevents || MOCK_DICE(epoll_break))
				break;
		}
	}
	if (used == 0) {
		m0_mutex_unlock(&m_mock_lock);
		m0_nanosleep(timeout * 1000000 / 10, NULL);
		m0_mutex_lock(&m_mock_lock);
	}
	return mock_ret(used);
}

static int mock_epoll_ctl(int epfdnum, int op, int fdnum,
			  struct epoll_event *event)
{
	int i;
	struct mock_fd *set;
	struct mock_fd *fd;
	if (mock_prologue() || MOCK_DICE(epoll_ctl_err))
		return mock_ret(mock_err());
	set = userfd(epfdnum);
	fd  = userfd(fdnum);
	M0_PRE(set->md_type == MT_EPOLL);
	M0_PRE(fd->md_type == MT_SOCK);
	switch (op) {
	case EPOLL_CTL_ADD:
		for (i = 0; i < set->md_tail; ++i) {
			if (set->md_buf.u_epoll[i].ee_fd == 0)
				break;
		}
		if (i == set->md_tail)
			set->md_tail++;
		M0_ASSERT(set->md_tail <= set->md_len);
		set->md_buf.u_epoll[i].ee_fd = fdnum;
		set->md_buf.u_epoll[i].ee_event = *event;
		break;
	case EPOLL_CTL_DEL:
	case EPOLL_CTL_MOD:
		for (i = 0; i < set->md_tail; ++i) {
			if (set->md_buf.u_epoll[i].ee_fd == fdnum)
				break;
		}
		if (i == set->md_tail)
			return mock_ret(-ENOENT);
		if (op == EPOLL_CTL_DEL)
			set->md_buf.u_epoll[i].ee_fd = 0;
		else
			set->md_buf.u_epoll[i].ee_event = *event;
		break;
	}
	return mock_ret(0);
}

static const struct sock_ops mock_ops = {
	.so_socket       = &mock_socket,
	.so_accept4      = &mock_accept4,
	.so_listen       = &mock_listen,
	.so_bind         = &mock_bind,
	.so_connect      = &mock_connect,
	.so_readv        = &mock_readv,
	.so_writev       = &mock_writev,
	.so_close        = &mock_close,
	.so_shutdown     = &mock_shutdown,
	.so_setsockopt   = &mock_setsockopt,
	.so_epoll_create = &mock_epoll_create,
	.so_epoll_wait   = &mock_epoll_wait,
	.so_epoll_ctl    = &mock_epoll_ctl
};

static const struct sock_ops std_ops = {
	.so_socket       = &socket,
	.so_accept4      = &accept4,
	.so_listen       = &listen,
	.so_bind         = &bind,
	.so_connect      = &connect,
	.so_readv        = &readv,
	.so_writev       = &writev,
	.so_close        = &close,
	.so_shutdown     = &shutdown,
	.so_setsockopt   = &setsockopt,
	.so_epoll_create = &epoll_create,
	.so_epoll_wait   = &epoll_wait,
	.so_epoll_ctl    = &epoll_ctl
};

#include "ut/ut.h"

static struct sock_ut_conf conf_0 = {
	.uc_maxfd       = 1000,
	.uc_sockbuf_len = 64000,
	.uc_epoll_len   = 1000
};

static void tm_event_cb(const struct m0_net_tm_event *ev)
{
	m0_semaphore_up(&m_tm_state);
}

static struct m0_net_tm_callbacks m_tm_cb = {
	.ntc_event_cb = &tm_event_cb
};

static int tm_init(struct m0_net_transfer_mc *tm, const char *addr)
{
	int result;

	M0_SET0(tm);
	tm->ntm_state = M0_NET_TM_UNDEFINED;
	tm->ntm_callbacks = &m_tm_cb;
	result = m0_net_tm_init(tm, &m_dom);
	if (result == 0) {
		result = m0_net_tm_start(tm, addr);
		if (result == 0) {
			while (tm->ntm_state == M0_NET_TM_STARTING)
				m0_semaphore_down(&m_tm_state);
		}
	}
	return tm->ntm_state == M0_NET_TM_STARTED ? 0 : -EINVAL;
}

static void tm_fini(struct m0_net_transfer_mc *tm)
{
	int result;

	if (tm->ntm_state == M0_NET_TM_STARTED) {
		result = m0_net_tm_stop(tm, false);
		M0_ASSERT(result == 0);
	}
	while (tm->ntm_state == M0_NET_TM_STOPPING)
		m0_semaphore_down(&m_tm_state);
	M0_ASSERT(M0_IN(tm->ntm_state, (M0_NET_TM_STOPPED, M0_NET_TM_UNDEFINED,
					M0_NET_TM_INITIALIZED,
					M0_NET_TM_FAILED)));
	if (M0_IN(tm->ntm_state, (M0_NET_TM_STOPPED, M0_NET_TM_INITIALIZED,
				  M0_NET_TM_FAILED)))
		m0_net_tm_fini(tm);
}

struct ub_op;
struct ut_buf {
	struct m0_net_buffer     ub_buf;
	struct m0_semaphore      ub_sem;
	struct ub_op            *ub_op;
	int                      ub_status;
	bool                     ub_busy;
	bool                     ub_queued;
	void                    *ub_msg;
	m0_bcount_t              ub_len;
	struct m0_net_end_point *ub_ep;
};

static void buf_event_cb(const struct m0_net_buffer_event *ev)
{
	struct ut_buf *ub = ev->nbe_buffer->nb_app_private;

	m0_mutex_lock(&m_ut_lock);
	M0_ASSERT(ev->nbe_buffer == &ub->ub_buf);
	M0_ASSERT(ub->ub_busy);
	ub->ub_status = ev->nbe_status;
	m0_semaphore_up(&ub->ub_sem);
	ub->ub_busy = false;
	m0_mutex_unlock(&m_ut_lock);
}

const struct m0_net_buffer_callbacks m_buf_cb = {
	.nbc_cb = {
		[M0_NET_QT_MSG_RECV]          = &buf_event_cb,
		[M0_NET_QT_MSG_SEND]          = &buf_event_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = &buf_event_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND] = &buf_event_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = &buf_event_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = &buf_event_cb
	}
};

static int ut_buf_queue(struct ut_buf *buf, struct m0_net_transfer_mc *tm,
			int qtype, const char *addr, m0_time_t to, char *msg)
{
	int                   result;
	struct m0_net_buffer *nb = &buf->ub_buf;

	m0_mutex_lock(&m_ut_lock);
	M0_ASSERT(tm->ntm_state == M0_NET_TM_STARTED);
	M0_SET0(buf);
	m0_semaphore_init(&buf->ub_sem, 0);
	buf->ub_msg      = msg;
	buf->ub_len      = strlen(msg) + 1;
	nb->nb_buffer    = M0_BUFVEC_INIT_BUF(&buf->ub_msg, &buf->ub_len);
	nb->nb_length    = m0_vec_count(&nb->nb_buffer.ov_vec);
	nb->nb_offset    = 0;
	nb->nb_qtype     = qtype;
	nb->nb_callbacks = &m_buf_cb;
	nb->nb_ep        = buf->ub_ep;
	nb->nb_min_receive_size = 1;
	nb->nb_max_receive_msgs = 1;
	nb->nb_app_private      = buf;
	if (addr != NULL)
		result = m0_net_end_point_create(&buf->ub_ep, tm, addr);
	else
		result = 0;
	if (result == 0) {
		nb->nb_ep = buf->ub_ep;
		result = m0_net_buffer_register(nb, &m_dom);
		if (result == 0) {
			nb->nb_timeout = to;
			result = m0_net_buffer_add(nb, tm);
			if (result == 0) {
				buf->ub_busy   = true;
				buf->ub_queued = true;
			}
		}
	}
	m0_mutex_unlock(&m_ut_lock);
	return 0;
}

static void ut_buf_wait(struct ut_buf *ub)
{
	if (ub->ub_queued)
		m0_semaphore_down(&ub->ub_sem);
	m0_mutex_lock(&m_ut_lock);
	M0_ASSERT(!ub->ub_busy);
	if (ub->ub_ep != NULL)
		m0_net_end_point_put(ub->ub_ep);
	ub->ub_queued = false;
	m0_semaphore_fini(&ub->ub_sem);
	if (ub->ub_buf.nb_flags & M0_NET_BUF_REGISTERED)
		m0_net_buffer_deregister(&ub->ub_buf, &m_dom);
	m0_mutex_unlock(&m_ut_lock);
}

#define ST "stream"
static void smoke_with_1(const struct sock_ops *sop, struct sock_ut_conf *conf,
			 bool canfail)
{
	struct m0_net_transfer_mc tm0   = {};
	struct m0_net_transfer_mc tm1   = {};
	struct ut_buf             ub0   = {};
	struct ut_buf             ub1   = {};
	char                      msg[] = "What hath GOD wrought?";
	char                      rep[] = "-- --- .-. ... . -.-. --- -.. .";
	char                     *addr0 = "inet:" ST ":127.0.0.1@3001";
	char                     *addr1 = "inet:" ST ":127.0.0.1@3002";
	m0_time_t                 to;

	ut_init(sop, conf);
	to = canfail ? m0_time_from_now(1, 0) : M0_TIME_NEVER;
	tm_init(&tm0, addr0);
	tm_init(&tm1, addr1);
	if (tm1.ntm_state == M0_NET_TM_STARTED)
		ut_buf_queue(&ub1, &tm1, M0_NET_QT_MSG_RECV, NULL,  to, rep);
	if (tm0.ntm_state == M0_NET_TM_STARTED)
		ut_buf_queue(&ub0, &tm0, M0_NET_QT_MSG_SEND, addr1, to, msg);
	ut_buf_wait(&ub1);
	ut_buf_wait(&ub0);
	M0_ASSERT(ergo(!canfail, memcmp(msg, rep, ARRAY_SIZE(msg)) == 0));
	tm_fini(&tm1);
	tm_fini(&tm0);
	ut_fini();
}

static void smoke_with(const struct sock_ops *sop, struct sock_ut_conf *conf,
		       bool canfail, int rep, int n2, int n3)
{
	int i;
	for (i = 0; i < rep; ++i)
		smoke_with_1(sop, conf, canfail);
}

static struct sock_ut_conf conf_delay = {
	.uc_maxfd       = 1000,
	.uc_sockbuf_len = 64000,
	.uc_epoll_len   = 1000,
	.uc_delay       = 1000,
	.uc_delay_ns    = M0_TIME_ONE_SECOND / 500
};

static struct sock_ut_conf conf_stutter = {
	.uc_maxfd        = 1000,
	.uc_sockbuf_len  = 64000,
	.uc_epoll_len    = 1000,
	.uc_accept4_miss = 1000,
	.uc_readv_again  = 1000,
	.uc_readv_0      = 1000,
	.uc_readv_break  = 1000,
	.uc_writev_again = 1000,
	.uc_writev_0     = 1000,
	.uc_writev_break = 1000,
	.uc_epoll_break  = 1000
};

static struct sock_ut_conf conf_spam = {
	.uc_maxfd            = 1000,
	.uc_sockbuf_len      = 64000,
	.uc_epoll_len        = 1000,
	.uc_err              = 1000,
	.uc_socket_err       = 1000,
	.uc_accept4_err      = 1000,
	.uc_listen_err       = 1000,
	.uc_bind_err         = 1000,
	.uc_connect_err      = 1000,
	.uc_readv_err        = 1000,
	.uc_writev_err       = 1000,
	.uc_setsockopt_err   = 1000,
	.uc_close_err        = 1000,
	.uc_epoll_create_err = 1000,
	.uc_epoll_wait_err   = 1000,
	.uc_epoll_ctl_err    = 1000,
	.uc_alloc_err        = 1000
};

static struct sock_ut_conf conf_spamik = {
	.uc_maxfd            = 1000,
	.uc_sockbuf_len      = 64000,
	.uc_epoll_len        = 1000,
	.uc_err              =   10,
	.uc_socket_err       =  100,
	.uc_accept4_err      =  100,
	.uc_listen_err       =  100,
	.uc_bind_err         =  100,
	.uc_connect_err      =  100,
	.uc_readv_err        =  100,
	.uc_writev_err       =  100,
	.uc_setsockopt_err   =  100,
	.uc_close_err        =  100,
	.uc_epoll_create_err =  100,
	.uc_epoll_wait_err   =   10,
	.uc_epoll_ctl_err    =  100,
	.uc_alloc_err        =  100
};

static struct sock_ut_conf conf_adhd = {
	.uc_maxfd            = 1000,
	.uc_sockbuf_len      = 64000,
	.uc_epoll_len        = 1000,
	.uc_readv_skip       = 1000,
	.uc_readv_flip       = 1000
};

static struct sock_ut_conf *ut_confs[] = {
	&conf_delay, &conf_stutter, &conf_spam, &conf_adhd
};

static struct sock_ut_conf *ut_confs1[] = {
	&conf_delay, &conf_stutter, &conf_spamik, &conf_adhd
};

static bool ut_canfail[] = {
	false, false, true, true
};
static char ut_conf_name[] = {
	'd', 's', 'S', 'a'
};

M0_BASSERT(ARRAY_SIZE(ut_confs) == ARRAY_SIZE(ut_canfail));
M0_BASSERT(ARRAY_SIZE(ut_confs) == ARRAY_SIZE(ut_conf_name));
M0_BASSERT(ARRAY_SIZE(ut_confs) == ARRAY_SIZE(ut_confs1));

static void comb_build(int bits, bool *canfail, struct sock_ut_conf *comb,
		       struct sock_ut_conf **confs)
{
	int j;
	int k;
	M0_SET0(comb);
	*canfail = false;
	*comb = conf_0;
	for (j = 0; j < ARRAY_SIZE(ut_confs); ++j) {
		if (bits & (1 << j)) {
			int *dst = (void *)comb;
			int *src = (void *)confs[j];
			for (k = 0; k < sizeof *comb / sizeof(int); ++k){
				if (src[k] != 0)
					dst[k] = src[k];
			}
			*canfail |= ut_canfail[j];
		}
	}
}

static void smoked_comb(const struct sock_ops *sop, struct sock_ut_conf *conf,
			bool canfail, int bits, int rep, int n3)
{
	struct sock_ut_conf comb;
	comb_build(bits, &canfail, &comb, ut_confs);
	smoke_with(sop, &comb, canfail, rep, 0, 0);
}

/*
 * @@GLARING
 *
 * "Glaring" is the proper collective noun for cats (teste Book of Saint
 * Albans).
 */

enum {
	O_MSG,
	O_BLK_RECV,
	O_BLK_SEND,

	O_NR
};

static const int qtypes[O_NR][2] = {
	{ M0_NET_QT_MSG_RECV,          M0_NET_QT_MSG_SEND },
	{ M0_NET_QT_PASSIVE_BULK_RECV, M0_NET_QT_ACTIVE_BULK_SEND },
	{ M0_NET_QT_PASSIVE_BULK_SEND, M0_NET_QT_ACTIVE_BULK_RECV }
};

static const int receiver[O_NR] = { 0, 0, 1 };

struct g_tm {
	struct m0_net_transfer_mc   gt_tm;
	struct m0_net_end_point   **gt_other;
};

struct g_op;

struct g_buf {
	struct m0_net_buffer gb_buf;
	bool                 gb_used;
	bool                 gb_queued;
	struct g_op         *gb_op;
	int                  gb_rc;
};

struct g_op {
	bool          go_used;
	int           go_opc;
	struct g_buf *go_buf[2];
};

static struct g_tm  *g_tm;
static struct g_buf *g_buf;
static struct g_op  *g_op;
static int           g_op_nr;
static int           g_tm_nr;
static m0_time_t     g_to;
static struct m0_semaphore g_op_free;
static int           g_par;
static int           g_par_max;
static bool          g_canfail;

static struct g_err {
	int         e_queued;
	m0_bcount_t e_nob;
	int         e_done;
	int         e_success;
	int         e_timeout;
	int         e_cancelled;
	int         e_error;
	m0_time_t   e_time;
} g_err[O_NR];

static void g_buf_done(struct g_buf *me, int rc)
{
	struct g_buf *it;
	struct g_op  *op  = me->gb_op;
	int           self;

	m0_mutex_lock(&m_ut_lock);
	M0_ASSERT(me->gb_used);
	M0_ASSERT(me == op->go_buf[0] || me == op->go_buf[1]);
	g_err[op->go_opc].e_done++;
	me->gb_rc = rc;
	if (rc == 0) {
		g_err[op->go_opc].e_success++;
		g_err[op->go_opc].e_nob +=
			m0_vec_count(&me->gb_buf.nb_buffer.ov_vec);
		g_err[op->go_opc].e_time +=
			m0_time_now() - me->gb_buf.nb_add_time;
	} else if (rc == -ETIMEDOUT)
		g_err[op->go_opc].e_timeout++;
	else if (rc == -ECANCELED)
		g_err[op->go_opc].e_cancelled++;
	else
		g_err[op->go_opc].e_error++;
	self = me == op->go_buf[1];
	it = op->go_buf[1 - self];
	if (!it->gb_queued) {
		if (rc == 0 && it->gb_rc == 0 && op->go_opc != O_MSG &&
		    m_conf->uc_readv_skip == 0 && m_conf->uc_readv_flip == 0) {
			struct m0_bufvec_cursor src;
			struct m0_bufvec_cursor dst;

			m0_bufvec_cursor_init(&src, &me->gb_buf.nb_buffer);
			m0_bufvec_cursor_init(&dst, &it->gb_buf.nb_buffer);
			M0_ASSERT(m0_bufvec_cursor_cmp(&src, &dst) == 0);
		}
		me->gb_used = false;
		it->gb_used = false;
		me->gb_op = NULL;
		it->gb_op = NULL;
		op->go_used = false;
		m0_semaphore_up(&g_op_free);
		g_par--;
	}
	me->gb_buf.nb_ep = NULL;
	m0_net_desc_free(&me->gb_buf.nb_desc);
	me->gb_queued = false;
	m0_mutex_unlock(&m_ut_lock);
}

static void g_event_cb(const struct m0_net_buffer_event *ev)
{
	struct g_buf *me = ev->nbe_buffer->nb_app_private;

	M0_ASSERT(ev->nbe_buffer == &me->gb_buf);
	g_buf_done(me, ev->nbe_status);
}

const struct m0_net_buffer_callbacks g_buf_cb = {
	.nbc_cb = {
		[M0_NET_QT_MSG_RECV]          = &g_event_cb,
		[M0_NET_QT_MSG_SEND]          = &g_event_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = &g_event_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND] = &g_event_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = &g_event_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = &g_event_cb
	}
};

static void bvec_fill(struct m0_bufvec *bv)
{
	m0_bcount_t i;
	m0_bcount_t j;
	char        x = ((long)bv) >> 11;
	for (i = 0; i < bv->ov_vec.v_nr; ++i) {
		for (j = 0; j < bv->ov_vec.v_count[i]; ++j) {
			((char *)bv->ov_buf[i])[j] = x;
		}
	}
}

M0_INTERNAL uint64_t bvec_check(struct m0_bufvec *bv)
{
	m0_bcount_t i;
	m0_bcount_t j;
	char        x = ((long)bv) >> 11;
	for (i = 0; i < bv->ov_vec.v_nr; ++i) {
		for (j = 0; j < bv->ov_vec.v_count[i]; ++j) {
			if (((char *)bv->ov_buf[i])[j] != x)
				return (i << 32) | j;
		}
	}
	return ~0ULL;
}

static int g_buf_init(struct g_buf *buf)
{
	struct m0_net_buffer *nb = &buf->gb_buf;
	struct m0_bufvec     *vec = &nb->nb_buffer;
	m0_bcount_t seg_nr_max =
		min64(m0_net_domain_get_max_buffer_segments(&m_dom), 2048);
	m0_bcount_t seg_max =
		min64(m0_net_domain_get_max_buffer_segment_size(&m_dom),
		      1024 * 1024);
	m0_bcount_t nob = min64(m0_net_domain_get_max_buffer_size(&m_dom),
				1024 * 1024);
	int idx = 0;
	int i;
	m0_bcount_t frag[seg_nr_max]; /* VLA */

	while (nob > 0 && idx < seg_nr_max) {
		m0_bcount_t seg = min64(nob, mock_rnd(mock_rnd(2) ?
						      seg_max : 10) + 1);
		nob -= frag[idx++] = seg;
	}
	M0_ALLOC_ARR(vec->ov_buf, idx);
	if (vec->ov_buf == NULL)
		return -ENOMEM;
	M0_ALLOC_ARR(vec->ov_vec.v_count, idx);
	if (vec->ov_vec.v_count == NULL)
		return -ENOMEM;
	vec->ov_vec.v_nr = idx;
	for (i = 0; i < idx; ++i) {
		vec->ov_vec.v_count[i] = frag[i];
		vec->ov_buf[i] = m0_alloc(frag[i]);
		if (vec->ov_buf[i] == NULL)
			return -ENOMEM;
	}
	nb->nb_length    = m0_vec_count(&nb->nb_buffer.ov_vec);
	nb->nb_offset    = 0;
	nb->nb_callbacks = &g_buf_cb;
	nb->nb_min_receive_size = 1;
	nb->nb_max_receive_msgs = 1;
	nb->nb_app_private      = buf;
	return m0_net_buffer_register(nb, &m_dom);
}

static void g_buf_fini(struct g_buf *buf)
{
	int i;
	struct m0_bufvec *vec = &buf->gb_buf.nb_buffer;
	M0_ASSERT(!buf->gb_used);
	M0_ASSERT(buf->gb_op == NULL);
	if (buf->gb_buf.nb_flags & M0_NET_BUF_REGISTERED)
		m0_net_buffer_deregister(&buf->gb_buf, &m_dom);
	if (vec->ov_buf != NULL) {
		for (i = 0; i < vec->ov_vec.v_nr; ++i) {
			if (vec->ov_buf[i] != NULL)
				m0_free(vec->ov_buf[i]);
		}
		m0_free0(&vec->ov_buf);
	}
	m0_free0(&vec->ov_vec.v_count);
	M0_SET0(buf);
}

static int g_tm_init(struct g_tm *tm)
{
	static const char fmt[] = "inet:" ST ":127.0.0.1@%i";
	char pad[100];
	int result;

	sprintf(pad, fmt, (int)(tm - g_tm) + 3000);
	result = tm_init(&tm->gt_tm, pad);
	if (result == 0) {
		M0_ALLOC_ARR(tm->gt_other, g_tm_nr);
		result = tm->gt_other != NULL ? 0 : -ENOMEM;
	}
	return result;
}

static void g_tm_fini(struct g_tm *tm)
{
	int i;
	if (tm->gt_other != NULL) {
		for (i = 0; i < g_tm_nr; ++i)
			if (tm->gt_other[i] != NULL)
				m0_net_end_point_put(tm->gt_other[i]);
		m0_free0(&tm->gt_other);
	}
	tm_fini(&tm->gt_tm);
	M0_SET0(tm);
}

static int g_tm_lookup(struct g_tm *tm)
{
	int i;
	int result;
	for (i = 0; i < g_tm_nr; ++i) {
		if (i == tm - g_tm)
			continue;
		result = m0_net_end_point_create(&tm->gt_other[i], &tm->gt_tm,
					       g_tm[i].gt_tm.ntm_ep->nep_addr);
		if (result != 0)
			return result;
	}
	return 0;
}

static void g_op_init(struct g_op *op)
{
}

static void g_op_fini(struct g_op *op)
{
	M0_SET0(op);
}

static struct g_buf *g_buf_get(void)
{
	int           i;
	int           buf_nr = 2 * g_op_nr;
	struct g_buf *buf;
	for (i = mock_rnd(buf_nr); g_buf[i % buf_nr].gb_used; ++i)
		;
	buf = &g_buf[i % buf_nr];
	buf->gb_used = true;
	return buf;
}

static void g_op_select(void)
{
	struct g_tm  *tm0;
	struct g_tm  *tm1;
	struct g_op  *op;
	struct g_buf *b[2];
	int           i;
	int           larger;
	int           result;
	struct m0_net_buffer *nb[2];

	m0_semaphore_down(&g_op_free);
	m0_mutex_lock(&m_ut_lock);
	if (mock_rnd(20) < 15) {
		b[0] = &g_buf[mock_rnd(2 * g_op_nr)];
		m0_net_buffer_del(&b[0]->gb_buf,
				  b[0]->gb_buf.nb_tm ?: &g_tm[0].gt_tm);
	}
	tm0 = &g_tm[mock_rnd(g_tm_nr)];
	tm1 = &g_tm[(tm0 - g_tm + mock_rnd(g_tm_nr - 1) + 1) % g_tm_nr];
	M0_ASSERT(tm0 != tm1);
	for (op = &g_op[0]; op < &g_op[g_op_nr] && op->go_used; ++op)
		;
	M0_ASSERT(!op->go_used);
	op->go_used = true;
	b[0] = g_buf_get();
	b[1] = g_buf_get();
	m0_mutex_unlock(&m_ut_lock);

	larger = b[1]->gb_buf.nb_length >= b[0]->gb_buf.nb_length;
	op->go_opc = mock_rnd(O_NR);
	op->go_buf[    receiver[op->go_opc]] = b[    larger];
	op->go_buf[1 - receiver[op->go_opc]] = b[1 - larger];
	nb[0] = &op->go_buf[0]->gb_buf;
	nb[1] = &op->go_buf[1]->gb_buf;
	for (i = 0; i < 2; ++i) {
		nb[i]->nb_timeout = g_to == M0_TIME_NEVER ?
			g_to : m0_time_now() + g_to;
		nb[i]->nb_qtype = qtypes[op->go_opc][i];
		b[i]->gb_op = op;
		b[i]->gb_queued = true;
		bvec_fill(&b[i]->gb_buf.nb_buffer);
	}
	g_par++;
	g_par_max = max64(g_par, g_par_max);
	result = m0_net_buffer_add(nb[0], &tm0->gt_tm);
	if (result == 0) {
		g_err[op->go_opc].e_queued++;
		if (op->go_opc != O_MSG)
			result = m0_net_desc_copy(&nb[0]->nb_desc,
						  &nb[1]->nb_desc);
		if (result == 0) {
			if (op->go_opc == O_MSG)
				nb[1]->nb_ep = tm1->gt_other[tm0 - g_tm];
			result = m0_net_buffer_add(nb[1], &tm1->gt_tm);
			if (result == 0)
				g_err[op->go_opc].e_queued++;
		}
		if (result != 0)
			g_buf_done(op->go_buf[1], result);
	} else {
		op->go_buf[1]->gb_queued = false;
		g_buf_done(op->go_buf[0], result);
	}
}

static int glaring_init(const struct sock_ops *sop, struct sock_ut_conf *conf,
			int tm_nr, int op_nr, bool canfail)
{
	int i;
	int result;
	ut_init(sop, conf);
	/* Enable realiable allocations for setup. */
	if (conf->uc_alloc_err > 0)
		m0_fi_disable("m0_alloc", "fail_allocation");
	g_tm_nr = tm_nr;
	g_op_nr = op_nr;
	g_to = canfail ? M0_MKTIME(5, 0) : M0_TIME_NEVER;
	g_par = 0;
	g_par_max = 0;
	g_canfail = canfail;
	M0_SET_ARR0(g_err);
	M0_ALLOC_ARR(g_tm, tm_nr);
	if (g_tm == NULL)
		return -ENOMEM;
	for (i = 0; i < tm_nr; ++i) {
		result = g_tm_init(&g_tm[i]);
		if (result != 0)
			return result;
	}
	for (i = 0; i < tm_nr; ++i) {
		result = g_tm_lookup(&g_tm[i]);
		if (result != 0)
			return result;
	}
	M0_ALLOC_ARR(g_buf, 2 * op_nr);
	if (g_buf == NULL)
		return -ENOMEM;
	for (i = 0; i < 2 * op_nr; ++i) {
		result = g_buf_init(&g_buf[i]);
		if (result != 0)
			return result;
	}
	M0_ALLOC_ARR(g_op, op_nr);
	if (g_op == NULL)
		return -ENOMEM;
	for (i = 0; i < op_nr; ++i)
		g_op_init(&g_op[i]);
	m0_semaphore_init(&g_op_free, op_nr);
	if (conf->uc_alloc_err > 0)
		m0_fi_enable_each_nth_time("m0_alloc", "fail_allocation",
					   10000 / conf->uc_alloc_err);
	return 0;
}

static void glaring_fini()
{
	int i;
	m0_semaphore_fini(&g_op_free);
	if (g_op != NULL) {
		for (i = 0; i < g_op_nr; ++i)
			g_op_fini(&g_op[i]);
	}
	m0_free0(&g_op);
	if (g_buf != NULL) {
		for (i = 0; i < 2 * g_op_nr; ++i)
			g_buf_fini(&g_buf[i]);
	}
	m0_free0(&g_buf);
	if (g_tm != NULL) {
		for (i = 0; i < g_tm_nr; ++i)
			g_tm_fini(&g_tm[i]);
	}
	m0_free0(&g_tm);
	ut_fini();
}

static void glaring_with(const struct sock_ops *sop, struct sock_ut_conf *conf,
			 bool canfail, int scale, int n2, int n3)
{
	struct sock_ut_conf c = *conf;
	int square = scale * scale;
	int i;

	c.uc_maxfd = max32(c.uc_maxfd, 2 * square);
	c.uc_epoll_len = max32(c.uc_epoll_len, 2 * square);
	for (i = 0; i < 10; ++i) {
		if (glaring_init(sop, &c, scale, square, canfail || true) == 0)
			break;
		glaring_fini();
	}
	if (i < 10) {
		for (i = 0; i < 2 * square; ++i)
			g_op_select();
		for (i = 0; i < square; ++i)
			m0_semaphore_down(&g_op_free);
		printf("\npar-max: %i\n", g_par_max);
		if (getenv("M0_SOCK_UT_PRINT") != NULL) {
			for (i = 0; i < ARRAY_SIZE(g_err); ++i) {
				printf("%i: %i %"PRId64" %i %i %i %i %i %"
				       PRId64" %f\n", i,
				       g_err[i].e_queued,  g_err[i].e_nob,
				       g_err[i].e_done,    g_err[i].e_success,
				       g_err[i].e_timeout, g_err[i].e_cancelled,
				       g_err[i].e_error,   g_err[i].e_time,
				       1000.0 * g_err[i].e_nob/g_err[i].e_time);
			}
		}
		glaring_fini();
	}
}

/* Felis silvestrus. */
static void glaring_comb(const struct sock_ops *sop, struct sock_ut_conf *conf,
			 bool canfail, int bits, int scale, int gauge)
{
	struct sock_ut_conf comb;
	static struct sock_ut_conf scaled[ARRAY_SIZE(ut_confs1)];
	static struct sock_ut_conf *ref[ARRAY_SIZE(ut_confs1)];
	int i;
	int j;

	for (i = 0; i < ARRAY_SIZE(scaled); ++i) {
		int *field = (void *)&scaled[i];
		scaled[i] = *ut_confs1[i];
		ref[i] = &scaled[i];
		for (j = 0; j < sizeof scaled[i] / sizeof(int); ++j)
			field[j] /= gauge;
		scaled[i].uc_maxfd       = conf_0.uc_maxfd;
		scaled[i].uc_sockbuf_len = conf_0.uc_sockbuf_len;
		scaled[i].uc_epoll_len   = conf_0.uc_epoll_len;
	}
	comb_build(bits, &canfail, &comb, ref);
	glaring_with(sop, &comb, canfail, scale, 0, 0);
}

extern const struct m0_ut *m0_ut_current_test __attribute__((weak));

struct sock_ut_test {
	void (*sut_f)(const struct sock_ops *sop, struct sock_ut_conf *conf,
		      bool canfail, int n1, int n2, int n3);
	const struct sock_ops *sut_sop;
	struct sock_ut_conf   *sut_conf;
	bool                   sut_canfail;
	int                    sut_n1;
	int                    sut_n2;
	int                    sut_n3;
} sock_ut_test[M0_UT_SUITE_TESTS_MAX];

static struct m0_ut_suite net_sock_ut;

static void sock_ut_entry(void)
{
	int idx = m0_ut_current_test - &net_sock_ut.ts_tests[0];
	struct sock_ut_test *sut = &sock_ut_test[idx];
	M0_PRE(IS_IN_ARRAY(idx, sock_ut_test));
	sut->sut_f(sut->sut_sop, sut->sut_conf, sut->sut_canfail,
		   sut->sut_n1, sut->sut_n2, sut->sut_n3);
}

M0_INTERNAL struct m0_ut_suite *m0_net_sock_ut_build(void)
{
	int   pos = 0;
	int   i;
	int   j;
	int   scale;

#define NAME(fmt, ...) ({			\
	char *__name = NULL;			\
	m0_asprintf(&__name, fmt, __VA_ARGS__); \
	M0_ASSERT(__name != NULL);		\
	__name;				\
})

#define ADD(name, ...) ({						\
	M0_ASSERT(pos < ARRAY_SIZE(sock_ut_test));			\
	sock_ut_test[pos] = (struct sock_ut_test){ __VA_ARGS__ };	\
	net_sock_ut.ts_tests[pos] = (struct m0_ut){ name, &sock_ut_entry, \
						    "Nikita" };	\
	pos++;								\
})

	ADD("std-smoke",     &smoke_with, &std_ops,  &conf_0,       false,  1);
	ADD("mock-smoke",    &smoke_with, &mock_ops, &conf_0,       false,  1);
	ADD("delay-smoke",   &smoke_with, &mock_ops, &conf_delay,   false, 10);
	ADD("stutter-smoke", &smoke_with, &mock_ops, &conf_stutter, false, 10);
	ADD("spam-smoke",    &smoke_with, &mock_ops, &conf_spam,     true, 10);
	ADD("adhd-smoke",    &smoke_with, &mock_ops, &conf_adhd,     true, 10);
	for (scale = 1; scale < 3; scale++) {
		ADD(NAME("tabby-%i", 10 * scale),
		    &glaring_with, &std_ops, &conf_0, false, 10 * scale);
	}
	for (i = 0; i < (1 << ARRAY_SIZE(ut_confs)); ++i) {
		char pad[] = "-----------------------------";
		bool cfail = false;
		for (j = 0; j < ARRAY_SIZE(ut_confs); ++j) {
			if (i & (1 << j)) {
				cfail |= ut_canfail[j];
				pad[j] = ut_conf_name[j];
			}
		}
		pad[j] = 0;
		ADD(NAME("smoked-%s", pad),
		    &smoked_comb, &mock_ops, NULL, cfail, i, 10, 0);
		for (scale = 1; scale < 3; scale++) {
			int gauge = 1;
			for (gauge = 1; gauge < 1000; gauge *= 30) {
				ADD(NAME("wildcat-%i-%s/%i", 10 * scale, pad,
					 gauge),
				    &glaring_comb, &mock_ops, NULL, cfail, i,
				    10 * scale, gauge);
			}
		}
	}
	return &net_sock_ut;
#undef ADD
#undef NAME
}

static struct m0_ut_suite net_sock_ut = {
	.ts_name = "sock-ut"
};

#undef MOCK_DICE
#undef EP_GET
#undef EP_PUT
#undef SAFE
#undef EP_F
#undef EP_P
#undef EP_FL
#undef EP_PL
#undef SOCK_F
#undef SOCK_P
#undef B_F
#undef B_P
#undef TLOG
#undef EP_DEBUG
#undef USE_INVARIANTS
#undef MOCK_LNET
#undef SOP
#undef M0_TRACE_SUBSYSTEM

/** @} end of netsock group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
