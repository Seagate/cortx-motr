=================
3rd Party Software
=================

Requirement 1: SW-10
=================

.. list-table::
   :header-rows: 1

   * - ID
     - Feature
     - Description
     - Priority
   * - SW-10
     - Lyve Rack must be running on the supported CentOS version 
     - 8.x
     - P0

**Use cases**
=================
.. list-table::
   :header-rows: 1

   * - **Field**
     - **Value**
   * - **Scenario Name**
     - [u.sw-10.motr-build]
   * - **Context**
     - OS Major Update
   * - **Trigger**
     - Cent OS update from 7.8 to 8.x
   * - **Requirements**
     - motr should build and compile, unit and system test should pass
   * - **Interaction**
     - #. Build list of dependency package  
       #. Check if any changes needed in motr if packages are changed
       #. If needed change motr and test 

Requirement 2: SW-20
=================

.. list-table::
   :header-rows: 1

   * - ID
     - Feature
     - Description
     - Priority
   * - SW-20
     - All 3rd party applications must be running recent, maintained versions  
     - 
     - P0

**Use cases**
=================
.. list-table::
   :header-rows: 1

   * - **Field**
     - **Value**
   * - **Scenario Name**
     - [u.sw-20.motr-3rd-party-version-check]
   * - **Context**
     - Validation of CORTX 3rd party sofware 
   * - **Trigger**
     - CORTX Release 
   * - **Requirements**
     - motr should use latest version of all 3rd party code
   * - **Interaction**
     - #. Prepare list of 3rd party software used and check if they are latest e.g.
       #. Check libfabric version 
       #. Check code version used for erasure coding

Requirement 3: SW-30
=================

.. list-table::
   :header-rows: 1

   * - ID
     - Feature
     - Description
     - Priority
   * - SW-30
     - Any failure of any 3rd party SW component must be detected by CORTX and exposed via Alerts and Telemetry system 
     - 
     - P0

**Use cases**
=================
.. list-table::
   :header-rows: 1

   * - **Field**
     - **Value**
   * - **Scenario Name**
     - [u.sw-20.motr-3rd-party-failure-notification]
   * - **Context**
     - Normal system operation
   * - **Trigger**
     - Error returned by operation involving 3rd party
   * - **Requirements**
     - Error notification to clearly distinguish between CORTX and 3rd party failure
   * - **Interaction**
     - #. Prepare list of failure due to 3rd party SW, encountered in normal operation
       #. Check if IEM is being raised for each such failure
       #. If missing add IEM
       #. Note: Notify SSPL and CSM for the new IEM being added

Requirement 4: SW-40
=================

.. list-table::
   :header-rows: 1

   * - ID
     - Feature
     - Description
     - Priority
   * - SW-40
     - CORTX should have no kernel dependencies  
     - LNET -> libfabric. Is there any other kernel dependency? 
     - P0

**Use cases**
=================
.. list-table::
   :header-rows: 1

   * - **Field**
     - **Value**
   * - **Scenario Name**
     - [u.sw-20.motr-no-kernel-dependency]
   * - **Context**
     - Normal system operation
   * - **Trigger**
     - Moving towards container based implementation for motr
   * - **Requirements**
     - All CORTX component should be user space
   * - **Interaction**
     - #. Integrate libfabric in motr
       #. Remove the need for m0d to get UUID (UUID is received from Kernel)

Requirement 5-6: SW-50 & SW-60
=================

.. list-table::
   :header-rows: 1

   * - ID
     - Feature
     - Description
     - Priority
   * - SW-50 & SW-60
     - CORTX should use no 3rd party SW unless it is appropriately permissively licensed. Legal recommendations must be followed 
     - Legal recommendations must be followed (Galois -> ISA, Btree implementation, balloc) 
     - P0

**Use cases**
=================
.. list-table::
   :header-rows: 1

   * - **Field**
     - **Value**
   * - **Scenario Name**
     - [u.sw-20.motr-legal-recommendation]
   * - **Context**
     - Normal system operation
   * - **Trigger**
     - Maintain license compatibility
   * - **Requirements**
     - All 3rd party SW used should be approved by legal
   * - **Interaction**
     - #. Prepare list of 3rd party software used
       #. Get its usage approval from Legal team
       #. If not approved, rewrite or replace with approved software.
       #. e.g Galois, btree and balloc