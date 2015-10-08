/*
	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987
*/

#ifndef __NAMES_H
#define __NAMES_H

// Sizes (OBSIZE_*) and field offsets (OFST_*) of instances of several
// well-known internal classes.
enum ClassOffsets {
    OBSIZE_class = 5,
    OFST_class_name = 1,
    OFST_class_size = 2,
    OFST_class_methods = 3,
    OFST_class_superClass = 4,
    OFST_class_variables = 5,

    OBSIZE_method = 8,
    OFST_method_text = 1,
    OFST_method_message = 2,
    OFST_method_bytecodes = 3,
    OFST_method_literals = 4,
    OFST_method_stackSize = 5,
    OFST_method_temporarySize = 6,
    OFST_method_methodClass = 7,
    OFST_method_watch = 8,

    OBSIZE_context = 6,
    OFST_context_linkPtr = 1,
    OFST_context_method = 2,
    OFST_context_arguments = 3,
    OFST_context_temporaries = 4,

    OBSIZE_block = 6,
    OFST_block_context = 1,
    OFST_block_argumentCount = 2,
    OFST_block_argumentLocation = 3,
    OFST_block_bytecountPosition = 4,

    OBSIZE_process = 3,
    OFST_process_stack = 1,
    OFST_process_stackTop = 2,
    OFST_process_linkPtr = 3,
};

// Index of nilobj; this needs to be a #define so that we can use it
// for compile-time initialization.
#define NIL_OBJ ((object)0)

// Indexes of well known objects
extern const object nilobj;
extern object trueobj;        // the pseudo variable true
extern object falseobj;       // the pseudo variable false */

// Well-known symbols; these are a (faster) special case in the
// interpreter.
extern object unSyms[], binSyms[];

extern object globalSymbol(char *str);
extern void nameTableInsert(object dict, int hash, object key, object value);
extern object hashEachElement(object dict, int hash, int (*fun)(object));
extern int strHash(char *str);
extern object globalKey(char *str);
extern object nameTableLookup(object dict, char *str);
extern void initCommonSymbols(void);

#endif
