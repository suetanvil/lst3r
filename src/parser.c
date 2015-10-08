/*
	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Method parser - parses the textual description of a method,
	generating bytecodes and literals.

	This parser is based around a simple minded recursive descent
	parser.
	It is used both by the module that builds the initial virtual image,
	and by a primitive when invoked from a running Smalltalk system.

	The latter case could, if the bytecode interpreter were fast enough,
	be replaced by a parser written in Smalltalk.  This would be preferable,
	but not if it slowed down the system too terribly.

	To use the parser the routine setInstanceVariables must first be
	called with a class object.  This places the appropriate instance
	variables into the memory buffers, so that references to them
	can be correctly encoded.

	As this is recursive descent, you should read it SDRAWKCAB !
		(from bottom to top)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"
#include "memory.h"
#include "names.h"
#include "interp.h"
#include "lex.h"
#include "news.h"
#include "tty.h"

#include "parser.h"

/* all of the following limits could be increased (up to
   256) without any trouble.  They are kept low
   to keep memory utilization down */
enum ParserConsts {
    CODE_LIMIT       = 256,  /* maximum number of bytecodes permitted */
    LITERAL_LIMIT    = 128,  /* maximum number of literals permitted */
    TEMPORARY_LIMIT  = 32,   /* maximum number of temporaries permitted */
    ARGUMENT_LIMIT   = 32,   /* maximum number of arguments permitted */
    INSTANCE_LIMIT   = 32,   /* maximum number of instance vars permitted */
};

static boolean parseok;		/* parse still ok? */

static int codeTop;			/* top position filled in code array */
static byte codeArray[CODE_LIMIT];	/* bytecode array */
static int literalTop;			/*  ... etc. */
static object literalArray[LITERAL_LIMIT];
static int temporaryTop;
static char *temporaryName[TEMPORARY_LIMIT];
static int argumentTop;
static char *argumentName[ARGUMENT_LIMIT];
static int instanceTop;
static char *instanceName[INSTANCE_LIMIT];

static int maxTemporary;		/* highest temporary see so far */
static char selector[80];		/* message selector */

enum blockstatus { NotInBlock, InBlock, OptimizedBlock } blockstat;

static void block(struct LexContext *ctx);
static void body(struct LexContext *ctx);
static void assignment(struct LexContext *ctx, char *name);
static void genMessage(boolean toSuper, int argumentCount, object messagesym);
static void genInteger(int val);
static boolean nameTerm(char *name);
static int parseArray(struct LexContext *ctx);
static boolean unaryContinuation(struct LexContext *ctx,boolean superReceiver);
static boolean binaryContinuation(struct LexContext *ctx,boolean superReceiver);
static void statement(struct LexContext *ctx);
static void messagePattern (struct LexContext *ctx);
static void temporaries (struct LexContext *ctx);
static boolean term(struct LexContext *ctx);
static void parsePrimitive (struct LexContext *ctx);
static void expression (struct LexContext *ctx);


static void
compilError(char *selector, char *str1, char *str2) {
    compilErrorMsg(selector, str1, str2);
    parseok = FALSE;
}

void
setInstanceVariables (object aClass) {
    int i, limit;
    object vars;

    if (aClass == nilobj) {
        instanceTop = 0;
    } else {
        setInstanceVariables(basicAt(aClass, OFST_class_superClass));
        vars = basicAt(aClass, OFST_class_variables);
        if (vars != nilobj) {
            limit = sizeField(vars);
            for (i = 1; i <= limit; i++) {
                instanceName[++instanceTop] = charPtr(basicAt(vars, i));
            }
        }
    }
}

static void
genCode (int value) {
    if (codeTop >= CODE_LIMIT) {
        compilError(selector, "too many bytecode instructions in method",
                    "");
    }  else {
        codeArray[codeTop++] = value;
    }
}

static void
genInstruction (enum ByteCodes high, int low) {
    if (low >= 16) {
        genInstruction(BC_Extended, high);
        genCode(low);
    } else {
        genCode(high * 16 + low);
    }
}

// Wrapper around genInstruction to enforce the type of special
// bytecode.
static inline void genDoSpecial(enum SpecialByteCodes sbc) {
    genInstruction(BC_DoSpecial, sbc);
}


static int
genLiteral (object aLiteral) {
    if (literalTop >= LITERAL_LIMIT) {
        compilError(selector, "too many literals in method", "");
    } else {
        literalArray[++literalTop] = aLiteral;
        incr(aLiteral);
    }
    return (literalTop - 1);
}

static void
genInteger (		/* generate an integer push */
    int val
) {
    if (val == -1) {
        genInstruction(BC_PushConstant, CC_minusOne);
    } else if ((val >= 0) && (val <= 2)) {
        genInstruction(BC_PushConstant, val);
    } else {
        genInstruction(BC_PushLiteral, genLiteral(newInteger(val)));
    }
}

static char *glbsyms[] = { "currentInterpreter", "nil", "true", "false",
                           NULL,
};

static boolean
nameTerm (char *name) {
    int i;
    boolean done = FALSE;
    boolean isSuper = FALSE;

    /* it might be self or super */
    if (streq(name, "self") || streq(name, "super")) {
        genInstruction(BC_PushArgument, 0);
        done = TRUE;
        if (streq(name, "super")) {
            isSuper = TRUE;
        }
    }

    /* or it might be a temporary (reverse this to get most recent first) */
    if (!done)
        for (i = temporaryTop; (!done) && (i >= 1); i--)
            if (streq(name, temporaryName[i])) {
                genInstruction(BC_PushTemporary, i - 1);
                done = TRUE;
            }

    /* or it might be an argument */
    if (!done)
        for (i = 1; (!done) && (i <= argumentTop); i++)
            if (streq(name, argumentName[i])) {
                genInstruction(BC_PushArgument, i);
                done = TRUE;
            }

    /* or it might be an instance variable */
    if (!done)
        for (i = 1; (!done) && (i <= instanceTop); i++) {
            if (streq(name, instanceName[i])) {
                genInstruction(BC_PushInstance, i - 1);
                done = TRUE;
            }
        }

    /* or it might be a global constant */
    if (!done)
        for (i = 0; (!done) && glbsyms[i]; i++)
            if (streq(name, glbsyms[i])) {
                genInstruction(BC_PushConstant, i + 4);
                done = TRUE;
            }

    /* not anything else, it must be a global */
    /* must look it up at run time */
    if (!done) {
        genInstruction(BC_PushLiteral, genLiteral(newSymbol(name)));
        genMessage(FALSE, 0, newSymbol("value"));
    }

    return (isSuper);
}

static int
parseArray (struct LexContext *ctx) {
    int i, size, base;
    object newLit, obj;

    base = literalTop;
    nextToken(ctx);
    while (parseok && (ctx->token != TOK_CLOSING)) {
        switch (ctx->token) {
        case TOK_ARRAYBEGIN:
            parseArray(ctx);
            break;

        case TOK_INTCONST:
            genLiteral(newInteger(ctx->tokenInteger));
            nextToken(ctx);
            break;

        case TOK_FLOATCONST:
            genLiteral(newFloat(ctx->tokenFloat));
            nextToken(ctx);
            break;

        case TOK_NAMECONST:
        case TOK_NAMECOLON:
        case TOK_SYMCONST:
            genLiteral(newSymbol(ctx->tokenString));
            nextToken(ctx);
            break;

        case TOK_BINARY:
            if (streq(ctx->tokenString, "(")) {
                parseArray(ctx);
                break;
            }
            if (streq(ctx->tokenString, "-") && isdigit(peek(ctx))) {
                nextToken(ctx);
                if (ctx->token == TOK_INTCONST) {
                    genLiteral(newInteger(-ctx->tokenInteger));
                } else if (ctx->token == TOK_FLOATCONST) {
                    genLiteral(newFloat(-ctx->tokenFloat));
                } else
                    compilError(selector, "negation not followed",
                                "by number");
                nextToken(ctx);
                break;
            }
            genLiteral(newSymbol(ctx->tokenString));
            nextToken(ctx);
            break;

        case TOK_CHARCONST:
            genLiteral(newChar(ctx->tokenInteger));
            nextToken(ctx);
            break;

        case TOK_STRCONST:
            genLiteral(newStString(ctx->tokenString));
            nextToken(ctx);
            break;

        default:
            compilError(selector, "illegal text in literal array",
                        ctx->tokenString);
            nextToken(ctx);
            break;
        }
    }

    if (parseok) {
        if (!streq(ctx->tokenString, ")"))
            compilError(selector,
                        "array not terminated by right parenthesis",
                        ctx->tokenString);
        else {
            nextToken(ctx);
        }
    }
    size = literalTop - base;
    newLit = newArray(size);
    for (i = size; i >= 1; i--) {
        obj = literalArray[literalTop];
        basicAtPut(newLit, i, obj);
        decr(obj);
        literalArray[literalTop] = nilobj;
        literalTop = literalTop - 1;
    }
    return (genLiteral(newLit));
}

static boolean
term (struct LexContext *ctx) {
    boolean superTerm = FALSE;	/* TRUE if term is pseudo var super */

    if (ctx->token == TOK_NAMECONST) {
        superTerm = nameTerm(ctx->tokenString);
        nextToken(ctx);
    } else if (ctx->token == TOK_INTCONST) {
        genInteger(ctx->tokenInteger);
        nextToken(ctx);
    } else if (ctx->token == TOK_FLOATCONST) {
        genInstruction(BC_PushLiteral, genLiteral(newFloat(ctx->tokenFloat)));
        nextToken(ctx);
    } else if ((ctx->token == TOK_BINARY) && streq(ctx->tokenString, "-")) {
        nextToken(ctx);
        if (ctx->token == TOK_INTCONST) {
            genInteger(-ctx->tokenInteger);
        } else if (ctx->token == TOK_FLOATCONST) {
            genInstruction(BC_PushLiteral, genLiteral(newFloat(-ctx->tokenFloat)));
        } else {
            compilError(selector, "negation not followed", "by number");
        }
        nextToken(ctx);
    } else if (ctx->token == TOK_CHARCONST) {
        genInstruction(BC_PushLiteral, genLiteral(newChar(ctx->tokenInteger)));
        nextToken(ctx);
    } else if (ctx->token == TOK_SYMCONST) {
        genInstruction(BC_PushLiteral, genLiteral(newSymbol(ctx->tokenString)));
        nextToken(ctx);
    } else if (ctx->token == TOK_STRCONST) {
        genInstruction(BC_PushLiteral, genLiteral(newStString(ctx->tokenString)));
        nextToken(ctx);
    } else if (ctx->token == TOK_ARRAYBEGIN) {
        genInstruction(BC_PushLiteral, parseArray(ctx));
    } else if ((ctx->token == TOK_BINARY) && streq(ctx->tokenString, "(")) {
        nextToken(ctx);
        expression(ctx);
        if (parseok) {
            if ((ctx->token != TOK_CLOSING) || !streq(ctx->tokenString, ")")) {
                compilError(selector, "Missing Right Parenthesis", "");
            } else {
                nextToken(ctx);
            }
        }
    } else if ((ctx->token == TOK_BINARY) && streq(ctx->tokenString, "<")) {
        parsePrimitive(ctx);
    } else if ((ctx->token == TOK_BINARY) && streq(ctx->tokenString, "[")) {
        block(ctx);
    } else {
        compilError(selector, "invalid expression start", ctx->tokenString);
    }

    return superTerm;
}// term 

static void
parsePrimitive (struct LexContext *ctx) {
    int primitiveNumber, argumentCount;

    if (nextToken(ctx) != TOK_INTCONST) {
        compilError(selector, "primitive number missing", "");
    }
    primitiveNumber = ctx->tokenInteger;
    nextToken(ctx);
    argumentCount = 0;
    while (parseok && !((ctx->token == TOK_BINARY) && streq(ctx->tokenString, ">"))) {
        term(ctx);
        argumentCount++;
    }
    genInstruction(BC_DoPrimitive, argumentCount);
    genCode(primitiveNumber);
    nextToken(ctx);
}

static void
genMessage (boolean toSuper, int argumentCount, object messagesym) {
    boolean sent = FALSE;
    int i;

    if (!toSuper && argumentCount == 0) {
        for (i = 0; !sent && unSyms[i]; i++) {
            if (messagesym == unSyms[i]) {
                genInstruction(BC_SendUnary, i);
                sent = TRUE;
            }// if
        }// for
    }// if
    
    if (!toSuper && argumentCount == 1) {
        for (i = 0; !sent && binSyms[i]; i++) {
            if (messagesym == binSyms[i]) {
                genInstruction(BC_SendBinary, i);
                sent = TRUE;
            }// if 
        }// for 
    }// if 
    
    if (!sent) {
        genInstruction(BC_MarkArguments, 1 + argumentCount);
        if (toSuper) {
            genDoSpecial(SBC_SendToSuper);
            genCode(genLiteral(messagesym));
        } else {
            genInstruction(BC_SendMessage, genLiteral(messagesym));
        }// if .. else
    }// if 
}

static boolean
unaryContinuation (struct LexContext *ctx, boolean superReceiver) {
    int i;
    boolean sent;

    while (parseok && (ctx->token == TOK_NAMECONST)) {
        /* first check to see if it could be a temp by mistake */
        for (i = 1; i < temporaryTop; i++)
            if (streq(ctx->tokenString, temporaryName[i]))
                compilWarn(selector, "message same as temporary:",
                           ctx->tokenString);
        for (i = 1; i < argumentTop; i++)
            if (streq(ctx->tokenString, argumentName[i]))
                compilWarn(selector, "message same as argument:",
                           ctx->tokenString);
        /* the next generates too many spurious messages */
        /* for (i=1; i < instanceTop; i++)
           if (streq(ctx->tokenString, instanceName[i]))
           compilWarn(selector,"message same as instance",
           ctx->tokenString); */

        sent = FALSE;

        if (!sent) {
            genMessage(superReceiver, 0, newSymbol(ctx->tokenString));
        }
        /* once a message is sent to super, reciever is not super */
        superReceiver = FALSE;
        nextToken(ctx);
    }
    return (superReceiver);
}

static boolean
binaryContinuation (struct LexContext *ctx, boolean superReceiver) {
    boolean superTerm;
    object messagesym;

    superReceiver = unaryContinuation(ctx, superReceiver);
    while (parseok && (ctx->token == TOK_BINARY)) {
        messagesym = newSymbol(ctx->tokenString);
        nextToken(ctx);
        superTerm = term(ctx);
        unaryContinuation(ctx, superTerm);
        genMessage(superReceiver, 1, messagesym);
        superReceiver = FALSE;
    }
    return (superReceiver);
}

static int
optimizeBlock (struct LexContext *ctx, int instruction, boolean dopop) {
    int location;
    enum blockstatus savebstat;

    savebstat = blockstat;
    genDoSpecial(instruction);
    location = codeTop;
    genCode(0);
    if (dopop) {
        genDoSpecial(SBC_PopTop);
    }
    nextToken(ctx);
    if (streq(ctx->tokenString, "[")) {
        nextToken(ctx);
        if (blockstat == NotInBlock) {
            blockstat = OptimizedBlock;
        }
        body(ctx);
        if (!streq(ctx->tokenString, "]")) {
            compilError(selector, "missing close", "after block");
        }
        nextToken(ctx);
    } else {
        binaryContinuation(ctx, term(ctx));
        genMessage(FALSE, 0, newSymbol("value"));
    }
    codeArray[location] = codeTop + 1;
    blockstat = savebstat;
    return (location);
}

static boolean
keyContinuation (struct LexContext *ctx, boolean superReceiver) {
    int i, j, argumentCount;
    boolean sent, superTerm;
    object messagesym;
    char pattern[80];

    superReceiver = binaryContinuation(ctx, superReceiver);
    if (ctx->token == TOK_NAMECOLON) {
        if (streq(ctx->tokenString, "ifTrue:")) {
            i = optimizeBlock(ctx, SBC_BranchIfFalse, FALSE);
            if (streq(ctx->tokenString, "ifFalse:")) {
                codeArray[i] = codeTop + 3;
                optimizeBlock(ctx, SBC_Branch, TRUE);
            }
        } else if (streq(ctx->tokenString, "ifFalse:")) {
            i = optimizeBlock(ctx, SBC_BranchIfTrue, FALSE);
            if (streq(ctx->tokenString, "ifTrue:")) {
                codeArray[i] = codeTop + 3;
                optimizeBlock(ctx, SBC_Branch, TRUE);
            }
        } else if (streq(ctx->tokenString, "whileTrue:")) {
            j = codeTop;
            genDoSpecial(SBC_Duplicate);
            genMessage(FALSE, 0, newSymbol("value"));
            i = optimizeBlock(ctx, SBC_BranchIfFalse, FALSE);
            genDoSpecial(SBC_PopTop);
            genDoSpecial(SBC_Branch);
            genCode(j + 1);
            codeArray[i] = codeTop + 1;
            genDoSpecial(SBC_PopTop);
        } else if (streq(ctx->tokenString, "and:")) {
            optimizeBlock(ctx, SBC_AndBranch, FALSE);
        } else if (streq(ctx->tokenString, "or:")) {
            optimizeBlock(ctx, SBC_OrBranch, FALSE);
        } else {
            pattern[0] = '\0';
            argumentCount = 0;
            while (parseok && (ctx->token == TOK_NAMECOLON)) {
                strcat(pattern, ctx->tokenString);
                argumentCount++;
                nextToken(ctx);
                superTerm = term(ctx);
                binaryContinuation(ctx, superTerm);
            }
            sent = FALSE;

            /* check for predefined messages */
            messagesym = newSymbol(pattern);

            if (!sent) {
                genMessage(superReceiver, argumentCount, messagesym);
            }
        }
        superReceiver = FALSE;
    }
    return (superReceiver);
}

static void
continuation (struct LexContext *ctx, boolean superReceiver) {
    superReceiver = keyContinuation(ctx, superReceiver);

    while (parseok && ctx->token==TOK_CLOSING && streq(ctx->tokenString,";")) {
        genDoSpecial(SBC_Duplicate);
        nextToken(ctx);
        keyContinuation(ctx, superReceiver);
        genDoSpecial(SBC_PopTop);
    }
}

static void
expression (struct LexContext *ctx) {
    boolean superTerm;
    char assignname[60];

    if (ctx->token == TOK_NAMECONST) {	/* possible assignment */
        strcpy(assignname, ctx->tokenString);
        nextToken(ctx);
        if ((ctx->token == TOK_BINARY) && streq(ctx->tokenString, "<-")) {
            nextToken(ctx);
            assignment(ctx, assignname);
        } else {		/* not an assignment after all */
            superTerm = nameTerm(assignname);
            continuation(ctx, superTerm);
        }
    } else {
        superTerm = term(ctx);
        if (parseok) {
            continuation(ctx, superTerm);
        }
    }
}

static void
assignment (struct LexContext *ctx, char *name) {
    int i;
    boolean done;

    done = FALSE;

    /* it might be a temporary */
    for (i = temporaryTop; (!done) && (i > 0); i--)
        if (streq(name, temporaryName[i])) {
            expression(ctx);
            genInstruction(BC_AssignTemporary, i - 1);
            done = TRUE;
        }

    /* or it might be an instance variable */
    for (i = 1; (!done) && (i <= instanceTop); i++)
        if (streq(name, instanceName[i])) {
            expression(ctx);
            genInstruction(BC_AssignInstance, i - 1);
            done = TRUE;
        }

    if (!done) {		/* not known, handle at run time */
        genInstruction(BC_PushArgument, 0);
        genInstruction(BC_PushLiteral, genLiteral(newSymbol(name)));
        expression(ctx);
        genMessage(FALSE, 2, newSymbol("assign:value:"));
    }
}

static void
statement (struct LexContext *ctx) {

    if ((ctx->token == TOK_BINARY) && streq(ctx->tokenString, "^")) {
        nextToken(ctx);
        expression(ctx);
        if (blockstat == InBlock) {
            /* change return point before returning */
            genInstruction(BC_PushConstant, CC_contextConst);
            genMessage(FALSE, 0, newSymbol("blockReturn"));
            genDoSpecial(SBC_PopTop);
        }
        genDoSpecial(SBC_StackReturn);
    } else {
        expression(ctx);
    }
}

static void
body (struct LexContext *ctx) {
    /* empty blocks are same as nil */
    if ( (blockstat == InBlock || blockstat == OptimizedBlock)
         &&
         (ctx->token == TOK_CLOSING && streq(ctx->tokenString, "]"))
        ) {
        genInstruction(BC_PushConstant, CC_nilConst);
        return;
    }// if 

    while (parseok) {
        statement(ctx);
        if (ctx->token == TOK_CLOSING) {
            if (streq(ctx->tokenString, ".")) {
                nextToken(ctx);
                if (ctx->token == TOK_INPUTEND) {
                    break;
                } else {	/* pop result, go to next statement */
                    genDoSpecial(SBC_PopTop);
                }
            } else {
                break;    /* leaving result on stack */
            }
        } else if (ctx->token == TOK_INPUTEND) {
            break;    /* leaving result on stack */
        } else {
            compilError(selector, "invalid statement ending; token is ",
                        ctx->tokenString);
        }
    }
}

static void
block (struct LexContext *ctx) {
    int saveTemporary, argumentCount, fixLocation;
    object tempsym, newBlk;
    enum blockstatus savebstat;

    saveTemporary = temporaryTop;
    savebstat = blockstat;
    argumentCount = 0;
    nextToken(ctx);
    if ((ctx->token == TOK_BINARY) && streq(ctx->tokenString, ":")) {
        while (parseok && (ctx->token == TOK_BINARY) && streq(ctx->tokenString, ":")) {
            if (nextToken(ctx) != TOK_NAMECONST)
                compilError(selector, "name must follow colon",
                            "in block argument list");
            if (++temporaryTop > maxTemporary) {
                maxTemporary = temporaryTop;
            }
            argumentCount++;
            if (temporaryTop > TEMPORARY_LIMIT)
                compilError(selector, "too many temporaries in method",
                            "");
            else {
                tempsym = newSymbol(ctx->tokenString);
                temporaryName[temporaryTop] = charPtr(tempsym);
            }
            nextToken(ctx);
        }
        if ((ctx->token != TOK_BINARY) || !streq(ctx->tokenString, "|"))
            compilError(selector, "block argument list must be terminated",
                        "by |");
        nextToken(ctx);
    }
    newBlk = newBlock();
    basicAtPut(newBlk, OFST_block_argumentCount, newInteger(argumentCount));
    basicAtPut(newBlk, OFST_block_argumentLocation,
               newInteger(saveTemporary + 1));
    genInstruction(BC_PushLiteral, genLiteral(newBlk));
    genInstruction(BC_PushConstant, CC_contextConst);
    genInstruction(BC_DoPrimitive, 2);
    genCode(29);
    genDoSpecial(SBC_Branch);
    fixLocation = codeTop;
    genCode(0);
    /*genDoSpecial(SBC_PopTop); */
    basicAtPut(newBlk, OFST_block_bytecountPosition, newInteger(codeTop + 1));
    blockstat = InBlock;
    body(ctx);
    if ((ctx->token == TOK_CLOSING) && streq(ctx->tokenString, "]")) {
        nextToken(ctx);
    } else {
        compilError(selector, "block not terminated by ]", "");
    }
    genDoSpecial(SBC_StackReturn);
    codeArray[fixLocation] = codeTop + 1;
    temporaryTop = saveTemporary;
    blockstat = savebstat;
}

static void
temporaries (struct LexContext *ctx) {
    object tempsym;

    temporaryTop = 0;
    if (ctx->token == TOK_BINARY && streq(ctx->tokenString, "|")) {
        nextToken(ctx);
        while (ctx->token == TOK_NAMECONST) {
            if (++temporaryTop > maxTemporary) {
                maxTemporary = temporaryTop;
            }
            if (temporaryTop > TEMPORARY_LIMIT)
                compilError(selector, "too many temporaries in method",
                            "");
            else {
                tempsym = newSymbol(ctx->tokenString);
                temporaryName[temporaryTop] = charPtr(tempsym);
            }
            nextToken(ctx);
        }
        if ((ctx->token != TOK_BINARY) || !streq(ctx->tokenString, "|"))
            compilError(selector, "temporary list not terminated by bar",
                        "");
        else {
            nextToken(ctx);
        }
    }
}

static void
messagePattern (struct LexContext *ctx) {
    object argsym;

    argumentTop = 0;
    strcpy(selector, ctx->tokenString);
    if (ctx->token == TOK_NAMECONST) {	/* unary message pattern */
        nextToken(ctx);
    } else if (ctx->token == TOK_BINARY) {		/* binary message pattern */
        nextToken(ctx);
        if (ctx->token != TOK_NAMECONST)
            compilError(selector,
                        "binary message pattern not followed by name",
                        selector);
        argsym = newSymbol(ctx->tokenString);
        argumentName[++argumentTop] = charPtr(argsym);
        nextToken(ctx);
    } else if (ctx->token == TOK_NAMECOLON) {	/* keyword message pattern */
        selector[0] = '\0';
        while (parseok && (ctx->token == TOK_NAMECOLON)) {
            strcat(selector, ctx->tokenString);
            nextToken(ctx);
            if (ctx->token != TOK_NAMECONST)
                compilError(selector, "keyword message pattern",
                            "not followed by a name");
            if (++argumentTop > ARGUMENT_LIMIT) {
                compilError(selector, "too many arguments in method", "");
            }
            argsym = newSymbol(ctx->tokenString);
            argumentName[argumentTop] = charPtr(argsym);
            nextToken(ctx);
        }
    } else {
        compilError(selector, "illegal message selector", ctx->tokenString);
    }
}

boolean
parse (object method, char *text, boolean savetext) {
    int i;
    object bytecodes, theLiterals;
    byte *bp;
    
    struct LexContext *ctx = newLexer();
    lexinit(ctx, text);
    nextToken(ctx);
    
    parseok = TRUE;
    blockstat = NotInBlock;
    codeTop = 0;
    literalTop = temporaryTop = argumentTop = 0;
    maxTemporary = 0;

    messagePattern(ctx);
    if (parseok) {
        temporaries(ctx);
    }
    if (parseok) {
        body(ctx);
    }
    if (parseok) {
        genDoSpecial(SBC_PopTop);
        genDoSpecial(SBC_SelfReturn);
    }

    if (!parseok) {
        basicAtPut(method, OFST_method_bytecodes, nilobj);
    } else {
        bytecodes = newByteArray(codeTop);
        bp = bytePtr(bytecodes);
        for (i = 0; i < codeTop; i++) {
            bp[i] = codeArray[i];
        }
        basicAtPut(method, OFST_method_message, newSymbol(selector));
        basicAtPut(method, OFST_method_bytecodes, bytecodes);
        if (literalTop > 0) {
            theLiterals = newArray(literalTop);
            for (i = 1; i <= literalTop; i++) {
                basicAtPut(theLiterals, i, literalArray[i]);
                decr(literalArray[i]);
            }
            basicAtPut(method, OFST_method_literals, theLiterals);
        } else {
            basicAtPut(method, OFST_method_literals, nilobj);
        }
        basicAtPut(method, OFST_method_stackSize, newInteger(6));
        basicAtPut(method, OFST_method_temporarySize,
                   newInteger(1 + maxTemporary));
        if (savetext) {
            basicAtPut(method, OFST_method_text, newStString(text));
        }
        
        freeLexer(ctx);
        return TRUE;
    }

    freeLexer(ctx);
    return FALSE;
}
