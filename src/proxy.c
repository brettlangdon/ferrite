#include "proxy.h"

size_t curl_write(char* ptr, size_t size, size_t nmemb, CURL_RESULT* result){
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
  char* base_url = (char*)arg;
  CURL* curl = curl_easy_init();
  if(!curl){
    fprintf(stderr, "Could not initialize curl\n");
    return NULL;
  }
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);

  while(1){
    char* next;
    queue_get(&requests, &next);
    int url_size = strlen(base_url) + strlen(next) + 1;
    char* url = (char*)malloc(url_size * sizeof(char));
    sprintf(url, "%s%s", base_url, next);

    CURL_RESULT* result = malloc(sizeof(CURL_RESULT));
    result->data = malloc(sizeof(char));
    result->data[0] = 0;
    result->size = 0;
    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, result);
    res = curl_easy_perform(curl);
    kcdbset(db, next, strlen(next), result->data, strlen(result->data));

    if(url != NULL){
      free(url);
    }
    if(next != NULL){
      free(next);
    }
    free(result->data);
    free(result);
  }
}
