/*
	Little Smalltalk, version 3
	Written by Tim Budd, Oregon State University, June 1988

	routines used in reading in textual descriptions of classes
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "memory.h"
#include "names.h"
#include "lex.h"
#include "news.h"
#include "tty.h"

#include "parser.h"


/*
	the following are switch settings, with default values
*/
static boolean savetext = TRUE;

/*
	we read the input a line at a time, putting lines into the following
	buffer.  In addition, all methods must also fit into this buffer.

    (This gets used in a couple of different places; it's global
    (within the module) because I have a vague idea that someone may
    want to port this to some really space-restricted hardware where
    this would make sense.)
*/
static char textBuffer[TEXT_BUFFER_SIZE];

/*
	findClass gets a class object,
	either by finding it already or making it
	in addition, it makes sure it has a size, by setting
	the size to zero if it is nil.
*/
static object
findClass (char *name) {
    object newobj;

    newobj = globalSymbol(name);
    if (newobj == nilobj) {
        newobj = newClass(name);
    }
    if (basicAt(newobj, OFST_class_size) == nilobj) {
        basicAtPut(newobj, OFST_class_size, newInteger(0));
    }
    return newobj;
}

/*
	readDeclaration reads a declaration of a class
*/
static void
readClassDeclaration (struct LexContext *ctx) {
    object classObj, super, vars;
    int i, size, instanceTop;
    object instanceVariables[15];

    if (nextToken(ctx) != TOK_NAMECONST) {
        sysError("bad file format", "no name in declaration");
    }
    classObj = findClass(ctx->tokenString);
    size = 0;
    if (nextToken(ctx) == TOK_NAMECONST) {	/* read superclass name */
        super = findClass(ctx->tokenString);
        basicAtPut(classObj, OFST_class_superClass, super);
        size = intValue(basicAt(super, OFST_class_size));
        nextToken(ctx);
    }
    if (ctx->token == TOK_NAMECONST) {	/* read instance var names */
        instanceTop = 0;
        while (ctx->token == TOK_NAMECONST) {
            instanceVariables[instanceTop++] = newSymbol(ctx->tokenString);
            size++;
            nextToken(ctx);
        }
        vars = newArray(instanceTop);
        for (i = 0; i < instanceTop; i++) {
            basicAtPut(vars, i + 1, instanceVariables[i]);
        }
        basicAtPut(classObj, OFST_class_variables, vars);
    }
    basicAtPut(classObj, OFST_class_size, newInteger(size));
}

/*
	readClass reads a class method description
*/
static void
readMethods (FILE *fd, struct LexContext *ctx, boolean printit) {
    object classObj, methTable, theMethod, selector;
    char *cp, *eoftest;

    if (nextToken(ctx) != TOK_NAMECONST) {
        sysError("missing name", "following Method keyword");
    }
    classObj = findClass(ctx->tokenString);
    setInstanceVariables(classObj);
    if (printit) {
        cp = charPtr(basicAt(classObj, OFST_class_name));
    }

    /* now find or create a method table */
    methTable = basicAt(classObj, OFST_class_methods);
    if (methTable == nilobj) {	/* must make */
        methTable = newDictionary();
        basicAtPut(classObj, OFST_class_methods, methTable);
    }

    /* now go read the methods */
    char lineBuffer[512];
    do {
        if (lineBuffer[0] == '|') {	/* get any left over text */
            strncpy(textBuffer, &lineBuffer[1], sizeof(textBuffer));
        } else {
            textBuffer[0] = '\0';
        }
        
        while ((eoftest = fgets(lineBuffer, sizeof(lineBuffer), fd)) != NULL) {
            if ((lineBuffer[0] == '|') || (lineBuffer[0] == ']')) {
                break;
            }
            strncat(textBuffer, lineBuffer,
                    sizeof(textBuffer) - strlen(lineBuffer));

            if(strlen(textBuffer) >= sizeof(textBuffer) - 2) {
                sysError("line too long for internal buffer","");
            }
        }
        
        if (eoftest == NULL) {
            sysError("unexpected end of file", "while reading method");
            break;
        }

        /* now we have a method */
        theMethod = newMethod();
        if (parse(theMethod, textBuffer, savetext)) {
            selector = basicAt(theMethod, OFST_method_message);
            basicAtPut(theMethod, OFST_method_methodClass, classObj);
            if (printit) {
                dspMethod(cp, charPtr(selector));
            }
            nameTableInsert(methTable, (int) selector,
                            selector, theMethod);
        } else {
            /* get rid of unwanted method */
            incr(theMethod);
            decr(theMethod);
            givepause();
        }

    } while (lineBuffer[0] != ']');
}

/*
	fileIn reads in a module definition
*/
void fileIn(FILE * fd, boolean printit) {
    struct LexContext *ctx = newLexer();
    
    while (fgets(textBuffer, sizeof(textBuffer), fd) != NULL) {
        lexinit(ctx, textBuffer);
        nextToken(ctx);
        
        if (ctx->token == TOK_INPUTEND) {
            /* do TOK_NOTHING, get next line */
        } else if (ctx->token == TOK_BINARY && streq(ctx->tokenString, "*")) {
            /* do TOK_NOTHING, its a comment */
        } else if (ctx->token == TOK_NAMECONST &&
                   streq(ctx->tokenString, "Class")) {
            readClassDeclaration(ctx);
        } else if (ctx->token == TOK_NAMECONST &&
                   streq(ctx->tokenString, "Methods")) {
            readMethods(fd, ctx, printit);
        } else {
            sysError("unrecognized line", textBuffer);
        }
    }

    freeLexer(ctx);
}// fileIn
