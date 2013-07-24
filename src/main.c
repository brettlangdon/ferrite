#include <kclangc.h>
#include <getopt.h>
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
int record_expire_time = 3600;

static struct option long_options[] = {
  {"port", 1, 0, 0},
  {"url", 1, 0, 0},
  {"workers", 1, 0, 0},
  {"cache", 1, 0, 0},
  {"expire", 1, 0, 0},
  {"help", 0, 0, 0},
  {NULL, 0, NULL, 0}
};

void usage(){
  const char* usage_str =
    "Usage: ferrite [-h|--help] [-p|--port <NUM>] [-w|--workers <NUM>] [-u|--url <STRING>]\r\n"
    "                  [-c|--cache <STRING>] [-e|--expire <NUM>]\r\n"
    "\t-p, --port\t- which port number to bind too [default: 7000]\r\n"
    "\t-w, --workers\t- how many background workers to spawn [default: 10]\r\n"
    "\t-u, --url\t- which url to proxy requests to [default: http://127.0.0.1:8000]\r\n"
    "\t-c, --cache\t- kyoto cabinet cache to use [default: \"*\"]\r\n"
    "\t-e, --expire\t- the expiration time in seconds from when a record is cached, 0 to disable [default:3600]\r\n"
    "\t-h, --help\t- display this message\r\n";
  printf("%s", usage_str);
}

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
  printf("Shutting down ferrite\r\n");
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

int main(int argc, char* argv[]){
  int i, addr_len, yes;
  int port = 7000;
  int pool_size = 10;
  pthread_t pool[pool_size];
  char* cache_file = "*";
  char* base_url = "http://127.0.0.1:8000/";
  struct sockaddr_in addr, client_addr;

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);


  int c;
  int option_index = 0;
  while((c = getopt_long(argc, argv, "hp:u:w:c:e:", long_options, &option_index)) != -1){
    if(c == 0){
      switch(option_index){
      case 0:
	c = 'p';
	break;
      case 1:
	c = 'u';
	break;
      case 2:
	c = 'w';
	break;
      case 3:
	c = 'c';
	break;
      case 4:
	c = 'e';
	break;
      case 5:
	c = 'h';
	break;
      default:
	c = '?';
	break;
      }
    }
    switch(c){
    case 'p':
      port = atoi(optarg);
      if(port <= 0){
	fprintf(stderr, "Invalid Port Number: %s\r\n", optarg);
	exit(1);
      }
      break;
    case 'u':
      base_url = optarg;
      break;
    case 'w':
      pool_size = atoi(optarg);
      if(pool_size <= 0){
	fprintf(stderr, "Invalid Worker Count: %s\r\n", optarg);
	exit(1);
      }
      break;
    case 'c':
      cache_file = optarg;
      break;
    case 'e':
      record_expire_time = atoi(optarg);
      if(record_expire_time <= 0){
	fprintf(stderr, "Invalid Expiration Time: %s\r\n", optarg);
	exit(1);
      }
      break;
    case 'h':
      usage();
      exit(0);
    case '?':
    default:
      fprintf(stderr, "Unknown Option: %c\r\n", c);
      usage();
      exit(-1);
      break;
    }
  }

  queue_init(&requests);

  printf("Using Expire Time of %d sec\r\n", record_expire_time);
  printf("Opening Cache File: \"%s\"\r\n", cache_file);
  db = kcdbnew();
  if(!kcdbopen(db, cache_file, KCOWRITER | KCOCREATE)){
    fprintf(stderr, "Error opening cache: %s\n", kcecodename(kcdbecode(db)));
    return -1;
  }

  printf("Starting %d worker threads\r\n", pool_size);
  for(i = 0; i < pool_size; ++i){
    pthread_create(&(pool[i]), NULL, call_proxy, (void*)base_url);
  }
  printf("Connected to %s\r\n", base_url);


  server = socket(PF_INET, SOCK_STREAM, 0);
  if(!server){
    fprintf(stderr, "Could not create socket\r\n");
    return -1;
  }

  port = 7000;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  yes = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if(bind(server, (struct sockaddr*)&addr, sizeof(addr)) < 0){
    perror("Error binding socket");
    return -1;
  }

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
