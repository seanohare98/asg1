#include "../myftp.h"

#define MAX_SERVERS 5

// macro to calculate max
#define max(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })

// server info
struct server
{
  char address[13];
  char port[6];
  int status;
};

// deployment configuration (client)
typedef struct deployment
{
  int n;
  int k;
  int block_size;
  struct server server_list[MAX_SERVERS];
  int sd[MAX_SERVERS];
} Config;

// function declarations
uint8_t *decode_data(int n, int k, Stripe *stripe, size_t block_size, Config *settings, int *goodRows, int *badRows, int available);