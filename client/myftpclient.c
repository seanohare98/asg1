#include "myftpclient.h"

int main(int argc, char **argv)
{
  struct message_s *packet = malloc(sizeof(struct message_s));
  fd_set read_fds;
  fd_set write_fds;
  int available = 0;
  int max_sd = 0;

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
      packet->type = (unsigned char)0xA1;
    else if (strcmp(argv[2], "get") == 0)
      packet->type = (unsigned char)0xB1;
    else if (strcmp(argv[2], "put") == 0)
      packet->type = (unsigned char)0xC1;
    packet->length = 6;
  }

  // parse clientconfig.txt
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
    fscanf(getConfig, "%d ", &settings->block_size);
    fseek(getConfig, 0L, SEEK_SET);
    char line[256];
    int lineNum = 0;
    while (fgets(line, sizeof(line), getConfig))
    {
      if (lineNum >= 3)
      {
        strcpy(settings->server_list[lineNum - 3].address, strtok(line, ":"));
        strncpy(settings->server_list[lineNum - 3].port, strtok(NULL, "\n"), sizeof(settings->server_list[lineNum - 3].port));
      }
      lineNum++;
    }
    fclose(getConfig);
    // printf("%d\t%d\t%d\n", settings->n, settings->k, settings->block_size);
  }

  for (int i = 0; i < settings->n; i++)
  {
    // get address and port
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    int port = atoi(settings->server_list[i].port);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(settings->server_list[i].address);
    server_addr.sin_port = htons(port);

    //connect to server
    settings->server_list[i].status = connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (settings->server_list[i].status < 0)
    {
      printf("Error: %s (Errno:%d)\n", strerror(errno), errno);
      close(sd);
    }
    else
    {
      // fcntl(sd, F_SETFL, O_NONBLOCK);
      settings->sd[available] = sd;
      max_sd = max(max_sd, sd);
      printf("SD: %d\t", settings->sd[available]);
      available++;
    }
    printf("Address: %s\t", settings->server_list[i].address);
    printf("Port: %s\t", settings->server_list[i].port);
    printf("Connection Status: %d\n", settings->server_list[i].status);
  }

  // LIST_REQUEST
  if (packet->type == (unsigned char)0xA1)
  {
    while (1)
    {
      FD_ZERO(&write_fds);
      for (int j = 0; j < available; j++)
      {
        FD_SET(settings->sd[j], &write_fds);
      }
      select(max_sd + 1, NULL, &write_fds, NULL, NULL);
      for (int k = 0; k < available; k++)
      {
        if (FD_ISSET(settings->sd[k], &write_fds))
        {
          struct message_s *convertedPacket = htonp(packet);
          payload *list_reply = (payload *)malloc(sizeof(payload));
          send(settings->sd[k], convertedPacket, sizeof(struct message_s), 0);
          recv(settings->sd[k], list_reply, sizeof(payload), 0);
          while (list_reply->done != 'y')
          {
            printf("%s\n", list_reply->fileName);
            recv(settings->sd[k], list_reply, sizeof(payload), 0);
          }
          exit(0);
        }
      }
    }
  }
  // GET_REQUEST
  if (packet->type == (unsigned char)0xB1)
  {
    while (1)
    {
      FD_ZERO(&write_fds);
      for (int j = 0; j < available; j++)
      {
        FD_SET(settings->sd[j], &write_fds);
      }
      select(max_sd + 1, NULL, &write_fds, NULL, NULL);
      for (int k = 0; k < available; k++)
      {
        if (FD_ISSET(settings->sd[k], &write_fds))
        {
          int fileSize;
          struct message_s *convertedPacket = htonp(packet);

          // send header packet
          if ((send(settings->sd[k], convertedPacket, sizeof(struct message_s), 0)) < 0)
          {
            printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
          }

          // build payload
          struct readFile *get = (struct readFile *)malloc(sizeof(struct readFile));
          char local[1024] = "./";
          strcat(local, argv[3]);
          strcpy(get->fileName, argv[3]);
          get->done = 'n';

          // send payload
          if ((send(settings->sd[k], get, sizeof(struct readFile), 0)) < 0)
          {
            printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
          }

          // receive reply header
          if ((recv(settings->sd[k], convertedPacket, sizeof(struct message_s), 0)) <= 0)
          {
            printf("Error receving packets: %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
          }

          // recieve payload
          if (convertedPacket->type == 0xB2)
          {
            // recieve FILE_DATA header
            recv(settings->sd[k], convertedPacket, sizeof(struct message_s), 0);
            convertedPacket = ntohp(convertedPacket);
            fileSize = convertedPacket->length - 6;

            //clear/create file and write to it
            FILE *fClear, *fWrite;
            fClear = fopen(local, "wb");
            fclose(fClear);
            fWrite = fopen(local, "ab");

            if (recFile(settings->sd[k], fWrite, fileSize) == 0)
              printf("FILE_DATA protocol recieved\n");
          }
          else
            printf("No such file exists...\n");
          exit(0);
        }
      }
    }
  }
  // PUT_REQUEST
  if (packet->type == (unsigned char)0xC1)
  {

    double fileSize;
    char filePath[1024] = "./";
    strcat(filePath, argv[3]);
    FILE *fGET = fopen(filePath, "rb");
    if (!fGET)
    {
      printf("No such file exists...\n");
      exit(0);
    }
    fseek(fGET, 0L, SEEK_END);
    fileSize = ftell(fGET);
    fseek(fGET, 0L, SEEK_SET);
    printf("FILE %s is %f BYTES\n", filePath, fileSize);
    // printf("%f testing\n", fileSize / (settings->block_size * settings->k));
    int numStripes = ceil(fileSize / (settings->block_size * settings->k));
    double emptySize = (numStripes * settings->k * settings->block_size) - fileSize;
    emptySize /= settings->block_size;
    int zeroBlocks = floor(emptySize);
    printf("Number of Stripes: %d\t Empty Space: %f\t Zero Blocks: %d\n", numStripes, emptySize, zeroBlocks);
    Stripe *stripeList = malloc(numStripes * sizeof(Stripe));
    for (int i = 0; i < numStripes; ++i)
    {
      stripeList[i].stripe_id = i;
      // stripeList[i].blocks = malloc(settings->n * sizeof(unsigned char) * settings->block_size);
      for (int j = 0; j < settings->n; j++)
      {
        stripeList[i].blocks[j] = malloc(sizeof(unsigned char) * settings->block_size);
        printf("%d %d of blocks = %p\t", i, j, &stripeList[i].blocks[j]);
        printf("%ld \n", malloc_usable_size(&stripeList[i].blocks[j]));
      }
      stripeList[i].data_block = &stripeList[i].blocks[0];
      stripeList[i].parity_block = &stripeList[i].blocks[settings->k];
      stripeList[i].encode_matrix = malloc(sizeof(uint8_t) * (settings->n * settings->k));
      stripeList[i].table = malloc(sizeof(uint8_t) * (32 * settings->k * (settings->n - settings->k)));

      for (int j = 0; j < settings->k; j++)
      {
        // stripeList[i].data_block[j] = malloc(settings->block_size);
        printf("%d %d of data_block = %p\t", i, j, &stripeList[i].data_block[j]);
        printf("%ld \n", malloc_usable_size(&stripeList[i].data_block[j]));
      }

      for (int j = 0; j < (settings->n - settings->k); j++)
      {
        // stripeList[i].parity_block[j] = malloc(settings->block_size);
        printf("%d %d of parity_block = %p\t", i, j, &stripeList[i].parity_block[j]);
        printf("%ld \n", malloc_usable_size(&stripeList[i].parity_block[j]));
      }

      // encode(settings->n, settings->k, &stripeList[i], settings->block_size);
    }
    char buff[256];
    int bytes_read = fread(buff, sizeof(unsigned char), settings->block_size, fGET);
    printf("%d    dfsdfsd", bytes_read);

    while (1)
    {
      FD_ZERO(&write_fds);
      for (int j = 0; j < available; j++)
      {
        FD_SET(settings->sd[j], &write_fds);
      }
      select(max_sd + 1, NULL, &write_fds, NULL, NULL);
      for (int k = 0; k < settings->n; k++)
      {
        if (FD_ISSET(settings->sd[k], &write_fds))
        {
          exit(0);
        }
      }
    }
  }
  return 0;
}

// printf("TESTING SD %d\n", settings->sd[k]);

// void list(int sd, struct message_s *packet)
// {
// convert packet to network protocol
//   struct message_s *convertedPacket;
//   convertedPacket = htonp(packet);
//   payload *list_reply = (payload *)malloc(sizeof(payload));
//   if (send(sd, convertedPacket, sizeof(struct message_s), 0) < 0)
//   {
//     printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
//     exit(0);
//   }
//   recv(sd, list_reply, sizeof(payload), 0);

//   while (list_reply->done != 'y')
//   {
//     printf("%s\n", list_reply->fileName);
//     recv(sd, list_reply, sizeof(payload), 0);
//   }
// }
// GET_REQUEST
// if (packet->type == (unsigned char)0xB1)
// {
// printf("Trying to send packet: GET_REQUEST\n");

// append payload length to packet->length
// packet->length += strlen(argv[4]);
// int fileSize;
// // convert packet to network protocol
// struct message_s *convertedPacket;
// convertedPacket = htonp(packet);

// // send header packet
// if ((send(sd, convertedPacket, sizeof(struct message_s), 0)) < 0)
// {
//   printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
//   exit(0);
// }

// // printf("Trying to send packet: GET_REQUEST Payload\n");

// // build payload
// struct readFile *get = (struct readFile *)malloc(sizeof(struct readFile));
// char local[1024] = "./";
// strcat(local, argv[4]);
// strcpy(get->fileName, argv[4]);
// get->done = 'n';

// // send payload
// if ((send(sd, get, sizeof(struct readFile), 0)) < 0)
// {
//   printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
//   exit(0);
// }

// // printf("Sent payload. Waiting for reply...\n");

// // receive reply header
// if ((recv(sd, convertedPacket, sizeof(struct message_s), 0)) <= 0)
// {
//   printf("Error receving packets: %s (Errno:%d)\n", strerror(errno), errno);
//   exit(0);
// }

// // printf("Recieved reply header. Waiting for payload...\n");

// // recieve payload
// if (convertedPacket->type == 0xB2)
// {
//   // recieve FILE_DATA header
//   recv(sd, convertedPacket, sizeof(struct message_s), 0);
//   convertedPacket = ntohp(convertedPacket);
//   fileSize = convertedPacket->length - 6;

//   //clear/create file and write to it
//   FILE *fClear, *fWrite;
//   fClear = fopen(local, "wb");
//   fclose(fClear);
//   fWrite = fopen(local, "ab");

//   if (recFile(sd, fWrite, fileSize) == 0)
//     printf("FILE_DATA protocol recieved\n");
// }
// else
//   printf("No such file exists...\n");

// return 0;
// }

// // PUT_REQUEST
// if (packet->type == (unsigned char)0xC1)
// {
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

//   return 0;
// }
//   return 0;
// }

// int result;
// socklen_t result_len = sizeof(result);
// if (getsockopt(settings->sd[k], SOL_SOCKET, SO_ERROR, &result, &result_len) != -1)
// {
//   printf("SELECTED result: %d:\n", result);
//   printf("Address: %s\t", settings->server_list[k].address);
//   printf("Port: %s\t", settings->server_list[k].port);
//   printf("Sd: %d\t", settings->sd[k]);
//   printf("Connection Status: %d\n", settings->server_list[k].status);
// }
