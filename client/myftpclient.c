#include "myftpclient.h"

int main(int argc, char **argv)
{

  struct message_s *packet = (struct message_s *)malloc(sizeof(struct message_s));

  // validate arguments
  if (argc < 4)
  {
    printf("Error: Too few arguments\n");
    exit(0);
  }

  // validate arguments (file)
  else if (argc < 5 && (strcmp(argv[3], "put") == 0 || strcmp(argv[3], "get") == 0))
  {
    printf("Error: Too few arguments (no file name)\n");
    exit(0);
  }

  // construct packet
  else
  {
    unsigned char myftp[6] = "myftp";
    myftp[5] = '\0';
    memcpy(packet->protocol, myftp, 5);

    if (strcmp(argv[3], "list") == 0)
      packet->type = ((unsigned char)0xA1);
    else if (strcmp(argv[3], "get") == 0)
      packet->type = ((unsigned char)0xB1);
    else if (strcmp(argv[3], "put") == 0)
      packet->type = ((unsigned char)0xC1);

    packet->length = 6;
  }

  // set up socket with specified ip:port
  int port = atoi(argv[2]);
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in server_addr;
  pthread_t worker;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr((char *)argv[1]);
  server_addr.sin_port = htons(port);

  // connect sockets
  if (connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    printf("Connection error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }

  // LIST_REQUEST
  if (packet->type == ((unsigned char)0xA1))
  {
    int bytes;
    // convert packet to network protocol
    struct message_s *convertedPacket;
    struct message_s *replyHeader;
    convertedPacket = htonp(packet);

    // printf("Trying to send packet: LIST_REQUEST\n");

    // send packet
    if (send(sd, convertedPacket, sizeof(struct message_s), 0) < 0)
    {
      printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }

    // printf("Sent packet. Waiting for reply...\n");

    // recieve packet(s)
    if (recv(sd, convertedPacket, sizeof(struct message_s), 0) <= 0)
    {
      printf("Error receving packet(s): %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }

    // printf("Recieved reply header\n");

    payload *list_reply = (payload *)malloc(sizeof(payload));
    recv(sd, list_reply, sizeof(payload), 0);

    while (list_reply->done != 'y')
    {
      printf("%s\n", list_reply->fileName);
      recv(sd, list_reply, sizeof(payload), 0);
    }
    // printf("Finished\n");
  }

  // GET_REQUEST
  if (packet->type == (unsigned char)0xB1)
  {

    // printf("Trying to send packet: GET_REQUEST\n");

    // append payload length to packet->length
    packet->length += strlen(argv[4]);
    int fileSize;
    // convert packet to network protocol
    struct message_s *convertedPacket;
    convertedPacket = htonp(packet);

    // send header packet
    if ((send(sd, convertedPacket, sizeof(struct message_s), 0)) < 0)
    {
      printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }

    // printf("Trying to send packet: GET_REQUEST Payload\n");

    // build payload
    struct readFile *get = (struct readFile *)malloc(sizeof(struct readFile));
    char local[1024] = "./";
    strcat(local, argv[4]);
    strcpy(get->fileName, argv[4]);
    get->done = 'n';

    // send payload
    if ((send(sd, get, sizeof(struct readFile), 0)) < 0)
    {
      printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }

    // printf("Sent payload. Waiting for reply...\n");

    // receive reply header
    if ((recv(sd, convertedPacket, sizeof(struct message_s), 0)) <= 0)
    {
      printf("Error receving packets: %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }

    // printf("Recieved reply header. Waiting for payload...\n");

    // recieve payload
    if (convertedPacket->type == 0xB2)
    {
      // recieve FILE_DATA header
      recv(sd, convertedPacket, sizeof(struct message_s), 0);
      convertedPacket = ntohp(convertedPacket);
      fileSize = convertedPacket->length - 6;

      //clear/create file and write to it
      FILE *fClear, *fWrite;
      fClear = fopen(local, "wb");
      fclose(fClear);
      fWrite = fopen(local, "ab");

      if (recFile(sd, fWrite, fileSize) == 0)
        printf("FILE_DATA protocol recieved\n");
    }
    else
      printf("No such file exists...\n");

    exit(0);
  }

  // PUT_REQUEST
  if (packet->type == ((unsigned char)0xC1))
  {

    // printf("Trying to send packet: PUT_REQUEST\n");

    // check if file exists locally
    int fileSize;
    char localPath[1024] = "./";
    strcat(localPath, argv[4]);
    FILE *fPUT;
    fPUT = fopen(localPath, "rb");
    if (fPUT)
    {
      // append file name to packet->length
      packet->length += strlen(argv[4]);
      fseek(fPUT, 0L, SEEK_END);
      fileSize = ftell(fPUT);
      fseek(fPUT, 0L, SEEK_SET);

      // convert packet to network protocol
      struct message_s *convertedPacket;
      convertedPacket = htonp(packet);

      // send header packet
      if ((send(sd, convertedPacket, sizeof(struct message_s), 0)) < 0)
      {
        printf("Error sending packets: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
      }

      // printf("Trying to send packet: PUT_REQUEST Payload\n");

      // build payload
      payload *put = (payload *)malloc(sizeof(payload));
      strcpy(put->fileName, argv[4]);
      put->done = 'n';

      // send payload
      if ((send(sd, put, sizeof(payload), 0)) < 0)
      {
        printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
      }

      // printf("Sent payload. Waiting for reply...\n");

      // receive reply header
      if ((recv(sd, convertedPacket, sizeof(struct message_s), 0)) <= 0)
      {
        printf("Error receving packets: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
      }

      // printf("Recieved reply header. Trying to send FILE_DATA header...\n");

      // if server responds, send file
      if (convertedPacket->type == ((unsigned char)0xC2))
      {
        //build FILE_DATA header
        unsigned char myftp[6] = "myftp";
        myftp[5] = '\0';
        memcpy(convertedPacket->protocol, myftp, 5);
        convertedPacket->type = 0xFF;
        convertedPacket->length = 6 + fileSize;
        convertedPacket = htonp(convertedPacket);

        // send FILE_DATA header
        if ((send(sd, convertedPacket, sizeof(struct message_s), 0)) < 0)
        {
          printf("Error sending packets: %s (Errno:%d)\n", strerror(errno), errno);
          exit(0);
        }

        // printf("Trying to send FILE_DATA Payload\n");

        if (sendFile(sd, fPUT, fileSize) == 0)
          printf("FILE_DATA protocol sent\n");
      }
      // else, exit thread
      else
        printf("No reply from server\n");

      return 0;
    }
    else
      printf("No such file exists locally\n");

    return 0;
  }
  return 0;
}
