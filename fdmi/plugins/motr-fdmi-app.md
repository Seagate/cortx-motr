# Cortx Motr FDMI Application 

FDMI applications can be seen as traditional object or key-value applications that has a more closed relationship with Cortx-Motr. 

In this folder, we have an example of an FDMI application (fdmi_sample_plugin) that will be compiled as part of the initial Motr compilation using the Makefile.am dependency and the Makefile.sub file.

To run a simple test of this example, you can execute the following command inside this directory:

`./fdmi_plugin_st.sh`

This shell script starts Motr services using a specific filter configuration, starts the fdmi_plugin_sample application to listen for specific records that match with the filter already setup and do some key-value operations that reflects this specifications. 

The output will show whether the test was successful or not. 

A second way to test this fdmi_sample_plugin will be by directly running the fdmi_sample app wrapper with the following command:

`./fdmi_app`

This method will need to have a Cortx-motr cluster already up and running beforehand that includes FDMI capabilities by adding a fdmi_filters section in the CDF.yaml config file. 
You can check if your cluster is running with FDMI filter capability by looking at the /etc/motr/confd.xc file in which case will have the specific filter-id for the fdmi_sample_plugin. 

Example of such record in /etc/motr/confd.xc file will be:

' {0x6c| ((^l|1:81), 2, ^l|1:81, "", ^n|1:3, ^v|1:66, [1: "Bucket-Name"], [1: "192.168.12.2@tcp:12345:4:1"])},'

More details about the FDMI design and settings can be found in this link: 

[FDMI Design](https://github.com/Seagate/cortx-motr/blob/main/fdmi/fdmi.c).





