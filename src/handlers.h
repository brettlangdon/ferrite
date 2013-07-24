#ifndef FERRITE_HANDLERS
#define FERRITE_HANDLERS
#include <kclangc.h>
#include <time.h>
#include "common.h"
#include "queue.h"

void handle_stats(KCLIST* tokens, FILE* client);
void handle_version(KCLIST* tokens, FILE* client);
void handle_flush(KCLIST* tokens, FILE* client);
void handle_delete(KCLIST* tokens, FILE* client);
void handle_get(KCLIST* tokens, FILE* client);
void handle_help(KCLIST* tokens, FILE* client);
int handle_command(char* buffer, FILE* client);
#endif
