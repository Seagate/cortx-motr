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

## Create and Test a "Hello" FDMI sample Application 

1.  Follow the [motr QSG guide](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst) in cortx-motr repo to build Motr.

2.  Follow the [hare QSG guide](https://github.com/Seagate/cortx-hare/blob/main/README.md) in the cortx-hare repo to get a Motr cluster up and running.

    2.1. Need to edit the config file (CDF) to add a filter. Examples of CDF file can be found [here](https://github.com/Seagate/cortx-hare/blob/25274c3787b1adab1eac3a4ee5b9fef80035ebc1/cfgen/examples/singlenode.yaml#L51). In our "Hello" example, we need to modify the fdmi_filters section to be as follow:
    
         ```
         # This FDMI filter will trigger the FDMI application whenever 
         # there is a key-value pair added to Motr that matches values to substrings
         fdmi_filters:
             - name: test             # name for the filter
               node: localhost        # node where it runs
               client_index: 0        # index of the client
               substrings: ["hello"]  # substrings to match
         ```
         
    2.2. Run `hctl status` to verify your cluster is up. The current FDMI design only works for key-vale pairs or metadata fields and not for objects.
         
3.  Open a new terminal window and launch the `fdmi_app_hello` script that starts the `fdmi_sample_plugin` and listen for that filter.
         
    3.1. Example of this output, after running the `fdmi_app_hello`, will be as follow:
    
         ```
         Using the following settings:
         plugin-path = ./fdmi_sample_plugin
         hctl-path = hctl
         config-path = /etc/motr/confd.xc
         filter-id = using config-path
         Register SIGINT signal handler
         Cluster info:
             {'profile_fid': '0x7000000000000001:0x43', 'ha_endpoint': '10.230.242.37@tcp:12345:1:1', 'local_endpoint': '10.230.242.37@tcp:12345:4:1', 'process_fid':       '0x7200000000000001:0x22', 'fdmi_filter_fid': '0x6c00000000000001:0x45'}
         Listening for FDMI events on:
         ./fdmi_sample_plugin -l 10.230.242.37@tcp:12345:4:1 -h 10.230.242.37@tcp:12345:1:1 -p 0x7000000000000001:0x43 -f 0x7200000000000001:0x22 -g 0x6c00000000000001:0x45
         ```
         
    3.2. Go inside the `cortx-motr/fdmi/plugins` directory and you will see the `fdmi_app_hello` python script and `fdmi_sample_plugin` binary file that was compiled as part of motr compilation. More details about these programs can be found in the next section of this tutorial. To launch, run this command: `./fdmi_app_hello`.
        
4.  Then in previous windows terminal used for starting cluster, run some key-value operations with the [m0kv](https://github.com/Seagate/cortx-motr/tree/main/motr/m0kv) util by using the following commands:

    4.1. Create an index and put new key and value:
    
         ```
         # The -l, -h, -f, and -p are the cluster parameters
         # -l and -p arguments are set to be the second m0 client in the cluster
         # More details of how to use the m0kv util can be seen with -h option
    
         # Create first an index 1:5 (this value can be changed)
         m0kv -l 10.230.242.37@tcp:12345:4:2 -h 10.230.242.37@tcp:12345:1:1 -f '<0x7200000000000001:0x25>' -p '<0x7000000000000001:0x43>' -s index create 1:5
          
         # Put a new key and value that do not match the filter 
         m0kv -l 10.230.242.37@tcp:12345:4:2 -h 10.230.242.37@tcp:12345:1:1 -f '<0x7200000000000001:0x25>' -p '<0x7000000000000001:0x43>' -s index put 1:5 key1 value1
          
         # Put a new key with value that matches the filter and observe ouput of the fdmi_app_hello plugin in the other windows terminal
         m0kv -l 10.230.242.37@tcp:12345:4:2 -h 10.230.242.37@tcp:12345:1:1 -f '<0x7200000000000001:0x25>' -p '<0x7000000000000001:0x43>' -s index put 1:5 key2 hello
          
         # Put a new key with value that has more than one ocurrence of the word hello and observe ouput of the fdmi_app_hello plugin in the other windows terminal
         m0kv -l 10.230.242.37@tcp:12345:4:2 -h 10.230.242.37@tcp:12345:1:1 -f '<0x7200000000000001:0x25>' -p '<0x7000000000000001:0x43>' -s index put 1:5 key3 hello_new_hello_world
         ```
     
    4.2. Values can have the "hello" word as substring and still the filter will be triggered.
            
5.  Example of the `fdmi_app_hello` output for the case that matches the value will be as follows:
    ```sh
    Match on key-value pair: key='key2', value='hello'
    Number of time hello appears: 1
    Match on key-value pair: key='key3', value='hello_new_hello_world'
    Number of time hello appears: 2 
    ```

## How to create and test other FDMI Applications with examples

All FDMI related service code and examples are in the `cortx-motr/fdmi` directory in the following link: [fdmi dir]( https://github.com/Seagate/cortx-motr/tree/main/fdmi)

All the FDMI demo documents are available in `cortx-motr/doc/fdmi-demo` directory. [Click here]( https://github.com/Seagate/cortx-motr/tree/main/doc/fdmi_demo/demo-fdmi) to check. 

One of the FDMI Application example is within the `plugins` directory named as [fdmi_sample_plugin]( https://github.com/Seagate/cortx-motr/blob/main/fdmi/plugins/fdmi_sample_plugin.c). 

This application is a FDMI sample plugin written in C using `motr/client.h` interface which connects to the cluster (FDMI source instance) and registers a listener to listen for Key-Value events matching the specific FDMI filter substrings. This application then merely prints to standard output the matched records.

The executable binary file will be compiled as part of the initial Motr compilation using the corresponding `Makefile.am` dependency and the [Makefile.sub]( https://github.com/Seagate/cortx-motr/blob/main/fdmi/plugins/Makefile.sub) file defined in the same folder.

The `fdmi_sample_plugin` application can be tested in two forms: 

-  Running the `fdmi_app` python script
-  Running the `fdmi_plugin_st` shell script

For the first case, `fdmi_sample_plugin` communicates with the [fdmi_app]( https://github.com/Seagate/cortx-motr/blob/main/fdmi/plugins/fdmi_app) python script by printing to standard output all the FDMI records. 

Before testing with the `fdmi_app`, we need to have a CORTX-Motr cluster already up and running with FDMI capabilities by following these steps:

-  First, deploy a CORTX-Motr using specific configuration file and the instructions provided in the cortx-hare repo following this link: 
[CORTX deployment]( https://github.com/Seagate/cortx-hare/blob/main/README.md)

-  Second, add FDMI capabilities by defining and customizing the fdmi_filters subsection in the `CDF.yaml` config file used for deploying with `hctl` command. 
Example of a config file with the needed subsection created can be found in the following link: 
[FDMI CDF file](https://github.com/Seagate/cortx-hare/blob/25274c3787b1adab1eac3a4ee5b9fef80035ebc1/cfgen/examples/singlenode.yaml#L51).

After following previous steps, you can check if your cluster is running with FDMI filter capability by looking at the `/etc/motr/confd.xc` file which should be automatically created by the running motr instance. If FDMI is working correctly, there will be an entry that starts with '{0x6c' and which includes the string 'filter'.

Next, we are ready to test this setup. 
In order to do that, we need to run the `fdmi_app` script typing in the console the following command:

`$sudo ./fdmi_app`

The basic arguments needed are the cluster info which will be picked by default from the `etc/motr/confd.xc` config file if not specified at the time of running. This way the FDMI plugin knows where to connect.

Examples of the flags you can provide to the python script are:
-  `-pp`: `plugin path`
-  `-le`: `Local endpoint`
-  `-fi`: `Filter id`
-  `-ha`: `HA endpoint`
-  `-pf`: `Profile fid`
-  `-sf`: `Process fid`

All the flags can be known by running the help:`-h` option.

For the second example of testing the `fdmi_sample_plugin` application, we can can run this command:

`./fdmi_plugin_st.sh`

This interactive shell script will start Motr services using a specific filter configuration. 
For this case, we do not need to deploy the CORTX-Motr cluster beforehand using `hctl` command and specifying the fdmi_filter subsection in the `CDF.yaml` config file since this is internally managed by the script.

However, we need to modify manually the `/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh` within the `build_conf()` function to add the FDMI_FILTER_SUBSTRINGS and the FDMI_FILTER which will include the number of substrings used.

The `fdmi_plugin_st.sh` shell script starts the `fdmi_plugin_sample` application to listen for specific records that match with the filter already setup and do some key-value operations using the [m0kv](https://github.com/Seagate/cortx-motr/tree/main/motr/m0kv) util that reflects these specifications. 

The output will show whether the test was successful or not.

## Word count application for Hackathon event using S3 PUT requests

Another fdmi example is the modification of the `fdmi_app` python script named as `fdmi_app_word_count`. 
This application prints to standard output the most popular words for new object requests.

1.  First, we need to setup a Motr cluster with a CORTX S3 server running.

    1.1. Follow [cortx-s3server QSG](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md).
     
    1.2. To run the cluster with `hctl` command, we need to modify the fdmi_filters section of the CDF file to be as follow:
    
         ```
         # This FDMI filter will trigger the FDMI application whenever 
         # there is a key-value pair added to Motr that matches values to substrings
         fdmi_filters:
           - name: test             # name for the filter
             node: localhost        # node where it runs
             client_index: 0        # index of the client
             substrings: ["Bucket-Name", "Object-name"]  # substrings to match
         ```
         
    1.3. This configuration will make the filter to be triggered with new PUT requests that have "Bucket-Name" and "Object-Name" metadata fields.
          
2.  Setup your own AWS Client in another windows terminal by following step 5 in the [cortx-s3server QSG](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md).
 
    2.1. To connect to your own AWS Client and read the object data, you need to change the `aws_access_key_id` and `aws_secret_access_key` inside the `connect_client()` function for the `fdmi_app_word_count` with your own credentials. 
    
3.  To configure AWS in the server side (windows terminal where you cluster is running), follow step 6 of the same document (cortx-s3server QSG).

4.  Launch the FDMI application by typing in another windows terminal the following command:

    `./fdmi_app_word_count`
    
5.  On the AWS Client side, run some aws S3 operations such as create a bucket and put an object into the bucket.

    5.1. Examples of commands for creating S3 buckets and putting objects into buckets can be found in the Procedure section of the cortx-s3server QSG.
    
    5.2. Observe the output of the windows terminal where the FDMI plugin is running and you will see new records appearing for every PUT requests of new objects.

The `fdmi_app_word_count` also allows to emulate FDMI events by polling a local directory for new created files using the `ld` option to pass the directory path and the `np` option for the number of most frequent words that you want to print to standard output.

For this emulation, each file represents a new created object, and the file content represents the object data. 

Example of running this application polling the `~/test` directory every 3 seconds and printing the 30 most popular words within each file will be the following command:

`./fdmi_app_word_count -ld ~/test -ss 3 -np 30`

## References
 
More details about the FDMI design and settings can be found in this link: 

[FDMI design](https://github.com/Seagate/cortx-motr/blob/main/doc/motr-in-prose.md#fdmi-architecture)

[FDMI code comments](https://github.com/Seagate/cortx-motr/blob/main/fdmi/fdmi.c)

[FDMI Github Page for Hackathon event](https://cortx.link/UCD)

## Tested by

-  Dec 7, 2021: Liana Valdes Rodriguez (liana.valdes@seagate.com / lvald108@fiu.edu) tested using CentOS Linus release 7.8.2003 x86_64
