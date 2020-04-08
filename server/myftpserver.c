#include "myftpserver.h"

int main(int argc, char **argv)
{
  // check if port was provided
  if (argc < 2)
  {
    printf("Error: Too few arguments\n");
    exit(1);
  }
  // set up threadList and initiate mutex
  threads.size = 0;
  pthread_mutex_init(&thread_mutex, NULL);

  // set up socket with configuration
  char configPath[1024] = "./";
  strcat(configPath, argv[1]);
  FILE *getConfig = fopen(configPath, "rb");
  Config *settings = (struct deployment *)malloc(sizeof(struct deployment));
  if (!getConfig)
  {
    printf("No configuration file found\n");
    exit(0);
  }
  else
  {
    char line[256];
    int lineNum = 0;
    fscanf(getConfig, "%d", &settings->n);
    fscanf(getConfig, "%d", &settings->k);
    fscanf(getConfig, "%d", &settings->server_id);
    fscanf(getConfig, "%u", &settings->block_size);
    // printf("%d\t%d\t%d\t%u\n", settings->n, settings->k, settings->server_id, settings->block_size);
    fseek(getConfig, 0L, SEEK_SET);
    while (fgets(line, sizeof(line), getConfig))
    {
      // printf("lineNum: %d\t", lineNum);
      // printf("text: %s\n", line);
      if (lineNum == 4)
      {
        strcpy(settings->port, line);
        // printf("Port: %s\n", settings->port);
      }
      lineNum++;
    }
    fclose(getConfig);
  }

  int port = atoi(settings->port);
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  long val = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1)
  {
    perror("Error: Setsockopt\n");
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

  // keep socket open
  printf("Server listening on port %d\n", port);
  addr_len = sizeof(client_addr);
  while (1)
  {
    client_sd = accept(sd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_sd < 0)
    {
      printf("Accept Error: %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }
    // check if max connections reached
    else if (threads.size == MAX_CONNECTIONS - 1)
    {
      printf("Too many active connections...\n");
      continue;
    }
    // create new thread from group
    else
    {
      threads.workers[threads.size].sd = client_sd;
      threads.workers[threads.size].id = threads.size;
      if (pthread_create(&threads.workers[threads.size].thread, NULL, connection_handler, (void *)&threads.workers[threads.size]) != 0)
      {
        printf("Failed to make thread\n");
      }
      else
      {
        //move pointer to next available worker thread
        pthread_mutex_lock(&thread_mutex);
        threads.size++;
        pthread_mutex_unlock(&thread_mutex);
        // printf("Threads in use: %d\n", threads.size);
      }
    }
  }

  return 0;
}

// worker threads to handle client connections
void *connection_handler(void *sDescriptor)
{
  // set up descriptor, packet, convertedPacket
  // printf("New thread created\n");
  int bytes;
  struct threadData data = *(struct threadData *)sDescriptor;
  struct message_s *packet = (struct message_s *)malloc(sizeof(struct message_s));
  struct message_s *convertedPacket;

  while (1)
  {
    // recieve header
    bytes = recv(data.sd, packet, sizeof(struct message_s), 0);

    // nothing being sent from client
    if (bytes == 0)
    {
      printf("Connection closed\n");
      pthread_mutex_lock(&thread_mutex);
      threads.size--;
      pthread_mutex_unlock(&thread_mutex);
      break;
    }

    // convert header into host protocol
    convertedPacket = ntohp(packet);

    // LIST_REPLY
    if (convertedPacket->type == ((unsigned char)0xA1))
    {
      // send LIST_REPLY
      payload *reply = (payload *)malloc(sizeof(payload));
      memset(reply->fileName, '\0', sizeof(reply->fileName));
      DIR *dir;
      struct dirent *sd;

      // list dir contents
      if ((dir = opendir("./data")) != NULL)
      {
        // while director isn't empty, send payload with null terminated file name
        while ((sd = readdir(dir)) != NULL)
        {
          reply->done = 'n';
          strcpy(reply->fileName, sd->d_name);
          if (send(data.sd, reply, sizeof(payload), 0) < 0)
          {
            printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
          }
        }
      }
      reply->done = 'y';
      if (send(data.sd, reply, sizeof(payload), 0) < 0)
      {
        printf("Error sending packet(s): %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
      }
      closedir(dir);
      printf("LIST_REPLY protocol sent\n");
      pthread_mutex_lock(&thread_mutex);
      threads.size--;
      pthread_mutex_unlock(&thread_mutex);
      break;
    }

    // GET_REPLY
    else if (convertedPacket->type == ((unsigned char)0xB1))
    {
      int fileSize;
      char filePath[1024] = "./data/";
      FILE *fGET;
      struct readFile *get = (struct readFile *)malloc(sizeof(struct readFile));
      memset(get->fileName, '\0', sizeof(get->fileName));

      // printf("Waiting for payload...\n");

      // recieve payload
      recv(data.sd, get, sizeof(struct readFile), 0);

      // printf("Recieved payload\n");

      //build reply
      struct message_s *newPacket = (struct message_s *)malloc(sizeof(struct message_s));
      newPacket->length = 6;

      //check if file exists
      strcat(filePath, get->fileName);
      fGET = fopen(filePath, "rb");

      if (!fGET)
      {
        newPacket->type = 0xB3; //file doesn't exist
      }
      else
      {
        newPacket->type = 0xB2; //file does exist
        fseek(fGET, 0L, SEEK_END);
        fileSize = ftell(fGET);
        fseek(fGET, 0L, SEEK_SET);
      }

      // convert reply to network protocol
      newPacket->length += strlen(get->fileName);
      newPacket = htonp(newPacket);

      // printf("Trying to send packet: GET_REPLY\n");

      // send reply
      if ((send(data.sd, newPacket, sizeof(struct message_s), 0)) < 0)
      {
        printf("Error sending packets: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
      }

      // printf("Sent packet. Building payload...\n");

      // if file exists, send it
      if (newPacket->type == 0xB2)
      {

        //build FILE_DATA header
        unsigned char myftp[6] = "myftp";
        myftp[5] = '\0';
        memcpy(newPacket->protocol, myftp, 5);
        newPacket->type = 0xFF;
        newPacket->length = 6 + fileSize;
        newPacket = htonp(newPacket);

        // send FILE_DATA header
        if ((send(data.sd, newPacket, sizeof(struct message_s), 0)) < 0)
        {
          printf("Error sending packets: %s (Errno:%d)\n", strerror(errno), errno);
          exit(0);
        }

        // printf("Trying to send FILE_DATA PAyload\n");

        //send file
        if (sendFile(data.sd, fGET, fileSize) == 0)
          printf("FILE_DATA protocol sent\n");
      }
      // else, exit thread
      else
        printf("GET_REPLY protocol sent\n");
      pthread_mutex_lock(&thread_mutex);
      threads.size--;
      pthread_mutex_unlock(&thread_mutex);
      break;
    }

    // PUT_REPLY
    else if (convertedPacket->type == ((unsigned char)0xC1))
    {
      int fileSize;
      char filePath[1024] = "./data/";
      struct readFile *get = (struct readFile *)malloc(sizeof(struct readFile));
      memset(get->fileName, '\0', sizeof(get->fileName));

      // printf("Waiting for payload...\n");

      // recieve payload
      recv(data.sd, get, sizeof(struct readFile), 0);

      // printf("Recieved payload\n");

      //clear file and open for writing
      FILE *fClear, *fWrite;
      strcat(filePath, get->fileName);
      fClear = fopen(filePath, "wb");
      fclose(fClear);
      fWrite = fopen(filePath, "ab");

      //build reply
      struct message_s *newPacket = (struct message_s *)malloc(sizeof(struct message_s));
      unsigned char myftp[6] = "myftp";
      myftp[5] = '\0';
      memcpy(newPacket->protocol, myftp, 5);
      newPacket->type = 0xC2;
      newPacket->length = 6;

      // convert newPacket to network protocol
      newPacket = htonp(newPacket);

      // send reply
      if ((send(data.sd, newPacket, sizeof(struct message_s), 0)) < 0)
      {
        printf("Error sending packets: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
      }

      // printf("Sent packet. Waiting for reply...\n");

      // recieve FILE_DATA header
      recv(data.sd, newPacket, sizeof(struct message_s), 0);
      newPacket = ntohp(newPacket);
      fileSize = newPacket->length - 6;

      // recieve file;
      if (newPacket->type == ((unsigned char)0xFF))
      {
        if (recFile(data.sd, fWrite, fileSize) == 0)
          printf("FILE_DATA protocol recieved\n");
      }

      fclose(fWrite);
      pthread_mutex_lock(&thread_mutex);
      threads.size--;
      pthread_mutex_unlock(&thread_mutex);
      break;
    }
  }
  return 0;
}