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

// protocol structure
struct message_s
{
  unsigned char protocol[5];
  unsigned char type;
  unsigned int length;
} __attribute__((packed));

// request details
typedef struct readFile
{
  char fileName[1024];
  char rawName[1024];
  long fileSize;
  int server_no;
  char done;
} payload;

// block structure
typedef struct block
{
  unsigned char *data;
} Block;

// stripe structure
typedef struct stripe
{
  int stripe_id;
  Block *data_block;
  Block *parity_block;
  Block *blocks;
  unsigned char *encode_matrix;
  unsigned char *table;
} Stripe;

// function declarations
struct message_s *ntohp(struct message_s *packet);
struct message_s *htonp(struct message_s *packet);
void gf_gen_rs_matrix(unsigned char *matrix, int n, int k);
int gf_invert_matrix(unsigned char *in_mat, unsigned char *out_mat, const int k);
void ec_init_tables(int k, int rows, unsigned char *matrix, unsigned char *table);
void ec_encode_data(int len, int src_len, int dest_len, unsigned char *table, unsigned char **src, unsigned char **dest);
uint8_t *encode(int n, int k, Stripe *stripe, size_t block_size);
// generate random file:  base64 /dev/urandom | head -c 10000000 > file.txt
// view file binary:      xxd -b file