#include "../myftp.h"

#define MAX_CONNECTIONS 10

// deployment configuration (server)
typedef struct deployment
{
  int n;
  int k;
  int server_id;
  long block_size;
  char port[6];
} Config;

// thread metadata
struct threadData
{
  int id;
  int sd;
  pthread_t thread;
  Config *settings;
};

// manage worker threads
typedef struct group
{
  struct threadData workers[MAX_CONNECTIONS];
  int size;
} threadGroup;

// global variables/function declarations
threadGroup threads;
pthread_mutex_t thread_mutex;
void *connection_handler(void *sDescriptor);
