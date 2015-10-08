
#ifndef __PARSER_H__
#define __PARSER_H__

#include "interp.h"

extern void setInstanceVariables(object aClass);
extern boolean parse(object method, char *text, boolean savetext);

#endif
