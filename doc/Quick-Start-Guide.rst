=================
Quick Start Guide
=================
This guide provides information on how to get Motr ready.

*************
Prerequisites
*************
The prerequisite that is necessary to install the Motr component is mentioned below.

- CentOS-7 for x86_64 platform (ARM64 platform support work is in progress).

- **Ansible** is needed::

    sudo yum install epel-release # Install EPEL yum repo
    sudo yum install ansible

Get the Sources
===============
Clone Motr::

    git clone --recursive https://github.com/Seagate/cortx-motr.git

Build
=====

1. Build and install the necessary dependencies::

    cd cortx-motr
    sudo scripts/install-build-deps

2. Check the Lustre network interface configuration::

    sudo vi /etc/modprobe.d/lnet.conf

   Use ``ip a`` command to get a list of network interfaces.
   Then modify ``lnet.conf`` to use one of the listed network interfaces.
   After this run::

    sudo modprobe lnet
    sudo lctl list_nids

3. To build Motr, run::

    scripts/m0 make

   Note: use ``scripts/m0 rebuild`` command to re-build Motr.
 
RPMs Generation
===============

To build RPMs, run::

    make rpms

The generated RPMs will be placed at ``$HOME/rpmbuild/RPMS/$(arch)/`` directory.

Running Tests
=============

Unit Test
---------
- To run unit tests, use this command::

    sudo scripts/m0 run-ut

  Note: running Time (approximate) - 20 to 30 minutes

- To list all available unit tests::

    sudo scripts/m0 run-ut -l

- To run some specific unit test(s)::

    sudo scripts/m0 run-ut -t libm0-ut,be-ut

Kernel Space Unit Test
----------------------
- To run kernel space unit tests, use this command::

    sudo scripts/m0 run-kut

System Tests
------------
- To list all available system tests, run the following command::

    sudo scripts/m0 run-st -l

- To run Motr sanity test, use the following command::

    sudo scripts/m0 run-st 52motr-singlenode-sanity

- To run all system tests::

    sudo scripts/m0 run-st

  Note: it might take several hours to finish.
  
Unit Benchmark
--------------
- To run unit benchmarks, use the following command::

    sudo scripts/m0 run-ub

  Running Time (approximate) - 60 to 70 minutes

- To list all available unit benchmarks::

    sudo scripts/m0 run-ub -l

- To run some specific unit benchmark(s), e.g. "ad-ub"::

    sudo scripts/m0 run-ub -t ad-ub

Troubleshooting
================
- If the pip installation fails while installing build dependencies,
  run the following commands::

    sudo python -m pip uninstall pip setuptools
    sudo scripts/install-build-deps

- If an installation failure occurs due to the dependency of ``pip3`` ,
  run the following commands::

    sudo yum install -y python36-setuptools
    sudo easy_install-3.6 pip

- If an installation failure occurs due to ``ply`` dependency,
  run the following command::

    pip3 install ply

- If ``lctl list_nids`` does not render an output, do the following:

  1. Create the ``lnet.conf`` file, if it does not exist. And make sure
     the interface name is specified correctly there::

       $ cat /etc/modprobe.d/lnet.conf
       options lnet networks=tcp(eth1) config_on_load=1

     Check the network interfaces in your system with ``ip a`` command.

  2. Restart the ``lnet`` service, and check LNet NIDs::

       sudo systemctl restart lnet
       sudo lctl list_nids

- For other errors, please check our `FAQs <https://github.com/Seagate/cortx/blob/master/doc/Build-Installation-FAQ.md>`_.

Build the documentation
=======================

To create Motr documentation files, make sure you first install ``latex`` and ``ghostscript``::

    sudo yum install doxygen
    sudo yum install texlive-pdftex texlive-latex-bin texlive-texconfig* texlive-latex* texlive-metafont* texlive-cmap* texlive-ec texlive-fncychap* texlive-pdftex-def texlive-fancyhdr* texlive-titlesec* texlive-multirow texlive-framed* texlive-wrapfig* texlive-parskip* texlive-caption texlive-ifluatex* texlive-collection-fontsrecommended texlive-collection-latexrecommended
    sudo yum install ghostscript


Then in Motr folder run::

    make doc

The files will be generated at doc/html/ folder.


Tested by:

- June 21, 2021: Daniar Kurniawan (daniar@uchicago.edu) in CentOS 7.9.2003 on a Chameleon node (type=compute_skylake).

- May 23, 2021: Bo Wei (bo.b.wei@seagate.com) in CentOS 7.9.2009 on a Windows laptop running VirtualBox 6.1.

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
