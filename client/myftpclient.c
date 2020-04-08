#include "myftpclient.h"

int main(int argc, char **argv)
{
  struct message_s *packet = malloc(sizeof(struct message_s));

  // validate arguments
  if (argc < 3)
  {
    printf("Error: Too few arguments\n");
    exit(0);
  }
  // validate arguments (file)
  else if (argc < 4 && (strcmp(argv[2], "put") == 0 || strcmp(argv[2], "get") == 0))
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
    if (strcmp(argv[2], "list") == 0)
      packet->type = ((unsigned char)0xA1);
    else if (strcmp(argv[2], "get") == 0)
      packet->type = ((unsigned char)0xB1);
    else if (strcmp(argv[2], "put") == 0)
      packet->type = ((unsigned char)0xC1);
    packet->length = 6;
  }

  // set up socket with configuration
  char configPath[1024] = "./";
  strcat(configPath, argv[1]);
  FILE *getConfig = fopen(configPath, "rb");
  Config *settings = malloc(sizeof(struct deployment));
  if (!getConfig)
  {
    printf("No configuration file found\n");
    exit(0);
  }
  else
  {
    fscanf(getConfig, "%d ", &settings->n);
    fscanf(getConfig, "%d ", &settings->k);
    fscanf(getConfig, "%u ", &settings->block_size);
    // printf("%d\t%d\t%d\n", settings->n, settings->k, settings->block_size);
    fseek(getConfig, 0L, SEEK_SET);
    char line[256];
    int lineNum = 0;
    while (fgets(line, sizeof(line), getConfig))
    {
      // printf("lineNum: %d\t", lineNum);
      // printf("text: %s\n", line);
      if (lineNum >= 3)
      {
        strcpy(settings->server_list[lineNum - 3].address, strtok(line, ":"));
        strncpy(settings->server_list[lineNum - 3].port, strtok(NULL, "\n"), sizeof(settings->server_list[lineNum - 3].port));
        // printf("Address: %s\t", settings->server_list[lineNum - 3].address);
        // printf("Port: %s\n", settings->server_list[lineNum - 3].port);
      }
      lineNum++;
    }
    fclose(getConfig);
  }

  // connect to all servers
  for (int i = 0; i < settings->n; i++)
  {
    int port = atoi(settings->server_list[i].port);
    settings->server_list[i].sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(settings->server_list[i].address);
    server_addr.sin_port = htons(port);
    //connect to server
    if (connect(settings->server_list[i].sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
      // printf("Connection error: %s (Errno:%d)\n", strerror(errno), errno);
      settings->server_list[i].isActive = 0;
    }
    else
    {
      settings->server_list[i].isActive = 1;
    }
  }

  for (int i = 0; i < settings->n; i++)
  {
    printf("Address: %s\t", settings->server_list[i].address);
    printf("Port: %s\n", settings->server_list[i].port);
    printf("Sd: %d\t", settings->server_list[i].sd);
    printf("IsActive: %d\n", settings->server_list[i].isActive);
  }

  fd_set read_fds;
  fd_set write_fds;
  int max_sd = settings->server_list[settings->n].sd;
  // LIST_REQUEST
  if (packet->type == ((unsigned char)0xA1))
  {
    while (1)
    {
      FD_ZERO(&read_fds);

      for (int i = 0; i < settings->n; i++)
      {
        FD_SET(settings->server_list[i].sd, &read_fds);
      }

      select(max_sd + 1, &read_fds, NULL, NULL, NULL);

      for (int i = 0; i < settings->n; i++)
      {
        if (FD_ISSET(settings->server_list[i].sd, &read_fds))
        {
          // convert packet to network protocol
          payload *list_reply = (payload *)malloc(sizeof(payload));
          struct message_s *convertedPacket;
          convertedPacket = htonp(packet);

          // send LIST_REQUEST
          if (send(settings->server_list[i].sd, convertedPacket, sizeof(struct message_s), 0) < 0)
          {
            printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
          }

          // receive LIST_REPLY
          recv(settings->server_list[i].sd, list_reply, sizeof(payload), 0);
          while (list_reply->done != 'y')
          {
            printf("%s\n", list_reply->fileName);
            recv(settings->server_list[i].sd, list_reply, sizeof(payload), 0);
          }
        }
      }
    }
  }

  // GET_REQUEST
  if (packet->type == (unsigned char)0xB1)
  {

    //   // printf("Trying to send packet: GET_REQUEST\n");

    //   // append payload length to packet->length
    //   packet->length += strlen(argv[4]);
    //   int fileSize;
    //   // convert packet to network protocol
    //   struct message_s *convertedPacket;
    //   convertedPacket = htonp(packet);

    //   // send header packet
    //   if ((send(sd, convertedPacket, sizeof(struct message_s), 0)) < 0)
    //   {
    //     printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
    //     exit(0);
    //   }

    //   // printf("Trying to send packet: GET_REQUEST Payload\n");

    //   // build payload
    //   struct readFile *get = (struct readFile *)malloc(sizeof(struct readFile));
    //   char local[1024] = "./";
    //   strcat(local, argv[4]);
    //   strcpy(get->fileName, argv[4]);
    //   get->done = 'n';

    //   // send payload
    //   if ((send(sd, get, sizeof(struct readFile), 0)) < 0)
    //   {
    //     printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
    //     exit(0);
    //   }

    //   // printf("Sent payload. Waiting for reply...\n");

    //   // receive reply header
    //   if ((recv(sd, convertedPacket, sizeof(struct message_s), 0)) <= 0)
    //   {
    //     printf("Error receving packets: %s (Errno:%d)\n", strerror(errno), errno);
    //     exit(0);
    //   }

    //   // printf("Recieved reply header. Waiting for payload...\n");

    //   // recieve payload
    //   if (convertedPacket->type == 0xB2)
    //   {
    //     // recieve FILE_DATA header
    //     recv(sd, convertedPacket, sizeof(struct message_s), 0);
    //     convertedPacket = ntohp(convertedPacket);
    //     fileSize = convertedPacket->length - 6;

    //     //clear/create file and write to it
    //     FILE *fClear, *fWrite;
    //     fClear = fopen(local, "wb");
    //     fclose(fClear);
    //     fWrite = fopen(local, "ab");

    //     if (recFile(sd, fWrite, fileSize) == 0)
    //       printf("FILE_DATA protocol recieved\n");
    //   }
    //   else
    //     printf("No such file exists...\n");

    //   exit(0);
  }

  // PUT_REQUEST
  if (packet->type == ((unsigned char)0xC1))
  {

    // // printf("Trying to send packet: PUT_REQUEST\n");

    // // check if file exists locally
    // int fileSize;
    // char localPath[1024] = "./";
    // strcat(localPath, argv[4]);
    // FILE *fPUT;
    // fPUT = fopen(localPath, "rb");
    // if (fPUT)
    // {
    //   // append file name to packet->length
    //   packet->length += strlen(argv[4]);
    //   fseek(fPUT, 0L, SEEK_END);
    //   fileSize = ftell(fPUT);
    //   fseek(fPUT, 0L, SEEK_SET);

    //   // convert packet to network protocol
    //   struct message_s *convertedPacket;
    //   convertedPacket = htonp(packet);

    //   // send header packet
    //   if ((send(sd, convertedPacket, sizeof(struct message_s), 0)) < 0)
    //   {
    //     printf("Error sending packets: %s (Errno:%d)\n", strerror(errno), errno);
    //     exit(0);
    //   }

    //   // printf("Trying to send packet: PUT_REQUEST Payload\n");

    //   // build payload
    //   payload *put = (payload *)malloc(sizeof(payload));
    //   strcpy(put->fileName, argv[4]);
    //   put->done = 'n';

    //   // send payload
    //   if ((send(sd, put, sizeof(payload), 0)) < 0)
    //   {
    //     printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
    //     exit(0);
    //   }

    //   // printf("Sent payload. Waiting for reply...\n");

    //   // receive reply header
    //   if ((recv(sd, convertedPacket, sizeof(struct message_s), 0)) <= 0)
    //   {
    //     printf("Error receving packets: %s (Errno:%d)\n", strerror(errno), errno);
    //     exit(0);
    //   }

    //   // printf("Recieved reply header. Trying to send FILE_DATA header...\n");

    //   // if server responds, send file
    //   if (convertedPacket->type == ((unsigned char)0xC2))
    //   {
    //     //build FILE_DATA header
    //     unsigned char myftp[6] = "myftp";
    //     myftp[5] = '\0';
    //     memcpy(convertedPacket->protocol, myftp, 5);
    //     convertedPacket->type = 0xFF;
    //     convertedPacket->length = 6 + fileSize;
    //     convertedPacket = htonp(convertedPacket);

    //     // send FILE_DATA header
    //     if ((send(sd, convertedPacket, sizeof(struct message_s), 0)) < 0)
    //     {
    //       printf("Error sending packets: %s (Errno:%d)\n", strerror(errno), errno);
    //       exit(0);
    //     }

    //     // printf("Trying to send FILE_DATA Payload\n");

    //     if (sendFile(sd, fPUT, fileSize) == 0)
    //       printf("FILE_DATA protocol sent\n");
    //   }
    //   // else, exit thread
    //   else
    //     printf("No reply from server\n");

    //   return 0;
    // }
    // else
    //   printf("No such file exists locally\n");

    // return 0;
  }
  return 0;
}
