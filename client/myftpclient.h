#include "../myftp.h"
#define MAX_SERVERS 5
#define max(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })

struct server
{
  char address[13];
  char port[6];
  int status;
  // int sd;
};

typedef struct deployment
{
  int n;
  int k;
  unsigned int block_size;
  struct server server_list[MAX_SERVERS];
  int sd[MAX_SERVERS];
} Config;
