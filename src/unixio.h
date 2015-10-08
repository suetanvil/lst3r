
#ifndef __UNIXIO_H
#define __UNIXIO_H

extern void imageRead(FILE *fp);
extern void imageWrite(FILE *fp);
extern object ioPrimitive(int number, object *arguments);
extern int fread_chk(FILE * fp, char *p, int s);
extern void fwrite_chk(FILE * fp, char *p, int s);

#endif
