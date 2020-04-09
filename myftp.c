#include "myftp.h"

// convert from network to host protocol
struct message_s *ntohp(struct message_s *packet)
{
  struct message_s *converted = (struct message_s *)malloc(sizeof(struct message_s));
  memcpy(converted->protocol, packet->protocol, 5);
  converted->type = packet->type;
  converted->length = (unsigned int)ntohs(packet->length);
  return converted;
}

// convert from host to network protocol
struct message_s *htonp(struct message_s *packet)
{
  struct message_s *converted = (struct message_s *)malloc(sizeof(struct message_s));
  memcpy(converted->protocol, packet->protocol, 5);
  converted->type = packet->type;
  converted->length = (unsigned int)htons(packet->length);
  return converted;
}

// send a file
int sendFile(int sd, FILE *fp, int fileSize)
{
  int remainingBytes = fileSize;
  int bytes = 0;
  payload *reply = (payload *)malloc(sizeof(payload));
  int toRead;
  // loop to ensure all data is sent
  while (remainingBytes > 0)
  {
    // printf("Bytes left to send: %d\n", remainingBytes);
    // reset payload
    bytes = 0;
    // memset(reply->fileName, '\0', sizeof(reply->fileName));
    reply->done = 'n';

    // read file in chunks of 1024 elements
    if (remainingBytes >= 1024)
      toRead = 1024;
    else
      toRead = remainingBytes;

    bytes = fread(reply->fileName, sizeof(char), toRead, fp);
    // printf("%d  %d\n", bytes, toRead);

    // upon successful read
    if (bytes > 0)
    {
      if (bytes < 1024)
      {
        if (feof(fp))
          reply->done = 'y'; // indicate end of file was reached
        if (ferror(fp))
        {
          printf("Error reading file\n");
          return -1;
        }
      }
    }
    else
    {
      printf("Error reading file\n");
      return -1;
    }

    // printf("Trying to send packet: FILE_DATA\n");
    if (feof(fp))
      reply->done = 'y';
    // send packet
    if ((send(sd, reply, sizeof(payload), 0)) < 0)
    {
      printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
      return -1;
    }
    else
      remainingBytes -= bytes;
  }

  fclose(fp);
  return 0;
}

// recieve a file
int recFile(int sd, FILE *fp, int fileSize)
{
  // printf("Size of file to recieve: %d\n", fileSize);
  int remainingBytes = fileSize;
  int bytes = 0;
  int toWrite;
  payload *reply = (payload *)malloc(sizeof(payload));
  reply->done = 'n';
  // loop to ensure all data is recieved
  while (remainingBytes > 0)
  {
    // printf("Bytes left to recieve: %d\n", remainingBytes);
    // reset payload
    reply->done = 'n';
    bytes = 0;

    if ((bytes = recv(sd, reply, sizeof(payload), 0)) <= 0)
    {
      fprintf(stderr, "Error receiving file\n");
      return -1;
    }

    if (remainingBytes >= 1024)
      toWrite = 1024;
    else
      toWrite = remainingBytes;

    // printf("%d\n", toWrite);
    fwrite(reply->fileName, sizeof(char), toWrite, fp);
    remainingBytes -= bytes;
  }

  fclose(fp);
  return 0;
}

uint8_t *encode(int n, int k, Stripe *stripe, size_t block_size)
{
  printf("HERE: %d, %d, %p %ld\n", n, k, stripe, block_size);
  // uint8_t *encode_matrix = malloc(sizeof(uint8_t) * (n * k));
  // uint8_t *decode_matrix = malloc(sizeof(uint8_t) * (k * k));
  // uint8_t *invert_matrix = malloc(sizeof(uint8_t) * (k * k));
  gf_gen_rs_matrix(stripe->encode_matrix, n, k);

  ec_init_tables(k, n - k, &stripe->encode_matrix[k * k], stripe->table);

  unsigned char **blocks_data = malloc(sizeof(unsigned char **) * n);

  ec_encode_data(block_size, k, n - k, stripe->table, stripe->data_block, &stripe->parity_block);

  return stripe->encode_matrix;
}

int readFile(FILE *fp, unsigned char *block, int block_size)
{
  int bytes_read = fread(block, sizeof(unsigned char), block_size, fp);
  return bytes_read;
}