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

// int numStripes(file_size, block_size, k)
// {
//   // ceil( file_size / (block_size * k))
//   // n - k blocks are reserved for parity
// }
