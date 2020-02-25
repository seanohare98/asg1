#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <pthread.h>

// packet struct to be sent/recieved
struct message_s
{
  unsigned char protocol[5]; //  protocol string (5 bytes)
  unsigned char type;        //  type (1 byte)
  unsigned int length;       //  length (header + payload) (4 bytes)
} __attribute__((packed));

struct message_s *ntohp(struct message_s *packet);
void sendF(struct message_s *, int, FILE *);
void recF(struct message_s *, struct message_s *, int, FILE *);