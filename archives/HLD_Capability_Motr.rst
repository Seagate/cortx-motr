==========================
HLD of Capability in Motr
==========================

In a distributed system, authentication is essential. Authentication between different components, locally or remotely, are used as permission checking, while the system handles operations.

Authentication, based on capability[0], will be used in Motr. In this document, we will present a stub for capability-based authentication for Motr.

***************
Definitions
***************

- Object. This may be global object (file), or component object (file), or locks, layouts, etc.

- Capability. This is a permission assigned to an object, which allows some specific operations to be carried on this object from some specific user.

***************
Requirements
***************

- [r.capa.issuer] capability is issued by some server (meta-data server, or data server). The server is considered to be trusted entity in the system.

- [r.capa.owner] capability is tied to some object and user.

- [r.capa.expire] capability will expire at sometime.

- [r.capa.renew] capability can be renewed from client.

- [r.capa.authenticate] capability can be authenticated by server, maybe different from the issuer of this capability. This is called remote authentication.

- [r.capa.communicate] capability along with object can be transferred from one node to another.

- [r.capa.encypted] capability transferred to client is encrypted. It is opaque to client. Client cannot decrypt to reveal the object owner and user.

- [r.capa.type] various capability flavors are supported: null, omg (owner-group-mode), compound.

******************
Design Highlights
******************

There is a capability master on a node. Capabilities for objects (or component objects) are issued by this master, and can be authenticated later.









