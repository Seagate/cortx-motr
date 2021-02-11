================
ISC User Guide
================

******************
Preparing Library
******************

APIs from external library can not be linked directly with a Motr instance. A library is supposed to have a function named motr_lib_init(). This function will then link the relevant APIs with Motr. Every function to be linked with motr shall confine to the following signature:

::

 int comp(struct m0_buf *args, struct m0_buf *out,

          struct m0_isc_comp_private *comp_data, int *rc)

All relevant library APIs shall be prepared with a wrapper confining to this signature. Let libarray be the library we intend to link with Motr, with following APIs: arr_max(), arr_min(), arr_histo().

Registering APIs
=================

motr_lib_init() links all the APIs. Here is an example code (please see iscservice/isc.h for more details):

  ::

   void motr_lib_init(void)

   {

        rc = m0_isc_comp_register(arr_max, “max”,

                                  string_to_fid1(“arr_max”));

        if (rc != 0)

             error_handle(rc);

        rc = m0_isc_comp_register(arr_min, “min”,

                                  string_to_fid(“arr_min”));

        if (rc != 0)

            error_handle(rc);

        rc = m0_isc_comp_register(arr_histo, “arr_histo”,

                                  string_to_fid(“arr_histo”));

        if (rc != 0)

            error_handle(rc);

   }
   
   
*******************
Registering Library
*******************

Let libpath be the path the library is located at. The program needs to load the same at each of the Motr node. This needs to be done using:

  ::

   int m0_spiel_process_lib_load2(struct m0_spiel *spiel,

                                  struct m0_fid *proc_fid,

                                  char *libpath)

This will ensure that motr_lib_init() is called to register the relevant APIs.

***************
Invoking API
***************

Motr has its own RPC mechanism to invoke a remote operation. In order to conduct a computation on data stored with Motr it’s necessary to share the computation’s fid (a unique identifier associated with it during its registration) and relevant input arguments. Motr uses fop/fom framework to execute an RPC. A fop represents a request to invoke a remote operation and it shall be populated with relevant parameters by a client. A request is executed by a server using a fom. The fop for ISC service is self-explanatory. Examples in next subsection shall make it more clear.

  ::

   /** A fop for the ISC service */

   struct m0_fop_isc {

           /** An identifier of the computation registered with the

               Service.

           */

            struct m0_fid fi_comp_id;

            /**

             * An array holding the relevant arguments for the

             * computation.

             * This might involve gfid, cob fid, and few other parameters

             * relevant to the required computation.

            */

              struct m0_rpc_at_buf fi_args;

           /**

            * An rpc AT buffer requesting the output of computation.

            */

            struct m0_rpc_at_buf fi_ret;

            /** A cookie for fast searching of a computation. */

            struct m0_cookie fi_comp_cookie;

       } M0_XCA_RECORD M0_XCA_DOMAIN(rpc)3;

************
Examples
************

**Hello-World**

Consider a simple API that on reception of string “Hello” responds with “World” along with return code 0. For any other input it does not respond with any string, but returns an error code of -EINVAL. Client needs to send m0_isc_fop populated with “Hello”. First we will see how client or caller needs to initialise certain structures and send them across. Subsequently we will see what needs to be done at the server side. Following code snippet illustrates how we can initialize m0_isc_fop .

   ::

    /**

     * prerequisite: in_string is null terminated.

     * isc_fop : A fop to be populated.

     * in_args : Input to be shared with ISC service.

     * in_string: Input string.

     * conn : An rpc-connection to ISC service. Should be established

     * beforehand.

     */

    int isc_fop_init(struct m0_fop_isc *isc_fop, struct m0_buf *in_args,

                     char *in_string, struct m0_rpc_conn *conn)

    {

          int rc;

          /* A string is mapped to a mero buffer. */

          m0_buf_init(in_args, in_string, strlen(in_string));

          /* Initialise RPC adaptive transmission data structure. */

          m0_rpc_at_init(&isc_fop->fi_args);

          /* Add mero buffer to m0_rpc_at */
