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

struct message_s
{
  unsigned char protocol[5]; //  protocol string (5 bytes)
  unsigned char type;        //  type (1 byte)
  unsigned int length;       //  length (header + payload) (4 bytes)
} __attribute__((packed));

int main(int argc, char **argv)
{
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in server_addr;
  pthread_t worker;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = htons(12345);
  if (connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    printf("connection error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }
  else
  {
    sleep(1999);
  }
  return 0;
}
