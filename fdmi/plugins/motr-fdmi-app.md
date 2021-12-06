# CORTX-Motr FDMI Applications / Plugins 

FDMI stands for File Data Manipulation Interface, and it is a feature in CORTX that allows developers to add new capabilities and functionalities to the system without having to natively modify the core code. 

This extension interface is really powerful since it gives more flexibility to developers so they can modify and deploy in CORTX independently on additional and dedicated nodes without compromising the fast path for Motr. 

In addition, this interface inherits the horizontal scalability property from MOTR while being reliable since it will be transactionally coupled with the core.
  
FDMI applications can be seen as traditional object or key-value applications that has a more closed relationship with CORTX-Motr. 

## FDMI Implementations Overview

FDMI is a scalable publish-subscribe interface where each Motr instance works as a source and produces records that describe operations. 

On the other side, there is a FDMI Application or Plugin that registers a filter in the Motr Filter Database to selects records that match such filters.

The filter substrings to match the `Value` part of the index Key/Value pair are configured at the CORTX initialization time.

The FDMI source instances send matched records to the FDMI Application or Plugin in batches with their transactional contexts.

For each record, the FDMI Application or Plugin performs actions and sends acknowledgements back to the source instances using Motr RPC calls.

## How to create and test FDMI Applications with examples

All FDMI related service code and examples are in the `cortx-motr/fdmi` directory in the following link: [fdmi dir]( https://github.com/Seagate/cortx-motr/tree/main/fdmi)

One of the FDMI Application example is within the `plugins` directory named as [fdmi_sample_plugin]( https://github.com/Seagate/cortx-motr/blob/main/fdmi/plugins/fdmi_sample_plugin.c). 

This application is a FDMI sample plugin written in C using `motr/client.h` interface which connects to the cluster (FDMI source instance) and registers a listener to listen for Key-Value events matching the specific FDMI filter substrings. This application then merely prints to standard output the matched records.

The executable binary file will be compiled as part of the initial Motr compilation using the corresponding `Makefile.am` dependency and the [Makefile.sub]( https://github.com/Seagate/cortx-motr/blob/main/fdmi/plugins/Makefile.sub) file defined in the same folder.

The `fdmi_sample_plugin` application can be tested in two forms: 

- Running the `fdmi_app` python script
- Running the `fdmi_plugin_st` shell script

For the first case, `fdmi_sample_plugin` communicates with the [fdmi_app]( https://github.com/Seagate/cortx-motr/blob/main/fdmi/plugins/fdmi_app) python script by printing to standard output all the FDMI records. 

To test this setup, we need to run the `fdmi_app` script typing in the console the following command:

`$sudo ./fdmi_app`

The basic arguments needed are the cluster info which will be picked by default from the `etc/motr/confd.xc` config file if not specified at the time of running. This way the FDMI plugin knows where to connect.

Examples of the flags you can provide to the python script are:
- `-pp`: `plugin path`
- `-le`: `Local endpoint`
- `-fi`: `Filter id`
- `-ha`: `HA endpoint`
- `-pf`: `Profile fid`
- `-sf`: `Process fid`

All the flags can be known by running the help:`-h` option.

Before testing, we need to have a CORTX-Motr cluster already up and running with FDMI capabilities by following these steps:

-  First, deploy a CORTX-Motr using specific configuration file and the instructions provided in the cortx-hare repo following this link: 
[CORTX deployment]( https://github.com/Seagate/cortx-hare/blob/main/README.md)

-  Second, add FDMI capabilities by defining and customizing the fdmi_filters subsection in the `CDF.yaml` config file used for deploying with `hctl` command. 
Example of a config file with the needed subsection created can be found in the following link: 
[FDMI CDF file](https://github.com/Seagate/cortx-hare/blob/main/cfgen/examples/singlenode.yaml).

After following previous steps, you can check if your cluster is running with FDMI filter capability by looking at the `/etc/motr/confd.xc` file in which case will have the filter-id specified. 

For instance, a common `confd.xc` file with FDMI defined will have an entry that follows the following format:
`{0x6c| (($FDMI_FILTER_ID), 2, $FDMI_FILTER_ID, \"{2|(0,[2:({1|(3,{2|0})}),({1|(3,{2|0})})])}\", $NODE, $DIX_PVERID, [3: $FDMI_FILTER_STRINGS], [1: \"$lnet_nid:$FDMI_PLUGIN_EP\"])}`, where the fields are specified as follow:
- `FDMI_FILTER_ID`: `Filter id`
- `NODE`: `Node in the cluster`
- `DIX_PVERID`: `Version ID`
- `FDMI_FILTER_STRINGS`: `Filter substrings`
- `FDMI_PLUGIN_EP`: `Local endpoint`

For the second example of testing the `fdmi_sample_plugin` application, we can can run this command:

`./fdmi_plugin_st.sh`

This interactive shell script will start Motr services using a specific filter configuration. 
For this case, we do not need to deploy the CORTX-Motr cluster beforehand using `hctl` command and specifying the fdmi_filter subsection in the `CDF.yaml` config file since this is internally managed by the script.

However, we need to modify manually the `/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh` within the `build_conf()` function to add the FDMI_FILTER_SUBSTRINGS and the FDMI_FILTER which will include the number of substrings used.

The `fdmi_plugin_st.sh` shell script starts the `fdmi_plugin_sample` application to listen for specific records that match with the filter already setup and do some key-value operations using the [m0kv](https://github.com/Seagate/cortx-motr/tree/main/motr/m0kv) util that reflects these specifications. 

The output will show whether the test was successful or not.

## Word count application for Hackathon event

Another fdmi example is the modification of the `fdmi_app` python script named as `fdmi_app_word_count`. 
This application prints to standard output the most popular words for new PUT object requests.

To run this script, you need to type in the console the following command:

`./fdmi_app_word_count`

To connect to your own AWS Client and read the object data you need to change the `aws_access_key_id` and `aws_secret_access_key` inside the `connect_client()` function. 

This script also allows to emulate FDMI events by polling a local directory for new created files using the `ld` option to pass the directory path and the `np` option for the number of most frequent words that you want to print to standard output.

For this emulation, each file represents a new created object, and the file content represents the object data. 

Example of running this application polling the `~/test` directory every 3 seconds and printing the 30 most popular words within each file will be the following command:

`./fdmi_app_word_count -ld ~/test -ss 3 -np 30`

## Tested by

- Sep 28, 2021: Liana Valdes Rodriguez (liana.valdes@seagate.com / lvald108@fiu.edu) tested using CentOS Linus release 7.8.2003 x86_64

## References
 
More details about the FDMI design and settings can be found in this link: 

[FDMI design](https://github.com/Seagate/cortx-motr/blob/main/doc/motr-in-prose.md#fdmi-architecture)

[FDMI code comments](https://github.com/Seagate/cortx-motr/blob/main/fdmi/fdmi.c)

[FDMI Github Page for Hackathon event](https://cortx.link/UCD)
