/*
	Little Smalltalk, version 3
	Main Driver
	written By Tim Budd, September 1988
	Oregon State University
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "names.h"
#include "tty.h"
#include "unixio.h"
#include "interp.h"



static const char *
getScript(int *argc, char **argv) {
    const char *script = NULL;

    int dest = 0;
    for (int src = 0; src < *argc; src++) {
        if (streq("-e", argv[src])) {
            if (src + 1 >= *argc) {
                sysError("Trailing '-p' argument.", "");
                exit(1);
            }

            src++;
            script = argv[src];
            continue;
        }

        argv[dest] = argv[src];
        ++dest;
    }

    *argc = dest;
    return script;
}



static void
run() {
    object firstProcess;

    firstProcess = globalSymbol("systemProcess");
    if (firstProcess == nilobj) {
        sysError("no initial process", "in image");
        exit(1);
        return;
    }

    while (execute(firstProcess, 15000)) {
        // ...
    }

    /* exit and return - belt and suspenders, but it keeps lint happy */
    exit(0);
}// run


int
main(int argc, char **argv) {
    FILE *fp;
    char *p, buffer[120];

    initMemoryManager();

    strcpy(buffer, "systemImage");
    p = buffer;

    // Look for a -e argument.
    const char *script = getScript(&argc, argv);

    if (argc != 1) {
        p = argv[1];
    }

    fp = fopen(p, "rb");

    if (fp == NULL) {
#if 0   // Image format isn't stable enough for this
        strcpy(buffer, "/usr/share/lst3/systemImage");
        fp = fopen(p, "r");
#endif
        if (fp == NULL) {
            sysError("cannot open image", p);
            exit(1);
        }
    }

    imageRead(fp);
    initCommonSymbols();

    printf("Little Smalltalk, Version 3.1(r)\n");
    printf("Written by Tim Budd (Oregon State University) and contributers.\n");

    // Store the script in global variable 'launchscript'.  The actual
    // execution will happen in Smalltalk code in Scheduler.
    object launchscriptRef = globalSymbol("launchscript");
    if (script && launchscriptRef) {
        setStringValue(launchscriptRef, script);
    }

    run();

    return 0;
}
