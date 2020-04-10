// common libraries
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
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

typedef struct block
{
  unsigned char *data;
} Block;

typedef struct stripe
{
  int stripe_id; //stripe id
  Block *data_block;
  Block *parity_block;
  Block *blocks;
  unsigned char *encode_matrix;
  unsigned char *table;
} Stripe;
// int numStripes(file_size, block_size, k);
// If all of the k blocks are the data blocks, you don't have to perform the decode procedure. If at most n-k blocks are parity
// blocks, then you have to use ec_encode_data() to recover the missing data blocks.
// store metadata for file size, and block numbers
// metadata number of stripes and blocks

// common function declarations
struct message_s *ntohp(struct message_s *packet);
struct message_s *htonp(struct message_s *packet);
int sendFile(int sd, FILE *fp, int fileSize);
int recFile(int sd, FILE *FP, int fileSize);
void gf_gen_rs_matrix(unsigned char *matrix, int n, int k);
void ec_init_tables(int k, int rows, unsigned char *matrix, unsigned char *table);
void ec_encode_data(int len, int src_len, int dest_len, unsigned char *table, unsigned char **src, unsigned char **dest);
uint8_t *encode(int n, int k, Stripe *stripe, size_t block_size);

// uint8_t *decode(int n, int k, Stripe *stripe, size_t block_size, Config *config);
