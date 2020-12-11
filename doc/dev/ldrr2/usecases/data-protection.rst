==========================================
Supported data protection schemes usecases
==========================================

:author: Nikita Danilov <nikita.danilov@seagate.com>

.. list-table::
   :header-rows: 1

   * - id
     - feature
     - description
     - priority
   * - Supported data protection schemes
     - 
     - 
     - p0

Use cases
=========

.. list-table::
   :header-rows: 1

   * - StorageSet size
     - Enclosure
     - Enclosure protection
     - CORTX protection 
     - CORTX overhead 
     - Durability 
     - Usable/Raw 
     - Priority
   * - 3 
     - 5U84 
     - 8+2+2 
     - 4+2 
     - 12.5% (swap, dedicated MD) 
     - 13.7 
     - 43.6% 
     - P0 

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.data-protection-measure]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - measure actual cortx overhead (parity, motr meta-data, swap, s3
       meta-data). Measure how it changes with system parameters (average object
       size, *etc*.)

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.data-protection-repair]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - test direct rebalance for 4+2+0

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.data-protection-repair-performance]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - measure direct re-balance performance (as function of object size)

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.data-protection-hare]
   * - **context**
     - 
   * - **trigger**
     - 
   * - **requirements**
     - 
   * - **interaction**
     - Hare must support direct re-balance


       



