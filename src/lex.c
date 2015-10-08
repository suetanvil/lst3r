/*
	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	lexical analysis routines for method parser
	should be called only by parser
*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "memory.h"
#include "lex.h"

//static TokenType nextTokenIn (struct TokenVal *val);

/* global variables returned by lexical analyser */

/* local variables used only by lexical analyser */
static char pushBuffer[10];	/* pushed back buffer */
static long longresult;		/* value used when building int tokens */

struct LexContext *
newLexer () {
    struct LexContext *ctx = ck_calloc(1, sizeof(struct LexContext));
    
    lexinit(ctx, NULL);
    
    return ctx;
}

void
freeLexer(struct LexContext *ctx) {
    if (ctx){
        free(ctx);
    }
}

/* lexinit - initialize the lexical analysis routines */
void
lexinit (struct LexContext *ctx, char *str) {
    ctx->pushindex = 0;
    ctx->cc = 0;
    ctx->cp = str;

    ctx->token = TOK_NOTHING;
    ctx->tokenString[0] = 0;
    ctx->tokenInteger = 0;
    ctx->tokenFloat = 0.0;
}// lexinit 

/* pushBack - push one character back into the input */
static void
pushBack (struct LexContext *ctx, int c) {
    pushBuffer[ctx->pushindex++] = c;
}

/* nextChar - retrieve the next char, from buffer or input */
static char
nextChar (struct LexContext *ctx) {
    if (ctx->pushindex > 0) {
        ctx->cc = pushBuffer[--(ctx->pushindex)];
    } else if (*(ctx->cp)) {
        ctx->cc = *(ctx->cp)++;
    } else {
        ctx->cc = '\0';
    }
    return ctx->cc;
}

/* peek - take a peek at the next character */
char
peek (struct LexContext *ctx) {
    pushBack(ctx, nextChar(ctx));
    return (ctx->cc);
}

/* isClosing - characters which can close an expression */
static boolean
isClosing (int c) {
    switch (c) {
    case '.':
    case ']':
    case ')':
    case ';':
    case '\"':
    case '\'':
        return (TRUE);
    }
    return (FALSE);
}

/* isSymbolChar - characters which can be part of symbols */
static boolean
isSymbolChar (int c) {
    if (isdigit(c) || isalpha(c)) {
        return TRUE;
    }
    if (isspace(c) || isClosing(c)) {
        return FALSE;
    }
    return TRUE;
}

/* singleBinary - binary characters that cannot be continued */
static boolean
singleBinary (int c) {
    switch (c) {
    case '[':
    case '(':
    case ')':
    case ']':
        return (TRUE);
    }
    return (FALSE);
}

/* binarySecond - return TRUE if char can be second char in binary symbol */
static boolean
binarySecond (int c) {
    if (isalpha(c) || isdigit(c) || isspace(c) || isClosing(c) ||
            singleBinary(c)) {
        return (FALSE);
    }
    return (TRUE);
}

TokenType
nextToken (struct LexContext *ctx) {
    char *tp;
    boolean sign;

    /* skip over blanks and comments */
    while (nextChar(ctx) && (isspace(ctx->cc) || (ctx->cc == '"')))
        if (ctx->cc == '"') {
            /* read comment */
            while (nextChar(ctx) && (ctx->cc != '"'));
            if (!ctx->cc) {
                break;    /* break if we run into eof */
            }
        }

    tp = ctx->tokenString;
    *tp++ = ctx->cc;

    if (!ctx->cc) {		/* end of input */
        ctx->token = TOK_INPUTEND;
    }

    else if (isalpha(ctx->cc)) {	/* identifier */
        while (nextChar(ctx) && isalnum(ctx->cc)) {
            *tp++ = ctx->cc;
        }
        if (ctx->cc == ':') {
            *tp++ = ctx->cc;
            ctx->token = TOK_NAMECOLON;
        } else {
            pushBack(ctx, ctx->cc);
            ctx->token = TOK_NAMECONST;
        }
    }

    else if (isdigit(ctx->cc)) {	/* number */
        longresult = ctx->cc - '0';
        while (nextChar(ctx) && isdigit(ctx->cc)) {
            *tp++ = ctx->cc;
            longresult = (longresult * 10) + (ctx->cc - '0');
        }
        if (longCanBeInt(longresult)) {
            ctx->tokenInteger = longresult;
            ctx->token = TOK_INTCONST;
        } else {
            ctx->token = TOK_FLOATCONST;
            ctx->tokenFloat = (double) longresult;
        }
        if (ctx->cc == '.') {	/* possible float */
            if (nextChar(ctx) && isdigit(ctx->cc)) {
                *tp++ = '.';
                do {
                    *tp++ = ctx->cc;
                } while (nextChar(ctx) && isdigit(ctx->cc));
                if (ctx->cc) {
                    pushBack(ctx, ctx->cc);
                }
                ctx->token = TOK_FLOATCONST;
                *tp = '\0';
                ctx->tokenFloat = atof(ctx->tokenString);
            } else {
                /* nope, just an ordinary period */
                if (ctx->cc) {
                    pushBack(ctx, ctx->cc);
                }
                pushBack(ctx, '.');
            }
        } else {
            pushBack(ctx, ctx->cc);
        }

        if (nextChar(ctx) && ctx->cc == 'e') {	/* possible float */
            if (nextChar(ctx) && ctx->cc == '-') {
                sign = TRUE;
                nextChar(ctx);
            } else {
                sign = FALSE;
            }
            if (ctx->cc && isdigit(ctx->cc)) {	/* yep, its a float */
                *tp++ = 'e';
                if (sign) {
                    *tp++ = '-';
                }
                while (ctx->cc && isdigit(ctx->cc)) {
                    *tp++ = ctx->cc;
                    nextChar(ctx);
                }
                if (ctx->cc) {
                    pushBack(ctx, ctx->cc);
                }
                *tp = '\0';
                ctx->token = TOK_FLOATCONST;
                ctx->tokenFloat = atof(ctx->tokenString);
            } else {		/* nope, wrong again */
                if (ctx->cc) {
                    pushBack(ctx, ctx->cc);
                }
                if (sign) {
                    pushBack(ctx, '-');
                }
                pushBack(ctx, 'e');
            }
        } else if (ctx->cc) {
            pushBack(ctx, ctx->cc);
        }
    }

    else if (ctx->cc == '$') {	/* character constant */
        ctx->tokenInteger = (int) nextChar(ctx);
        ctx->token = TOK_CHARCONST;
    }

    else if (ctx->cc == '#') {	/* symbol */
        tp--;			/* erase pound sign */
        if (nextChar(ctx) == '(') {
            ctx->token = TOK_ARRAYBEGIN;
        } else {
            pushBack(ctx, ctx->cc);
            while (nextChar(ctx) && isSymbolChar(ctx->cc)) {
                *tp++ = ctx->cc;
            }
            pushBack(ctx, ctx->cc);
            ctx->token = TOK_SYMCONST;
        }
    }

    else if (ctx->cc == '\'') {	/* string constant */
        tp--;			/* erase pound sign */
strloop:
        while (nextChar(ctx) && (ctx->cc != '\'')) {
            *tp++ = ctx->cc;
        }
        /* check for nested quote marks */
        if (ctx->cc && nextChar(ctx) && (ctx->cc == '\'')) {
            *tp++ = ctx->cc;
            goto strloop;
        }
        pushBack(ctx, ctx->cc);
        ctx->token = TOK_STRCONST;
    }

    else if (isClosing(ctx->cc)) {	/* closing expressions */
        ctx->token = TOK_CLOSING;
    }

    else if (singleBinary(ctx->cc)) {	/* single binary expressions */
        ctx->token = TOK_BINARY;
    }

    else {			/* anything else is binary */
        if (nextChar(ctx) && binarySecond(ctx->cc)) {
            *tp++ = ctx->cc;
        } else {
            pushBack(ctx, ctx->cc);
        }
        ctx->token = TOK_BINARY;
    }

    *tp = '\0';
    return ctx->token;
}
