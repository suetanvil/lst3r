/*
    Little Smalltalk, version 2
    Written by Tim Budd, Oregon State University, July 1987
*/

#ifndef __MEMORY_H
#define __MEMORY_H

#include <stdlib.h>
#include <assert.h>

/*

    Original comment:

        The memory module itself is defined by over a dozen routines.

        All of these could be defined by procedures, and indeed this
        was originally done.  However, for efficiency reasons, many of
        these procedures can be replaced by macros generating in-line
        code.  For the latter approach to work, the structure of the
        object table must be known.  For this reason, it is given here.
        Note, however, that outside of the files memory.c and unixio.c (or
        macio.c on the macintosh) ONLY the macros described in this file
        make use of this structure: therefore modifications or even
        complete replacement is possible as long as the interface remains
        consistent.
    
    I have replaced the macros with inline functions and moved object
    reading/writing code into memory.c (C I/O is nicely standardized
    these days and anyway, the actual I/O code still resides in
    unixio.c).  The object table still needs to be here though to be
    visible to the inline functions.  Alas.

*/

struct objectStruct {
    object class;           // also next-free-object pointer if unalloc'd
    count_int referenceCount;   // saturates at COUNT_MAX
    size_int size;         // size in objects if >=0; else -(size in bytes)
    object *memory;
};

extern struct objectStruct *ObjectTable;

/* the dictionary symbols is the source of all symbols in the system */
extern object symbols;

extern const object nilobj;


extern object allocObject(size_int memorySize);
extern object allocByte(size_int size);
extern object allocStr(char *str);
extern void initMemoryManager(void);
extern void sysDecr(object z);
extern void byteAtPut(object z, int i, int x);
extern int objectCount(void);
extern int freeCount(void);
extern void imageWrite(FILE * fp);
extern void imageRead(FILE * fp);
extern void setStringValue(object x, const char *str); 
extern void printObjectTable(const char *filename);

static inline boolean isInteger(object x);

// Extract the object table index from an object reference.
static inline int oNdx(object x) {
    assert(!isInteger(x));
    return x >> 1;
}

static inline int ndxToObj(int index) { return index << 1; }


/*
  OBSOLETE COMMENT:

    The most basic routines to the memory manager are incr and decr,
    which increment and decrement reference counts in objects.  By separating
    decrement from memory freeing, we could replace these as procedure calls
    by using the following macros (thereby saving procedure calls):

  These are now inline functions (Hooray for C99!).  In addition, incr
  will not overflow the reference count and decr will not decrement a
  maxed-out reference count.  (This results in a tenured object if it
  gets too many references, but that's not a huge problem and anyway,
  saving and loading the image will take care of it if it's garbage.)
*/
static inline void incr(object z) {
    if (!z || isInteger(z)) { return; }
    
    struct objectStruct *obj = &ObjectTable[oNdx(z)];
    if (obj->referenceCount < COUNT_MAX) {
        obj->referenceCount++;
    }
}// incr

static inline void decr(object z) {
    if (!z || isInteger(z)) { return; }
    
    struct objectStruct *obj = &ObjectTable[oNdx(z)];
    if (obj->referenceCount < COUNT_MAX) { obj->referenceCount--; }
    if (obj->referenceCount == 0) { sysDecr(z); }
}// decr


/*
    Integer objects are (but need not be) treated specially.

    In this memory manager, negative integers are just left as is, but
    positive integers are changed to x*2+1.  Either a negative or an odd
    number is therefore an integer, while a nonzero even number is an
    object pointer (multiplied by two).  Zero is reserved for the object ``nil''
    Since newInteger does not fill in the class field, it can be given here.
    If it was required to use the class field, it would have to be deferred
    until names.h
*/

static inline boolean isInteger(object x)   { return (x & 1) || x < 0; }
static inline object newInteger(int x)      { return x < 0 ? x : (x << 1) + 1;}
static inline int intValue(object x) {
    assert(isInteger(x));
    return x < 0 ? x : x >> 1;
}





/*
    Finally, a few routines (or macros) are used to access or set
    class fields and size fields of objects
*/

static inline struct objectStruct* getObjStruct(object x) {
    assert(!isInteger(x) && oNdx(x) < OBJECT_TABLE_MAX);
    struct objectStruct *ob = &ObjectTable[oNdx(x)];
//    assert(ob->referenceCount > 0);
    assert(!isInteger(ob->class));  // TODO: better way to detect allocated objects
    return ob;
}

static inline object classField(object x) {
    object cl = getObjStruct(x)->class;
    assert(getObjStruct(cl));   // Ensure it's a valid object
    return cl;
}

static inline void setClass(object obj, object cls) {
    getObjStruct(obj)->class = cls;
    incr(cls);
}

static inline short  sizeField(object x) { return getObjStruct(x)->size; }
static inline object *sysMemPtr(object x){ return getObjStruct(x)->memory;}
/* static inline object *memoryPtr(object x) { */
/*     return isInteger(x) ? NULL : sysMemPtr(x); */
/* } */

static inline byte* bytePtr(object x) {
    struct objectStruct *ob = getObjStruct(x);
    assert(ob->size <= 0);
    return (byte *) ob->memory;
}
static inline char* charPtr(object x) { return (char *) bytePtr(x); }



/*
    There are four routines used to access fields within an object.

    Again, some of these could be replaced by macros, for efficiency
    basicAt(x, i) - ith field (start at 1) of object x
    basicAtPut(x, i, v) - put value v in object x
    byteAt(x, i) - ith field (start at 0) of object x
//  byteAtPut(x, i, v) - put value v in object x
*/

static inline object basicAt(object x, int i) {
    assert(!isInteger(x));

    struct objectStruct *op = &ObjectTable[oNdx(x)];

    assert(op->size >= 0 && i <= op->size);
    assert(i > 0);
    
    return op->memory[i-1];
}

static inline int byteAt(object x, int i) {
    assert(i > 0 && i <= -sizeField(x));
    return (int)(bytePtr(x)[i-1]);
}

static inline void simpleAtPut(object x,int i,object y){
    assert(i > 0 && i <= sizeField(x));
    sysMemPtr(x)[i-1] = y;
}

static inline void basicAtPut(object x, int i, object y) {
    simpleAtPut(x, i, y);
    incr(y);
}

static inline void fieldAtPut(object x, int i, object y) {
    decr(basicAt(x,i));
    basicAtPut(x,i,y);
}

#endif



