==============================
CORTX Use Case for LR R2 (GUI)
==============================

:author: Huang Hua <hua.huang@seagate.com>


Requirements
============

.. list-table::
   :header-rows: 1

   * - id
     - feature
     - description
     - priority
   * - GUI-10
     - GUI should show status (Online/Degraded = 1 or more nodes are down/Offline) of the cluster
     -
     - p0


Use cases
=========

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-10.query-cluster-status]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI queries status of Motr services, including ONLINE/DEGRADED/DOWN/OFFLINE from Hare.
       Hare provides such information, by *hctl status* and *hctl status -d*.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-10.cache-cluster-status]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI caches status of Motr services in local DB.


.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-10.show-cluster-status]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI shows the latest status of Motr services.


Requirements
============

.. list-table::
   :header-rows: 1

   * - id
     - feature
     - description
     - priority
   * - GUI-12
     - GUI should show configuration of the cluster
     - Network parameters for all networks (subnet/gw/IPs)
     - p0

Use cases
=========

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-12.input-cluster-configuration]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI provides web pages to accept user input of cluster configuration, including network (subnet/gw/IPs)

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-12.import-cluster-configuration]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI accepts importing cluster configurations from file (with fixed and predefined format).

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-12.query-motr-configuration]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI queries Motr information from Hare, including services, pools, disks/controllers/racks, etc.
       GUI handles multiple pools and metadata pools correctly.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-12.show-cluster-configuration]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI shows the cluster configuration of networks and Motr.



Requirements
============

.. list-table::
   :header-rows: 1

   * - id
     - feature
     - description
     - priority
   * - GUI-50
     - The dashboard should be able to present performance data at the cluster level.
     -
     - p0

Use cases
=========

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-50.calc-performance-data]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - S3 keeps tracks of current performance data in last minute, GB/s of read, GB/s of write, IOPS of KV op.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-50.query-performance-data]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI queries performance data from S3 every minute.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-50.aggregate-performance-data]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI aggregates the performance data from multiple S3 nodes.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-50.store-performance-data]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI stores the performance data to local DB.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-50.show-performance-data]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI reads performance data from local DB, show performance data of last 1 minute, 10 minutes, 1 hour, 1 day.

Requirements
============

.. list-table::
   :header-rows: 1

   * - id
     - feature
     - description
     - priority
   * - GUI-70
     - Alerts should be shown for all components in the cluster.
     - Nodes, all network components, SW components, etc. The exact list should be defined in DR.
     - p0

Use cases
=========

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-70.accept-alert]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI accepts alert message from various components. Alert message includes component name, alert type,
       alert level, timestamp, message body. The alert message in encoded as YAML or JSON string.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-70.query-space-usage]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI queries space usage from Motr for all pools.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-70.alert-for-space-usage]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI generates alerts if pool space usage exceeds predefined threshold.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-70.generate-IEM-message]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - Various components, including Motr, generate IEM messages into log.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-70.parse-IEM-message]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - Logs are parsed to retieve IEM messages.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-70.show-alerts]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - GUI shows the alerts on web pages.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-70.clear-alerts]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - Users can read and ack to clear the alerts.



Requirements
============

.. list-table::
   :header-rows: 1

   * - id
     - feature
     - description
     - priority
   * - GUI-80
     - Lyve Pilot connect screen must be updated to support new LP requirements.
     - A placeholder, to be better defined with LP team.
     - p0

Use cases
=========

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.GUI-80.lyve-pilot-connect]
   * - **context**
     - ldr r2 development
   * - **trigger**
     -
   * - **requirements**
     -
   * - **interaction**
     - TODO placeholder
