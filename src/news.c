/*
	little smalltalk, version 3.1
	written by tim budd, July 1988

	new object creation routines
	built on top of memory allocation, these routines
	handle the creation of various kinds of objects
*/

#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "names.h"
#include "news.h"

static object arrayClass = NIL_OBJ;     /* the class Array */
static object intClass = NIL_OBJ;       /* the class Integer */
static object stringClass = NIL_OBJ;	/* the class String */
static object symbolClass = NIL_OBJ;	/* the class Symbol */


object
getClass (		/* getClass - get the class of an object */
    object obj
) {
    if (isInteger(obj)) {
        if (intClass == nilobj) {
            intClass = globalSymbol("Integer");
        }
        return (intClass);
    }
    return (classField(obj));
}

object
newArray (int size) {
    object newObj;

    newObj = allocObject(size);
    if (arrayClass == nilobj) {
        arrayClass = globalSymbol("Array");
    }
    setClass(newObj, arrayClass);
    return newObj;
}

object
newBlock (void) {
    object newObj;

    newObj = allocObject(OBSIZE_block);
    setClass(newObj, globalSymbol("Block"));
    return newObj;
}

object
newByteArray (int size) {
    object newobj;

    newobj = allocByte(size);
    setClass(newobj, globalSymbol("ByteArray"));
    return newobj;
}

object
newChar (int value) {
    object newobj;

    newobj = allocObject(1);
    basicAtPut(newobj, 1, newInteger(value));
    setClass(newobj, globalSymbol("Char"));
    return (newobj);
}

object
newClass (char *name) {
    object newObj, nameObj;

    newObj = allocObject(OBSIZE_class);
    setClass(newObj, globalSymbol("Class"));

    /* now make name */
    nameObj = newSymbol(name);
    basicAtPut(newObj, OFST_class_name, nameObj);

    /* now put in global symbols table */
    nameTableInsert(symbols, strHash(name), nameObj, newObj);

    return newObj;
}

object
copyFrom (object obj, int start, int size) {
    object newObj;
    int i;

    newObj = newArray(size);
    for (i = 1; i <= size; i++) {
        basicAtPut(newObj, i, basicAt(obj, start));
        start++;
    }
    return newObj;
}

object
newContext (int link, object method, object args, object temp) {
    object newObj;

    newObj = allocObject(OBSIZE_context);
    setClass(newObj, globalSymbol("Context"));
    basicAtPut(newObj, OFST_context_linkPtr, newInteger(link));
    basicAtPut(newObj, OFST_context_method, method);
    basicAtPut(newObj, OFST_context_arguments, args);
    basicAtPut(newObj, OFST_context_temporaries, temp);
    return newObj;
}

object
newDictionary () {
    object newObj;
    const int METHOD_TABLE_SIZE = 39;   // Duplicated in Smalltalk code.
    

    newObj = allocObject(1);
    setClass(newObj, globalSymbol("Dictionary"));
    basicAtPut(newObj, 1, newArray(METHOD_TABLE_SIZE));
    return newObj;
}

object
newFloat (double d) {
    object newObj;

    newObj = allocByte((int) sizeof(double));
    memcpy(charPtr(newObj), (char *) &d, (int) sizeof(double));
    setClass(newObj, globalSymbol("Float"));
    return newObj;
}

double
floatValue (object o) {
    double d;

    memcpy((char *) &d, charPtr(o), (int) sizeof(double));
    return d;
}

object
newLink (object key, object value) {
    object newObj;

    newObj = allocObject(3);
    setClass(newObj, globalSymbol("Link"));
    basicAtPut(newObj, 1, key);
    basicAtPut(newObj, 2, value);
    return newObj;
}

object
newMethod (void) {
    object newObj;

    newObj = allocObject(OBSIZE_method);
    setClass(newObj, globalSymbol("Method"));
    return newObj;
}

object
newStString (char *value) {
    object newObj;

    newObj = allocStr(value);
    if (stringClass == nilobj) {
        stringClass = globalSymbol("String");
    }
    setClass(newObj, stringClass);
    return (newObj);
}

object
newSymbol (char *str) {
    object newObj;

    /* first see if it is already there */
    newObj = globalKey(str);
    if (newObj) {
        return newObj;
    }

    /* not found, must make */
    newObj = allocStr(str);
    if (symbolClass == nilobj) {
        symbolClass = globalSymbol("Symbol");
    }
    setClass(newObj, symbolClass);
    nameTableInsert(symbols, strHash(str), newObj, nilobj);
    return newObj;
}
