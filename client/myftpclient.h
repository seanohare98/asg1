#include "../myftp.h"

#define MAX_SERVERS 5

struct server
{
  char address[13];
  char port[6];
  int isActive;
  int sd;
};

typedef struct deployment
{
  int n;
  int k;
  unsigned int block_size;
  struct server server_list[MAX_SERVERS];
} Config;
