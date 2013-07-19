#include <kclangc.h>
#include <signal.h>
#include <uv.h>

static uv_loop_t* loop = NULL;
static KCDB* db = NULL;
static sig_atomic_t cache_miss = 0;
static sig_atomic_t hits = 0;

const char* VERSION = "0.0.1";

void lower(char* word){
  int length = sizeof(word) / sizeof(char);
  int i;
  for(i = 0; i < length; ++i){
    word[i] = tolower(word[i]);
  }
}

uv_buf_t alloc_buffer(uv_handle_t *handle, size_t suggested_size) {
  return uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void handle_write(uv_write_t *req, int status) {
  if(status == -1){
    fprintf(stderr, "Write error %s\n", uv_err_name(uv_last_error(loop)));
  }
  free(req->data);
  free(req);
}

char** get_tokens(char* base){
  size_t size = 0;
  int i = 0;
  char** tokens = malloc(size);
  char* token = strtok(base, " ");
  while(token){
    int last = strlen(token) - 1;
    if(token[last] == '\n'){
      token[last] = '\0';
      continue;
    } else if(token[last] == '\r'){
      token[last] = '\0';
      continue;
    }
    ++size;
    tokens = realloc(tokens, size);
    lower(token);
    tokens[i] = token;
    ++i;
    token = strtok(NULL, " ");
  }

  return tokens;
}

uv_buf_t handle_version(){
  char buffer[1024];
  int n;
  n = sprintf(buffer, "VERSION %s\n", VERSION);
  return uv_buf_init(buffer, n);
}

uv_buf_t handle_flush_all(){
  kcdbclear(db);
  return uv_buf_init("OK\n", 3);
}

uv_buf_t handle_stats(){
  char buffer[1024];
  int n;
  float ratio;
  if(!hits && !cache_miss){
    ratio = 0;
  } else{
    ratio = (float)hits / (hits + cache_miss);
  }
  char* format = "STAT cache_miss %d\nSTAT hits %d\nSTAT hit_ratio %2.2f\nEND\n";
  n = sprintf(buffer, format, cache_miss, hits, ratio);
  return uv_buf_init(buffer, n);
}

uv_buf_t handle_get(char** tokens){
  uv_buf_t out;
  if(tokens[1]){
    char* result_buffer;
    size_t result_size, key_size;
    key_size = strlen(tokens[1]);
    result_buffer = kcdbget(db, tokens[1], key_size, &result_size);

    int result = !!result_buffer;
    if(!result){
      ++cache_miss;
      result_buffer = "{}";
    } else{
      ++hits;
    }

    char buffer[1024];
    int n;
    n = sprintf(buffer, "VALUE 0 %lu\n%s\nEND\n", strlen(result_buffer), result_buffer);

    if(result){
      kcfree(result_buffer);
    }
    out = uv_buf_init(buffer, n);
  } else{
    char buffer[128];
    int n;
    n = sprintf(buffer, "Invalid GET command: \"GET <REQUEST>\"\n");
    out = uv_buf_init(buffer, n);
  }

  return out;
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
  uv_buf_t out;

  char** tokens = get_tokens(buf.base);

  char* command = tokens[0];
  if(strcmp(command, "get") == 0){
    out = handle_get(tokens);
  } else if(strcmp(command, "stats") == 0){
    out = handle_stats();
  } else if(strcmp(command, "flush_all") == 0){
    out = handle_flush_all();
  } else if(strcmp(command, "version") == 0){
    out = handle_version();
  } else if(strcmp(command, "quit") == 0){
    uv_close((uv_handle_t*) client, NULL);
    return;
  } else{
    char buffer[128];
    int n;
    n = sprintf(buffer, "Unknown command: %s\n", tokens[0]);
    out = uv_buf_init(buffer, n);
  }

  buf.len = nread;
  uv_write(req, client, &out, 1, handle_write);
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
  if(!kcdbopen(db, "*", KCOWRITER | KCOCREATE)){
    fprintf(stderr, "Error opening cache: %s\n", kcecodename(kcdbecode(db)));
    return -1;
  }

  char* key = "www.magnetic.com";
  kcdbset(db, key, strlen(key), "{}", 2);

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
