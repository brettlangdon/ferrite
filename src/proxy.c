#ifndef FAST_CACHE_PROXY
#define FAST_CACHE_PROXY
#include <curl/curl.h>

#include "common.c"
#include "queue.c"

struct curl_result {
  char* data;
  size_t size;
};

size_t curl_write(char* ptr, size_t size, size_t nmemb, struct curl_result* result){
  size_t realsize = size * nmemb;

  result->data= realloc(result->data, result->size + realsize + 1);
  if(result->data == NULL){
    fprintf(stderr, "Out of memory receiving curl results\n");
    return 0;
  }
  memcpy(&(result->data[result->size]), ptr, realsize);
  result->size += realsize;
  result->data[result->size] = 0;
  return realsize;
}

void* call_proxy(void* arg){
  char next[1024];
  while(1){
    queue_get(&requests, next);
    char* url = (char*)malloc(1024 * sizeof(char));
    sprintf(url, "http://127.0.0.1:8000/%s", next);
    CURL* curl = curl_easy_init();
    if(!curl){
      fprintf(stderr, "Could not initialize curl\n");
      return;
    }

    struct curl_result* result = malloc(sizeof(struct curl_result));
    result->data = malloc(sizeof(char));
    result->data[0] = 0;
    result->size = 0;
    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, result);
    res = curl_easy_perform(curl);
    kcdbset(db, next, strlen(next), result->data, strlen(result->data));

    curl_easy_cleanup(curl);
    free(url);
    free(result->data);
    free(result);
  }
}


#endif
