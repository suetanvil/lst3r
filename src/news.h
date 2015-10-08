#ifndef __NEWS_H
#define __NEWS_H

object getClass(object obj);
object newArray(int size);
object newBlock(void);
object newByteArray(int size);
object newChar(int value);
object newClass(char *name);
object copyFrom(object obj, int start, int size);
object newContext(int link, object method, object args, object temp);
object newDictionary(void);
object newFloat(double d);
double floatValue(object o);
object newLink(object key, object value);
object newMethod(void);
object newStString(char *value);
object newSymbol(char *str);

#endif
