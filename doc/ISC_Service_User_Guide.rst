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
