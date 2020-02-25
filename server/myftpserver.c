#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CONNECTIONS 10

struct message_s
{
  unsigned char protocol[5]; //  protocol string (5 bytes)
  unsigned char type;        //  type (1 byte)
  unsigned int length;       //  length (header + payload) (4 bytes)
} __attribute__((packed));

/*
 * Worker thread to send replies (list_reply, get_reply, put_reply, file_data)
 */
void *connection_handler(void *sDescriptor)
{
  int newSocket = *((int *)sDescriptor);
  printf("The new socker = %d\n", newSocket);
  printf("we in a new thread\n");
  sleep(1999);
  close(newSocket);
  pthread_exit(NULL);
}

int main(int argc, char **argv)
{
  //check if port was provided
  if (argc < 2)
  {
    printf("ERROR: please provide port\n");
    exit(1);
  }

  // set up socket with specified port
  int port = atoi(argv[1]);
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  int client_sd, new_sd;
  socklen_t addr_len;
  struct sockaddr_in server_addr, client_addr;
  pthread_t *threadGroup = malloc(sizeof(pthread_t) * MAX_CONNECTIONS);
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  // bind socket
  if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    printf("bind error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }

  // listen for connections (backlog of 10)
  if (listen(sd, 10) < 0)
  {
    printf("listen error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }

  // keep socket open
  int threadCount = 0;
  while (1)
  {
    addr_len = sizeof(client_addr);
    client_sd = accept(sd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_sd < 0)
    {
      printf("accept error: %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }
    else
    {
      if (pthread_create(&threadGroup[threadCount], NULL, connection_handler, &client_sd) != 0)
        printf("Failed to make thread\n");
      else
        threadCount++;
    }

    //re-join threads once all 10 are used up
    if (threadCount >= 10)
    {
      for (int i = 0; i < 10; i++)
        pthread_join(threadGroup[i], NULL);
      threadCount = 0;
    }
  }

  return 0;
}
