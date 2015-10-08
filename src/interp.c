/*
    Little Smalltalk version 3
    Written by Tim Budd, Oregon State University, July 1988

    bytecode interpreter module

    given a process object, execute bytecodes in a tight loop.

    performs subroutine calls for
        a) finding a non-cached method
        b) executing a primitive

    otherwise simply loops until time slice has ended
*/

#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "names.h"
#include "interp.h"
#include "primitive.h"
#include "news.h"
#include "tty.h"

static boolean watching = 0;

/* the following are manipulated by primitives */
object processStack;
int linkPointer;

static object method, messageToSend;

static int
messTest (object obj) {
    return obj == messageToSend;
}

/* a cache of recently executed methods is used for fast lookup */
#define CACHE_SIZE 211
static struct {
    object cacheMessage;	/* the message being requested */
    object lookupClass;		/* the class of the receiver */
    object cacheClass;		/* the class of the method */
    object cacheMethod;		/* the method itself */
} methodCache[CACHE_SIZE];




/* flush an entry from the cache (usually when its been recompiled) */
void
flushCache (object messageToSend, object class) {
    int hash;

    hash = (((int) messageToSend) + ((int) class)) / CACHE_SIZE;
    methodCache[hash].cacheMessage = nilobj;
}

/*
	findMethod
		given a message and a class to start looking in,
		find the method associated with the message
*/
static boolean
findMethod (object *methodClassLocation) {
    object methodTable, methodClass;

    method = nilobj;
    methodClass = *methodClassLocation;

    for (; methodClass != nilobj; methodClass =
                basicAt(methodClass, OFST_class_superClass)) {
        methodTable = basicAt(methodClass, OFST_class_methods);
        method = hashEachElement(methodTable, messageToSend, messTest);
        if (method != nilobj) {
            break;
        }
    }

    if (method == nilobj) {	/* it wasn't found */
        methodClass = *methodClassLocation;
        return FALSE;
    }

    *methodClassLocation = methodClass;
    return TRUE;
}


static object
growProcessStack (int top, int toadd) {
    int size, i;
    object newStack;

    if (toadd < 100) {
        toadd = 100;
    }
    size = sizeField(processStack) + toadd;
    newStack = newArray(size);
    for (i = 1; i <= top; i++) {
        basicAtPut(newStack, i, basicAt(processStack, i));
    }
    return newStack;
}


static inline int
methodStackSize(object x) {
    return intValue(basicAt(x, OFST_method_stackSize));
}


static inline int
methodTempSize(object x) {
    return intValue(basicAt(x, OFST_method_temporarySize));
}

boolean
execute (object aProcess, int maxsteps) {
#   define NEXT_BYTE() *(bp + byteOffset++)
#   define IPUSH(x) incr(*++stackTop=(x))

#   define STACKTOP_PUT(x) decr(*stackTop); incr(*stackTop = (x))
#   define STACKTOP_FREE() decr(*stackTop); *stackTop-- = nilobj

    // note that IPOP leaves x with excess reference count
#   define IPOP(x) x = *stackTop; *stackTop-- = nilobj

#   define PROCESS_STACK_TOP() ((stackTop-psb)+1)
#   define PROCESS_STACK_AT(n) *(psb+(n-1))

#   define RECEIVER_AT(n) *(rcv+n)
#   define RECEIVER_AT_PUT(n,x) decr(RECEIVER_AT(n)); incr(RECEIVER_AT(n)=(x))

#   define ARGUMENTS_AT(n) *(arg+n)

#   define TEMPORARY_AT(n) *(temps+n)
#   define TEMPORARY_AT_PUT(n,x) decr(TEMPORARY_AT(n)); incr(TEMPORARY_AT(n)=(x))

#   define LITERALS_AT(n) *(lits+n)

    object returnedObject;
    int returnPoint, timeSliceCounter;
    object *stackTop, *psb, *rcv, *arg, *temps, *lits, *cntx;
    object contextObject, *primargs;
    int byteOffset;
    object methodClass, argarray;
    int i, j;
    int low;
    int high;
    byte *bp;

    /* unpack the instance variables from the process */
    processStack = basicAt(aProcess, OFST_process_stack);
    psb = sysMemPtr(processStack);
    j = intValue(basicAt(aProcess, OFST_process_stackTop));
    stackTop = psb + (j - 1);
    linkPointer = intValue(basicAt(aProcess, OFST_process_linkPtr));

    /* set the process time-slice counter before entering loop */
    timeSliceCounter = maxsteps;

    /* retrieve current values from the linkage area */
readLinkageBlock:
    contextObject = PROCESS_STACK_AT(linkPointer + 1);
    returnPoint = intValue(PROCESS_STACK_AT(linkPointer + 2));
    byteOffset = intValue(PROCESS_STACK_AT(linkPointer + 4));
    if (contextObject == nilobj) {
        contextObject = processStack;
        cntx = psb;
        arg = cntx + (returnPoint - 1);
        method = PROCESS_STACK_AT(linkPointer + 3);
        temps = cntx + linkPointer + 4;
    } else {			/* read from context object */
        cntx = sysMemPtr(contextObject);
        method = basicAt(contextObject, OFST_context_method);
        arg = sysMemPtr(basicAt(contextObject, OFST_context_arguments));
        temps = sysMemPtr(basicAt(contextObject, OFST_context_temporaries));
    }

    if (!isInteger(ARGUMENTS_AT(0))) {
        rcv = sysMemPtr(ARGUMENTS_AT(0));
    }

readMethodInfo:
    lits = sysMemPtr(basicAt(method, OFST_method_literals));
    bp = bytePtr(basicAt(method, OFST_method_bytecodes)) - 1;

    while (--timeSliceCounter > 0) {
        low = (high = NEXT_BYTE()) & 0x0F;
        high >>= 4;
        if (high == BC_Extended) {
            high = low;
            low = NEXT_BYTE();
        }
        switch (high) {

        case BC_PushInstance:
            IPUSH(RECEIVER_AT(low));
            break;

        case BC_PushArgument:
            IPUSH(ARGUMENTS_AT(low));
            break;

        case BC_PushTemporary:
            IPUSH(TEMPORARY_AT(low));
            break;

        case BC_PushLiteral:
            IPUSH(LITERALS_AT(low));
            break;

        case BC_PushConstant:
            switch (low) {
            case CC_zero:
            case CC_one:
            case CC_two:
                IPUSH(newInteger(low));
                break;

            case CC_minusOne:
                IPUSH(newInteger(-1));
                break;

            case CC_contextConst:
                /* check to see if we have made a block context yet */
                if (contextObject == processStack) {
                    /* not yet, do it now - first get real return point */
                    returnPoint =
                        intValue(PROCESS_STACK_AT(linkPointer + 2));
                    contextObject =
                        newContext(linkPointer, method,
                                   copyFrom(processStack, returnPoint,
                                            linkPointer - returnPoint),
                                   copyFrom(processStack, linkPointer + 5,
                                            methodTempSize(method)));
                    basicAtPut(processStack, linkPointer + 1,
                               contextObject);
                    IPUSH(contextObject);
                    /* save byte pointer then restore things properly */
                    fieldAtPut(processStack, linkPointer + 4,
                               newInteger(byteOffset));
                    goto readLinkageBlock;

                }
                IPUSH(contextObject);
                break;

            case CC_nilConst:
                IPUSH(nilobj);
                break;

            case CC_trueConst:
                IPUSH(trueobj);
                break;

            case CC_falseConst:
                IPUSH(falseobj);
                break;

            default:
                sysError("unimplemented constant", "pushConstant");
            }
            break;

        case BC_AssignInstance:
            RECEIVER_AT_PUT(low, *stackTop);
            break;

        case BC_AssignTemporary:
            TEMPORARY_AT_PUT(low, *stackTop);
            break;

        case BC_MarkArguments:
            returnPoint = (PROCESS_STACK_TOP() - low) + 1;
            timeSliceCounter++;	/* make sure we do send */
            break;

        case BC_SendMessage:
            messageToSend = LITERALS_AT(low);

doSendMessage:
            arg = psb + (returnPoint - 1);
            if (isInteger(ARGUMENTS_AT(0)))
                /* should fix this later */
            {
                methodClass = getClass(ARGUMENTS_AT(0));
            } else {
                rcv = sysMemPtr(ARGUMENTS_AT(0));
                methodClass = classField(ARGUMENTS_AT(0));
            }

doFindMessage:
            /* look up method in cache */
            i = (((int) messageToSend) + ((int) methodClass)) % CACHE_SIZE;
            if ((methodCache[i].cacheMessage == messageToSend) &&
                    (methodCache[i].lookupClass == methodClass)) {
                method = methodCache[i].cacheMethod;
                methodClass = methodCache[i].cacheClass;
            } else {
                methodCache[i].lookupClass = methodClass;
                if (!findMethod(&methodClass)) {
                    /* not found, we invoke a smalltalk method */
                    /* to recover */
                    j = PROCESS_STACK_TOP() - returnPoint;
                    argarray = newArray(j + 1);
                    for (; j >= 0; j--) {
                        IPOP(returnedObject);
                        basicAtPut(argarray, j + 1, returnedObject);
                        decr(returnedObject);
                    }
                    IPUSH(basicAt(argarray, 1));	/* push receiver back */
                    IPUSH(messageToSend);
                    messageToSend =
                        newSymbol("message:notRecognizedWithArguments:");
                    IPUSH(argarray);
                    /* try again - if fail really give up */
                    if (!findMethod(&methodClass)) {
                        sysWarn("can't find", "error recovery method");
                        /* just quit */
                        return FALSE;
                    }
                }
                methodCache[i].cacheMessage = messageToSend;
                methodCache[i].cacheMethod = method;
                methodCache[i].cacheClass = methodClass;
            }

            if (watching && (basicAt(method, OFST_method_watch) != nilobj)) {
                /* being watched, we send to method itself */
                j = PROCESS_STACK_TOP() - returnPoint;
                argarray = newArray(j + 1);
                for (; j >= 0; j--) {
                    IPOP(returnedObject);
                    basicAtPut(argarray, j + 1, returnedObject);
                    decr(returnedObject);
                }
                IPUSH(method);	/* push method */
                IPUSH(argarray);
                messageToSend = newSymbol("watchWith:");
                /* try again - if fail really give up */
                methodClass = classField(method);
                if (!findMethod(&methodClass)) {
                    sysWarn("can't find", "watch method");
                    /* just quit */
                    return FALSE;
                }
            }

            /* save the current byte pointer */
            fieldAtPut(processStack, linkPointer + 4,
                       newInteger(byteOffset));

            /* make sure we have enough room in current process */
            /* stack, if not make stack larger */
            i = 6 + methodTempSize(method) + methodStackSize(method);
            j = PROCESS_STACK_TOP();
            if ((j + i) > sizeField(processStack)) {
                processStack = growProcessStack(j, i);
                psb = sysMemPtr(processStack);
                stackTop = (psb + j);
                fieldAtPut(aProcess, OFST_process_stack, processStack);
            }

            byteOffset = 1;
            /* now make linkage area */
            /* position 0 : old linkage pointer */
            IPUSH(newInteger(linkPointer));
            linkPointer = PROCESS_STACK_TOP();
            /* position 1 : context object (nil means stack) */
            IPUSH(nilobj);
            contextObject = processStack;
            cntx = psb;
            /* position 2 : return point */
            IPUSH(newInteger(returnPoint));
            arg = cntx + (returnPoint - 1);
            /* position 3 : method */
            IPUSH(method);
            /* position 4 : bytecode counter */
            IPUSH(newInteger(byteOffset));
            /* then make space for temporaries */
            temps = stackTop + 1;
            stackTop += methodTempSize(method);
            /* break if we are too big and probably looping */
            if (sizeField(processStack) > 1800) {
                timeSliceCounter = 0;
            }
            goto readMethodInfo;

        case BC_SendUnary:
            /* do isNil and notNil as special cases, since */
            /* they are so common */
            if ((!watching) && (low <= 1)) {
                if (*stackTop == nilobj) {
                    STACKTOP_PUT(low ? falseobj : trueobj);
                    break;
                }
            }
            returnPoint = PROCESS_STACK_TOP();
            messageToSend = unSyms[low];
            goto doSendMessage;
            break;

        case BC_SendBinary:
            /* optimized as long as arguments are int */
            /* and conversions are not necessary */
            /* and overflow does not occur */
            if ((!watching) && (low <= 12)) {
                primargs = stackTop - 1;
                returnedObject = primitive(low + 60, primargs);
                if (returnedObject != nilobj) {
                    /* pop arguments off stack , push on result */
                    STACKTOP_FREE();
                    STACKTOP_PUT(returnedObject);
                    break;
                }
            }
            /* else we do it the old fashion way */
            returnPoint = PROCESS_STACK_TOP() - 1;
            messageToSend = binSyms[low];
            goto doSendMessage;

        case BC_DoPrimitive:
            /* low gives number of arguments */
            /* next byte is primitive number */
            primargs = (stackTop - low) + 1;
            /* next byte gives primitive number */
            i = NEXT_BYTE();
            /* a few primitives are so common, and so easy, that
               they deserve special treatment */
            switch (i) {
            case 5:		/* set watch */
                watching = !watching;
                returnedObject = watching ? trueobj : falseobj;
                break;

            case 11:		/* class of object */
                returnedObject = getClass(*primargs);
                break;
            case 21:		/* object equality test */
                if (*primargs == *(primargs + 1)) {
                    returnedObject = trueobj;
                } else {
                    returnedObject = falseobj;
                }
                break;
            case 25:		/* basicAt: */
                j = intValue(*(primargs + 1));
                returnedObject = basicAt(*primargs, j);
                break;
            case 31:		/* basicAt:Put: */
                j = intValue(*(primargs + 1));
                fieldAtPut(*primargs, j, *(primargs + 2));
                returnedObject = nilobj;
                break;
            case 53:		/* set time slice */
                timeSliceCounter = intValue(*primargs);
                returnedObject = nilobj;
                break;
            case 58:		/* allocObject */
                j = intValue(*primargs);
                returnedObject = allocObject(j);
                break;
            case 87:		/* value of symbol */
                returnedObject = globalSymbol(charPtr(*primargs));
                break;
            default:
                returnedObject = primitive(i, primargs);
                break;
            }
            /* increment returned object in case pop would destroy it */
            incr(returnedObject);
            /* pop off arguments */
            while (low-- > 0) {
                STACKTOP_FREE();
            }
            /* returned object has already been incremented */
            IPUSH(returnedObject);
            decr(returnedObject);
            break;

doReturn:
            {
                object lp = basicAt(processStack, linkPointer);

                returnPoint = intValue(basicAt(processStack, linkPointer + 2));
                while (PROCESS_STACK_TOP() >= returnPoint) {
                    STACKTOP_FREE();
                }
                
                /* returned object has already been incremented */
                IPUSH(returnedObject);
                decr(returnedObject);
                
                /* now go restart old routine */
                if (lp != nilobj) {
                    linkPointer = intValue(lp);
                    goto readLinkageBlock;
                } else {
                    linkPointer = 0;    // Redundant?
                    return FALSE; /* all done */
                }
            }

        case BC_DoSpecial:
            switch (low) {
            case SBC_SelfReturn:
                incr(returnedObject = ARGUMENTS_AT(0));
                goto doReturn;

            case SBC_StackReturn:
                IPOP(returnedObject);
                goto doReturn;

            case SBC_Duplicate:
                /* avoid possible subtle bug */
                returnedObject = *stackTop;
                IPUSH(returnedObject);
                break;

            case SBC_PopTop:
                IPOP(returnedObject);
                decr(returnedObject);
                break;

            case SBC_Branch:
                /* avoid a subtle bug here */
                i = NEXT_BYTE();
                byteOffset = i;
                break;

            case SBC_BranchIfTrue:
                IPOP(returnedObject);
                i = NEXT_BYTE();
                if (returnedObject == trueobj) {
                    /* leave nil on stack */
                    stackTop++;
                    byteOffset = i;
                }
                decr(returnedObject);
                break;

            case SBC_BranchIfFalse:
                IPOP(returnedObject);
                i = NEXT_BYTE();
                if (returnedObject == falseobj) {
                    /* leave nil on stack */
                    stackTop++;
                    byteOffset = i;
                }
                decr(returnedObject);
                break;

            case SBC_AndBranch:
                IPOP(returnedObject);
                i = NEXT_BYTE();
                if (returnedObject == falseobj) {
                    IPUSH(returnedObject);
                    byteOffset = i;
                }
                decr(returnedObject);
                break;

            case SBC_OrBranch:
                IPOP(returnedObject);
                i = NEXT_BYTE();
                if (returnedObject == trueobj) {
                    IPUSH(returnedObject);
                    byteOffset = i;
                }
                decr(returnedObject);
                break;

            case SBC_SendToSuper:
                i = NEXT_BYTE();
                messageToSend = LITERALS_AT(i);
                rcv = sysMemPtr(ARGUMENTS_AT(0));
                methodClass = basicAt(method, OFST_method_methodClass);
                /* if there is a superclass, use it
                   otherwise for class Object (the only
                   class that doesn't have a superclass) use
                   the class again */
                returnedObject = basicAt(methodClass, OFST_class_superClass);
                if (returnedObject != nilobj) {
                    methodClass = returnedObject;
                }
                goto doFindMessage;

            default:
                sysError("invalid doSpecial", "");
                break;
            }
            break;

        default:
            sysError("invalid bytecode", "");
            break;
        }
    }

    /* before returning we put back the values in the current process */
    /* object */

    fieldAtPut(processStack, linkPointer + 4, newInteger(byteOffset));
    fieldAtPut(aProcess, OFST_process_stackTop, newInteger(PROCESS_STACK_TOP()));
    fieldAtPut(aProcess, OFST_process_linkPtr, newInteger(linkPointer));

    return TRUE;

#   undef NEXT_BYTE
#   undef STACKTOP_PUT
#   undef STACKTOP_FREE
#   undef PROCESS_STACK_TOP
#   undef PROCESS_STACK_AT
#   undef RECEIVER_AT
#   undef RECEIVER_AT_PUT
#   undef ARGUMENTS_AT
#   undef TEMPORARY_AT
#   undef TEMPORARY_AT_PUT
#   undef LITERALS_AT
}
