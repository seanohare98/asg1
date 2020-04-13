#include "myftpclient.h"

int main(int argc, char **argv)
{
  // state management variables
  struct message_s *packet = malloc(sizeof(struct message_s));
  fd_set read_fds, write_fds;
  int available = 0, max_sd = 0;

  // validate arguments
  if (argc < 3)
  {
    printf("Error: Too few arguments\n");
    exit(0);
  }

  // validate file arguments
  else if (argc < 4 && (strcmp(argv[2], "put") == 0 || strcmp(argv[2], "get") == 0))
  {
    printf("Error: Too few arguments (no file name)\n");
    exit(0);
  }

  // assign protocol
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

  // parse configuration file
  char configPath[1024] = "./";
  strcat(configPath, argv[1]);
  FILE *getConfig = fopen(configPath, "rb");
  Config *settings = malloc(sizeof(struct deployment));

  // no configuration error
  if (!getConfig)
  {
    printf("No configuration file found\n");
    exit(0);
  }
  // use configuration file
  else
  {
    // scan n, k, block_size
    fscanf(getConfig, "%d ", &settings->n);
    fscanf(getConfig, "%d ", &settings->k);
    fscanf(getConfig, "%ld ", &settings->block_size);
    fseek(getConfig, 0L, SEEK_SET);
    char line[256];
    int lineNum = 0;

    // scan server info
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
  }

  // check server connections
  for (int i = 0; i < settings->n; i++)
  {
    // create socket
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    int port = (short)atoi(settings->server_list[i].port);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(settings->server_list[i].address);
    server_addr.sin_port = htons(port);

    // attempt to connect
    settings->server_list[i].status = connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // error connecting
    if (settings->server_list[i].status < 0)
    {
      // printf("Error: %s (Errno:%d)\n", strerror(errno), errno);
      close(sd);
    }

    // record active servers
    else
    {
      // fcntl(sd, F_SETFL, O_NONBLOCK);
      settings->sd[available] = sd;
      max_sd = max(max_sd, sd);
      // printf("SD: %d\t", settings->sd[available]);
      available++;
    }
    printf("Address: %s\t", settings->server_list[i].address);
    printf("Port: %s\t", settings->server_list[i].port);
    printf("Connection Status: %d\n", settings->server_list[i].status);
  }

  // LIST_PROTOCOL
  if (packet->type == (unsigned char)0xA1)
  {
    // check server availability
    if (available < 1)
    {
      printf("Not enough servers online for action...\n");
      exit(0);
    }

    while (1)
    {
      // clear fd set
      FD_ZERO(&write_fds);

      // set fd set
      for (int j = 0; j < available; j++)
      {
        FD_SET(settings->sd[j], &write_fds);
      }

      // use select
      select(max_sd + 1, NULL, &write_fds, NULL, NULL);

      // check socket descriptors
      for (int k = 0; k < available; k++)
      {
        if (FD_ISSET(settings->sd[k], &write_fds))
        {
          // request directory contents
          struct message_s *convertedPacket = htonp(packet);
          payload *list_reply = (payload *)malloc(sizeof(payload));
          send(settings->sd[k], convertedPacket, sizeof(struct message_s), 0);

          // receive and print directory contents
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

  // GET_PROTOCOL
  if (packet->type == (unsigned char)0xB1)
  {
    // check server availability
    if (available < settings->k)
    {
      printf("Not enough servers online for action...\n");
      exit(0);
    }

    // state management variables
    double emptySize;
    long fileSize;
    int zeroBlocks, currentStripe = 0, gotBlocks = 0, workNodes = 0, errNodes = 0, serverId = 0, needFileSize = 1, numStripes = 1, failed = (settings->n - available), didGet[available];
    memset(didGet, 0, sizeof(didGet));
    int *goodRows = malloc(sizeof(int) * available);
    int *badRows = malloc(sizeof(int) * failed);
    Stripe *stripeList;

    // construct active/inactive server lists
    while (serverId < settings->n)
    {
      if (settings->server_list[serverId].status == 0)
      {
        goodRows[workNodes] = serverId;
        workNodes++;
      }
      else
      {
        badRows[errNodes] = serverId;
        errNodes++;
      }
      serverId++;
    }

    // get stripes
    while (currentStripe < numStripes)
    {
      // needFileSize = 2 indicates stripes need to be built
      if (needFileSize == 2)
      {
        printf("BUILDING STRIPES\n");
        numStripes = ceil((double)fileSize / (settings->block_size * settings->k));
        emptySize = (numStripes * settings->k * settings->block_size) - fileSize;
        emptySize /= settings->block_size;
        zeroBlocks = floor(emptySize);
        stripeList = malloc(numStripes * sizeof(Stripe));
        for (int idx = 0; idx < numStripes; idx++)
        {
          stripeList[idx].stripe_id = idx;
          stripeList[idx].blocks = malloc(settings->n * sizeof(Block));
          for (int b = 0; b < settings->n; b++)
          {
            stripeList[idx].blocks[b].data = malloc(sizeof(unsigned char) * settings->block_size);
          }
          stripeList[idx].data_block = &stripeList[idx].blocks[0];
          stripeList[idx].parity_block = &stripeList[idx].blocks[settings->k];
          stripeList[idx].encode_matrix = malloc(sizeof(uint8_t) * (settings->n * settings->k));
          stripeList[idx].table = malloc(sizeof(uint8_t) * (32 * settings->k * (settings->n - settings->k)));
        }
        needFileSize = 0;
      }

      // clear fd set
      FD_ZERO(&write_fds);
      int set = 0;
      // set active connections
      for (int j = 0; j < available; j++)
      {
        printf("didGet[%d] == %d\t", j, settings->sd[j]);
        if (didGet[j] != 1)
        {
          FD_SET(settings->sd[j], &write_fds);
          set++;
        }
      }
      printf("TOTAL SET: %d\n", set);
      // use select
      select(max_sd + 1, NULL, &write_fds, NULL, NULL);

      // check socket descriptors
      for (int k = 0; k < set; k++)
      {
        if (FD_ISSET(settings->sd[k], &write_fds))
        {
          // needFileSize = 1 indicates client should request fileSize
          if (needFileSize == 1)
          {
            // request file info
            packet->type = (unsigned char)0xB4;
            struct message_s *convertedPacket = htonp(packet);
            send(settings->sd[k], convertedPacket, sizeof(struct message_s), 0);
            payload *sizeData = malloc(sizeof(payload));
            strcpy(sizeData->fileName, argv[3]);
            send(settings->sd[k], sizeData, sizeof(payload), 0);
            recv(settings->sd[k], sizeData, sizeof(payload), 0);

            // check file exists
            if (sizeData->done == 'y')
            {
              printf("No such file found...\n");
              exit(0);
            }

            // receive fileSize
            fileSize = ntohl(sizeData->fileSize);
            printf("Requesting %s (%ld bytes)\n", argv[3], fileSize);
            needFileSize = 2;
            break;
          }

          // skip unnecessary socket
          else if (didGet[k] == 1)
          {
            printf("Already got from SD: %d\n", settings->sd[k]);
          }
          else
          {
            // request block data
            packet->type = (unsigned char)0xB1;
            struct message_s *convertedPacket = htonp(packet);
            send(settings->sd[k], convertedPacket, sizeof(struct message_s), 0);
            printf("loop: %d\t", k);

            // format = name_stripe
            payload *get = malloc(sizeof(payload));
            char fileLabel[10];
            strcpy(get->fileName, argv[3]);
            strcat(get->fileName, "_");
            sprintf(fileLabel, "%d", currentStripe + 1);
            strcat(get->fileName, fileLabel);
            if (currentStripe == numStripes - 1)
            {
              get->done = 'y';
              printf("get->done (%c)\t", get->done);
            }
            else
              get->done = 'n';

            send(settings->sd[k], get, sizeof(payload), 0);

            // recieve serverId and block data
            payload *serverData = malloc(sizeof(payload));
            recv(settings->sd[k], serverData, sizeof(payload), 0);
            int server_no = ntohs(serverData->server_no) - 1;
            printf("SERVERNO RECIEVED: %d", server_no);

            // get block
            long bytes = recv(settings->sd[k], stripeList[currentStripe].blocks[server_no].data, sizeof(unsigned char) * settings->block_size, 0);
            printf("STRIPE %d of %d, Server: %d, bytes: %ld\n", currentStripe, numStripes, server_no, bytes);
            // check for errors
            if (bytes < 0)
            {
              printf("Something went wrong...\n");
              printf("Error: %s (Errno:%d)\n", strerror(errno), errno);
              exit(0);
            }

            gotBlocks++;
            didGet[k] = 1;

            // move to next stripe
            if (gotBlocks == available)
            {
              currentStripe++;
              gotBlocks = 0;
              memset(didGet, 0, sizeof(didGet));
            }
          }
        }
      }
    }

    // clear file and open for writing
    FILE *clearFile = fopen(argv[3], "wb");
    fclose(clearFile);
    FILE *writeFile = fopen(argv[3], "ab");
    int needsDecode = 0;

    // check for failed blocks
    for (int idx = 0; idx < settings->k; idx++)
    {
      if (settings->server_list[idx].status != 0)
        needsDecode = 1;
    }

    // write stripes to file
    for (int i = 0; i < numStripes; i++)
    {

      // decode failed stripes
      if (needsDecode)
      {
        printf("NEEDS DECODE\n");
        decode_data(settings->n, settings->k, &stripeList[i], settings->block_size, settings, goodRows, badRows, available);
      }

      // write blocks (exclude zero blocks)
      for (int j = 0; j < (settings->k - zeroBlocks); j++)
      {
        // last block case
        if ((i == numStripes - 1) && (j == settings->k - zeroBlocks - 1))
        {
          // calculate size
          double lastBlockEmpty = (long)(emptySize * settings->block_size) - (zeroBlocks * settings->block_size);
          long lastBlockSize = settings->block_size - (long)lastBlockEmpty;

          // write remaining
          fwrite(stripeList[i].blocks[j].data, sizeof(unsigned char), lastBlockSize, writeFile);
          fclose(writeFile);
        }
        // write whole block
        else
        {
          fwrite(stripeList[i].blocks[j].data, sizeof(unsigned char), settings->block_size, writeFile);
        }
      }
    }
    exit(0);
  }

  // PUT_PROTOCOL
  if (packet->type == (unsigned char)0xC1)
  {
    // check server availability
    if (available < settings->n)
    {
      printf("Not enough servers online for action...\n");
      exit(0);
    }

    // check for file
    long fileSize;
    char filePath[1024] = "./";
    strcat(filePath, argv[3]);
    FILE *fGET = fopen(filePath, "rb");

    // no file error
    if (!fGET)
    {
      printf("No file found...\n");
      fclose(fGET);
      exit(0);
    }
    // get fileSize
    fseek(fGET, 0L, SEEK_END);
    fileSize = ftell(fGET);
    fseek(fGET, 0L, SEEK_SET);
    printf("Sending %s (%ld bytes)\n", argv[3], fileSize);

    // chunking calculations
    int numStripes = ceil((double)fileSize / (settings->block_size * settings->k));
    double emptySize = (numStripes * settings->k * settings->block_size) - fileSize;
    emptySize /= settings->block_size;
    int zeroBlocks = floor(emptySize);
    Stripe *stripeList = malloc(numStripes * sizeof(Stripe));

    // allocate block memory
    for (int idx = 0; idx < numStripes; idx++)
    {
      stripeList[idx].stripe_id = idx;
      stripeList[idx].blocks = malloc(settings->n * sizeof(Block));
      for (int b = 0; b < settings->n; b++)
      {
        stripeList[idx].blocks[b].data = malloc(sizeof(unsigned char) * settings->block_size);
        // printf("Stripe %d Block %d data = %p\t", idx, b, stripeList[idx].blocks[b].data);
        // printf("%ld \n", malloc_usable_size(stripeList[idx].blocks[b].data));
      }
      stripeList[idx].data_block = &stripeList[idx].blocks[0];
      stripeList[idx].parity_block = &stripeList[idx].blocks[settings->k];
      stripeList[idx].encode_matrix = malloc(sizeof(uint8_t) * (settings->n * settings->k));
      stripeList[idx].table = malloc(sizeof(uint8_t) * (32 * settings->k * (settings->n - settings->k)));
      // for (int a = 0; a < settings->k; a++)
      // {
      //   printf("Stripe %d data_block %d data = %p\t", idx, a, stripeList[idx].data_block[a].data);
      //   printf("%ld \n", malloc_usable_size(stripeList[idx].data_block[a].data));
      // }

      // for (int c = 0; c < (settings->n - settings->k); c++)
      // {
      //   printf("Stripe %d parity_block %d data = %p\t", idx, c, stripeList[idx].parity_block[c].data);
      //   printf("%ld \n", malloc_usable_size(stripeList[idx].parity_block[c].data));
      // }
      //printf("Number of Stripes: %d\t Empty Space: %f\t Zero Blocks: %d\n", numStripes, emptySize, zeroBlocks);
    }

    // encode file data into stripes
    for (int i = 0; i < numStripes; i++)
    {
      for (int j = 0; j < settings->k; j++)
      {
        int bytes_read = fread(stripeList[i].blocks[j].data, sizeof(unsigned char), settings->block_size, fGET);
        // printf("Read DATA: %s\n", stripeList[i].blocks[j].data);
      }
      encode(settings->n, settings->k, &stripeList[i], settings->block_size);
    }
    fclose(fGET);

    // send stripes
    int currentStripe = 0, sentBlocks = 0, didSend[settings->n];
    memset(didSend, 0, sizeof(didSend));
    while (currentStripe < numStripes)
    {
      // clear fd set
      FD_ZERO(&write_fds);
      int set = 0;
      // set fd set
      for (int j = 0; j < available; j++)
      {
        printf("j=%d\t didSend[j]= %d \t", j, didSend[j]);
        if (didSend[j] != 1)
        {
          FD_SET(settings->sd[j], &write_fds);
          set++;
        }
      }
      printf("TOTAL SET: %d\n", set);
      // use select
      select(max_sd + 1, NULL, &write_fds, NULL, NULL);

      // check descriptors
      for (int k = 0; k < set; k++)
      {
        if (FD_ISSET(settings->sd[k], &write_fds))
        {
          // already sent to sd
          if (didSend[k] == 1)
          {
            printf("Already sent to SD: %d\n", settings->sd[k]);
          }
          else
          {
            printf("loop: %d \t", k);
            // send request
            struct message_s *convertedPacket = htonp(packet);
            send(settings->sd[k], convertedPacket, sizeof(struct message_s), 0);

            // recieve server_no
            payload *serverData = malloc(sizeof(payload));
            recv(settings->sd[k], serverData, sizeof(payload), 0);
            int server_no = ntohs(serverData->server_no) - 1;

            // send fileName and fileSize
            payload *put = malloc(sizeof(payload));
            strcpy(put->fileName, argv[3]);
            strcpy(put->rawName, argv[3]);
            char fileLabel[256];
            strcat(put->fileName, "_");
            sprintf(fileLabel, "%d", currentStripe + 1);
            strcat(put->fileName, fileLabel);
            put->fileSize = htonl(fileSize);
            if (currentStripe == numStripes - 1)
              put->done = 'y';
            else
              put->done = 'n';
            send(settings->sd[k], put, sizeof(payload), 0);

            // send block
            long bytes = send(settings->sd[k], stripeList[currentStripe].blocks[server_no].data, sizeof(unsigned char) * settings->block_size, 0);
            printf("stripe: %d of %d | bytes:%ld | server_no: %d\n", currentStripe + 1, numStripes, bytes, server_no);

            // check for errors
            if (bytes < 0)
            {
              printf("Something went wrong...\n");
              exit(0);
            }

            didSend[k] = 1;
            sentBlocks++;

            // move to next stripe
            if (sentBlocks == settings->n)
            {
              currentStripe++;
              sentBlocks = 0;
              memset(didSend, 0, sizeof(didSend));
            }
          }
        }
      }
    }
    exit(0);
  }
  return 0;
}

uint8_t *decode_data(int n, int k, Stripe *stripe, size_t block_size, Config *settings, int *goodRows, int *badRows, int available)
{
  // generate matrix and tables
  gf_gen_rs_matrix(stripe->encode_matrix, n, k);
  uint8_t *invert_matrix = malloc(sizeof(uint8_t) * (k * k));
  uint8_t *errors_matrix = malloc(sizeof(uint8_t) * (k * k));
  uint8_t *decode_matrix = malloc(sizeof(uint8_t) * (k * k));
  int numFailed = n - available;

  // skip failed rows to create error matrix
  for (int i = 0; i < k; i++)
  {
    int r = goodRows[i];
    for (int j = 0; j < k; j++)
    {
      errors_matrix[k * i + j] = stripe->encode_matrix[k * r + j];
    }
  }

  // invert error matrix
  gf_invert_matrix(errors_matrix, invert_matrix, k);

  // construct decode matrix
  for (int i = 0; i < numFailed; i++)
  {
    int r = badRows[i];
    for (int j = 0; j < k; j++)
    {
      decode_matrix[k * i + j] = invert_matrix[k * r + j];
    }
  }
  // fill excess with 0s
  for (int i = numFailed; i < k; i++)
  {
    for (int j = 0; j < k; j++)
    {
      decode_matrix[k * i + j] = 0;
    }
  }

  // generate expanded tables
  ec_init_tables(k, numFailed, decode_matrix, stripe->table);

  // arrange blocks into src and dest
  int src = 0, dest = 0;
  unsigned char **srcBlocks = malloc(sizeof(unsigned char **) * (n - numFailed));
  unsigned char **destBlocks = malloc(sizeof(unsigned char **) * numFailed);
  while (src + dest < n)
  {
    if (settings->server_list[src + dest].status == 0)
    {
      srcBlocks[src] = stripe->blocks[src + dest].data;
      src++;
    }
    else
    {
      destBlocks[dest] = stripe->blocks[src + dest].data;
      dest++;
    }
  }

  // decode missing blocks
  ec_encode_data(block_size, k, numFailed, stripe->table, srcBlocks, destBlocks);
  return stripe->table;
}