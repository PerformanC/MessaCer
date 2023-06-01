#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include "../libs/jsmn.h"
#include "../libs/jsmn-find.h"
#include "../libs/cthreads.h"

int fd;
char *serverPassword = "1234";

struct clientInfo {
  int socket;
  char *name;
  int authorized;
};

#define MAX_CLIENTS 4 + (1)

struct clientInfo *clientSockets;

void addClient(int clientSocket, char *name) {
  int i = 0;
  while (i < MAX_CLIENTS) {
    if (clientSockets[i].socket == 0) {
      clientSockets[i].socket = clientSocket;
      clientSockets[i].name = name;
      clientSockets[i].authorized = 1;

      break;
    }

    i++;
  }
}

int getClient(int clientSocket) {
  int i = 0;
  while (i < MAX_CLIENTS) {
    if (clientSockets[i].socket == clientSocket) return i;

    i++;
  }

  return -1;
}

void removeClient(int clientSocket) {
  int i = 0;
  while (i < MAX_CLIENTS) {
    if (clientSockets[i].socket == clientSocket) {
      clientSockets[i].socket = 0;
      clientSockets[i].name = NULL;
      clientSockets[i].authorized = 0;

      break;
    }

    i++;
  }
}

void sendToAllExcept(char *message, int len, int clientSocket) {
  int i = 0;
  while (i < MAX_CLIENTS && clientSockets[i].socket != 0) {
    if (clientSockets[i].socket != clientSocket) 
      write(clientSockets[i].socket, message, len);

    i++;
  }
}

int snprintf(char *str, size_t size, const char *format, ...);

void *listenPayloads(void *data) {
  int socketDesc = *(int *)data;

  char message[2000];
  size_t msgSize;

  int r, payloadSize, clientIndex;
  jsmn_parser parser;
  jsmntok_t tokens[256];
  jsmnf_loader loader;
  jsmnf_pair pairs[256];
  jsmnf_pair *op, *username, *password, *messageS;
  char opStr[16], usernameStr[16], passwordStr[16], payload[4000];

  while ((msgSize = recv(socketDesc, message, 4000, 0)) > 0) {
    jsmn_init(&parser);
    r = jsmn_parse(&parser, message, msgSize, tokens, 256);
    if (r < 0) {
      puts("[parser]: Failed to parse JSON, ignoring.");

      memset(message, 0, msgSize);

      continue;
    }

    jsmnf_init(&loader);
    r = jsmnf_load(&loader, message, tokens, parser.toknext, pairs, 256);
    if (r < 0) {
      puts("[parser]: Failed to load JSMNF, ignoring.");

      memset(message, 0, msgSize);

      continue;
    }

    op = jsmnf_find(pairs, message, "op", sizeof("op") - 1);

    if (op == NULL) {
      puts("[parser]: No operation specified, ignoring.");

      memset(message, 0, msgSize);

      continue;
    }

    printf("[receiver]: Received message: %s\n", message);

    snprintf(opStr, sizeof(opStr), "%.*s", (int)op->v.len, message + op->v.pos);
  
    if (strcmp(opStr, "msg") == 0) {
      messageS = jsmnf_find(pairs, message, "msg", sizeof("msg") - 1);

      if (messageS->v.len == 0) {
        puts("[chat]: No message specified, ignoring.");

        memset(message, 0, msgSize);

        continue;
      }

      clientIndex = getClient(socketDesc);

      printf("[chat]: %s: %.*s\n", clientSockets[clientIndex].name, (int)messageS->v.len, message + messageS->v.pos);

      payloadSize = snprintf(payload, sizeof(payload), "{\"op\":\"msg\",\"msg\":\"%.*s\",\"author\":\"%s\"}", (int)messageS->v.len, message + messageS->v.pos, clientSockets[clientIndex].name);

      sendToAllExcept(payload, payloadSize, socketDesc);
    }
    if (strcmp(opStr, "auth") == 0) {
      username = jsmnf_find(pairs, message, "username", sizeof("username") - 1);
      password = jsmnf_find(pairs, message, "password", sizeof("password") - 1);

      if (username == NULL || password == NULL) {
        puts("[auth]: No username or password specified, ignoring.");

        memset(message, 0, msgSize);

        continue;
      }

      snprintf(usernameStr, sizeof(usernameStr), "%.*s", (int)username->v.len, message + username->v.pos);
      snprintf(passwordStr, sizeof(passwordStr), "%.*s", (int)password->v.len, message + password->v.pos);

      if (strcmp(passwordStr, serverPassword) == 0) {
        addClient(socketDesc, usernameStr);

        payloadSize = snprintf(payload, sizeof(payload), "{\"op\":\"auth\",\"status\":\"ok\"}");

        write(socketDesc, payload, payloadSize);

        payloadSize = snprintf(payload, sizeof(payload), "{\"op\":\"userJoin\",\"username\":\"%s\"}", usernameStr);

        sendToAllExcept(payload, payloadSize, socketDesc);

        printf("[auth]: \"%s\" is now authorized.\n", usernameStr);
      } else {
        printf("[auth]: \"%s\" not authorized.\n", usernameStr);

        payloadSize = snprintf(payload, sizeof(payload), "{\"op\":\"auth\",\"status\":\"fail\"}");

        write(socketDesc, payload, payloadSize);

        close(socketDesc);
      }

      memset(message, 0, msgSize);
    }
  }

  if (msgSize == 0) {
    puts("[receiver]: Client disconnected");
    fflush(stdout);
  } else {
    perror("[receiver]: recv failed");
  }

  payloadSize = snprintf(payload, sizeof(payload), "{\"op\":\"userLeave\",\"username\":\"%s\"}", clientSockets[getClient(socketDesc)].name);

  sendToAllExcept(payload, payloadSize, socketDesc);

  removeClient(socketDesc);

  return NULL;
}

void endProcess(int signal) {
  int i = 0;
  (void) signal;

  while (i < MAX_CLIENTS) {
    if (clientSockets[i].socket != 0) {
      shutdown(clientSockets[i].socket, SHUT_RDWR);
      close(clientSockets[i].socket);
    }

    i++;
  }

  free(clientSockets);

  puts("[socket]: Closing server..");

  if (shutdown(fd, SHUT_RDWR) < 0) {
    perror("[socket]: shutdown failed.");
  }

  if (close(fd) < 0) {
    perror("[socket]: close failed.");
  }

  exit(0);
}

int main(void) {
  int address = sizeof(struct sockaddr_in), socketDesc, i = 0;
  struct sockaddr_in server, client;
  struct cthreads_thread thread;
  struct cthreads_args args;

  signal(SIGINT, endProcess);

  fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
    perror("[system]: create socket failed");
    return 1;
	}

  puts("[socket]: socket created");

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(8888);

  if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("[socket]: bind failed");

    return 1;
  }

  puts("[socket]: bind done");

  if (listen(fd, 3) == -1) {
    perror("[socket]: listen failed");

    return 1;
  }

  clientSockets = malloc(sizeof(struct clientInfo) * MAX_CLIENTS);

  while (i < MAX_CLIENTS - 1) {
    clientSockets[i].socket = 0;
    clientSockets[i].name = NULL;
    clientSockets[i++].authorized = 0;
  }

  puts("[socket]: Ready to accept connections.");

  while ((socketDesc = accept(fd, (struct sockaddr *)&client, (socklen_t *)&address))) {
    if (socketDesc == -1) {
      perror("[socket]: accept failed");

      return 1;
    }

    puts("[socket]: A connection was accepted.");

    cthreads_thread_create(&thread, NULL, listenPayloads, (void *)&socketDesc, &args);

    puts("[system]: Thread created for new connection.");
  }

  endProcess(0);

  return 0;
}
