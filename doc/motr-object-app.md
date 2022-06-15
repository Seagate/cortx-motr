# CORTX Motr application (object)

Here we will create a simple CORTX Motr application to create an object, write  
some data to this object, read data back from this object, and then delete it.  
Source code is available at: [example1.c](/motr/examples/example1.c)

checkout the source code, make, run unit test

$ git clone --recursive git@github.com:Seagate/cortx-motr.git -b main

$ cd cortx-motr

$ sudo ./scripts/m0 make

$ sudo ./scripts/m0 run-ut

Create a source code file, and start programming.

$ cd your\_project\_dir

$ touch example1.c

Include necessary header files

```
#include "motr/client.h"
```

There are also various Motr libraries header files that can be used in  
Motr applications. Please refer to source code "lib/"

Define necessary variables (global or in main() function)

```
    static struct m0_client         *m0_instance = NULL;
    static struct m0_container       motr_container;
    static struct m0_config          motr_conf;
    static struct m0_idx_dix_config  motr_dix_conf;
```

Get configuration arguments from command line

```

#include "motr/client.h"

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

The most important and necessary configuration parameters:

*   HA Addr : HA service addr in Motr service
*   Local Addr : Local addr used to connect to Motr service
*   Profile FID: The Profile FID in Motr service
*   Process FID: The Process FID in Motr service

These parameters can be queried with one of the following options:

```
- `hctl status` will show these parameters
```

If you've followed these instructions [Cluster Setup](https://github.com/Seagate/Cortx/blob/main/doc/Cluster_Setup.md) or [Quick Start Guide](/doc/Quick-Start-Guide.rst) to setup a Cortx Motr system. Or

```
- Run "motr/examples/setup_a_running_motr_system.sh" to setup a single node Motr, and parameters will be shown there.
```

The first function to use Cortx Motr is to call m0\_client\_init():

```
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

And as the final step, application needs to call m0\_client\_fini():

```
    m0_client_fini(m0_instance, true);
```

The steps to create an object: function object\_create().

*   Init the m0\_obj struct with m0\_obj\_init().
*   Init the object create operation with m0\_entity\_create(). An ID is needed  
    for this object. In this example, ID is configured from command line. Developers  
    are responsible to generate an unique ID for their object.
*   Launch the operation with m0\_op\_launch().
*   Wait for the operation to be executed: stable or failed with m0\_op\_wait().
*   Retrieve the result.
*   Finalize and free the operation with m0\_op\_fini() and m0\_op\_free().
*   Finalize the m0\_obj struct with m0\_entity\_fini().

The steps to open to an object: function object\_open().

*   Init the m0\_obj struct with m0\_obj\_init().
*   Init the object open operation with m0\_entity\_open().
*   Launch the operation with m0\_op\_launch().
*   Wait for the operation to be executed: stable or failed with m0\_op\_wait().
*   Retrieve the result.
*   Finalize and free the operation with m0\_op\_fini() and m0\_op\_free().
*   But don't call m0\_entity\_fini() on this obj because we want to keep it open.

The steps to close/finalize an object.  
Finalize the m0\_obj struct with m0\_entity\_fini().

The steps to write to an existing object: function object\_write().

*   Init the m0\_obj struct with m0\_obj\_init().
*   Open the object.
*   Allocate indexvec (struct m0\_indexvec), data buf (struct m0\_bufvec), attr buf (struct m0\_bufvec)

```
       with specified count and block size. Please note, Motr I/O must be performed with multiple blocks with some 4K-aligned block size.
```

*   Fill the indexvec with desired logical offset within the object, and correct buffer size.
*   Fill the data buf with your data.
*   Fill the attr buf with your attr data.
*   Init the write operation with m0\_obj\_op().
*   Launch the write operation with m0\_op\_launch().
*   Wait for the operation to be executed: stable or failed with m0\_op\_wait().
*   Retrieve the result.
*   Free the indexvec and data buf and attr buf.
*   Finalize and free the operation with m0\_op\_fini() and m0\_op\_free().
*   Close/Finalize the object.

The steps to read from an exiting object: function object\_read().

*   Init the m0\_obj struct with m0\_obj\_init().
*   Open the object.
*   Allocate indexvec (struct m0\_indexvec), data buf (struct m0\_bufvec), attr buf (struct m0\_bufvec)

```
       with specified count and block size. Please note, Motr I/O must be performed with multiple blocks with some 4K-aligned block size.
```

*   Fill the indexvec with desired logical offset within the object, and correct buffer size.
*   Init the read operation with m0\_obj\_op().
*   Launch the read operation with m0\_op\_launch().
*   Wait for the operation to be executed: stable or failed with m0\_op\_wait().
*   Retrieve the result.
*   Retrieve the data from data buf and attr buf.
*   Free the indexvec and data buf and attr buf.
*   Finalize and free the operation with m0\_op\_fini() and m0\_op\_free().
*   Close/Finalize the object.

The steps to delete an existing object: function object\_delete().

Init the m0\_obj struct with m0\_obj\_init().

Open the object.

Init the delete operation with m0\_obj\_op().

Launch the delete operation with m0\_op\_launch().

Wait for the operation to be executed: stable or failed with m0\_op\_wait().

Retrieve the result.

Finalize and free the operation with m0\_op\_fini() and m0\_op\_free().

Close/Finalize the object.

## Troubleshoot

If run `example1` and encounter error "obj_id invalid. Please refer to M0_ID_APP in motr/client.c", it means the argument obj_id is too small. Try a number larger than 1048576.  

# Tested by

*   Mar 17, 2022: Bo Wei (bo.b.wei@seagate.com) tested using CentOS 7.9.
*   Sep 28, 2021: Liana Valdes Rodriguez (liana.valdes@seagate.com / lvald108@fiu.edu) tested using CentOS Linux release 7.8.2003 x86_64
