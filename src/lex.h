/*
	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987
*/

#ifndef __LEX_H
#define __LEX_H

/*
	values returned by the lexical analyzer
*/

typedef enum {
    TOK_NOTHING, TOK_NAMECONST, TOK_NAMECOLON,
    TOK_INTCONST, TOK_FLOATCONST, TOK_CHARCONST, TOK_SYMCONST,
    TOK_ARRAYBEGIN, TOK_STRCONST, TOK_BINARY, TOK_CLOSING, TOK_INPUTEND
} TokenType;

/* struct TokenVal { */
/*     char stringVal[80]; */
/*     int integerVal; */
/*     double floatVal; */
/* }; */


struct LexContext {
    int pushindex;          /* index of last pushed back char */
    char cc;                /* current character */
    char *cp;               /* character pointer */

    TokenType token;		/* token variety */
    char tokenString[TOKEN_MAX];	/* text of current token */
    int tokenInteger;       /* integer (or character) value of token */
    double tokenFloat;      /* floating point value of token */
    
};



extern char peek(struct LexContext *ctx);
extern TokenType nextToken(struct LexContext *ctx);
void lexinit (struct LexContext *ctx, char *str);
struct LexContext *newLexer (void);


//static inline void freeLexer(struct LexContext *ctx) { if (ctx){ free(ctx); } }
void freeLexer(struct LexContext *ctx);


#endif
