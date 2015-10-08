/*
	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Improved incorporating suggestions by
		Steve Crawley, Cambridge University, October 1987
		Steven Pemberton, CWI, Amsterdam, Oct 1987

	memory management module

	This is a rather simple, straightforward, reference counting scheme.
	There are no provisions for detecting cycles, nor any attempt made
	at compaction.  Free lists of various sizes are maintained.
	At present only objects up to 255 bytes can be allocated,
	which mostly only limits the size of method (in text) you can create.

	reference counts are not stored as part of an object image, but
	are instead recreated when the object is read back in.
	This is accomplished using a mark-sweep algorithm, similar
	to those used in garbage collection.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <ctype.h>

#include "common.h"

#include "memory.h"

#include "tty.h"
#include "unixio.h"
#include "names.h"



object symbols;			/* table of all symbols created */

/*
  OBSOLETE COMMENT:

	in theory the ObjectTable should only be accessible to the memory
	manager.  Indeed, given the right macro definitions, this can be
	made so.  Never the less, for efficiency sake some of the macros
	can also be defined to access the object table directly

	Some systems (e.g. the Macintosh) have static limits (e.g. 32K)
	which prevent the object table from being declared.
	In this case the object table must first be allocated via
	calloc during the initialization of the memory manager.

  TODO: grow the object table as necessary.    
*/

struct objectStruct *ObjectTable;
static object_int lastObject = -1;

// Head of free slot list; -1 is the end of the list 
static object freeListHead = -1;



/*
	The following variables are strictly local to the memory
	manager module

	FREELISTMAX defines the maximum size of any object.
*/

#define FREELISTMAX 2000
//static object objectFreeList[FREELISTMAX];	/* free list of objects */

static void visit(object x);



// Return the absolute size in bytes of an object with
// objectStruct.size value 'size'.
static int byteSize(int size) {
    return size < 0 ? -size : size * sizeof(object);
}

static void
addToFreeList(object x) {
    assert(ObjectTable[oNdx(x)].memory == NULL);
    assert(ObjectTable[oNdx(x)].referenceCount == 0);
    ObjectTable[oNdx(x)].class = freeListHead;
    freeListHead = x;
}


/* initialize the memory management module */
void
initMemoryManager (void) {
    ObjectTable = ck_calloc(OBJECT_TABLE_MAX, sizeof(struct objectStruct));

    /* object at location 0 is the nil object, so give it nonzero ref */
    ObjectTable[0].referenceCount = 1;
    ObjectTable[0].size = 0;

    lastObject = 0;
}



// allocate a new memory object.  size is in objects refs.
object
allocObject (size_int memorySize) {
    object_int newSlot;

    if (memorySize < OBJSIZE_MIN || memorySize > OBJSIZE_MAX) {
        sysError("New object size exceeds maximum.", "");
    }
    
    if (freeListHead < 0) {
        ++lastObject;
        if (lastObject >= OBJECT_TABLE_MAX) {
            // TODO: grow the object table if possible...
            sysError("Out of object slots.", "");
        }

        newSlot = lastObject;
    } else {
        newSlot = oNdx(freeListHead);
        freeListHead = ObjectTable[newSlot].class;
    }

    struct objectStruct *ob = &ObjectTable[newSlot];
    ob->class           = nilobj;
    ob->referenceCount  = 0;
    ob->size            = memorySize;
    ob->memory          = ck_calloc(byteSize(memorySize), sizeof(object));
    
    return newSlot << 1;
}

object
allocByte (size_int size) {
    object newObj;

    newObj = allocObject(size);

    /* negative size fields indicate bit objects */
    ObjectTable[oNdx(newObj)].size = -size;

    return newObj;
}// allocByte 

object
allocStr (char *str) {
    object newSym;

    assert(strlen(str) + 1 < OBJSIZE_MAX);
    
    newSym = allocByte(1 + (size_int)strlen(str));
    strcpy(charPtr(newSym), str);
    return (newSym);
}// allocStr 


static void
clearObjectStruct(struct objectStruct *ob) {
    if (ob->memory) { free(ob->memory); }
    ob->memory = NULL;
    ob->size = 0;
    ob->referenceCount = 0;
    ob->class = 0;
}

/* do the real work in the decr procedure */
void
sysDecr(object z) {
    struct objectStruct *ob = &ObjectTable[oNdx(z)];
    assert(ob->referenceCount == 0);
    assert(ob->size == 0 || ob->memory);

    // Deref the fields if this is a ref object
    for (int n = 0; n < ob->size; n++) {
        decr(ob->memory[n]);
    }// for 
    
    clearObjectStruct(ob);
    
    addToFreeList(z);
}// sysDecr

void
byteAtPut (object z, int i, int x) {
    byte *bp;

    if (isInteger(z)) {
        sysError("indexing integer", "byteAtPut");
    } else if ((i <= 0) || (i > 2 * -sizeField(z))) {
        fprintf(stderr, "index %d size %d\n", i, sizeField(z));
        sysError("index out of range", "byteAtPut");
    } else {
        bp = bytePtr(z);
        bp[i - 1] = x;
    }
}

// Change the value of an existing string object.  Possibly a hack.
void
setStringValue(object x, const char *str) {
    struct objectStruct *xp = &ObjectTable[oNdx(x)];
    assert(xp->class == globalSymbol("String"));

    size_t len = strlen(str) + 1;
    xp->memory = realloc(xp->memory, len);
    if(!xp->memory) { sysError("realloc failed!", ""); }
    
    xp->size = -len;
    
    strcpy((char*)xp->memory, str);
}// setStringValue



/*
  Traverse object memory after imageRead() (when all reference counts
  are zero) and setting them to their correct value.

  ORIGINAL COMMENT:

  The following routine assures that objects read in are really referenced,
  eliminating junk that may be in the object file but not referenced.
  It is essentially a marking garbage collector algorithm using the
  reference counts as the mark.

  Written by Steven Pemberton.
*/

static void
visit(object x) {
    if (!x || isInteger(x)) { return; }
    
    incr(x);

    // If this is the first visit, recursively visit the subfields.
    if (ObjectTable[oNdx(x)].referenceCount == 1) {

        visit(ObjectTable[oNdx(x)].class);

        // Visit object fields.  Non-object fields (i.e. byte data)
        // are skipped because the size is negative.
        int sz = sizeField(x);
        for (int i = 0; i < sz; i++) {
            visit(ObjectTable[oNdx(x)].memory[i]);
        }// for
    }// if
    
}// visit


// Go through the range of possible live objects in the object table
// (0..lastObject) and free (via sysDecr) any object with memory but a
// zero reference count.
static void
freeAfterVisit() {
    for (int n = 0; n < OBJECT_TABLE_MAX; n++) {
        if (ObjectTable[n].referenceCount == 0 && ObjectTable[n].memory) {
            // We can't use sysDecr here because that will update the
            // free list and decr referenced objects, neither of which
            // is expected right now.
            clearObjectStruct(&ObjectTable[n]);
        }// if 
    }// for 
}// freeAfterVisit


// Only works after post-image load.
static void
recreateFreeList() {
    for (int n = 0; n < lastObject; n++) {
        struct objectStruct *ob = &ObjectTable[n];
        if (ob->referenceCount == 0 && ob->memory == NULL) {
            addToFreeList(ndxToObj(n));
        }// if 
    }// for 
}// recreateFreeList


// Basic mark-and-sweep garbage collection to be run on a
// freshly-loaded image.  Sets the reference counts, deletes
// unreferenced objects and constructs the free list.
void
postLoadGarbageCollect() {
    // Set references counts, which are currently 0.
    visit(symbols);
    ObjectTable[0].referenceCount++;    // Ensure nil has at least one reference

    freeAfterVisit();    // And free up any unused objects

    recreateFreeList();
}// postLoadGarbageCollect


// Return the number of objects currently in use
int
objectCount (void) {
    int count = 0;
    for (int i = 0; i < OBJECT_TABLE_MAX; i++) {
        if (ObjectTable[i].referenceCount > 0) {
            count++;
        }
    }
    return count;
}

// Return the length of the free table entry list.
int
freeCount(void) {
    int count = 0;
    object lf = freeListHead;

    while (lf >= 0) {
        ++count;
        assert(ObjectTable[oNdx(lf)].referenceCount == 0);
        lf = ObjectTable[oNdx(lf)].class;
    }

    return count;
}// freeCount






// Format for object written to disk.
struct DummyObject {
    object_int  di;     // index
    object      cl;     // class ref
    size_int    ds;     // data size
    // ... data follows...
};


/*
	imageRead - read in an object image

    OBSOLETE COMMENT:

		we toss out the free lists built initially,
		reconstruct the linkages, then rebuild the free
		lists around the new objects.
		The only objects with nonzero reference counts
		will be those reachable from either symbols

    WARNING: should only be called on a newly-initialized (i.e. all
    zero) object table.
*/

void
imageRead(FILE * fp) {
    struct DummyObject dummyObject;

    fread_chk(fp, (char *) &symbols, sizeof(object));

    while (fread_chk(fp, (char *) &dummyObject, sizeof(dummyObject))) {

        // Index
        int i = dummyObject.di;
        if (i < 0 || i > OBJECT_TABLE_MAX) {
            sysError("reading index out of range", "");
        }
        lastObject = i > lastObject ? i : lastObject;

        // Class
        ObjectTable[i].class = dummyObject.cl;
        if (ObjectTable[i].class < 0 ||
            oNdx(ObjectTable[i].class) > OBJECT_TABLE_MAX)
        {
            fprintf(stderr, "index %d\n", dummyObject.cl);
            sysError("class out of range", "imageRead");
        }

        // Memory size
        ObjectTable[i].size = dummyObject.ds;

        int size = byteSize(dummyObject.ds);
        if (size != 0) {
            ObjectTable[i].memory = ck_calloc(size, sizeof(object));
            fread_chk(fp, (char *) ObjectTable[i].memory, size);
        } else {
            // Does this happen?
            ObjectTable[i].memory = NULL;
        }// if .. else
    }// while 

    postLoadGarbageCollect();
}// imageRead

void
imageWrite(FILE * fp) {
    struct DummyObject dummyObject;

    fwrite_chk(fp, (char *) &symbols, sizeof(object));

    for (int i = 0; i < OBJECT_TABLE_MAX; i++) {
        if (ObjectTable[i].referenceCount > 0) {
            dummyObject.di = i;
            dummyObject.cl = ObjectTable[i].class;
            dummyObject.ds = ObjectTable[i].size;
            fwrite_chk(fp, (char *) &dummyObject, sizeof(dummyObject));

            int size = byteSize(ObjectTable[i].size);
            if (size != 0) {
                fwrite_chk(fp, (char *) ObjectTable[i].memory, size);
            }// if 
        }// if 
    }// for 
}// imageWrite


// Write out a text file (named by filename) containing the contents
// of object memory in a human-friendly format.  This turns out to be
// extremely useful when debugging.
//
// It gets called shortly after building the initial image to create
// bootstrap/object-map.txt but can also be invoked directly from a
// modern debugger.
void
printObjectTable(const char *filename) {
    // There's currently no easy way to walk a Dictionary, so we just
    // look up likely-looking items instead.
    char *globals[] = {
        "Array", "Block", "Boolean", "ByteArray", "Char", "Class","Collection",
        "Context", "Dictionary", "False", "File", "Float", "Fraction",
        "IndexedCollection", "Integer", "Integer", "Interval", "Link",
        "List", "LongInteger", "Magnitude", "Method", "Number", "Object",
        "Process", "Random", "Scheduler", "Semaphore", "Set", "Smalltalk",
        "String", "Switch", "Symbol", "True", "UndefinedObject",
        NULL
    };

    FILE *fh = fopen(filename, "w");
    if (!fh) {
        fprintf(stderr, "Unable to dump object table to '%s'\n", filename);
        return;
    }

    fprintf(fh, "symbols=%d (index %d)\n", symbols, oNdx(symbols));
    
    fprintf(fh, "Symbols:\n");
    for (int n = 0; globals[n]; n++) {
        object id = globalSymbol(globals[n]);
        if (id) {
            fprintf(fh, "%s = %d\n", globals[n], id);
        }
    }
    fprintf(fh, "\n");
    
    for (int i = 0; i < OBJECT_TABLE_MAX; i++) {
        if (ObjectTable[i].referenceCount <= 0) continue;
        
        int di = i<<1;
        object cl = ObjectTable[i].class;
        int rc = ObjectTable[i].referenceCount;
        int size = ObjectTable[i].size;
        void *mem = ObjectTable[i].memory;
        
        fprintf(fh, "di=%d cl=%d ds=%d rc=%d |", di, cl, size, rc);

        if (size < 0) {
            fprintf(fh, "'");
            for (int n = 0; n < -size; n++) {
                char c = ((char *)mem)[n];
                fprintf(fh, "%c",
                        isprint(c) || isspace(c) ? c    :
                        c == 0                   ? '0'  : '?');
            }
            fprintf(fh, "'");
        }
        else {
            for(int n = 0; n < size; n++) {
                fprintf(fh, " %d", ((object*)mem)[n]);
            }
        }// if .. else
        fprintf(fh, "\n");
    }// for

    fclose(fh);
}// printObjectTable
