// common libraries
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

// header struct
struct message_s
{
  unsigned char protocol[5]; //  protocol string (5 bytes)
  unsigned char type;        //  type (1 byte)
  unsigned int length;       //  length (header + payload) (4 bytes)
} __attribute__((packed));

// payload struct
typedef struct readFile
{
  char fileName[1024];
  char done;
} payload;

// common function declarations
struct message_s *ntohp(struct message_s *packet);
struct message_s *htonp(struct message_s *packet);
int sendFile(int sd, FILE *fp, int fileSize);
int recFile(int sd, FILE *FP, int fileSize);
