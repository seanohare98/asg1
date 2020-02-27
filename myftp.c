#include "myftp.h"

// convert from network to host protocol
struct message_s *ntohp(struct message_s *packet)
{
  struct message_s *converted = (struct message_s *)malloc(sizeof(struct message_s));
  memcpy(converted->protocol, packet->protocol, 5);
  // strcpy(converted->protocol, packet->protocol);
  converted->type = packet->type;
  converted->length = (unsigned int)ntohs(packet->length);

  // printf("Protocol: %s\n", packet->protocol);
  // printf("Type Before: %x\n", packet->type);
  // printf("Type After: %x\n", (unsigned char)htons(packet->type));
  // printf("Type After in Converted: %x\n", (unsigned char)converted->type);
  // printf("Length in Converted: %d\n", (unsigned int)converted->length);

  return converted;
}

// convert from host to network protocol
struct message_s *htonp(struct message_s *packet)
{
  struct message_s *converted = (struct message_s *)malloc(sizeof(struct message_s));
  memcpy(converted->protocol, packet->protocol, 5);
  // strcpy(converted->protocol, packet->protocol);
  converted->type = packet->type;
  converted->length = (unsigned int)htons(packet->length);

  // printf("Protocol: %s\n", packet->protocol);
  // printf("Type Before: %x\n", packet->type);
  // printf("Type After: %x\n", (unsigned char)htons(packet->type));
  // printf("Type After in Converted: %x\n", (unsigned char)converted->type);
  // printf("Length in Converted: %d\n", (unsigned int)converted->length);
  return converted;
}

// send a file
int sendFile(int sd, FILE *fp, int fileSize)
{
  printf("Size of file to send: %d\n", fileSize);
  int remainingBytes = fileSize;
  int bytes = 0;
  payload *reply = (payload *)malloc(sizeof(payload));

  // loop to ensure all data is sent
  while (remainingBytes > 0)
  {
    printf("Bytes left to send: %d\n", remainingBytes);
    // reset payload
    bytes = 0;
    memset(reply->fileName, '\0', sizeof(reply->fileName));
    reply->done = 'n';

    // read file in chunks of 1024 elements
    bytes = fread(reply->fileName, 1, 1024, fp);

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
  payload *reply = (payload *)malloc(sizeof(payload));
  reply->done = 'n';
  // loop to ensure all data is recieved
  while (remainingBytes > 0 && reply->done == 'n')
  {
    printf("Bytes left to recieve: %d\n", remainingBytes);
    // reset payload
    reply->done = 'n';
    bytes = 0;

    // read file in chunks of 1024 elements
    if ((bytes = recv(sd, reply, sizeof(payload), 0)) <= 0)
    {
      fprintf(stderr, "Error receiving file\n");
      return -1;
    }
    else
      remainingBytes -= bytes;

    fwrite(reply->fileName, 1, sizeof(reply->fileName), fp);
  }

  fclose(fp);
  return 0;
}