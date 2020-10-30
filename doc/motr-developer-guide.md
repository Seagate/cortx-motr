# Cortx Motr Developer Guide

This document describes how to develop a simple application with Motr APIs.
This document also list links to more useful Motr applications with richer
features.

The first document developers should read is: [Motr Client API ](/motr/client.h).
It explains basic notations, terminologies, and data structures in Cortx Motr.
Developers are also assumed having a running Cortx Motr system: please refer to
[Cluster Setup](https://github.com/Seagate/Cortx/blob/main/doc/Cluster_Setup.md)
and [Quick Start Guide](/doc/Quick-Start-Guide.rst)

Cortx Motr provides object based operations and index (a.k.a key/value) based operations.

## A simple Cortx Motr application (object)

Here we will create a simple Cortx Motr application to crate an object, write
some data to this object, read data back from this object, and then delete it.
Source code is available at: [example1.c](/motr/examples/example1.c)

  * checkout the source code, make, run unit test
  
	$ git clone --recursive git@github.com:Seagate/cortx-motr.git -b main
	
	$ cd cortx-motr
	
	$ sudo ./scripts/m0 make
	
	$ sudo ./scripts/m0 run-ut
	

  * Create a source code file, and start programming.
  
	$ cd your_project_dir

	$ touch example1.c

  * Include necessary header files
```C	
	#include "motr/client.h"
```
	There are also various Motr libraries header files that can be used in
Motr applications. Please refer to source code "lib/"

  * Define necessary variables (global or in main() function)

```C
	static struct m0_client         *m0_instance = NULL;
	static struct m0_container       motr_container;
	static struct m0_config          motr_conf;
	static struct m0_idx_dix_config  motr_dix_conf;
```

  * Get configuration arguments from command line
```C

	#include "motr/client.h"
	#include "motr/idx.h"

	static struct m0_client         *m0_instance = NULL;
	static struct m0_container       motr_container;
	static struct m0_config          motr_conf;
	static struct m0_idx_dix_config  motr_dix_conf;

	int main(int argc, char *argv[])
	{
	        int rc;

	        motr_dix_conf.kc_create_meta = false;

	        motr_conf.mc_is_oostore            = true;
	        motr_conf.mc_is_read_verify        = false;
	        motr_conf.mc_ha_addr               = argv[1];
	        motr_conf.mc_local_addr            = argv[2];
	        motr_conf.mc_profile               = argv[3];
	        motr_conf.mc_process_fid           = argv[4];
	        motr_conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	        motr_conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	        motr_conf.mc_idx_service_id        = M0_IDX_DIX;
	        motr_conf.mc_idx_service_conf      = (void *)&motr_dix_conf;

	        return 0;
	}
```

  * The first function to use Cortx Motr is to call m0_client_init():
```C
        rc = m0_client_init(&m0_instance, &motr_conf, true);
        if (rc != 0) {
                printf("error in m0_client_init: %d\n", rc);
                exit(rc);
        }

        m0_container_init(&motr_container, NULL, &M0_UBER_REALM, m0_instance);
        rc = motr_container.co_realm.re_entity.en_sm.sm_rc;
        if (rc != 0) {
                printf("error in m0_container_init: %d\n", rc);
                exit(rc);
        }
```
  * And as the final step, application needs to call m0_client_fini():
```C
        m0_client_fini(m0_instance, true);
```

  * The steps to create an object: function object_create().
	- Init the m0_obj struct with m0_obj_init().
	- Init the object create operation with m0_entity_create().
	- Launch the operation with m0_op_launch().
	- Wait for the operation to be executed: stable or failed with m0_op_wait().
	- Retrieve the result.
	- Finalize and free the operation with m0_op_fini() and m0_op_free().
	- Finalize the m0_obj struct with m0_entity_fini().

	Please refer to function `object_create` in the example.

  * The steps to open to an object: function object_open().
	- Init the m0_obj struct with m0_obj_init().
	- Init the object open operation with m0_entity_open().
	- Launch the operation with m0_op_launch().
	- Wait for the operation to be executed: stable or failed with m0_op_wait().
	- Retrieve the result.
	- Finalize and free the operation with m0_op_fini() and m0_op_free().
	- But don't call m0_entity_fini() on this obj because we want to keep it open.

  * The steps to close/finalize an object.
	Finalize the m0_obj struct with m0_entity_fini().

  * The steps to write to an existing object: function object_write().
	- Init the m0_obj struct with m0_obj_init().
	- Open the object.
	- Allocate indexvec (struct m0_indexvec), data buf (struct m0_bufvec), attr buf (struct m0_bufvec)
                 with specified count and block size. Please note, Motr I/O must be performed with multiple blocks with some 4K-aligned block size.
	- Fill the indexvec with desired logical offset within the object, and correct buffer size.
	- Fill the data buf with your data.
	- Fill the attr buf with your attr data.
	- Init the write operation with m0_obj_op().
	- Launch the write operation with m0_op_launch().
	- Wait for the operation to be executed: stable or failed with m0_op_wait().
	- Retrieve the result.
	- Free the indexvec and data buf and attr buf.
	- Finalize and free the operation with m0_op_fini() and m0_op_free().
	- Close/Finalize the object.

  * The steps to read from an exiting object: function object_read().
	- Init the m0_obj struct with m0_obj_init().
	- Open the object.
	- Allocate indexvec (struct m0_indexvec), data buf (struct m0_bufvec), attr buf (struct m0_bufvec)
                 with specified count and block size. Please note, Motr I/O must be performed with multiple blocks with some 4K-aligned block size.
	- Fill the indexvec with desired logical offset within the object, and correct buffer size.
	- Init the read operation with m0_obj_op().
	- Launch the read operation with m0_op_launch().
	- Wait for the operation to be executed: stable or failed with m0_op_wait().
	- Retrieve the result.
	- Retrieve the data from data buf and attr buf.
	- Free the indexvec and data buf and attr buf.
	- Finalize and free the operation with m0_op_fini() and m0_op_free().
	- Close/Finalize the object.

  * The steps to delete an existing object: function object_delete().
	- Init the m0_obj struct with m0_obj_init().
	- Open the object.
	- Init the delete operation with m0_obj_op().
	- Launch the delete operation with m0_op_launch().
	- Wait for the operation to be executed: stable or failed with m0_op_wait().
	- Retrieve the result.
	- Finalize and free the operation with m0_op_fini() and m0_op_free().
	- Close/Finalize the object.

   * How to compile & build this example:
	- One way is to use the Motr building framework. This example is compiled and built
	within the Motr building framework. If you have a new application, add it to the
	top level Makefile.am and add a Makefile.sub in your directory.
	
	- You can also treat this example as a standalone application, and build it out of
	More building framework. Please refer to the comments in the source code.
	If Motr-devel source code or RPM is already installed, you may find the header
	files in "/usr/include/motr " dir and binary libraries in "/lib".

   * How to run this example:
	- The first way is to run application against an running Cortx Motr system. Please
	refer to [Cluster Setup](https://github.com/Seagate/Cortx/blob/main/doc/Cluster_Setup.md)
	and [Quick Start Guide](/doc/Quick-Start-Guide.rst).
	
	- The second way is to run the "motr/examples/setup_a_running_motr_system.sh", and then
	run this example application from another teminal.

## A simple Cortx Motr application (index)
   * TODO

## More examples, utilities, and applications
  * More Cortx Motr examples (object) and utilities are:
	- m0cp    : motr/st/utils/copy.c
	- m0cat   : motr/st/utils/cat.c
	- m0touch : motr/st/utils/touch.c
	- m0unlink: motr/st/utils/unlink.c
	- m0client: motr/st/utils/client.c
  * More Cortx Motr examples (index) and utilities are:
	- m0kv    : motr/m0kv/
  * A Cortx Motr performance utility:
	- m0crate: motr/m0crate/
  * A Cortx Motr GO bindings:
	- bindings/go/mio/
  * A Cortx Motr HSM utility and application:
	- hsm/
  * A Cortx Motr Python wrapper:
	- spiel/
