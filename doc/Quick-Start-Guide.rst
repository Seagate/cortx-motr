=================
Quick Start Guide
=================
This guide provides information to get Motr component ready.

*************
Prerequisites
*************
The prerequisite that is necessary to install the Motr component is mentioned below.

- If you are using CentOS version 7.8 or lower, you'll have to install the kernel dependencies by following this link: `http://vault.centos.org/centos/<CentOS-version>/os/x86_64/Packages/` where <CentOS-version> is replaced with you version of CentOS. For example, if you are using CentOS 7.8.2003, you'll have to install this kernel dependency:

  **$ sodu yum install https://vault.centos.org/centos/7.8.2003/os/x86_64/Packages/kernel-devel-3.10.0-1127.el7.x86_64.rpm**

- CentOS-7 for x86_64 platform (ARM64 platform support work is in progress).

- **Ansible** is needed. 

  - Install EPEL repo:
  
    **$ sudo yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm**

**********
Procedure
**********
The below mentioned procedure must be followed to install the Motr component and to perform tests.

1. Cloning the Source Code (Either Build from Source or RPM Generation)

2. Building the Source Code

3. Running the Tests

**Note**: The procedure mentioned above must be initiated in accordance with the order of listing.

Cloning the Source Code
=======================
To clone the source code, perform the following steps:

Run the following commands:

- **$ git clone --recursive https://github.com/Seagate/cortx-motr.git**

- **$ cd cortx-motr**


Building the Source Code
========================
Perform the below mentioned procedure to build the source code.

1. Build and install the necessary dependencies. Run the following command to install all the dependent packages.

   - **$ sudo ./scripts/install-build-deps**

2. Reboot your system. After reboot, run the following commands to check if the lustre network is functioning accurately.

   - **$ vi /etc/modprobe.d/lnet.conf**

     A proper configuration file is needed for LNet. Please use the command *ip a* to get a list of network interfaces and then modify the *lnet.conf* to use one of the network interfaces. Please refer to the `<#Troubleshooting>`_ section for more information.
              

   - **$ sudo modprobe lnet**

   - **$ sudo lctl list_nids**

3. To compile, run either of the following commands:

   - **$ scripts/m0 make**

   - **$ scripts/m0 rebuild**

   **Note**: The **scripts/m0 rebuild** command includes some clean up.
 
RPM Generation
===============

You can also build RPMs by running the following command.

- **$ make rpms**

The following RPMs are generated in the **$HOME/rpmbuild/RPMS/x86_64** directory.

- cortx-motr-1.0.0-1_git*_3.10.0_1062.el7.x86_64.rpm

- cortx-motr-debuginfo-1.0.0-1_git*_3.10.0_1062.el7.x86_64.rpm

- cortx-motr-devel-1.0.0-1_git*_3.10.0_1062.el7.x86_64.rpm
 
- cortx-motr-tests-ut-1.0.0-1_git*_3.10.0_1062.el7.x86_64.rpm

Note : Check contents of **$HOME/rpmbuild/RPMS/x86_64** directory.

Running Tests
=============
Unit Test
---------
- To perform unit tests, run the following command:

  - **$ sudo ./scripts/m0 run-ut**

  Running Time (approximate) - 20 to 30 minutes

- To list all available unit tests, run the following command:

  - **$ sudo ./scripts/m0 run-ut -l**

- To run some unit test(s), e.g. "libm0-ut", run the following command:

  - **$ sudo ./scripts/m0 run-ut -t libm0-ut**

Kernel Space Unit Test
----------------------
- To perform kernel space unit tests, run the following command:

  - **$ sudo ./scripts/m0 run-kut**

System Tests
------------
- To list all the system tests, run the following command:

  - **$ sudo ./scripts/m0 run-st -l**

- To perform the motr sanity test, run the following command:

  - **$ sudo ./scripts/m0 run-st 52motr-singlenode-sanity**

- To perform all the system tests, run the following command:

  - **$ sudo ./scripts/m0 run-st**
  
Unit Benchmark
---------
- To perform unit benchmarks, run the following command:

  - **$ sudo ./scripts/m0 run-ub**

  Running Time (approximate) - 60 to 70 minutes

- To list all available unit benchmarks, run the following command:

  - **$ sudo ./scripts/m0 run-ub -l**

- To run some unit benchmark(s), e.g. "ad-ub", run the following command:

  - **$ sudo ./scripts/m0 run-ub -t ad-ub**

Troubleshooting
================
- If the pip installation fails while installing build dependencies, run the following commands:

  - **$ python -m pip uninstall pip setuptools**
  - **$ sudo ./scripts/install-build-deps**

- If an installation failure occurs due to the dependency of *pip3* , run the following commands:

  - **$ sudo yum install -y python36-setuptools**
  - **$ sudo easy_install-3.6 pip**

- If an installation failure occurs due to *ply* dependency, run the following command:

  - **$ pip3 install ply**

- If **lctl list_nids** does not render an output, perform the following steps:

  1. Create the **lnet.conf** file, if it does not exist.

  2. Restart the **lnet** service, and run the following commands:

     - **cat /etc/modprobe.d/lnet.conf**

       - **options lnet networks=tcp(eth1) config_on_load=1**

     - **sudo systemctl restart lnet**

     - **sudo lctl list_nids**

       - 192.168.1.160@tcp

     **Note**: Make sure that the eth1 interface is present in the node by checking ifconfig. Else, update the new interface in the file.

- **Build the documents**

  - Steps used to 'make' this doc:
    
  - install pip itself:
      
    - curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
        
    - python get-pip.py
  
    - pip install -U sphinx (you may need to do "rpm -e --nodeps pyparsing.noarch")
    
    - pip install sphinxcontrib.plantuml
    
    - install jre (java runtime environment) from Java.com
    
    - install plantuml from plantuml.com
    
    - create such an executable shell script:
    
      .. code-block:: bash
      
       $ cat /bin/plantuml
       #!/bin/sh
       /somewhere_to_your/bin/java -jar /somewhere_to_your/plantuml.jar $@
       
        
Tested by:

- May 2, 2021: Christina Ku (christina.ku@seagate.com) in Red Hat Enterprise Linux Server release 7.7 (Maipo)

- Apr 16, 2021: Jalen Kan (jalen.j.kan@seagate.com) in CentOS 7.9.2009 on a windows laptop running VMware Workstation Pro 16

- Mar 12, 2021: Yanqing Fu (yanqing.f.fu@seagate.com) in Red Hat Enterprise Linux Server release 7.7 (Maipo)

- Jan 27, 2021: Patrick Hession (patrick.hession@seagate.com) in CentOS 7.8.2003 on a Windows laptop running VMWare Workstation Pro 16

- Jan 20, 2021: Mayur Gupta (mayur.gupta@seagate.com) on a Windows laptop running VMware Workstation Pro 16.

- Dec 1, 2020: Huang Hua (hua.huang@seagate.com) in CentOS 7.7.1908

- Nov 25, 2020: Philippe Daniel (CEA) 

- Oct 11, 2020: Saumya Sunder (saumya.sunder@seagate.com) on a Windows laptop running VMWare Workstation Pro 16

- Oct 02, 2020: Venkataraman Padmanabhan (venkataraman.padmanabhan@seagate.com) on a Windows laptop running VMWare Workstation Pro 16

- Aug 09, 2020: Venkataraman Padmanabhan (venkataraman.padmanabhan@seagate.com) on a Windows laptop running VMWare Workstation Pro 16
