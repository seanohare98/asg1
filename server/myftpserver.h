#include "../myftp.h"

#define MAX_CONNECTIONS 10

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
  // = malloc(sizeof(pthread_t) * MAX_CONNECTIONS);
  int size;
} threadGroup;

// global variable declarations
threadGroup threads;
pthread_mutex_t thread_mutex;

// function declarations
void *connection_handler(void *sDescriptor);
