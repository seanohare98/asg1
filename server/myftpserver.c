#include "myftpserver.h"

int main(int argc, char **argv)
{
  // validate arguments
  if (argc < 2)
  {
    printf("Error: Too few arguments\n");
    exit(1);
  }

  // initiate thread mutex
  threads.size = 0;
  pthread_mutex_init(&thread_mutex, NULL);

  // parse configuration file
  char configPath[1024] = "./";
  strcat(configPath, argv[1]);
  FILE *getConfig = fopen(configPath, "rb");
  Config *settings = (struct deployment *)malloc(sizeof(struct deployment));

  // no configuration error
  if (!getConfig)
  {
    printf("No configuration file found\n");
    fclose(getConfig);
    exit(0);
  }

  // use configuration file
  else
  {
    // scan n, k, server_id, block_size
    fscanf(getConfig, "%d", &settings->n);
    fscanf(getConfig, "%d", &settings->k);
    fscanf(getConfig, "%d", &settings->server_id);
    fscanf(getConfig, "%ld", &settings->block_size);
    fseek(getConfig, 0L, SEEK_SET);
    char line[256];
    int lineNum = 0;

    // scan port number
    while (fgets(line, sizeof(line), getConfig))
    {
      if (lineNum == 4)
        strcpy(settings->port, line);
      lineNum++;
    }
    fclose(getConfig);
  }

  // create socket
  int port = atoi(settings->port);
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  long val = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1)
  {
    perror("Error: setsockopt\n");
    exit(1);
  }
  int client_sd, new_sd;
  socklen_t addr_len;
  struct sockaddr_in server_addr, client_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  // bind socket
  if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    printf("Bind Error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }

  // listen for connections (backlog of 10)
  if (listen(sd, 10) < 0)
  {
    printf("Listen Error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }
  printf("Server listening on port %d\n", port);
  addr_len = sizeof(client_addr);

  while (1)
  {
    // receive connection
    client_sd = accept(sd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_sd < 0)
    {
      printf("Accept Error: %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }

    // check thread availability
    else if (threads.size == MAX_CONNECTIONS - 1)
    {
      printf("Too many active connections...\n");
      continue;
    }

    // create worker thread
    else
    {
      threads.workers[threads.size].sd = client_sd;
      threads.workers[threads.size].id = threads.size;
      threads.workers[threads.size].settings = settings;
      if (pthread_create(&threads.workers[threads.size].thread, NULL, connection_handler, (void *)&threads.workers[threads.size]) != 0)
      {
        printf("Failed to make thread\n");
      }
      else
      {
        pthread_mutex_lock(&thread_mutex);
        threads.size++;
        printf("ACTIVE THREADS: %d\t\t", threads.size);
        pthread_mutex_unlock(&thread_mutex);
      }
    }
  }
  return 0;
}

// worker threads
void *connection_handler(void *sDescriptor)
{
  struct threadData data = *(struct threadData *)sDescriptor;
  struct message_s *packet = (struct message_s *)malloc(sizeof(struct message_s));
  struct message_s *convertedPacket;
  int bytes, closeSocket = 0;

  printf("CLIENT (%d):\t\t", data.sd);

  while (closeSocket != 1)
  {
    // recieve protocol
    bytes = recv(data.sd, packet, sizeof(struct message_s), 0);

    // client inactive
    if (bytes == 0)
    {
      closeSocket = 1;
      break;
    }

    // convert header into host protocol
    convertedPacket = ntohp(packet);

    // LIST_PROTOCOL
    if (convertedPacket->type == ((unsigned char)0xA1))
    {
      payload *reply = (payload *)malloc(sizeof(payload));
      memset(reply->fileName, '\0', sizeof(reply->fileName));
      DIR *dir;
      struct dirent *sd;

      // list dir contents
      char dirPath[1024] = "./data/";
      // char buff[10];
      // sprintf(buff, "%d", data.settings->server_id);
      // strcat(dirPath, buff);
      // strcat(dirPath, "/");
      if ((dir = opendir(dirPath)) != NULL)
      {
        // while director isn't empty, send payload with null terminated file name
        while ((sd = readdir(dir)) != NULL)
        {
          size_t strLength = strlen(sd->d_name);
          size_t metaLength = strlen("_meta");

          // exclude _meta suffix
          if (strncmp(sd->d_name + strLength - metaLength, "_meta", metaLength) == 0)
          {
            strncpy(reply->fileName, sd->d_name, strLength - metaLength);
            reply->done = 'n';
            if (send(data.sd, reply, sizeof(payload), 0) < 0)
            {
              printf("Error sending packet(s): %s (Errno:%d)\t\t", strerror(errno), errno);
              closeSocket = 1;
              break;
            }
          }

          // include . and ..
          if (strncmp(sd->d_name, ".", strLength) == 0 || strncmp(sd->d_name, "..", strLength) == 0)
          {
            strcpy(reply->fileName, sd->d_name);
            reply->done = 'n';
            if (send(data.sd, reply, sizeof(payload), 0) < 0)
            {
              printf("Error sending packet(s): %s (Errno:%d)\t\t", strerror(errno), errno);
              closeSocket = 1;
              break;
            }
          }
        }
      }
      reply->done = 'y';
      if (send(data.sd, reply, sizeof(payload), 0) < 0)
      {
        printf("Error sending packet(s): %s (Errno:%d)\t\t", strerror(errno), errno);
        closeSocket = 1;
        break;
      }
      closedir(dir);
      closeSocket = 1;
      printf("LIST_PROTOCOL\t\t");
      break;
    }

    // GET_PROTOCOL
    else if (convertedPacket->type == ((unsigned char)0xB1))
    {
      char filePath[1024] = "./data/";
      // char pathLabel[10];
      // sprintf(pathLabel, "%d", data.settings->server_id);
      // strcat(filePath, pathLabel);
      // strcat(filePath, "/");
      struct readFile *get = malloc(sizeof(struct readFile));
      memset(get->fileName, '\0', sizeof(get->fileName));

      // recieve fileName
      recv(data.sd, get, sizeof(struct readFile), 0);

      // send server ID
      struct readFile *sendServerNo = malloc(sizeof(struct readFile));
      sendServerNo->server_no = htons(data.settings->server_id);
      send(data.sd, sendServerNo, sizeof(struct readFile), 0);

      //clear file and open for writing
      FILE *fRead;
      char targetFile[255];
      strcpy(targetFile, filePath);
      strcat(targetFile, get->fileName);
      fRead = fopen(targetFile, "rb");

      // read block data from system
      unsigned char *replyBlock = malloc(sizeof(unsigned char) * data.settings->block_size);
      if (fread(replyBlock, sizeof(unsigned char), data.settings->block_size, fRead) != data.settings->block_size)
      {
        printf("Reading Error\t\t");
        closeSocket = 1;
        pthread_mutex_lock(&thread_mutex);
        threads.size--;
        pthread_mutex_unlock(&thread_mutex);
        break;
      }
      fclose(fRead);

      // send block
      long bytes_sent = 0;
      while (bytes_sent < data.settings->block_size)
      {
        bytes_sent = send(data.sd, replyBlock, sizeof(unsigned char) * data.settings->block_size, 0);

        if (bytes_sent < 0 || bytes_sent < data.settings->block_size)
        {
          printf("ERRORRRR: %d", bytes_sent);
          printf("Somethings went wrong...\t\t");
          closeSocket = 1;
          break;
        }
      }

      // close connection
      if (get->done == 'y')
      {
        printf("GET_PROTOCOL\t\t");
        closeSocket = 1;
        break;
      }
    }

    // FILE_SIZE_PROTOCOL
    else if (convertedPacket->type == ((unsigned char)0xB4))
    {
      struct readFile *getSize = malloc(sizeof(struct readFile));
      memset(getSize->fileName, '\0', sizeof(getSize->fileName));
      recv(data.sd, getSize, sizeof(struct readFile), 0);
      char filePath[1024] = "./data/";
      // char pathLabel[10];
      // sprintf(pathLabel, "%d", data.settings->server_id);
      // strcat(filePath, pathLabel);
      // strcat(filePath, "/");
      strcat(filePath, getSize->fileName);
      strcat(filePath, "_meta");
      FILE *metaRead = fopen(filePath, "rb");
      struct readFile *replySize = malloc(sizeof(struct readFile));
      if (!metaRead)
      {
        replySize->done = 'y';
        send(data.sd, replySize, sizeof(struct readFile), 0);
        printf("File not found error!\t\t");
        fclose(metaRead);
        closeSocket = 1;
        break;
      }
      else
      {
        fscanf(metaRead, "%ld ", &replySize->fileSize);
        replySize->fileSize = htonl(replySize->fileSize);
        send(data.sd, replySize, sizeof(struct readFile), 0);
        printf("FILE_SIZE_PROTOCOL\t");
        fclose(metaRead);
      }
    }

    // PUT_PROTOCOL
    else if (convertedPacket->type == ((unsigned char)0xC1))
    {

      // send server ID
      struct readFile *sendServerNo = malloc(sizeof(struct readFile));
      sendServerNo->server_no = htons(data.settings->server_id);
      send(data.sd, sendServerNo, sizeof(struct readFile), 0);

      char filePath[1024] = "./data/";
      // char pathLabel[10];
      // sprintf(pathLabel, "%d", data.settings->server_id);
      // strcat(filePath, pathLabel);
      // strcat(filePath, "/");
      struct readFile *get = malloc(sizeof(struct readFile));
      memset(get->fileName, '\0', sizeof(get->fileName));

      // receive fileName and fileSize
      recv(data.sd, get, sizeof(struct readFile), 0);
      long fileSize = ntohl(get->fileSize);

      // clear file and open for writing
      FILE *fClear, *fWrite;
      char targetFile[255];
      strcpy(targetFile, filePath);
      strcat(targetFile, get->fileName);
      fClear = fopen(targetFile, "wb");
      fclose(fClear);
      fWrite = fopen(targetFile, "ab");

      // get block
      int bytes;
      long bytes_read = 0;
      unsigned char *getBlock = malloc(sizeof(unsigned char) * data.settings->block_size);
      while (bytes_read < data.settings->block_size)
      {
        bytes = recv(data.sd, getBlock, sizeof(unsigned char) * data.settings->block_size, 0);

        // check for errors
        if (bytes < 0)
        {
          printf("Somethings went wrong...\t\t");
          printf("BYTES: %d\n", bytes);
          closeSocket = 1;
          break;
        }
        bytes_read += fwrite(getBlock, sizeof(unsigned char), data.settings->block_size, fWrite);
      }
      fclose(fWrite);

      // write metadata and close connection
      if (get->done == 'y')
      {
        FILE *metaClear, *metaWrite;
        strcat(filePath, get->rawName);
        strcat(filePath, "_meta");
        metaClear = fopen(filePath, "wb");
        fclose(metaClear);
        metaWrite = fopen(filePath, "ab");
        char originalSize[10];
        sprintf(originalSize, "%ld", fileSize);
        int bytes_write = fwrite(originalSize, sizeof(unsigned char), strlen(originalSize), metaWrite);
        fclose(metaWrite);
        printf("PUT_PROTOCOL\t\t");
        printf("METADATA (size = %s)\t", originalSize);
        closeSocket = 1;
        break;
      }
    }
  }
  printf("CONNECTION CLOSED\n");
  pthread_mutex_lock(&thread_mutex);
  threads.size--;
  pthread_mutex_unlock(&thread_mutex);
  return 0;
};