=============================
data volume failures usecases
=============================

:author: Huang Hua <hua.huang@seagate.com>

+--------------+-----------------------------------------------------------------------+
| use cases    |                        Metadata Volume dies                           |
|              +----------------------------------+------------------------------------+
|              |              local               |               remote               |
|              +----------------+-----------------+------------------+-----------------+
|              | before         | after           |      before      |     after       |
+--------------+----------------+-----------------+------------------+-----------------+
|   GET        | md.local.before| md.local.after  |                  |                 |
+--------------+----------------+-----------------+------------------+-----------------+
|   PUT        |                |                 |                  |                 |
+--------------+----------------+-----------------+------------------+-----------------+
| MKBUCKET     |                |                 |                  |                 |
+--------------+----------------+-----------------+------------------+-----------------+


Use cases
=========

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.ad-50.md.local.before.GET]
   * - **context**
     - cortx system
   * - **trigger**
     - Get internal/external metadata
   * - **requirements**
     - Object GET involves to get metadata first from index (key/val store), and then get object data.
       In normal case without metadata volume failure, Motr client should try to get the key/val from the
       node with least network hop (from local node, from node in the same storageset, from the node on
       the same rack, ...) This is true for local pool within a storageset, or global pool.
   * - **interaction**
     -


`<get-metadata-volume-dies-normal.uml>`_


.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.ad-50.md.local.after.GET]
   * - **context**
     - cortx system when some metadata volume dies
   * - **trigger**
     - Get internal/external metadata
   * - **requirements**
     - In normal case without metadata volume failure, Motr client should try to get the key/val from the
       node with least network hop. If the metadata volume of some node dies, Motr client should skip
       that node, and get key/val from the next node which has its replica.
   * - **interaction**
     -

`<get-metadata-volume-dies.uml>`_
