=======================================
CORTX Use Case for LR R2 (GUI)
=======================================


+-------------+--------------------------------------------------+----------------------------+-------------+
| GUI-10      | GUI should show status (Online/Degraded = 1 or   |                            |             |
|             | more nodes are down/Offline) of the cluster      |                            |   P0        |
+-------------+--------------------------------------------------+----------------------------+-------------+


+---------------------------+-------------------------------------------------------------------------------+
|Use Case ID                | Use Case Description                                                          |
+===========================+===============================================================================+
|usecase.GUI-10.1           | GUI queries status of devices from Hare, caches it in local DB                |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-10.2           | GUI queries status of nodes from Hare, caches it in local DB                  |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-10.3           | GUI queries status of controllers from Hare, caches it in local DB            |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-10.4           | GUI queries status of networks from Hare, caches it in local DB               |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-10.5           | GUI queries status of racks from Hare, caches it in local DB                  |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-10.6           | GUI queries status of Motr services, including ONLINE/DEGRADED from Hare.     |
|                           | Hare provides such information, by *hctl status* and *hctc status -d*         |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-10.10          | GUI shows these status data on web pages.                                     |
+---------------------------+-------------------------------------------------------------------------------+


+-------------+--------------------------------------------------+----------------------------+-------------+
| GUI-12      | GUI should show configuration of the cluster     | Network parameters for all |             |
|             |                                                  | networks (subnet/gw/IPs)   | P0          |
+-------------+--------------------------------------------------+----------------------------+-------------+

+---------------------------+-------------------------------------------------------------------------------+
|Use Case ID                | Use Case Description                                                          |
+===========================+===============================================================================+
|usecase.GUI-12.1           | GUI accepts user inputs of cluster configuration information, including       |
|                           | network (subnet/gw/IPs)                                                       |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-12.2           | GUI queries Hare with the current Motr information, including services,       |
|                           | pools, profile id, process id, disk/controller/rack, etc. There might be      |
|                           | multiple data pools and metadata pools.                                       |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-12.3           | GUI accepts importing cluster configuration in some format(YAML, or JSON,     |
|                           | etc.).                                                                        |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-12.10          | GUI shows the cluster configuration on web pages                              |
+---------------------------+-------------------------------------------------------------------------------+


+-------------+--------------------------------------------------+----------------------------+-------------+
| GUI-50      | The dashboard should be able to present          |                            |             |
|             | performance data at the cluster level            |                            | P0          |
+-------------+--------------------------------------------------+----------------------------+-------------+

+---------------------------+-------------------------------------------------------------------------------+
|Use Case ID                | Use Case Description                                                          |
+===========================+===============================================================================+
|usecase.GUI-50.1           | GUI queries performance data from each S3server, including bandwidth (GB/s),  |
|                           | IOPS, in average for last 1 minute, 10 minutes, 1 hour, 1 day.                |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-50.2           | GUI aggeragates the performance data of all S3servers and generate cluster    |
|                           | level performance data.                                                       |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-50.3           | GUI accumulates performance data and store them in local DB.                  |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-50.4           | GUI shows the performance data on dashboard. It reads history data from local |
|                           | DB.                                                                           |
+---------------------------+-------------------------------------------------------------------------------+


+-------------+--------------------------------------------------+---------------------------------+--------+
| GUI-70      | Alerts should be shown for all components in the | Nodes, all network components,  |        |
|             | cluster.                                         | SW components, etc. The exact   | P0     |
|             |                                                  | list should be defined in DR.   |        |
+-------------+--------------------------------------------------+---------------------------------+--------+

+---------------------------+-------------------------------------------------------------------------------+
|Use Case ID                | Use Case Description                                                          |
+===========================+===============================================================================+
|usecase.GUI-70.1           | GUI accepts alerts/notifications from various components. Alerts include      |
|                           | component name, type, timestamp, message, level. Alert is encoded as a YAML   |
|                           | or JSON string.                                                               |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-70.2           | Motr sends space usage alerts if threshold (70%, 80%, 90%, 95%) meets.        |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-70.3           | Hare sends device/node/controller/networks/etc/ failure alerts.               |
+---------------------------+-------------------------------------------------------------------------------+
|usecase.GUI-70.4           | GUI stores these alerts in local DB, and shows them.                          |
+---------------------------+-------------------------------------------------------------------------------+


+-------------+--------------------------------------------------+---------------------------------+--------+
| GUI-80      | Lyve Pilot connect screen must be updated to     | A placeholder, to be better     |        |
|             | support new LP requirements.                     | defined with LP team.           | P0     |
+-------------+--------------------------------------------------+---------------------------------+--------+


+---------------------------+-------------------------------------------------------------------------------+
|Use Case ID                | Use Case Description                                                          |
+===========================+===============================================================================+
|usecase.GUI-80.1           | Placeholder for Lyve Pilot connect                                            |
+---------------------------+-------------------------------------------------------------------------------+
