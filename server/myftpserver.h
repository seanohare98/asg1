#include "../myftp.h"
#define MAX_CONNECTIONS 10

typedef struct deployment
{
  int n;
  int k;
  int server_id;
  unsigned int block_size;
  char port[6];
} Config;

// passed as argument to pthread_create()
struct threadData
{
  pthread_t thread;
  int id;
  int sd;
};

// array of worker threads
typedef struct group
{
  struct threadData workers[MAX_CONNECTIONS];
  int size;
} threadGroup;

// global variable declarations
threadGroup threads;
pthread_mutex_t thread_mutex;

// function declarations
void *connection_handler(void *sDescriptor);
