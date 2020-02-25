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

threadGroup threads;
pthread_mutex_t thread_mutex;
struct message_s *ntohp(struct message_s *packet);
void *connection_handler(void *sDescriptor);
void *list_reply(void *sDescriptor);