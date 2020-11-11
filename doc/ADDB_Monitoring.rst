================
ADDB Monitoring
================

ADDB records are posted by Motr software to record the occurrence of specific situations. These situations together represent the state of the cluster at any point in time. There is a need to maintain the state information of the cluster that would contain for example, summary statistics per node & globally, total storage space, total free space, total number of files, etc. To achieve this there is a  need to monitor all ADDB records that are being generated. This monitoring of ADDB records is discussed in this document. 

ADDB monitors serve two purposes:

- To support online statistics reporting, like similar to df, vmstat, top. This is needed both globally (summarized over all nodes) and locally. 

- Inform external world (HA & cluster management tools) about exceptional conditions like failures, overload, etc. 
