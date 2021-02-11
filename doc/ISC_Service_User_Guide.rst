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
          
            ::

   rc = m0_rpc_at_add(&isc_fop->fi_args, in_args, conn);

   if (rc != 0)

        return rc;

   /* Initialise the return buffer. */

   m0_rpc_at_init(&isc_fop->fi_ret);

   rc = m0_rpc_at_recv(&isc_fop->fi_ret, conn, REPLY_SIZE4, false);

   if (rc != 0)

         return rc;

   return 0;

   }

Let’s see how this fop is sent across to execute the required computation.

::

 #include “iscservice/isc.h”

 #include “fop/fop.h”

 #include “rpc/rpclib.h”

 ::

  int isc_fop_send_sync(struct m0_isc_fop *isc_fop,

                        struct m0_rpc_session *session)

  {

      struct m0_fop fop;

      struct m0_fop reply_fop5;

      /* Holds the reply from a computation. */

      struct m0_fop_isc_rep reply;

      struct m0_buf *recv_buf;

      struct m0_buf *send_buf;

      int rc;

      M0_SET0(&fop);

      m0_fop_init(&fop, &m0_fop_isc_fopt, isc_fop, m0_fop_release);

      /*

      * A blocking call that comes out only when reply or error in

      * sending is received.

      */

     rc = m0_rpc_post_sync(&fop, session, NULL, M0_TIME_IMMEDIATELY);

     if (rc != 0)

          return error_handle();

      /* Capture the reply from computation. */

      reply_fop = m0_rpc_item_to_fop(fop.f_item.ti_reply);

      reply = *(struct m0_fop_isc_rep *)m0_fop_data(reply_fop);

      /* Handle an error received during run-time. */

      if (reply.fir_rc != 0)

           return error_handle();

       /* Obtain the result of computation. */

       rc = m0_rpc_at_rep_get(isc_fop->fi_ret, reply.fir_ret, recv_buf);

       if (rc != 0) {

            comp_error_handle(rc, recv_buf);

       }

       if (!strcmp(fetch_reply(recv_buf), “World”)) {

            comp_error_handle(rc, recv_buf);

       } else {

             /* Process the reply. */

             reply_handle(recv_buf);

             /* Finalize relevant structure. */

             m0_rpc_at_fini(&isc_fop->fi_args);

             m0_rpc_at_fini(&reply.fir_ret);

    }

    return 0

  }

We now discuss the callee side code. Let’s assume that the function is registered as “greetings” with the service.


  ::

   void motr_lib_init(void)

   {

        rc = m0_isc_comp_register(greetings, “hello-world”,

                                  string_to_fid6(“greetings”));

        if (rc != 0)

                                  error_handle(rc);

   }

   int greetings(struct m0_buf *in, struct m0_buf *out,

                 struct m0_isc_comp_private *comp_data, int *rc)

   {

       char *out_str;

       if (m0_buf_streq(in, “Hello”)) {

             /*

              * The string allocated here should not be freed by

              * computation and Mero takes care of freeing it.

              */

             out_str = m0_strdup(“World”);

             if (out_str != NULL) {

                  m0_buf_init(out, out_str, strlen(out_str));

             rc = 0;

          } else

                *rc = -ENOMEM;

       } else

               *rc = -EINVAL;

       return M0_FSO_AGAIN;

      }
      
Min/Max
=========

Hello-World example sends across a string. In real applications the input can be a composition of multiple data types. It’s necessary to serialise a composite data type into a buffer. Motr provides a mechanism to do so using xcode/xcode.[ch]. Any other serialization mechanism that’s suitable and tested can also be used eg. Google’s Protocol buffers . But we have not tested any such external library for serialization and hence in this document would use Motr’s xcode APIs.

In this example we will see how to send a composite data type to a registered function. A declaration of an object that needs to be serialised shall be tagged with one of the types identified by xcode. Every member of this structure shall also be representable using xcode type. Please refer xcode/ut/ for different examples.

Suppose we have a collection of arrays of integers, each stored as a Motr object. Our aim 
is to find out the min or max of the values stored across all arrays. The caller communicates the list of global fids(unique identification of stored object in Motr) with the registered computation for min/max. The computation then returns the min or max of locally (on relevant node) stored values. The caller then takes min or max of all the received values. The following structure can be used to communicate with registered computation.

::

 /* Arguments for getting min/max. */

 struct arr_fids {

       /* Number of arrays stored with Mero. */

       uint32_t af_arr_nr;

       /* An array holding unique identifiers of arrays. */

       struct m0_fid *af_gfids

 } M0_XCA_SEQUENCE7;

Before sending the list of fids to identify the min/max it’s necessary to serialise it into a buffer, because it’s a requirement of ISC that all the computations take input in the form of a buffer. Following snippet illustrates the same.

::

 int arr_to_buff (struct arr_fids *in_array, struct m0_buf *out_buf)

 {

      int rc;

      rc = m0_xcode_obj_enc_to_buf(XCODE_OBJ8(arr_fids),

                                   &out_buf->b_addr,

                                   &out_buf->b_nob);

      if (rc != 0)

           error_handle(rc);

      return rc;

  }

The output buffer out_buf can now be used with RPC AT mechanism introduced in previous subsection. On the receiver side a computation can deserialize the buffer to convert into original structure. The following snippet demonstrates the same.

::

 int buff_to_arr(struct m0_buf *in_buf, struct arr_fids *out_arr)

 {

      int rc;

      rc = m0_xcode_obj_dec_from_buf(XCODE_OBJ(arr_fids),

                                     &in_buf->b_addr,

                                     in_buf->b_nob);

      if (rc != 0)

            error_handle(rc);

      return rc;

  }

Preparation and handling of a fop is similar to that in Hello-World example. Once a computation is invoked, it will read each object’s locally stored values, and find min/max of the same, eventually finding out min/max across all arrays stored locally. In the next example we shall see how a computation involving an IO can be designed.
