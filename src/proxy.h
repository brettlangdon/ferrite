#ifndef FERRITE_PROXY
#define FERRITE_PROXY

#include <curl/curl.h>
#include <time.h>
#include "common.h"
#include "queue.h"

typedef struct {
  char* data;
  size_t size;
} CURL_RESULT;

size_t curl_write(char* ptr, size_t size, size_t nmemb, CURL_RESULT* result);
void* call_proxy(void* arg);
#endif
