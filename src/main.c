#include <kclangc.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "handlers.h"
#include "proxy.h"
#include "queue.h"
#include "util.h"


sig_atomic_t misses = 0;
sig_atomic_t hits = 0;
sig_atomic_t connections = 0;


void* worker(void* arg){
  FILE* client = (FILE*) arg;

  char buffer[128];
  ++connections;
  int status;
  while(fgets(buffer, sizeof(buffer), client)){
    int last = strlen(buffer) - 1;
    for(; last > 0; --last){
      if(buffer[last] == '\r' || buffer[last] == '\n'){
	buffer[last] = 0;
      }
    }
    if(strlen(buffer)){
      status = handle_command(buffer, client);
      if(status == -1){
	break;
      }
    }
  }

  --connections;
  fclose(client);
  return 0;
}

void on_signal(){
  printf("Shutting down fast-cache\r\n");
  if(server){
    printf("Closing socket\r\n");
    shutdown(server, 2);
    close(server);
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
  int i, addr_len, yes, port, pool_size;
  pool_size = 10;
  pthread_t pool[pool_size];
  char* base_url = "http://127.0.0.1:8000/";
  struct sockaddr_in addr, client_addr;

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  queue_init(&requests);

  db = kcdbnew();
  if(!kcdbopen(db, "*#bnum=1000000#capsiz=1g", KCOWRITER | KCOCREATE)){
    fprintf(stderr, "Error opening cache: %s\n", kcecodename(kcdbecode(db)));
    return -1;
  }

  printf("Starting %d worker threads\r\n", pool_size);
  for(i = 0; i < pool_size; ++i){
    pthread_create(&(pool[i]), NULL, call_proxy, (void*)base_url);
  }


  server = socket(PF_INET, SOCK_STREAM, 0);
  if(!server){
    fprintf(stderr, "Could not create socket\r\n");
    return -1;
  }

  port = 7000;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(server, (struct sockaddr*)&addr, sizeof(addr)) < 0){
    perror("Error binding socket");
    return -1;
  }

  yes = 1;
  setsockopt(server,SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR), &yes, sizeof(yes));
  if(listen(server, 24) < 0){
    perror("Error binding socket");
    return -1;
  } else{
    printf("Listening on 0.0.0.0:%d\r\n", port);
    int client;
    pthread_t child;
    FILE *fp;
    while(1){
      addr_len = sizeof(client_addr);
      client = accept(server, 0, 0);
      fp = fdopen(client, "r+");
      pthread_create(&child, 0, worker, fp);
      pthread_detach(child);
    }
  }

  return 0;
}
