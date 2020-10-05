=====
ADDB
=====

This document summarizes Analysis and Diagnostics Data-Base (ADDB) discussions in the C2 architecture team. The goal is to provide a starting point for design level discussions and resource estimation. Note that this documents is not a substitute for a list of requirements or use-cases. Intended audience includes T1 team and C2 architects.

*********
Overview
*********

ADDB contains information describing ongoing activity of C2 system. This information is stored on persistent storage and can be later queried and retrieved to analyze system behavior.

*******
Items
*******

- The overall idea of ADDB is to accumulate and store certain auxiliary information during execution of C1 file operations. This information can be later used to analyze past system behavior. Compare this with Lustre logging system. The critical differences are: 

  - ADDB has format suitable for later processing, including querying; 

  - ADDB is populated systematically rather than in ad hoc manner; 

  - ADDB is designed to be stored on persistent storage.

- ADDB consists of records and each record consists of data points. A data point is a result of an individual measurement of a certain system parameter or a description of an event of interest.

- Examples of event data points are:

  - memory allocation failed; 

  - RPC timed out; 

  - disk transfer took longer than a threshold time 
