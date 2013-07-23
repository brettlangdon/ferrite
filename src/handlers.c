#include "handlers.h"

const char* VERSION = "0.0.1";
const char* STATS_FORMAT =
  "STAT connections %d\r\n"
  "STAT requests %d\r\n"
  "STAT hits %d\r\n"
  "STAT misses %d\r\n"
  "STAT hit_ratio %2.4f\r\n"
  "STAT backlog %d\r\n"
  "%s"
  "END\r\n";
const char* HELP_TEXT =
  "COMMANDS:\r\n"
  "  STATS - show statistics\r\n"
  "  GET <KEY> - get <KEY> from the cache or empty when not present\r\n"
  "  DELETE <KEY> - remove the entry from the cache if it exists\r\n"
  "  VERSION - show the current application version\r\n"
  "  FLUSH_ALL - clear the cache\r\n"
  "  HELP - show this message\r\n"
  "  QUIT - disconnect from the server\r\n";

void handle_stats(KCLIST* tokens, FILE* client){
  char out[1024];
  int total = hits + misses;
  float hit_ratio = 0;
  if(hits){
    hit_ratio = (float)hits / (float)(hits + misses);
  }
  int size = queue_size(&requests);
  char* status = kcdbstatus(db);
  KCLIST* stats = kclistnew();
  tokenize(stats, status, "\n");
  char status_buf[1024];
  strcpy(status_buf, "");
  int stat_count = kclistcount(stats);
  int i;
  for(i = 0; i < stat_count; ++i){
    size_t part_size;
    const char* part = kclistget(stats, i, &part_size);
    KCLIST* parts = kclistnew();
    tokenize(parts, (char*)part, "\t");
    char buf[128];
    if(kclistcount(parts) == 2){
      sprintf(buf, "STAT %s %s\r\n", kclistget(parts, 0, &part_size), kclistget(parts, 1, &part_size));
    }
    kclistdel(parts);
    strcat(status_buf, buf);
  }
  sprintf(out, STATS_FORMAT, connections, total, hits, misses, hit_ratio, size, status_buf);
  fputs(out, client);
}

void handle_version(KCLIST* tokens, FILE* client){
  char out[1024];
  sprintf(out, "VERSION %s\r\n", VERSION);
  fputs(out, client);
}

void handle_flush(KCLIST* tokens, FILE* client){
  if(kcdbclear(db)){
    fputs("OK\r\n", client);
  }
}

void handle_delete(KCLIST* tokens, FILE* client){
  if(kclistcount(tokens)){
    char* key;
    list_shift(tokens, &key);
    if(key == NULL){
      return;
    }
    if(kcdbremove(db, key, strlen(key))){
      fputs("DELETED\r\n", client);
    } else{
      fputs("NOT_FOUND\r\n", client);
    }
  } else{
    fputs("INVALID DELETE COMMAND: DELETE <KEY>\r\n", client);
  }
}

void handle_get(KCLIST* tokens, FILE* client){
  if(kclistcount(tokens)){
    char* key;
    list_shift(tokens, &key);
    if(key == NULL){
      return;
    }
    char out[1024];
    char* result_buffer;
    size_t result_size;
    result_buffer = kcdbget(db, key, strlen(key), &result_size);
    if(result_buffer){
      if(strcmp(result_buffer, "0") == 0){
	++misses;
	sprintf(out, "VALUE %s 0 0\r\n\r\nEND\r\n", key);
      } else{
	char* value = result_buffer + 11;
	char expiration[16];
	strncpy(expiration, result_buffer, 10);
	int exp = atoi(expiration);
	int now = (int)time(NULL);
	if(exp > 0 && exp < now){
	  ++misses;
	  value = "";
	  char reset_value[16];
	  sprintf(reset_value, "%10d:0", 0);
	  kcdbset(db, key, strlen(key), reset_value, strlen(reset_value));
	  queue_add(&requests, key);
	} else{
	  ++hits;
	}
	sprintf(out, "VALUE %s 0 %d\r\n%s\r\nEND\r\n", key, (int)strlen(value), value);
      }
      kcfree(result_buffer);
    } else{
      ++misses;
      sprintf(out, "VALUE %s 0 0\r\n\r\nEND\r\n", key);
      char value[16];
      int now = (int)time(NULL);
      sprintf(value, "%10d:0", now + 60);
      kcdbset(db, key, strlen(key), value, strlen(value));
      queue_add(&requests, key);
    }
    fputs(out, client);
    free(key);
  } else{
    fputs("INVALID GET COMMAND: GET <KEY>\r\n", client);
  }
}

void handle_help(KCLIST* tokens, FILE* client){
  fputs(HELP_TEXT, client);
}

int handle_command(char* buffer, FILE* client){
  int status = 0;
  KCLIST* tokens = kclistnew();
  tokenize(tokens, buffer, " ");
  char* command;
  list_shift(tokens, &command);
  if(command != NULL){
    if(strcmp(command, "get") == 0){
      handle_get(tokens, client);
    } else if(strcmp(command, "stats") == 0){
      handle_stats(tokens, client);
    } else if(strcmp(command, "delete") == 0){
      handle_delete(tokens, client);
    } else if(strcmp(command, "flush_all") == 0){
      handle_flush(tokens, client);
    } else if(strcmp(command, "version") == 0){
      handle_version(tokens, client);
    } else if(strcmp(command, "help") == 0){
      handle_help(tokens, client);
    } else if(strcmp(command, "quit") == 0){
      status = -1;
    } else{
      char out[1024];
      sprintf(out, "UNKNOWN COMMAND: %s\r\n", command);
      fputs(out, client);
    }
    free(command);
  }

  kclistdel(tokens);

  return status;
}
