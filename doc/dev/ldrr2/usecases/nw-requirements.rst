=============================
Network requirements usecases
=============================

:author: Madhavrao Vemuri <madhav.vemuri@seagate.com>

.. list-table::
   :header-rows: 1

   * - id
     - feature
     - description
     - priority
   * - NET-10
     - It should be possible to connect LR to 10G, 25G and 100G networks
     - 
     - p0
   * - NET-12
     - It should be possible to connect LR to Cisco, Juniper, Arista and Mellanox switches 
     - 
     - p0
   * - NET-20
     - LR must allow static IPs configuration for all interfaces
     - 
     - p0

Use cases
=========
 
 .. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.net-10.NIC-speed]
   * - **context**
     - cortx supported network
   * - **trigger**
     - Save cost with low speed NIC's
       Refer CT-10
   * - **requirements**
     - It should be possible to connect LR to 10G, 25G and
       100G networks
   * - **assumptions**
     - rdma is supported by NIC
   * - **interaction**
     - For LR R2 minimum 50Gbps network speed is needed.
       100G NIC with dual port is needed, one port is used for private data and
       other port is used for public data.
       If 25G is used two NIC's are needed for private data and two are for public
       data, so four ports needs to be reserved in the switch per node.
       For 10G NIC each node 6 cards are needed i.e per node 6 ports needs to be
       reserved in the switch.
       This may need more ports in the switch, which can add cost and complexity.
     
 .. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.net-12.switches-from-different-vendors]
   * - **context**
     - cortx supported network
   * - **trigger**
     - Avoid vendor lock-in for switches.
   * - **requirements**
     - It should be possible to connect LR to Cisco, Juniper, Arista and Mellanox switches.
   * - **assumptions**
     - Switches needs to support rdma and number of ports and their speeds should be same
   * - **interaction**
     -
     
 .. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.net-20.static-ip]
   * - **context**
     - cortx supported network
   * - **trigger**
     - 
   * - **requirements**
     - LR must allow static IPs configuration for all interfaces
   * - **interaction**
     -
