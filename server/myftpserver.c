#include "myftpserver.h"

// worker threads to handle client connections
void *connection_handler(void *sDescriptor)
{
  printf("We in a new thread\n");
  int newSocket = *((int *)sDescriptor);
  int bytes;
  struct threadData data = *(struct threadData *)sDescriptor;
  struct message_s *packet = (struct message_s *)malloc(sizeof(struct message_s)), *convertedPacket;

  while (1)
  {
    bytes = recv(data.sd, packet, sizeof(struct message_s), 0);
    if (bytes == 0)
    {
      printf("Connection closed\n");
      pthread_mutex_lock(&thread_mutex);
      threads.size--;
      pthread_mutex_unlock(&thread_mutex);
      break;
    }

    // convertedPacket = ntohp(packet);
    // printf("[%d]command #%d\n", data.sd, convertedPacket->length);
  }
  pthread_exit(NULL);
}

int main(int argc, char **argv)
{
  // check if port was provided
  if (argc < 2)
  {
    printf("Error: please provide port\n");
    exit(1);
  }

  // set up threadList and initiate mutex
  threads.size = 0;
  pthread_mutex_init(&thread_mutex, NULL);

  // set up socket with specified ip:port
  int port = atoi(argv[1]);
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  int client_sd, new_sd;
  socklen_t addr_len;
  struct sockaddr_in server_addr, client_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  // bind socket
  if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    printf("Bind Error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }

  // listen for connections (backlog of 10)
  if (listen(sd, 10) < 0)
  {
    printf("Listen Error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }

  // keep socket open
  printf("Server listening on port %d\n", port);
  addr_len = sizeof(client_addr);
  while (1)
  {
    printf("Threads in use: %d\n", threads.size + 1);
    client_sd = accept(sd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_sd < 0)
    {
      printf("Accept Error: %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }
    // check if max connections reached
    else if (threads.size == MAX_CONNECTIONS - 1)
    {
      printf("Too many active connections...\n");
      continue;
    }
    // create new thread from group
    else
    {
      threads.workers[threads.size].sd = client_sd;
      threads.workers[threads.size].id = threads.size;
      if (pthread_create(&threads.workers[threads.size].thread, NULL, connection_handler, (void *)&threads.workers[threads.size]) != 0)
      {
        printf("Failed to make thread\n");
      }
      else
      {
        //move pointer to next available worker thread
        pthread_mutex_lock(&thread_mutex);
        threads.size++;
        pthread_mutex_unlock(&thread_mutex);
      }
    }
  }

  return 0;
}
