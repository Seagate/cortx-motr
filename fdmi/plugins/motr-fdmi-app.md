# CORTX-Motr FDMI Applications / Plugins 

FDMI stands for File Data Manipulation Interface and it is a feature in CORTX that allows developers to add new capabilities and functionalities to the system without having to natively modify the core code. 

This extension interface is really powerful since it gives more flexibility to developers so they can modify and deploy in CORTX independently on additional and dedicaded nodes without compromising the fast path for Motr. 

In addition, this interface inherits the horizontal scalability property from MOTR while being reliable since it will be transactionally coupled with the core.
  
FDMI applications can be seen as traditional object or key-value applications that has a more closed relationship with CORTX-Motr. 

## FDMI Implementations Overview

FDMI is a scalable publish-subscribe interface where each Motr instance works as a source and produces records that describe operations. 

On the other side, there is a FDMI Application or Plugin that registers a filter in the Motr Filter Database to selects records that match such filters.

The filter substrings to match the `Value` part of the index Key/Value parir are configured at the CORTX initialization time.

The FDMI source instances send matched records to the FDMI Application or Plugin in batches with their transactional contexts.

For each record, the FDMI Application or Plugin performs actions and sends acknowledgements back to the source instances using Motr RPC calls.

## How to create and FDMI Application and run with examples

All FDMI related service code and examples are in the `cortx-motr/fdmi` directory. 

One of the FDMI Application example is within the `plugins` directory named as fdmi_sample_plugin. 

This application is written in C using `motr/client.h` interface and it adds the new MOTR client to the cluster and starts listening for FDMI records or events that match the FDMI filter substrings.  

The executable will be compiled as part of the initial Motr compilation using the Makefile.am dependency and the Makefile.sub file.

This program then communicates with the fdmi_app python wrapper by printing to stdout the records. 

To test this application we need to run the fdmi_app wrapper typing in the console the following command:

`sudo ./fdmi_app`

The basic arguments needed will be picked by default from the `etc/motr/confd.xc` config file if not specified at the time of running.

We will need to have a CORTX-Motr cluster already up and running with FDMI capabilities by adding the fdmi_filters section in the CDF.yaml config file that it is used for deploying with `hctl` command.
 
You can check if your cluster is running with FDMI filter capability by looking at the /etc/motr/confd.xc file in which case will have the specific filter-id specified. 

A second sample test that also runs the fdmi_sample_plugin program can be showed by using the following command:

`./s/fdmi_plugin_st/fdmi_plugin_st.sh`

This shell script starts Motr services using a specific filter configuration in which case we do not need to deploy the CORTX-Motr cluster beforehand using `hctl` command and specifying the fdmi_filter subsection in the CDF.yaml config file.

However, we need to modify manually the `/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh` withing the `build_conf()` function to add the FDMI_FILTER_SUBSTRINGS and the FDMI_FILTER to include the number of substrings used.

The `fdmi_plugin_st.sh` script starts the fdmi_plugin_sample application to listen for specific records that match with the filter already setup and do some key-value operations that reflects this specifications. 

The output will show whether the test was successful or not.

## Word count application for Hackathon event

Another fdmi example is the modification of the `fdmi_app` python script, `fdmi_app_word_count` that prints to stdout the most popular words in new PUT object requests.

To run this script, you need to type in the console the following command:

`./fdmi_app_word_count`

To connect to your own AWS Client and read the object data you need to change the `aws_access_key_id` and `aws_secret_access_key` inside the `connect_client()` function. 

This script also allows to emulate FDMI events by polling a local directory for new created files using the `ld` option to pass the directory path and the `np` option for the number of most frequent words that you want to print to stdout.

For this emulation, each file represents a new created object and the file content represents the object data. 

Example of running this application polling the `~/test` directory every 3 seconds and printing the 30 most popular words within each file will be the following command:

`./fdmi_app_word_count -ld ~/test -ss 3 -np 30`

## Tested by

* Sep 28, 2021: Liana Valdes Rodriguez (liana.valdes@seagate.com / lvald108@fiu.edu) tested using CentOS Linus release 7.8.2003 x86_64

## References
 
More details about the FDMI design and settings can be found in this link: 

[FDMI design](https://github.com/Seagate/cortx-motr/blob/main/doc/motr-in-prose.md#fdmi-architecture)

[FDMI code comments](https://github.com/Seagate/cortx-motr/blob/main/fdmi/fdmi.c)

[FDMI Github Page for Hackathon event](https://cortx.link/UCD)

