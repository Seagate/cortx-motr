=============================
data volume failures usecases
=============================

:author: Huang Hua <hua.huang@seagate.com>

+--------------+-----------------------------------------------------------------------+
| use cases    | Bad Block on Data Volume                                              |
|              +----------------------------------+------------------------------------+
|              |              local               |               remote               |
|              +----------------+-----------------+------------------+-----------------+
|              | before         | after           |      before      |     after       |
+--------------+----------------+-----------------+------------------+-----------------+
|   GET        | bd.l.b.GET     | bd.l.a.GET      |      <---        |     <---        |
+--------------+----------------+-----------------+------------------+-----------------+
|   PUT        |     N/A        |     N/A         |     N/A          |     N/A         |
+--------------+----------------+-----------------+------------------+-----------------+
| MKBUCKET     |     N/A        |     N/A         |     N/A          |     N/A         |
+--------------+----------------+-----------------+------------------+-----------------+


Object PUT and index kv_put does not detect bad block on data volume during write.

Object GET from local node and remote note are the same.

Use cases
=========

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.ad-50.bd.l.b.GET]
   * - **context**
     - cortx system
   * - **trigger**
     - 
   * - **requirements**
     - When a data block is written, checksum is generated for data units as a metadata.
       The checksum is generated on client, then passed to server, and verified by server
       to detect any network error. Then, data is written onto disk, and this checksum is
       stored as a metadata.
       When reading from object, the server reads the data units and its checksum, and then
       verify the checksum by calculating the checksum and compare it with the stored one.
       And then transfer data to client, along with the checksum. The checksum is verified
       again on client to detect any network failure.
   * - **interaction**
     -

.. list-table::
   :header-rows: 1

   * - **field**
     - **value**
   * - **scenario name**
     - [u.ad-50.bd.l.a.GET]
   * - **context**
     - cortx system
   * - **trigger**
     - 
   * - **requirements**
     - When reading from object, the server reads the data units and its checksum, and then
       verify the checksum by calculating the checksum and compare it with the stored one.
       When bad block happens, either the I/O itself returns error, or the checksum verification
       fails.
       When this happens, Motr server replies -EIO error for this request.
       Motr client handles this error by reading from other data unit or parity unit in this parity group,
       and re-construct data.
       Reading from other data unit or parity unit follows the same read process.
       If number of surviving data units and parity units drops under the number of 4, the reading request fails.
   * - **interaction**
     -

