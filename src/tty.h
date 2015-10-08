#ifndef __TTY_H
#define __TTY_H

extern void sysError(char *s1, char *s2);
extern void sysWarn(char *s1, char *s2);
extern void compilWarn(char *selector, char *str1, char *str2);
extern void compilErrorMsg(char *selector, char *str1, char *str2);
extern void dspMethod(char *cp, char *mp);
extern void givepause(void);
extern object sysPrimitive(int number, object *arguments);

#endif
