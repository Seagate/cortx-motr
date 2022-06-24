/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */


#pragma once

#ifndef __MOTR_DLD_TEMPLATE_H__
#define __MOTR_DLD_TEMPLATE_H__

/**
   @page DLD-fspec DLD Functional Specification Template
   <i>Mandatory. This page describes the external interfaces of the
   component. The section has mandatory sub-divisions created using the Doxygen
   @@section command.  It is required that there be Table of Contents at the
   top of the page that illustrates the sectioning of the page.</i>

   - @ref DLD-fspec-ds
   - @ref DLD-fspec-sub
   - @ref DLD-fspec-cli
   - @ref DLD-fspec-usecases
   - @ref DLDDFS "Detailed Functional Specification" <!-- Note link -->

   XXX
   DTM0 has two interfaces: dtx0 and domain. DTM0 domain is used to initiallize
   global dtm0-related structures. DTX0 is used to execute transactions.


   The Functional Specification section of the DLD shall be placed in a
   separate Doxygen page, identified as a @@subpage of the main specification
   document through the table of contents in the main document page.  The
   purpose of this separation is to co-locate the Functional Specification in
   the same source file as the Detailed Functional Specification.

   A table of contents should be created for the major sections in this page,
   as illustrated above.  It should also contain references to other
   @b external Detailed Functional Specification sections, which even
   though may be present in the same source file, would not be visibly linked
   in the Doxygen output.

   @section DLD-fspec-ds Data Structures
   <i>Mandatory for programmatic interfaces.  Components with programming
   interfaces should provide an enumeration and @b brief description of the
   major externally visible data structures defined by this component.  No
   details of the data structure are required here, just the salient
   points.</i>

   For example:
<table border="0">
<tr><td>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;</td><td>
   The @c dld_sample_ds1 structure tracks the density of the
   electro-magnetic field with the following:
@code
struct dld_sample_ds1 {
  ...
  int dsd_flux_density;
  ...
};
@endcode
   The value of this field is inversely proportional to the square of the
   number of lines of comments in the DLD.
</td></tr></table>
   Note the indentation above, accomplished by means of an HTML table
   is purely for visual effect in the Doxygen output of the style guide.
   A real DLD should not use such constructs.

   Simple lists can also suffice:
   - dld_sample_ds1
   - dld_bad_example

   The section could also describe what use it makes of data structures
   described elsewhere.

   Note that data structures are defined in the
   @ref DLDDFS "Detailed Functional Specification"
   so <b>do not duplicate the definitions</b>!
   Do not describe internal data structures here either - they can be described
   in the @ref DLD-lspec "Logical Specification" if necessary.

   @section DLD-fspec-sub Subroutines
   <i>Mandatory for programmatic interfaces.  Components with programming
   interfaces should provide an enumeration and brief description of the
   externally visible programming interfaces.</i>

   Externally visible interfaces should be enumerated and categorized by
   function.  <b>Do not provide details.</b> They will be fully documented in
   the @ref DLDDFS "Detailed Functional Specification".
   Do not describe internal interfaces - they can be described in the
   @ref DLD-lspec "Logical Specification" if necessary.

   @subsection DLD-fspec-sub-cons Constructors and Destructors

   @subsection DLD-fspec-sub-acc Accessors and Invariants

   @subsection DLD-fspec-sub-opi Operational Interfaces
   - dld_sample_sub1()

   @section DLD-fspec-cli Command Usage
   <i>Mandatory for command line programs.  Components that provide programs
   would provide a specification of the command line invocation arguments.  In
   addition, the format of any any structured file consumed or produced by the
   interface must be described in this section.</i>

   @section DLD-fspec-usecases Recipes
   <i>This section could briefly explain what sequence of interface calls or
   what program invocation flags are required to solve specific usage
   scenarios.  It would be very nice if these examples can be linked
   back to the HLD for the component.</i>

   Note the following references to the Detailed Functional Specification
   sections at the end of these Functional Specifications, created using the
   Doxygen @@see command:

   @see @ref DLDDFS "Sample Detailed Functional Specification"
 */

/**
   @defgroup DLDDFS Motr Sample Module
   @brief Detailed functional specification template.

   This page is part of the DLD style template.  Detailed functional
   specifications go into a module described by the Doxygen @@defgroup command.
   Note that you cannot use a hyphen (-) in the tag of a @@defgroup.

   Module documentation may spread across multiple source files.  Make sure
   that the @@addtogroup Doxygen command is used in the other files to merge
   their documentation into the main group.  When doing so, it is important to
   ensure that the material flows logically when read through Doxygen.

   You are not constrained to have only one module in the design.  If multiple
   modules are present you may use multiple @@defgroup commands to create
   individual documentation pages for each such module, though it is good idea
   to use separate header files for the additional modules.  In particular, it
   is a good idea to separate the internal detailed documentation from the
   external documentation in this header file.  Please make sure that the DLD
   and the modules cross-reference each other, as shown below.

   @see The @ref DLD "Motr Sample DLD" its
   @ref DLD-fspec "Functional Specification"
   and its @ref DLD-lspec-thread

   @{
 */


/* LOG */

/**
initializes log record iterator for a
     sdev participant. It iterates over all records that were in the log during
     last local process restart or during last remote process restart for the
     process that handles that sdev.
*/
m0_dtm0_log_iter_init();

m0_dtm0_log_iter_next() - gives next log record for the sdev participant.
m0_dtm0_log_iter_fini() - finalises the iterator. It MUST be done for every
     call of m0_dtm0_log_iter_init().
m0_dtm0_log_participant_restarted() - notifies the log that the participant
     has restarted. All iterators for the participant MUST be finalized at the
     time of the call. Any record that doesn't have P from the participant at
     the time of the call will be returned during the next iteration for the
     participant.

   @section pmach interface

m0_dtm0_log_p_get_local() - returns the next P message that becomes local.
     Returns M0_FID0 during m0_dtm0_log_stop() call. After M0_FID0 is returned
     new calls to the log MUST NOT be made.
m0_dtm0_log_p_put() - records that P message was received for the sdev
     participant.

   @section pruner interface

m0_dtm0_log_p_get_none_left() - returns dtx0 id for the dtx which has all
     participants (except originator) reported P for the dtx0. Also returns all
     dtx0 which were cancelled.
   - m0_dtm0_log_prune() - remove the REDO message about dtx0 from the log

   dtx0 interface, client & server

bool m0_dtm0_log_redo_add_intent() - function to check if the transaction
     has to be applied or not, and reserves a slot in the log for that
     record (in case if it has to be applied).

m0_dtm0_log_redo_add() - adds a REDO message and, optionally, P message, to
     the log.

   @section dtx0 interface, client only

m0_dtm0_log_redo_p_wait() - returns the number of P messages for the dtx
     and waits until either the number increases or m0_dtm0_log_redo_cancel() is
     called.
m0_dtm0_log_redo_cancel() - notification that the client doesn't need the
     dtx anymore. Before the function returns the op
m0_dtm0_log_redo_end() - notifies dtx0 that the operation dtx0 is a part of
     is complete. This function MUST be called for every m0_dtm0_log_redo_add().


/** @} */ /* DLDDFS end group */

#endif /*  __MOTR_DLD_TEMPLATE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
