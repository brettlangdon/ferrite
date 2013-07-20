#include <curl/curl.h>
#include <kclangc.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>


static const char* VERSION = "0.0.1";
static sig_atomic_t misses = 0;
static sig_atomic_t hits = 0;

static KCDB* db;
static KCLIST* queue;
static int sd;

const char* STATS_FORMAT =
  "STAT hits %d\r\n"
  "STAT misses %d\r\n"
  "STAT hit_ratio %d\r\n"
  "STAT records %d\r\n"
  "END\r\n";

typedef struct {
  KCLIST* list;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int size;
} ProxyQueue;
static ProxyQueue requests;

void list_shift(KCLIST* list, char* next){
  if(kclistcount(list)){
    size_t size;
    strcpy(next, kclistget(list, 0, &size));
    kclistshift(list);
  }
}

void queue_init(ProxyQueue q){
  q.list = kclistnew();
  q.size = 0;
  pthread_mutex_init(&q.mutex, NULL);
  pthread_cond_init(&q.cond, NULL);
}

void queue_add(ProxyQueue* q, char* value){
  pthread_mutex_lock(&q->mutex);
  //kclistpush(*(&q->list), value, strlen(value));
  *(&q->size) += 1;
  pthread_mutex_unlock(&q->mutex);
  pthread_cond_signal(&q->cond);
}

void queue_get(ProxyQueue* q, char* value){
  pthread_mutex_lock(&q->mutex);
  while(*(&q->size) == 0){
    pthread_cond_wait(&q->cond, &q->mutex);
  }
  //list_shift(*(&q->list), value);
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

void tokenize(KCLIST* tokens, char* base){
  char* remainder;
  char* token;
  char* ptr = base;
  while(token = strtok_r(ptr, " ", &remainder)){
    int last;
    int len = strlen(token);
    for(last = len - 1; last >= 0; --last){
      if(token[last] == '\n' || token[last] == '\r'){
	token[last] = 0;
	break;
      }
    }
    lower(token);
    kclistpush(tokens, (const char*)token, strlen(token));
    ptr = remainder;
  }
  free(token);
}

void* call_proxy(void* arg){
  char next[1024];
  printf("Waiting\r\n");
  queue_get(&requests, next);
  printf("Calling Proxy For %s\r\n", next);
}

void handle_stats(KCLIST* tokens, FILE* client){
  char out[1024];
  float hit_ratio = (float)hits / (float)(hits + misses);
  int records = kcdbcount(db);
  sprintf(out, STATS_FORMAT, hits, misses, hit_ratio, records);
  fputs(out, client);
}

void handle_version(KCLIST* tokens, FILE* client){
  char out[1024];
  sprintf(out, "VERSION %s\r\n", VERSION);
  fputs(out, client);
}

void handle_get(KCLIST* tokens, FILE* client){
  char key[128];
  list_shift(tokens, key);
  if(key){
    char out[1024];
    char* result_buffer;
    size_t result_size;
    result_buffer = kcdbget(db, key, strlen(key), &result_size);
    if(result_buffer){
      if(strcmp(result_buffer, "0") == 0){
	++misses;
	sprintf(out, "VALUE %s 0 2\r\n{}\r\nEND\r\n", key);
	kcfree(result_buffer);
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

int handle_command(KCLIST* tokens, FILE* client){
  int status = 0;
  char command[128];
  list_shift(tokens, command);
  if(command){
    if(strcmp(command, "get") == 0){
      handle_get(tokens, client);
    } else if(strcmp(command, "stats") == 0){
      handle_stats(tokens, client);
    } else if(strcmp(command, "version") == 0){
      handle_version(tokens, client);
    } else if(strcmp(command, "quit") == 0){
      status = -1;
    } else{
      printf("Unknown Command: %s\r\n", command);
      char out[1024];
      sprintf(out, "UNKNOWN COMMAND: %s\r\n", command);
      fputs(out, client);
    }
  } else{
    printf("No Command\r\n");
  }

  return status;
}

void* worker(void* arg){
  FILE* fp = (FILE*) arg;

  char buffer[100];

  KCLIST* tokens = kclistnew();
  int status;
  while(fgets(buffer, sizeof(buffer), fp)){
    int last = strlen(buffer) - 1;
    for(; last > 0; --last){
      if(buffer[last] == '\r' || buffer[last] == '\n'){
	buffer[last] = 0;
      }
    }
    if(strlen(buffer)){
      tokenize(tokens, buffer);
      status = handle_command(tokens, fp);
      if(status == -1){
	break;
      }
      kclistclear(tokens);
    }
  }

  kclistdel(tokens);
  fclose(fp);
  return 0;
}

void on_signal(){
  if(sd){
    printf("Closing socket\r\n");
    close(sd);
  }

  if(queue){
    kclistdel(queue);
  }

  if(db){
    printf("Closing Cache\r\n");
    kcdbclose(db);
  }
  exit(0);
}

int main(){
  signal(SIGINT, on_signal);
  int pool_size = 1;
  pthread_t pool[pool_size];

  queue_init(requests);

  queue = kclistnew();
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
