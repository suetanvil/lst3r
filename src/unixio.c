/*
	Little Smalltalk, version 2

	Unix specific input and output routines
	written by tim budd, January 1988
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "names.h"
#include "tty.h"
#include "news.h"
#include "filein.h"

/* Wrappers around fread and fwrite that catch and report IO errors in
 * a Smalltalk-ish way. */
int
fread_chk(FILE * fp, char *p, int s) {
    int r;

    r = fread(p, s, 1, fp);
    if (r && (r != 1)) {
        sysError("imageRead count error", "");
    }
    return r;
}

void
fwrite_chk(FILE * fp, char *p, int s) {
    if (fwrite(p, s, 1, fp) != 1) {
        sysError("imageWrite size error", "");
    }
}



/* i/o primitives - necessarily rather UNIX dependent;
	basically, files are all kept in a large array.
	File operations then just give an index into this array
*/
#define MAXFILES 20
/* we assume this is initialized to NULL */
static FILE *fp[MAXFILES];

object ioPrimitive(int number, object * arguments) {
    int i, j;
    char *p, buffer[1024];
    object returnedObject;

    returnedObject = nilobj;

    i = intValue(arguments[0]);

    switch (number) {
    case 0:			/* file open */
        i = intValue(arguments[0]);
        p = charPtr(arguments[1]);

        if (streq(p, "stdin")) {
            fp[i] = stdin;
        } else if (streq(p, "stdout")) {
            fp[i] = stdout;
        } else if (streq(p, "stderr")) {
            fp[i] = stderr;
        } else {
            fp[i] = fopen(p, charPtr(arguments[2]));
        }
        if (fp[i] == NULL) {
            returnedObject = nilobj;
        } else {
            returnedObject = newInteger(i);
        }
        break;

    case 1:			/* file close - recover slot */
        if (fp[i]) {
            fclose(fp[i]);
        }
        fp[i] = NULL;
        break;

    case 2:			/* file size */
    case 3:			/* file in */
        if (fp[i]) {
            fileIn(fp[i], TRUE);
        }
        break;

    case 4:			/* get character */
        sysError("file operation not implemented yet", "");

    case 5:			/* get string */
        if (!fp[i]) {
            break;
        }
        j = 0;
        buffer[j] = '\0';
        while (1) {
            if (fgets(&buffer[j], 512, fp[i]) == NULL) {
                return (nilobj);    /* end of file */
            }
            if (fp[i] == stdin) {
                /* delete the newline */
                j = strlen(buffer);
                if (buffer[j - 1] == '\n') {
                    buffer[j - 1] = '\0';
                }
            }
            j = strlen(buffer) - 1;
            if (buffer[j] != '\\') {
                break;
            }
            /* else we loop again */
        }
        returnedObject = newStString(buffer);
        break;

    case 7:			/* write an object image */
        if (fp[i]) {
            imageWrite(fp[i]);
        }
        returnedObject = trueobj;
        break;

    case 8:			/* print no return */
    case 9:			/* print string */
        if (!fp[i]) {
            break;
        }
        fputs(charPtr(arguments[1]), fp[i]);
        if (number == 8) {
            fflush(fp[i]);
        } else {
            fputc('\n', fp[i]);
        }
        break;

    default:
        sysError("unknown primitive", "filePrimitive");
    }

    return (returnedObject);
}
