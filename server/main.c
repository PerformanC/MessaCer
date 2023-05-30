#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include "../libs/jsmn.h"
#include "../libs/jsmn-find.h"
#include "../libs/json-build.h"
#include "../libs/cthreads.h"

int fd;
char *serverPassword = "1234";

struct thread_arg {
  int *client_sockets;
  int client_socket;
};

struct client_info {
  int socket;
  char *name;
  int authorized;
};

#define MAX_CLIENTS 4 + (1)

struct client_info *client_sockets;

/* UTILS - START */

void add_client(int client_socket, char *name) {
  int i = 0;
  while (i < MAX_CLIENTS) {
    if (client_sockets[i].socket == 0) {
      client_sockets[i].socket = client_socket;
      client_sockets[i].name = name;
      client_sockets[i].authorized = 1;
      break;
    }
    i++;
  }
}

int get_client(int client_socket) {
  int i = 0;
  while (i < MAX_CLIENTS) {
    if (client_sockets[i].socket == client_socket) {
      return i;
    }
    i++;
  }

  return -1;
}

void remove_client(int client_socket) {
  int i = 0;
  while (i < MAX_CLIENTS) {
    if (client_sockets[i].socket == client_socket) {
      client_sockets[i].socket = 0;
      client_sockets[i].name = NULL;
      client_sockets[i].authorized = 0;
      break;
    }
    i++;
  }
}

void sendToAllExcept(char *message, int len, int client_socket) {
  int i = 0;
  while (i < MAX_CLIENTS && client_sockets[i].socket != 0) {
    if (client_sockets[i].socket == client_socket) {
      i++;
      continue;
    }

    write(client_sockets[i].socket, message, len);
    i++;
  }
}

/* UTILS - END */

void *listen_msgs(void *data) {
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
    printf("[receiver]: Received message: %s\n", message);

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

    sprintf(opStr, "%.*s", (int)op->v.len, message + op->v.pos);
  
    if (strcmp(opStr, "msg") == 0) {
      messageS = jsmnf_find(pairs, message, "msg", sizeof("msg") - 1);

      clientIndex = get_client(socketDesc);

      printf("[chat]: %s: %.*s\n", client_sockets[clientIndex].name, (int)messageS->v.len, message + messageS->v.pos);

      payloadSize = sprintf(payload, "{\"op\":\"msg\",\"msg\":\"%.*s\",\"author\":\"%s\"}", (int)messageS->v.len, message + messageS->v.pos, client_sockets[clientIndex].name);

      sendToAllExcept(payload, payloadSize, socketDesc);

      memset(message, 0, msgSize);
    }
    if (strcmp(opStr, "auth") == 0) {
      username = jsmnf_find(pairs, message, "username", sizeof("username") - 1);
      password = jsmnf_find(pairs, message, "password", sizeof("password") - 1);

      if (username == NULL || password == NULL) {
        puts("[auth]: No username or password specified, ignoring.");

        memset(message, 0, msgSize);

        continue;
      }

      sprintf(usernameStr, "%.*s", (int)username->v.len, message + username->v.pos);
      sprintf(passwordStr, "%.*s", (int)password->v.len, message + password->v.pos);

      if (strcmp(passwordStr, serverPassword) == 0) {
        add_client(socketDesc, usernameStr);

        sendToAllExcept("{\"op\":\"userJoin\"}", 21, socketDesc);

        printf("[auth]: \"%s\" is now authorized.\n", usernameStr);

        memset(message, 0, msgSize);
      } else {
        printf("[auth]: \"%s\" not authorized.\n", usernameStr);

        payloadSize = sprintf(payload, "{\"op\":\"auth\",\"status\":\"fail\"}");

        write(socketDesc, payload, payloadSize);

        memset(message, 0, msgSize);

        close(socketDesc);
        
        continue;
      }
    }
  }

  if (msgSize == 0) {
    puts("[receiver]: Client disconnected");
    fflush(stdout);
  } else {
    perror("[receiver]: recv failed");
  }

  sendToAllExcept("{\"op\":\"userLeave\"}", 23, socketDesc);

  remove_client(socketDesc);

  return NULL;
}

void intHandler(int signal) {
  int i = 0;
  (void) signal;

  while (client_sockets[i].socket != 0) {
    shutdown(client_sockets[i].socket, SHUT_RDWR);
    close(client_sockets[i].socket);
    i++;
  }

  free(client_sockets);

  puts("[socket]: Closing server since received signint...");

  if (shutdown(fd, SHUT_RDWR) < 0) {
    perror("[socket]: shutdown failed.");
  }

  if (close(fd) < 0) {
    perror("[socket]: close failed.");
  }

  exit(0);
}

int main(void) {
  int address, socketDesc, i = 0;
  struct sockaddr_in server, client;
  struct cthreads_thread thread;

  signal(SIGINT, intHandler);

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

  client_sockets = malloc(sizeof(struct client_info) * 5);

  while (i < MAX_CLIENTS) {
    client_sockets[i].socket = 0;
    client_sockets[i].name = NULL;
    client_sockets[i].authorized = 0;
    i++;
  }

  puts("[socket]: Waiting for incoming connections...");

  address = sizeof(struct sockaddr_in);

  while ((socketDesc = accept(fd, (struct sockaddr *)&client, (socklen_t *)&address))) {
    puts("[socket]: Connection accepted");

    cthreads_thread_create(&thread, NULL, listen_msgs, (void *)&socketDesc);

    puts("[system]: Handler assigned to new user");

    if (socketDesc == -1) {
      perror("[socket]: accept failed");
      return 1;
    }
  }

  puts("[system]: Disconnecting server, see you later.");

  while (client_sockets[i].socket != 0) {
    shutdown(client_sockets[i].socket, SHUT_RDWR);
    close(client_sockets[i].socket);
    i++;
  }

  free(client_sockets);

  if (shutdown(fd, SHUT_RDWR) < 0) {
    perror("[socket]: shutdown failed.");

    return 1;
  }

  if (close(fd) < 0) {
    perror("[socket]: close failed.");

    return 1;
  }

  puts("[system]: Server disconnected.");

  return 0;
}
