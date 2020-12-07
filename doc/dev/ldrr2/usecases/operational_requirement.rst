=====================
Operation Requirement
=====================

Requirement 1: OP-20
=================

.. list-table::
   :header-rows: 1

   * - ID
     - Feature
     - Description
     - Priority
   * - OP-20
     - It should be possible to replace any FRU within the cluster 
     - See full list from LR R1 spec, including server replacement + private switch if it gets included 
     - P0

**Use cases**
=================
.. list-table::
   :header-rows: 1

   * - **Field**
     - **Value**
   * - **Scenario Name**
     - [u.op-20.motr-fru-replacement]
   * - **Context**
     - FRU Replacement
   * - **Trigger**
     - Component going bad
   * - **Requirements**
     - motr should handle disruption of caused with FRU replacement
   * - **Interaction**
     - #. Prepare list of FRU which affects motr operation
       #. HBA and its cable replacement : Motr needs to return error in case of storage unavailable, (currently it crashes)
       #. Network Card and its cable replacement : Motr should return error to active IOs
