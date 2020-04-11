#include "myftp.h"

// convert from network to host protocol
struct message_s *ntohp(struct message_s *packet)
{
  struct message_s *converted = (struct message_s *)malloc(sizeof(struct message_s));
  memcpy(converted->protocol, packet->protocol, 5);
  converted->type = packet->type;
  converted->length = (unsigned int)ntohl(packet->length);
  return converted;
}

// convert from host to network protocol
struct message_s *htonp(struct message_s *packet)
{
  struct message_s *converted = (struct message_s *)malloc(sizeof(struct message_s));
  memcpy(converted->protocol, packet->protocol, 5);
  converted->type = packet->type;
  converted->length = (unsigned int)htonl(packet->length);
  return converted;
}

// encode stripes
uint8_t *encode(int n, int k, Stripe *stripe, size_t block_size)
{
  // generate matrix and tables
  gf_gen_rs_matrix(stripe->encode_matrix, n, k);
  ec_init_tables(k, n - k, &stripe->encode_matrix[k * k], stripe->table);

  // arrange blocks
  unsigned char **blocks_data = malloc(sizeof(unsigned char **) * n);
  for (int i = 0; i < n; i++)
  {
    blocks_data[i] = stripe->blocks[i].data;
  }

  // encode blocks
  ec_encode_data(block_size, k, n - k, stripe->table, blocks_data, &blocks_data[k]);
  return stripe->encode_matrix;
}
