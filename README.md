ATLast 1.2 - https://www.fourmilab.ch/atlast/
=============================================

Atlast is based upon the FORTH-83 language, but has been extended in many ways and modified to better serve its mission as an embedded toolkit for open, programmable applications. Atlast is implemented in a single file, written in portable C; it has been ported to many different machines and operating systems, including MS-DOS, OS/2, the Macintosh, and a wide variety of Unix machines. Atlast includes native support for floating point, C-like strings, Unix-compatible file access, and a wide variety of facilities for embedding within applications. Integers are 32 bits (64 bits in the 64-bit version of Atlast) and identifiers can be up to 127 characters; extensive stack and heap pointer checking is available to aid in debugging. Atlast may be configured at compilation time to include only the facilities needed by a given application, thus saving memory and increasing execution speed (when error checking is disabled). 

Atlast was developed at Autodesk, Inc. Autodesk returned the rights to John Walker in 1991, and, subsequently placed the program in the public domain. Autodesk's connection with this program is purely historical: it is neither endorsed, used, nor supported by Autodesk, Inc. 

This is an import of Andrew Holt Atlast repository (by andrewtholt, committed Dec 29, 2018).

In this version, there are, new features and used as the basis for a scriptable unit test framework.

See also forthSrc andrewtholt repo for example scripts. 

Below is Michael's original README.

=================

This is a fork of John Walker's ATLAST FORTH embeddable interpreter. My goal is to change the code to use a single
struct to hold the state of the interpreter. I think that will make it easier to embed. The cost will be speed. Going
through the struct will prevent many optimizations that were performed against the global variables. For my purposes,
this is an acceptable tradeoff.

You can find Mr. Walker's original work at https://www.fourmilab.ch/atlast/atlast.html. It has also been added to
other repositories here on GitHub. Searching for "atlast" will take you to them.
