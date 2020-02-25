#include "myftpclient.h"

struct message_s newPacket;

int main(int argc, char **argv)
{
  // set up socket with specified ip:port
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
  server_addr.sin_port = htons(PORT);

  // connect to server
  if (connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    printf("Connection Error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }
  else

    return 0;
}
