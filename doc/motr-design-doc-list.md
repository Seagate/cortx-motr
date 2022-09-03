# Motr - Design Document List

| No. | Ref file from Motr Source Code     | Line no.   | PDF File Link                                                       | GitHub Link     | 
|-----|------------------------------------|------------|---------------------------------------------------------------------|-----------------|
| 1   | capa/capa.h                        | 48         | [HLD of Capability in Motr](/doc/PDF/HLD_of_Capability_in_Motr.pdf) | [HLD Capability Motr](/doc/HLD-Capability-Motr.md) |
| 2   | cas/service.c                      | 274        | [HLD of Catalogue Service](/doc/PDF/HLD_of_catalogue_service.pdf)   | [HLD of Catalogue Services](/doc/HLD-of-Catalogue-Service.md) |
|  3  |  cm/cp.c                           |     377    |  [HLD of SNS Repair](/doc/PDF/HLD_of_SNS_Repair.pdf)                |   [HLD of SNS Repair](/doc/HLD-of-SNS-Repair.md)              |
|  4  |          conf/confd.h              |     101    | [HLD of Configuration Schema](/doc/PDF/HLD_of_configuration.schema.pdf) |  [HLD of Configuration Schema](/doc/HLD-Configuration-Schema.md)   |                 |
|     |                                   |            | [HLD of Configuration Caching](/doc/PDF/HLD_of_configuration_caching.pdf) |  [HLD of Configuration Caching](doc/HLD-of-Motr-Caching.md)  |
|   5 |            conf/objs/pool.c       |  155       | [Pools in Configuration Schema](/doc/PDF/Pools_in_configuration_schema.pdf) |  [Pools in Configuration Schema](/doc/Pools_in_configuration_schema.rst)  |
|   6 |            dix/client.h           |  153       | [HLD of Distributed Indexing](/doc/HLD-of-distributed-indexing.md) |  [HLD of Distributed Indexing](/doc/HLD-of-distributed-indexing.md)  |
|   7 |            doc/reading-list.md    |  4         | [Reading List](/doc/PDF/Motr_reading_list.pdf) |  [Reading List](/doc/reading-list.md)  |
|     |                                   |  5         | [Data Organization](/doc/PDF/Motr_Data_Organization.pdf) |  [Data Organization](/doc/Data-Organization.md)  |
|     |                                   |  6         | [Motr Architecture 2](/doc/PDF/Motr_architectural_2-pager.pdf) |  [Motr Architecture 2](/doc/Motr_Architectural_2.rst)  |
|     |                                   |  11         | [Motr Architecture](/doc/PDF/Motr_%20Architecture_Documentation.pdf) |  [Motr Architecture](https://github.com/Seagate/cortx-motr/blob/main/doc/CORTX-MOTR-ARCHITECTURE.md)  |
|     |                                   |  12         | [FAQs](/doc/PDF/Motr_FAQ.pdf) |    |
|     |                                   |  13         | [Glossary](/doc/PDF/Glossary.pdf) |    |
|     |                                   |  18         | [Containers Overview](/doc/PDF/Containers_overview.pdf) |    |
|     |                                   |  23         | [DTM Overview](/doc/PDF/DTM_overview.pdf) |  [DTM](/doc/DTM.md)  |
|     |                                   |  24         |                                           | [Resource Management Interface](/doc/HLD-Resource-Management-Interface.md)    |
|     |                                   |  31         | [HLD of Version Numbers](/doc/PDF/HLD_of_version_numbers.pdf) | [HLD of Version Numbers](/doc/HLD-Version-Numbers.md)   |
|     |                                   |  47         | [Paxos](/doc/PDF/Paxos_overview.pdf) |  [Paxos](/doc/Paxos.rst)  |
|     |                                   |  52         | [Request Handler](/doc/PDF/Request_handler.pdf) | [Request Handler](/doc/Request_Handler.rst)  |
|     |                                   |  54         | [HLD of fop state machine](/doc/PDF/HLD_of_fop_state_machine.pdf) |  [HLD of fop state machine](/doc/PDF/HLD_of_fop_state_machine.pdf)  |
|     |                                   |  61         | [SNS Overview](/doc/PDF/SNS_overview.pdf) |  [SNS Overview](/doc/SNS_Overview.rst)  |
|     |                                   |  67         | [Motr Layouts](/doc/PDF/Motr_layouts.pdf) |                                         |
|     |                                   |  77         | [AR of RPC Layer](/doc/PDF/AR_of_rpc%20layer.pdf) |  [AR of RPC Layer](/doc/RPC_Layer_Core.rst)  |
|     |                                   |  89         | [ADDB Overview](/doc/PDF/ADDB_overview.pdf) |  [ADDB](/doc/ADDB.rst)  |
|  14 |      dtm/dtm.h                    |  71         | [HLD of DTM](/doc/PDF/HLD_of_distributed_transaction_manager.pdf) |    |
|  16 |       fdmi/fdmi.c                 |  842        | [HLD of FDMI](/doc/PDF/HLD_of_FDMI_(reformatted).pdf) |  [HLD of FDMI](/doc/HLD-of-FDMI.md)  |
|  18 |       file/file.c                 |  122        | [HLD of RM Interfaces](/doc/PDF/HLD_of_RM_interfaces.pdf) |  [HLD of RM Interfaces](/doc/HLD-Resource-Management-Interface.md)  |
|  20 |       fop/fom_generic.h           |  50         | [HLD of fop object iterator](/doc/PDF/HLD_of_fop_object_iterator.pdf) | [HLD of fop object iterator](/doc/HLD-fop-object-Iterator.md)  |
|  22 |       ioservice/io_foms.c         |  556        | [FOPFOM Programming Guide](/doc/PDF/FOPFOM_Programming_Guide.pdf) | [FOPFOM Programming Guide](/doc/FOPFOM-Programming-Guide.md)  |
|  23 |       ioservice/io_fops.c         |  526        | [HLD of FOL](/doc/PDF/HLD_of_FOL.pdf) | [HLD of FOL](/doc/HLD-of-FOL.md)  |
|     |       fop/fom_generic.h           |             | [HLD of Data Block Allocator](/doc/PDF/HLD_of_fop_object_iterator.pdf) | [HLD of Data Block Allocator](/doc/HLD-Data-Block-Allocator.md)  |
|  24 |       ioservice/io_fops.c         |  795        | [RPC Bulk Transfer Task Plan](/doc/PDF/RPC_Bulk_Transfer_Task_Plan.pdf) |
|  25 |          iscservice/isc.h         |  101        | [ISC Service User Guide](/doc/PDF/ISC_user_guide.pdf) |  [ISC Service User Guide](/doc/ISC-Service-User-Guide.md)
|  26 |          iscservice/isc.h         |  57         | [HLD_of_Object_Index](/doc/PDF/HLD_of_Object_Index_(COB).pdf) |  [HLD_of_Object_Index](/doc/HLD-of-Motr-Object-Index.md) 
|  27 |          layout/layout_db.c       |  336        | [HLD of Layout Schema](/doc/PDF/HLD_of_layout_schema.pdf) |  [HLD of Layout Schema](/doc/HLD-Layout-Schema.md) 
| 33  | net/bulk_emulation/mem_xprt.h     |     95      | [RPC Bulk Transfer Task Plan](/doc/PDF/RPC_Bulk_Transfer_Task_Plan.pdf) |          
|  34 |            	net/lnet/bev_cqueue.c |      61     | [HLD of Motr LNet Transport](/doc/PDF/HLD_Motr_LNet_Transport.pdf) |  [HLD of Motr LNet Transport](/doc/HLD-OF-Motr-LNet-Transport.md)
|  54	|                  reqh/reqh.h	    |       56	  | [HLD of Request Handler](/doc/PDF/HLD_of_request_handler.pdf) |   [HLD of Request Handler](/doc/Request_Handler.rst)
|  55 |     rm/rm_rwlock.c                |   145       | [HLD of RM Interfaces](/doc/PDF/HLD_of_RM_interfaces.pdf) |  [HLD of RM Interfaces](/doc/HLD-Resource-Management-Interface.md)
| 57 |       rpc/at.h        |   203   |  |  [RP Adaptive Transmission](/doc/RPC_Adaptive_Transmission.rst)
|  |     |         | [HLD of Function Shipping](/doc/PDF/HLD_of_Function_Shipping_and_In-Storage_Compute.pdf) |  
|   |   |   | [HLD of ADDB Monitoring](/doc/PDF/HLD_of_ADDB_Monitoring.pdf) |  [HLD of ADDB Monitoring](/doc/ADDB_Monitoring.md)
|  |  | | [HLD of Auxillary Databases](/doc/PDF/HLD_%20of_%20Auxiliary_Databases_%20for_%20SNS%20repair.pdf) |  [HLD of Auxillary Databases](/doc/HLD-of-Auxillary-Databases.md)
|   |   |   | [HLD of Background Scrub](/doc/PDF/HLD_of_Background_Scrub.pdf) |  [HLD of Background Scrub](/doc/HLD-Background-Scrub.md)
|   |   |   | [HLD of Motr Lostore](/doc/PDF/HLD_of_Motr_lostore.pdf) |  [HLD of Motr Lostore](/doc/HLD-of-Motr-Lostore.md)
|   |   |   | [HLD of Motr Network Benchmark](/doc/PDF/HLD_of_Motr_Network_Benchmark.pdf) |  
|   |   |   | [HLD of Configuration Caching](/doc/PDF/HLD_of_configuration_caching.pdf) |  [HLD of Configuration Caching](/doc/HLD-of-Motr-Caching.md)
|   |   |   | [HLD of Configuration Schema](/doc/PDF/HLD_of_configuration.schema.pdf) |  [HLD of Configuration Schema](/doc/HLD-Configuration-Schema.md)
|   |   |   | [HLD of Data Block Allocator](/doc/PDF/HLD_of_data-block-allocator.pdf) |  [HLD of Data Block Allocator](/doc/HLD_Data_Block_Allocator.rst)
|   |   |   | [Data Integrity in Motr](/doc/PDF/Data_integrity_in_Motr.pdf) |  [Data Integrity in Motr](/doc/End-to-end-Data-Integrity.md)
|   |   |   | [HLD of FDMI](/doc/PDF/HLD_of_FDMI_(reformatted).pdf) |  [HLD of FDMI](/doc/HLD-of-FDMI.md)
|   |   |   | [HLD of FOL](/doc/PDF/HLD_of_FOL.pdf) |  [HLD of FOL](/doc/HLD-of-FOL.md)
|   |   |   | [HLD of HA interface](/doc/PDF/HLD_of_HA_interface.pdf) |  [HLD of HA interface](/doc/HLD-of-Motr-HA-nterface.md)
|   |   |   | [HLD of Meta Data Back End](/doc/PDF/HLD_of_meta-data_back-end.pdf) |  [HLD of Meta Data Back End](/doc/HLD-Meta-Data-Back-End.md)
|   |   |   | [HLD of NBA](/doc/PDF/HLD_of_NBA.pdf) |  
|   |   |   | [HLD of Object Index](/doc/PDF/HLD_of_Object_Index_(COB).pdf) |  [HLD of Object Index](/doc/HLD-of-Motr-Object-Index.md)
|   |   |   | [HLD of RPC Formation](/doc/PDF/HLD_of_RPC_Formation.pdf) |  [HLD of RPC Formation](/doc/RPC_Formation.rst)
|   |   |   | [RPC Layer Core](/doc/PDF/HLD_of_rpc_layer_core.pdf) |  [RPC Layer Core](/doc/RPC_Layer_Core.rst)
|   |   |   | [HLD of SNS client](/doc/PDF/HLD_of_SNS_client.pdf) |  [HLD of SNS client](/doc/HLD-of-SNS-client.md)
|   |   |   | [HLD of SNS Repair](/doc/PDF/HLD_of_SNS_Repair.pdf) |  [HLD of SNS Repair](HLD-of-SNS-Repair.md)
|   |   |   | [HLD of SNS Server](/doc/PDF/HLD_of_SNS_server.pdf) |  [HLD of SNS Server](/doc/HLD_of_SNS_Server.rst)
|   |   |   | [HLD of Version Numbers](/doc/PDF/HLD_of_version_numbers.pdf) |  [HLD of Version Numbers](/doc/HLD-Version-Numbers.md)
|   |   |   | [HLD of Spiel API](/doc/PDF/HLD.spiel.api.pdf) | [HLD of Spiel API](/doc/HLD-of-Motr-Spiel-API.md)
|   |   |   | [Repair Auxdb](/doc/PDF/repair_auxdb.pdf) | [Repair Auxdb](/doc/Repair_auxdb.rst)
|   |   |   | [HLD of SSPL](/doc/PDF/SSPL_HLD.pdf) | [HLD of SSPL](/doc/SSPL_HLD.rst)
|   |   |   | [HLD of Build Environment](/doc/PDF/Build_Environment_HLD.pdf) | 
|   |   |   | [HLD of CaStor Management](/doc/PDF/CaStor_Management_HLD.pdf) | [HLD of CaStor Management](/doc/Castor-Management.md)
|   |   |   | [HLD of Meroepochs](/doc/PDF/Copy_of_Meroepochs_%20HLD.pdf) | [HLD of Meroepochs](/doc/Motr-Epochs-HLD.md)
|   |   |   | [Data Integrity in Motr](/doc/PDF/Data_integrity_in_Motr.pdf) | [Data Integrity in Motr](/doc/End-to-end-Data-Integrity.md)






