#include "util.h"

void list_shift(KCLIST* list, char** next){
  size_t size;
  const char* results = kclistget(list, 0, &size);
  *next = malloc((size * sizeof(char)) + 1);
  strcpy(*next, results);
  (*next)[size] = 0;
  kclistshift(list);
}


void lower(char* word){
  int length = strlen(word);
  int i;
  for(i = 0; i < length; ++i){
    word[i] = tolower(word[i]);
  }
}

void tokenize(KCLIST* tokens, char* base, char* delimiter){
  char* remainder;
  char* token;
  char* ptr = base;
  while((token = strtok_r(ptr, delimiter, &remainder))){
    int last;
    int len = strlen(token);
    for(last = len - 1; last >= 0; --last){
      if(token[last] == '\n' || token[last] == '\r'){
	token[last] = 0;
	break;
      }
    }
    lower(token);
    char new_token[1024];
    strcpy(new_token, token);
    new_token[strlen(token)] = 0;
    kclistpush(tokens, new_token, strlen(new_token) + 1);
    ptr = remainder;
  }
  free(token);
}
