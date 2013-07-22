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
  if(server){
    printf("Closing socket\r\n");
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
  signal(SIGINT, on_signal);
  int pool_size = 10;
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

  server = socket(PF_INET, SOCK_STREAM, 0);
  if(!server){
    fprintf(stderr, "Could not create socket\r\n");
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(7000);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(server, (struct sockaddr*)&addr, sizeof(addr)) < 0){
    fprintf(stderr, "Address already in use\r\n");
    return -1;
  }

  if(listen(server, 24) < 0){
    fprintf(stderr, "Address already in use\r\n");
    return -1;
  } else{
    printf("Listening on 0.0.0.0:7000\r\n");
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
