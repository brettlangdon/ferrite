#include <curl/curl.h>
#include <kclangc.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>


static const char* VERSION = "0.0.1";
static sig_atomic_t misses = 0;
static sig_atomic_t hits = 0;
static sig_atomic_t connections = 0;

static KCDB* db;
static int sd;

const char* STATS_FORMAT =
  "STAT connections %d\r\n"
  "STAT hits %d\r\n"
  "STAT misses %d\r\n"
  "STAT hit_ratio %2.4f\r\n"
  "%s"
  "END\r\n";


struct curl_result {
  char* data;
  size_t size;
};

typedef struct {
  KCLIST* list;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int size;
} ProxyQueue;
static ProxyQueue requests;

void list_shift(KCLIST* list, char* next){
  size_t size;
  const char* results = kclistget(list, 0, &size);
  strcpy(next, results);
  next[size] = 0;
  kclistshift(list);
}

void queue_del(ProxyQueue* q){
  kclistdel(*(&q->list));
  pthread_mutex_destroy(&q->mutex);
  pthread_cond_destroy(&q->cond);
}

void queue_init(ProxyQueue* q){
  *(&q->list) = kclistnew();
  *(&q->size) = 0;
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->cond, NULL);
}

void queue_add(ProxyQueue* q, char* value){
  pthread_mutex_lock(&q->mutex);
  kclistpush(*(&q->list), value, strlen(value) + 1);
  *(&q->size) += 1;
  pthread_mutex_unlock(&q->mutex);
  pthread_cond_signal(&q->cond);
}

void queue_get(ProxyQueue* q, char* value){
  pthread_mutex_lock(&q->mutex);
  while(*(&q->size) == 0){
    pthread_cond_wait(&q->cond, &q->mutex);
  }
  list_shift(*(&q->list), value);
  *(&q->size) -= 1;
  pthread_mutex_unlock(&q->mutex);
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
  while(token = strtok_r(ptr, delimiter, &remainder)){
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

void handle_stats(KCLIST* tokens, FILE* client){
  char out[1024];
  float hit_ratio = 0;
  if(hits){
    hit_ratio = (float)hits / (float)(hits + misses);
  }
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
  sprintf(out, STATS_FORMAT, connections, hits, misses, hit_ratio, status_buf);
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
    char key[128];
    list_shift(tokens, key);
    if(kcdbremove(db, key, strlen(key))){
      fputs("DELETED\r\n", client);
    }
  }
}

void handle_get(KCLIST* tokens, FILE* client){
  if(kclistcount(tokens)){
    char key[128];
    list_shift(tokens, key);
    char out[1024];
    char* result_buffer;
    size_t result_size;
    result_buffer = kcdbget(db, key, strlen(key), &result_size);
    if(result_buffer){
      if(strcmp(result_buffer, "0") == 0){
	++misses;
	sprintf(out, "VALUE %s 0 2\r\n{}\r\nEND\r\n", key);
      } else{
	++hits;
	sprintf(out, "VALUE %s 0 %d\r\n%s\r\nEND\r\n", key, (int)strlen(result_buffer), result_buffer);
      }
      kcfree(result_buffer);
    } else{
      ++misses;
      sprintf(out, "VALUE %s 0 2\r\n{}\r\nEND\r\n", key);
      kcdbset(db, key, strlen(key), "0", 1);
      queue_add(&requests, key);
    }
    fputs(out, client);
  } else{
    fputs("INVALID GET COMMAND: GET <KEY>\r\n", client);
    return;
  }
}

int handle_command(char* buffer, FILE* client){
  int status = 0;
  char command[1024];
  KCLIST* tokens = kclistnew();
  tokenize(tokens, buffer, " ");
  list_shift(tokens, command);
  if(command){
    if(strcmp(command, "get") == 0){
      handle_get(tokens, client);
    } else if(strcmp(command, "stats") == 0){
      handle_stats(tokens, client);
    } else if(strcmp(command, "flush_all") == 0){
      handle_flush(tokens, client);
    } else if(strcmp(command, "delete") == 0){
      handle_delete(tokens, client);
    } else if(strcmp(command, "version") == 0){
      handle_version(tokens, client);
    } else if(strcmp(command, "quit") == 0){
      status = -1;
    } else{
      char out[1024];
      sprintf(out, "UNKNOWN COMMAND: %s\r\n", command);
      fputs(out, client);
    }
  }

  kclistdel(tokens);

  return status;
}

void* worker(void* arg){
  FILE* fp = (FILE*) arg;

  char buffer[100];
  ++connections;
  int status;
  while(fgets(buffer, sizeof(buffer), fp)){
    int last = strlen(buffer) - 1;
    for(; last > 0; --last){
      if(buffer[last] == '\r' || buffer[last] == '\n'){
	buffer[last] = 0;
      }
    }
    if(strlen(buffer)){
      status = handle_command(buffer, fp);
      if(status == -1){
	break;
      }
    }
  }

  --connections;
  fclose(fp);
  return 0;
}

void on_signal(){
  if(sd){
    printf("Closing socket\r\n");
    close(sd);
  }

  if(&requests != NULL){
    printf("Deleting Request Queue\r\n");
    queue_del(&requests);
  }

  if(db){
    printf("Closing Cache\r\n");
    kcdbclose(db);
  }
  exit(0);
}

int main(){
  signal(SIGINT, on_signal);
  int pool_size = 5;
  pthread_t pool[pool_size];

  queue_init(&requests);

  db = kcdbnew();
  if(!kcdbopen(db, "*#bnum=1000000#capsiz=1g", KCOWRITER | KCOCREATE)){
    fprintf(stderr, "Error opening cache: %s\n", kcecodename(kcdbecode(db)));
    return -1;
  }

  int i;
  printf("Starting %d worker threads\r\n", pool_size);
  for(i = 0; i < pool_size; ++i){
    pthread_create(&(pool[i]), NULL, call_proxy, NULL);
  }

  struct sockaddr_in addr, client_addr;
  int addr_len;

  sd = socket(PF_INET, SOCK_STREAM, 0);
  if(!sd){
    fprintf(stderr, "Could not create socket\r\n");
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(7000);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(sd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
    fprintf(stderr, "Address already in use\r\n");
    return -1;
  }

  if(listen(sd, 24) < 0){
    fprintf(stderr, "Address already in use\r\n");
    return -1;
  } else{
    printf("Listening on 0.0.0.0:7000\r\n");
    int client;
    pthread_t child;
    FILE *fp;
    while(1){
      addr_len = sizeof(client_addr);
      client = accept(sd, 0, 0);
      fp = fdopen(client, "r+");
      pthread_create(&child, 0, worker, fp);
      pthread_detach(child);
    }
  }

  return 0;
}
