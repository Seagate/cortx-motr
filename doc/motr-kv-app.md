# Cortx Motr application (Index/Key-value)

Here we will create a simple Cortx Motr application to create an index, put  
some key/value pairs to this index, read them back, and then delete it.  
Source code is available at: [example2.c](/motr/examples/example2.c)

Motr index FID is a special type of FID. It must be the m0\_dix\_fid\_type.  
So, the index FID must be initialized with:

```
m0_fid_tassume((struct m0_fid*)&index_id, &m0_dix_fid_type);
```

The steps to create an index: function index\_create().

Init the m0\_idx struct with m0\_idx\_init().

Init the index create operation with m0\_entity\_create().

```
An ID is needed for this index. In this example, ID is configured from command line.
Developers are responsible to generate an unique ID for their indices.
```

Launch the operation with m0\_op\_launch().

Wait for the operation to be executed: stable or failed with m0\_op\_wait().

Retrieve the result.

Finalize and free the operation with m0\_op\_fini() and m0\_op\_free().

Finalize the m0\_idx struct with m0\_entity\_fini().

Please refer to function `index_create` in the example.

The steps to put key/value pairs to an existing index: function index\_put().  
`struct m0_bufvec` is used to store in-memory keys and values.

*   Allocate keys with proper number of vector and buffer length.
*   Allocate values with proper number of vector and buffer length.
*   Allocate an array to hold return value for every key/val pair.
*   Fill the key/value pairs into keys and vals.
*   Init the m0\_idx struct with m0\_idx\_init().
*   Init the put operation with m0\_idx\_op(&idx, M0\_IC\_PUT, &keys, &vals, &rcs, ...).
*   Launch the put operation with m0\_op\_launch().
*   Wait for the operation to be executed: stable or failed with m0\_op\_wait().
*   Retrieve the result.
*   Free the keys and vals.
*   Finalize and free the operation with m0\_op\_fini() and m0\_op\_free().

The steps to get key/val pairs from an exiting index: function index\_get().

*   Allocate keys with proper number of vector and buffer length.
*   Fill the keys with proper value;
*   Allocate values with proper number of vector and empty buffer.
*   Init the m0\_idx struct with m0\_idx\_init().
*   Init the put operation with m0\_idx\_op(&idx, M0\_IC\_GET, &keys, &vals, &rcs, ...).
*   Launch the put operation with m0\_op\_launch().
*   Wait for the operation to be executed: stable or failed with m0\_op\_wait().
*   Retrieve the result, and use the returned key/val pairs.
*   Free the keys and vals.
*   Finalize and free the operation with m0\_op\_fini() and m0\_op\_free().

The steps to delete an existing index: function index\_delete().

*   Init the m0\_idx struct with m0\_idx\_init().
*   Open the entity with m0\_entity\_open().
*   Init the delete operation with m0\_entity\_delete().
*   Launch the delete operation with m0\_op\_launch().
*   Wait for the operation to be executed: stable or failed with m0\_op\_wait().
*   Retrieve the result.
*   Finalize and free the operation with m0\_op\_fini() and m0\_op\_free().
