#include <curl/curl.h>
#include <kclangc.h>
#include <signal.h>
#include <uv.h>

static uv_loop_t* loop = NULL;
static KCDB* db = NULL;
static sig_atomic_t cache_miss = 0;
static sig_atomic_t hits = 0;

const char* VERSION = "0.0.1";

struct curl_result {
  char* data;
  size_t size;
};


void lower(char* word){
  int length = strlen(word);
  int i;
  for(i = 0; i < length; ++i){
    word[i] = tolower(word[i]);
  }
}

static uv_buf_t alloc_buffer(uv_handle_t *handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = malloc(suggested_size);
  buf.len = suggested_size;
  return buf;
}

void handle_write(uv_write_t *req, int status) {
  if(status == -1){
    fprintf(stderr, "Write error %s\n", uv_err_name(uv_last_error(loop)));
  }

  char* base = (char*)req->data;
  free(base);
  free(req);
}

void get_tokens(char** tokens, char* base){
  size_t size = 0;
  char* remainder;
  char* token;
  char* ptr = base;
  while(token = strtok_r(ptr, " ", &remainder)){
    int last;
    for(last = 0; last < strlen(token); ++last){
      if(token[last] == '\n' || token[last] == '\r'){
	token[last] = 0;
	break;
      }
    }
    lower(token);
    tokens[size] = token;
    ++size;
    ptr = remainder;
  }
  free(token);
}

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

void after_call_proxy(uv_work_t* req, int status){
  free(req);
}

void call_proxy(uv_work_t* req){
  char* key = (char*)req->data;
  char* url = (char*)malloc(1024 * sizeof(char));
  sprintf(url, "http://127.0.0.1:8000/%s", key);
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
  kcdbset(db, key, strlen(key), result->data, strlen(result->data));

  curl_easy_cleanup(curl);
  free(url);
  free(result->data);
  free(result);
}

void handle_version(char* buffer){
  sprintf(buffer, "VERSION %s\r\n", VERSION);
}

void handle_flush_all(char* buffer){
  if(kcdbclear(db)){
    sprintf(buffer, "OK\r\n");
  } else{
    sprintf(buffer, "FAILED\r\n");
  }
}

void handle_stats(char* buffer){
  float ratio;
  if(!hits && !cache_miss){
    ratio = 0;
  } else{
    ratio = (float)hits / (hits + cache_miss);
  }
  int records = kcdbcount(db);
  char* format = "STAT cache_miss %d\r\nSTAT hits %d\r\nSTAT hit_ratio %2.2f\r\nSTAT records %d\r\nEND\r\n";
  sprintf(buffer, format, cache_miss, hits, ratio, records);
}

void handle_get(char* buffer, char** tokens){
  if(tokens[1]){
    char* result_buffer;
    char key[1024];
    strcpy(key, tokens[1]);
    size_t result_size, key_size;
    key_size = strlen(key);
    result_buffer = kcdbget(db, key, key_size, &result_size);

    int result = !!result_buffer;
    if(result == 0){
      ++cache_miss;
      uv_work_t* req = malloc(sizeof(uv_work_t));
      req->data = malloc(sizeof(key));
      strcpy(req->data, key);
      uv_queue_work(loop, req, call_proxy, after_call_proxy);
      result_buffer = "";
      kcdbset(db, key, strlen(key), "0", 1);
    } else if(strcmp(result_buffer, "0") == 0){
      ++cache_miss;
      result_buffer = "";
      result = 0;
    } else{
      ++hits;
    }

    sprintf(buffer, "VALUE %s 0 %lu\r\n%s\r\nEND\r\n", key, strlen(result_buffer), result_buffer);
    if(result){
      kcfree(result_buffer);
    }
  } else{
    sprintf(buffer, "END\r\n");
  }
}

void handle_read(uv_stream_t *client, ssize_t nread, uv_buf_t buf){
  if(nread == -1){
    if (uv_last_error(loop).code != UV_EOF){
      fprintf(stderr, "Read error %s\n", uv_err_name(uv_last_error(loop)));
    }
    uv_close((uv_handle_t*) client, NULL);
    return;
  } else if(nread <= 0){
    return;
  }

  uv_write_t *req = (uv_write_t *) malloc(sizeof(uv_write_t));

  char* tokens[8];
  get_tokens(tokens, buf.base);

  char* buffer = (char*)malloc(1024 * sizeof(char));
  if(strcmp(tokens[0], "get") == 0){
    handle_get(buffer, tokens);
  } else if(strcmp(tokens[0], "stats") == 0){
    handle_stats(buffer);
  } else if(strcmp(tokens[0], "flush_all") == 0){
    handle_flush_all(buffer);
  } else if(strcmp(tokens[0], "version") == 0){
    handle_version(buffer);
  } else if(strcmp(tokens[0], "quit") == 0){
    uv_close((uv_handle_t*) client, NULL);
    return;
  } else{
    sprintf(buffer, "Unknown Command: %s\r\n", tokens[0]);
  }
  buf.base = malloc((strlen(buffer) * sizeof(char)) + 1);
  strcpy(buf.base, buffer);
  buf.len = strlen(buf.base);
  req->data = malloc((strlen(buffer) * sizeof(char)) + 1);
  strcpy(req->data, buffer);
  uv_write(req, client, &buf, 1, handle_write);
  free(buffer);
  free(*tokens);
  *tokens = NULL;
}

void on_new_connection(uv_stream_t* server, int status){
  if(status == -1){
    return;
  }

  uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, client);
  if(uv_accept(server, (uv_stream_t*) client) == 0){
    uv_read_start((uv_stream_t*) client, alloc_buffer, handle_read);
  } else {
    uv_close((uv_handle_t*) client, NULL);
  }
}

void on_signal(){
  if(loop){
    printf("Stopping event loop...\n");
    uv_stop(loop);
  }
  if(db){
    printf("Closing Cache...\n");
    kcdbclose(db);
  }
  exit(0);
}

int main(){
  signal(SIGINT, on_signal);

  printf("Opening Cache...\n");
  db = kcdbnew();
  if(!kcdbopen(db, "*#bnum=1000000#capsiz=1g", KCOWRITER | KCOCREATE)){
    fprintf(stderr, "Error opening cache: %s\n", kcecodename(kcdbecode(db)));
    return -1;
  }

  loop = uv_default_loop();
  uv_tcp_t server;
  uv_tcp_init(loop, &server);

  struct sockaddr_in bind_addr = uv_ip4_addr("0.0.0.0", 7000);
  uv_tcp_bind(&server, bind_addr);
  int r = uv_listen((uv_stream_t*) &server, 128, on_new_connection);
  if(r){
    fprintf(stderr, "Listen error %s\n", uv_err_name(uv_last_error(loop)));
    return 1;
  }

  printf("Listening at 0.0.0.0:7000\n");
  return uv_run(loop, UV_RUN_DEFAULT);
}
