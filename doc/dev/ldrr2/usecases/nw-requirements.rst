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
     - #. For LR R2 minimum 50Gbps network speed is needed for private data.
       #. 100G NIC with dual port is needed, one port is used for private data and
          other port is used for public data.
       #. If 25G is used two NIC's are needed for private data and two are for public
          data, so four ports needs to be reserved in the switch per node.
       #. For 10G NIC, each node 10 cards are needed i.e per node 10 ports needs to be
          reserved in the switch.
       #. Use of 25G or 10G NIC's may need more ports in the switch, which can add to
          the cost and complexity.
     
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
     - As per https://en.wikipedia.org/wiki/RDMA_over_Converged_Ethernet
       apart from Mellanox others don't support RoCE, but there are some other vendors
       mentioned in the wiki.
       https://www.marvell.com/products/ethernet-adapters-and-controllers/universal-rdma.html
       https://www.broadcom.com/products/ethernet-connectivity/network-adapters/200gb-nic-ocp/p2200g
     
 .. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.net-20.static-ip]
   * - **context**
     - cortx supported network
   * - **trigger**
     - For data interfaces static IP needs to be set before starting the cluster as in R1.
   * - **requirements**
     - LR must allow static IPs configuration for all interfaces
   * - **interaction**
     - As per MGM-110 and GUI-30 support for change in ip's after cluster is configured
       is not for P0.
