#ifndef FERRITE_UTIL
#define FERRITE_UTIL

#include <kclangc.h>

void list_shift(KCLIST* list, char** next);
void lower(char* word);
void tokenize(KCLIST* tokens, char* base, char* delimiter);
#endif
