============================
Understanding Layout Schema
============================

This document explains the Motr layout schema.

***************
Overview
***************
The layouts of the files are relevant for a disk-based file system. This is one of the most important services (or functionality) provided by the disk-based file-systems. They manage the underlying storage and optimally utilize the storage using user-opaque layout of the file data blocks as well as meta-data. The user accesses the files using the system calls and does not have to bother about the disk block management. The file layout refers to the arrangement of the disk blocks that belong to a file. The layouts are designed by taking into consideration the optimum storage utilization and efficient access to the data.

=======================
Traditional File Layout  
=======================

Most of the disk-based file-systems access any file using meta-data for that file, famously known on UNIX as inode. Along with other attributes of the file, the inode stores the file layout. In a classic file-system, this file layout is also called as block map. A typical EXT2 block map (a file layout) comprises of the following:

- 10 direct block

- 1 indirect block

- 1 doubly indirect block

- 1 triple indirect block

The traditional block map in an inode would contain logical disk block numbers. A block map for direct blocks is simply an array of block numbers. 
