=================
Quick Start Guide
=================
This guide provides information to get Motr component ready.

************
Prerequisite
************
The prerequisite that is necessary to install the Motr component is mentioned below.

- Kernel Version - 3.10.0-1062.el7

  - To know the version being used, type the following:

    - **uname -r**

  - Different kernel versions that come from Centos7.7 or RHEL7.7 are supported.

**********
Procedure
**********
The below mentioned procedure must be followed to install the Motr component and to perform tests.

1. Cloning the Source Code

2. Building the Source Code

3. Running the Tests

**Note**: The procedure mentioned above must be initiated in accordance with the order of listing.

Cloning the Source Code
=======================
To clone the source code, perform the following steps:

Run the following commands:

- **$ git clone --recursive git@github.com:Seagate/cortx-motr.git -b main**

- **$ cd cortx-motr**

**Note**: To clone the source code, it is necessary to generate the SSH public key. To generate the key, refer `SSH Public Key <https://github.com/Seagate/cortx/blob/main/doc/SSH_Public_Key.rst>`_.


Building the Source Code
========================
Perform the below mentioned procedure to build the source code.

1. Build and install the necessary dependencies. Run the following command to install all the dependent packages.

   - **$ sudo ./scripts/install-build-deps**

2. Reboot your system. After reboot, run the following commands to check if the lustre network is functioning accurately.

   - **$ sudo modprobe lnet**

   - **$ sudo lctl list_nids**

3. To compile, run either of the following commands:

   - **$ sudo ./scripts/m0 make**

   - **$ sudo ./scripts/m0 rebuild**

   **Note**: The **./scripts/m0 rebuild** command includes some clean up.
   
RPM Generation
===============

If you sure about generating RPMs, run the below mentioned commands.

- $make rpms

The following RPMs are generated in the **/root/rpmbuild/RPMS/x86_64** directory.

- cortx-motr-1.0.0-1_git*_3.10.0_1062.el7.x86_64.rpm

- cortx-motr-debuginfo-1.0.0-1_git*_3.10.0_1062.el7.x86_64.rpm

- cortx-motr-devel-1.0.0-1_git*_3.10.0_1062.el7.x86_64.rpm
 
- cortx-motr-tests-ut-1.0.0-1_git*_3.10.0_1062.el7.x86_64.rpm

Running Tests
=============
Unit Test
---------
- To perform unit tests, run the following command:

  - **$ sudo ./scripts/m0 run-ut**

  Running Time (approximate) - 20 to 30 minutes

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

Troubleshooting
================
- If the pip installation fails while installing build dependencies, run the following commands:

  - **$ python -m pip uninstall pip setuptools**
  - **$ sudo ./scripts/install-build-deps**

- If an installation failure occurs due to the dependency of *pip3* , run the following commands:

  - **$ sudo yum install -y python34-setuptools**
  - **$ sudo easy_install-3.4 pip**

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
    
      ::
      
       $ cat /bin/plantuml
      
       #!/bin/sh
      
       /somewhere_to_your/bin/java -jar /somewhere_to_your/plantuml.jar $@



