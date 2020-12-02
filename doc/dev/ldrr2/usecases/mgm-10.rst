===============
MGM-10 usecases
===============

:author: Nikita Danilov <nikita.danilov@seagate.com>

.. list-table::
   :header-rows: 1

   * - id
     - feature
     - description
     - priority
   * - mgm-10
     - Any configuration-related operation with the cluster should be possible
       via REST API
     - install & trouble-shooting may involve lower-level or not 15gmt. API access
     - p0

Use cases
=========

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.mgm-10.shutdown-cluster]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - **TBD** need a sequence diagram of cluster shutdown.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.mgm-10.shutdown-part]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - **TBD** need a sequence diagram of node of storageset shutdown. Node
       shutdown can be needed for upgrade of catastrophic recovery.


.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.mgm-10.startup-cluster]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - **TBD** need a sequence diagram of cluster startup.

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.mgm-10.startup-part]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - **TBD** need a sequence diagram of node of storageset startup.


.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.mgm-10.upgrade]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - 

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.mgm-10.ip-change]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - 

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.mgm-10.storage-set-addition-prep]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - storage set addition is not needed in p0, but p0 must be prepared to
       support node addition in p1 without reformat.
   * - **interaction**
     - 

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.mgm-10.replace-fru]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - 

