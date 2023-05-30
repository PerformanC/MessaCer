#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include "jsmn.h"
#include "jsmn-find.h"
#include "cthreads.h"

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

void send_to_all_except(char *message, int len, int client_socket) {
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

int snprintf(char *str, size_t size, const char *format, ...);

/* UTILS - END */

void *listen_msgs(void *data) {
  int socket_desc = *(int *)data;

  char message[2000];
  int msg_size;

  int r, payload_size, clientIndex;
  jsmn_parser parser;
  jsmntok_t tokens[256];
  jsmnf_loader loader;
  jsmnf_pair pairs[256];
  jsmnf_pair *op, *username, *password, *messageS;
  char opStr[16], usernameStr[16], passwordStr[16], messageStr[1980], payload[4000];

  while ((msg_size = recv(socket_desc, message, 4000, 0)) > 0) {
    printf("[MessaCerServer]: Received message: %s\n", message);

    jsmn_init(&parser);
    r = jsmn_parse(&parser, message, msg_size, tokens, 256);
    if (r < 0) {
      puts("[MessaCerServer]: Failed to parse JSON, ignoring.");
      memset(message, 0, sizeof(message));
      continue;
    }

    jsmnf_init(&loader);
    r = jsmnf_load(&loader, message, tokens, parser.toknext, pairs, 256);
    if (r < 0) {
      puts("[MessaCerServer]: Failed to load JSMNF, ignoring.");
      memset(message, 0, sizeof(message));
      continue;
    }

    op = jsmnf_find(pairs, message, "op", sizeof("op") - 1);

    if (op == NULL) {
      puts("[MessaCerServer]: No operation specified, ignoring.");
      memset(message, 0, sizeof(message));
      continue;
    }

    snprintf(opStr, sizeof(opStr), "%.*s", (int)op->v.len, message + op->v.pos);
  
    if (strcmp(opStr, "msg") == 0) {
      messageS = jsmnf_find(pairs, message, "msg", sizeof("msg") - 1);

      snprintf(messageStr, sizeof(messageStr), "%.*s", (int)messageS->v.len, message + messageS->v.pos);

      clientIndex = get_client(socket_desc);

      printf("[MessaCerServer]: %s: %s\n", client_sockets[clientIndex].name, messageStr);

      payload_size = snprintf(payload, sizeof(payload), "{\"action\":\"msg\",\"msg\":\"%s\",\"author\":\"%s\"}", messageStr, client_sockets[clientIndex].name);

      send_to_all_except(payload, payload_size, socket_desc);

      memset(message, 0, sizeof(message));
    }
    if (strcmp(opStr, "auth") == 0) {
      username = jsmnf_find(pairs, message, "username", sizeof("username") - 1);
      password = jsmnf_find(pairs, message, "password", sizeof("password") - 1);

      if (username == NULL || password == NULL) {
        puts("[MessaCerServer]: No username or password specified, ignoring.");
        memset(message, 0, sizeof(message));
        continue;
      }

      snprintf(usernameStr, sizeof(usernameStr), "%.*s", (int)username->v.len, message + username->v.pos);
      snprintf(passwordStr, sizeof(passwordStr), "%.*s", (int)password->v.len, message + password->v.pos);

      if (strcmp(passwordStr, serverPassword) == 0) {
        add_client(socket_desc, usernameStr);

        send_to_all_except("{\"action\":\"userJoin\"}", 21, socket_desc);

        printf("[MessaCerServer]: \"%s\" is now authorized.\n", usernameStr);
      } else {
        printf("[MessaCerServer]: \"%s\" not authorized.\n", usernameStr);

        payload_size = snprintf(payload, sizeof(payload), "{\"action\":\"auth\",\"status\":\"fail\"}");

        write(socket_desc, payload, payload_size);

        memset(message, 0, sizeof(message));

        close(socket_desc);
        
        continue;
      }

      memset(message, 0, sizeof(message));
    }
  }

  if (msg_size == 0) {
    puts("[MessaCerServer]: Client disconnected");
    fflush(stdout);
  } else if (msg_size == -1) {
    perror("[MessaCerServer]: recv failed");
  }

  send_to_all_except("{\"action\":\"userLeave\"}", 23, socket_desc);

  remove_client(socket_desc);

  return NULL;
}

void intHandler(int signal) {
  int i = 0;
  (void) signal;

  while (client_sockets[i].socket != 0) {
    close(client_sockets[i].socket);
    i++;
  }

  puts("[MessaCerServer]: Closing server since received signint...");

  if (close(fd) < 0) {
    perror("[MessaCerServer]: close failed.");
  }

  exit(0);
}

int main(void) {
  int address, socket_desc, i = 0;
  struct sockaddr_in server, client;
  struct cthreads_thread thread;

  signal(SIGINT, intHandler);

  fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
    perror("[MessaCerServer]: create socket failed");
    return 1;
	}

  puts("[MessaCerServer]: socket created");

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(8888);

  if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("[MessaCerServer]: bind failed");
    return 1;
  }

  puts("[MessaCerServer]: bind done");

  if (listen(fd, 3) == -1) {
    perror("[MessaCerServer]: listen failed");
    return 1;
  }

  client_sockets = malloc(sizeof(struct client_info) * 5);

  while (i < MAX_CLIENTS) {
    client_sockets[i].socket = 0;
    client_sockets[i].name = NULL;
    client_sockets[i].authorized = 0;
    i++;
  }

  puts("[MessaCerServer]: Waiting for incoming connections...");

  address = sizeof(struct sockaddr_in);

  while ((socket_desc = accept(fd, (struct sockaddr *)&client, (socklen_t *)&address))) {
    puts("[MessaCerServer]: Connection accepted");

    cthreads_thread_create(&thread, NULL, listen_msgs, (void *)&socket_desc);

    puts("[MessaCerServer]: Handler assigned");

    if (socket_desc == -1) {
      perror("[MessaCerServer]: accept failed");
      return 1;
    }
  }

  printf("[MessaCerServer]: Disconnecting server, see you later.\n");

  if (close(fd) < 0) {
    perror("[MessaCerServer]: close failed");
    return 1;
  }

  return 0;
}
