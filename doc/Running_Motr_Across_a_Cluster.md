# Running Motr Across a Cluster

This document provides information on how to build motr from source and then run a cluster of motr nodes.

## Prerequisites

1.  Create a fresh VM with CentOS 7.9.2009. ( Note: These steps were tested on Windows Hyper-V and VMware Workstation Pro 16, but should in theory work on any hypervisor)

2.  Follow the steps outline in this [guide](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst) to build Motr from sources on your node.

## Procedure

1.  ### Install HARE
    * Choose any node in your cluster, we will call this node the Main node for ease of naming. Follow the [HARE installation guide](https://github.com/Seagate/cortx-hare/blob/main/README.md), and install HARE. Once you get to the [Quick Start](https://github.com/Seagate/cortx-hare/blob/main/README.md#quick-start) you can return to this guide and continue with the next step below.

2.  ### Clone your VM
    * You can clone N times ( Note: This guide was tested with 2 nodes and 3 nodes, but in theory this should work for any N number of nodes )

3.  ### Configure your CDF ( cluster description file ) to "point" to all nodes in your cluster:
    1.  Create the CDF:
        ```sh
        cp /opt/seagate/cortx/hare/share/cfgen/examples/singlenode.yaml ~/CDF.yaml
        vi ~/CDF.yaml
        ```

    1. Configure the CDF to "point" to each node in the cluster:
        1. Add this text N-1 times, where N is the number of nodes to your CDF. The `ip a` will provide your available data_iface values, you must use the one that has `state UP`. Next, you can get the hostname for the node by running `cat /etc/hostname`. However, in some cases, the hostname might not be publicly recognizable by other nodes, so it's recommended to put an IP address instead of a hostname.
            ```sh
            - hostname: ssu0     # [user@]hostname
              data_iface: ens33        # name of data network interface
              #data_iface_type: o2ib  # type of network interface (optional);
                                      # supported values: "tcp" (default), "o2ib"
              m0_servers:
                - runs_confd: true
                  io_disks:
                    data: []
                - io_disks:
                    #meta_data: /path/to/meta-data/drive
                    data:
                        - /dev/loop0
                        - /dev/loop1
                        - /dev/loop2
                        - /dev/loop3
                        - /dev/loop4
                        - /dev/loop5
                        - /dev/loop6
                        - /dev/loop7
                        - /dev/loop8
                        - /dev/loop9
              m0_clients:
                  s3: 0         # number of S3 servers to start
                  other: 2      # max quantity of other Motr clients this host may have
            ```
            > A single node CDF should look like this:
                
                
                # Cluster Description File (CDF).
                # See `cfgen --help-schema` for the format description.
                nodes:
	            - hostname: ssu0     # [user@]hostname
	              data_iface: ens33        # name of data network interface
	              #data_iface_type: o2ib  # type of network interface (optional);
	                                      # supported values: "tcp" (default), "o2ib"
	                m0_servers:
	                      - runs_confd: true
	                        io_disks:
	                          data: []
	                      - io_disks:
	                          #meta_data: /path/to/meta-data/drive
	                          data:
	                            - /dev/loop0
	                            - /dev/loop1
	                            - /dev/loop2
	                            - /dev/loop3
	                            - /dev/loop4
	                            - /dev/loop5
	                            - /dev/loop6
	                            - /dev/loop7
	                            - /dev/loop8
	                            - /dev/loop9
	                    m0_clients:
	                      s3: 0         # number of S3 servers to start
	                      other: 2      # max quantity of other Motr clients this host may have
	                 pools:
	                   - name: the pool
	                     #type: sns  # optional; supported values: "sns" (default), "dix", "md"
	                     #disk_refs:
	                     #  - { path: /dev/loop0, node: localhost }
	                     #  - path: /dev/loop1
	                     #  - path: /dev/loop2
	                     #  - path: /dev/loop3
	                     #  - path: /dev/loop4
	                     #  - path: /dev/loop5
	                     #  - path: /dev/loop6
	                     #  - path: /dev/loop7
	                     #  - path: /dev/loop8
	                     #  - path: /dev/loop9
	                     data_units: 1
	                     parity_units: 0
	                     #allowed_failures: { site: 0, rack: 0, encl: 0, ctrl: 0, disk: 0 }
	                 #profiles:
	                 #  - name: default
	                 #    pools: [ the pool ]
                
            > Whereas a CDF with 3 nodes should look like this:
     
                # Cluster Description File (CDF).
                # See `cfgen --help-schema` for the format description.
                nodes:
	            - hostname: ssu0     # [user@]hostname
	              data_iface: ens33        # name of data network interface
	              #data_iface_type: o2ib  # type of network interface (optional);
	                                      # supported values: "tcp" (default), "o2ib"
	                m0_servers:
	                      - runs_confd: true
	                        io_disks:
	                          data: []
	                      - io_disks:
	                          #meta_data: /path/to/meta-data/drive
	                          data:
	                            - /dev/loop0
	                            - /dev/loop1
	                            - /dev/loop2
	                            - /dev/loop3
	                            - /dev/loop4
	                            - /dev/loop5
	                            - /dev/loop6
	                            - /dev/loop7
	                            - /dev/loop8
	                            - /dev/loop9
	                    m0_clients:
	                      s3: 0         # number of S3 servers to start
	                      other: 2      # max quantity of other Motr clients this host may have
	            
                    - hostname: ssu1     # [user@]hostname
	              data_iface: ens33        # name of data network interface
	              #data_iface_type: o2ib  # type of network interface (optional);
	                                      # supported values: "tcp" (default), "o2ib"
	                m0_servers:
	                      - runs_confd: true
	                        io_disks:
	                          data: []
	                      - io_disks:
	                          #meta_data: /path/to/meta-data/drive
	                          data:
	                            - /dev/loop0
	                            - /dev/loop1
	                            - /dev/loop2
	                            - /dev/loop3
	                            - /dev/loop4
	                            - /dev/loop5
	                            - /dev/loop6
	                            - /dev/loop7
	                            - /dev/loop8
	                            - /dev/loop9
	                    m0_clients:
	                      s3: 0         # number of S3 servers to start
	                      other: 2      # max quantity of other Motr clients this host may have
	           
                    - hostname: ssu2     # [user@]hostname
	              data_iface: ens33        # name of data network interface
	              #data_iface_type: o2ib  # type of network interface (optional);
	                                      # supported values: "tcp" (default), "o2ib"
	                m0_servers:
	                      - runs_confd: true
	                        io_disks:
	                          data: []
	                      - io_disks:
	                          #meta_data: /path/to/meta-data/drive
	                          data:
	                            - /dev/loop0
	                            - /dev/loop1
	                            - /dev/loop2
	                            - /dev/loop3
	                            - /dev/loop4
	                            - /dev/loop5
	                            - /dev/loop6
	                            - /dev/loop7
	                            - /dev/loop8
	                            - /dev/loop9
	                    m0_clients:
	                      s3: 0         # number of S3 servers to start
	                      other: 2      # max quantity of other Motr clients this host may have
	                 pools:
	                   - name: the pool
	                     #type: sns  # optional; supported values: "sns" (default), "dix", "md"
	                     #disk_refs:
	                     #  - { path: /dev/loop0, node: localhost }
	                     #  - path: /dev/loop1
	                     #  - path: /dev/loop2
	                     #  - path: /dev/loop3
	                     #  - path: /dev/loop4
	                     #  - path: /dev/loop5
	                     #  - path: /dev/loop6
	                     #  - path: /dev/loop7
	                     #  - path: /dev/loop8
	                     #  - path: /dev/loop9
	                     data_units: 1
	                     parity_units: 0
	                     #allowed_failures: { site: 0, rack: 0, encl: 0, ctrl: 0, disk: 0 }
	                 #profiles:
	                 #  - name: default
	                 #    pools: [ the pool ]

1. ### Disable the firewall on each node:
    This is needed by s3 server, no need to do this if you don't have s3 client (`s3: 0`) on the CDF (at `m0_clients` section).
    ```sh
    sudo systemctl stop firewalld 
    sudo systemctl disable firewalld
    ```

1. ### Make sure that selinux is turned off:
    This is needed by s3 server, no need to do this if you don't have s3 client (`s3: 0`) on the CDF (at `m0_clients` section).
    * `vi /etc/selinux/config` and make sure that selinux is turned off

1. ### Configure passwordless ssh:
    * On the main node:
        ```sh
        ssh-keygen
        ```
    * On ALL child nodes:
        ```sh
        vi /etc/ssh/sshd_config
        ```
        * And make sure that the following parameters are set:
            ```sh
            PasswordAuthentication yes
            PermitRootLogin yes
            ```
    * On the main node:
        ```sh
        ssh-copy-id -i ~/.ssh/foo root@<hostname of node or IP address>
        ```
    * On ALL child nodes:
        ```sh
        sudo passwd -l root
        vi /etc/ssh/sshd_config
        ```
        * And set the following parameters:
            ```sh
            PasswordAuthentication no
            PermitRootLogin without-password
            ```
1. ### Run the following commands on ALL ( main and child ) nodes:
    Should be run only once. If you run it twice, the second run will cancel out the setup. 
    ```sh
    sudo mkdir -p /var/motr
    for i in {0..9}; do
        sudo dd if=/dev/zero of=/var/motr/disk$i.img bs=1M seek=9999 count=1
        sudo losetup /dev/loop$i /var/motr/disk$i.img
    done
    ``` 
    Check using `lsblk | grep loop` and make sure that you have loop devices listed.
1. ### Start the cluster:
    Run this at the main node, the first node (hostname) listed at the CDF.
    ```sh
    hctl bootstrap --mkfs ~/CDF.yaml
    ```
1. ### Run I/O test:
    ```sh
    /opt/seagate/cortx/hare/libexec/m0crate-io-conf >/tmp/m0crate-io.yaml
    dd if=/dev/urandom of=/tmp/128M bs=1M count=128
    sudo m0crate -S /tmp/m0crate-io.yaml
    ```
1. ### Stop the cluster:
    ```sh
    hctl shutdown
    ```
### Tested by:
- Sep 15, 2021: Naga Kishore Kommuri (nagakishore.kommuri@seagate.com) using CentOS Linux release 7.9.2009, verified with git tag CORTX-2.0.0-77 (#7d4d09cc9fd32ec7690c94298136b372069f3ce3) on main branch
- Jul 2, 2021: Daniar Kurniawan (daniar@uchicago.edu) using CentOS 7.8.2003 on 4 bare-metal servers hosted by [Chameleon](https://www.chameleoncloud.org/) (node_type=Skylake).
- Feb 22, 2021: Mayur Gupta (mayur.gupta@seagate.com) using CentOS 7.8.2003 on a Windows laptop running VMware Workstation.
- Feb 10, 2021: Patrick Hession (patrick.hession@seagate.com) using CentOS 7.8.2003 on a Windows laptop running Windows Hyper-V.


