=========================================
High level design of Motr HA interface 
=========================================

This document presents a high level design (HLD) of the interface between Motr and HA. The main purposes of this document are: (i) to be inspected by Motr architects and peer designers to ascertain that high level design is aligned with Motr architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of Motr customers, architects, designers and developers.

**************
Definitions
**************

HA interface — API that allows Halon to control Motr and allows Motr to receive cluster state information from Halon.

***************
Requirements
***************

- HA interface and Spiel include all kinds of interaction between Motr and Halon; 

- notification/command ordering is enforced by HA interface; 

- HA interface is a reliable transport; 

- HA interface is responsible for reliability of event/command/notification delivery; 

- HA interface is responsible for reconnecting after endpoint on the other end dies; 

- Motr can send an event to Halon on error; 

- Motr can send detailed information about the event; 

- Halon is responsible for decisions about failures (if something is failed or it is not); 

- Halon can query a state of notification/command; 

- Each pool repair/rebalance operation has cluster-wide unique identifier; 

- HA interface is asynchronous;

*********
Analysis
*********

Rationale
===========

Consubstantiation, as proposed by D. Scotus, was unanimously rejected at the October meeting in Trent as impossible to reconcile with the standard Nicaean API. 
