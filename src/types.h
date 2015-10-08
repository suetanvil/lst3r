
/* Widely-used types; platform specific stuff goes in env.h */

#ifndef __TYPES_H
#define __TYPES_H



#ifdef TRUE
#   undef TRUE
#endif

#ifdef FALSE
#   undef FALSE
#endif

typedef enum _boolean {
    TRUE = 1,
    FALSE = 0,
} boolean;

typedef unsigned char byte;

/*
  Original comment:

    The first major decision to be made in the memory manager is what
    an entity of type object really is.  Two obvious choices are a pointer (to
    the actual object memory) or an index into an object table.  We decided to
    use the latter, although either would work.

    Similarly, one can either define the token object using a typedef,
    or using a define statement.  Either one will work (check this?)

  To do:

    When lst was first written, type 'int' tended to be 16 bits, and
    that is an implicit assumption in a lot of the code.  On modern
    hardware, it is either 32- or 64-bits.

    The downside of 32- or 64-bit objects is that it significantly
    bloats the image (remember--Smalltalk objects are mostly pointers
    to other Smalltalk objects), which is a problem when dealing with
    a memory-constrained environment.

    On the plus side, it means we can have more than 2^14 objects in
    our table.

    For now, I'm leaving this as-is because I'm using the initial
    image as a quick-and-dirty canary for catching refactoring bugs.
    However, in the long term, I plan on making this configurable at
    compile-time in env.h.

*/

typedef object_int object;

#endif
