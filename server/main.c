#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "cthreads.h"

struct thread_arg {
  int *client_sockets;
  int client_socket;
};

#define MAX_CLIENTS 4 + (1)

int *client_sockets;

/* UTILS - START */

void add_client(int *client_sockets, int client_socket) {
  int i = 0;
  while (client_sockets[i] != 0) 
    i++;

  client_sockets[i] = client_socket;
}

void remove_client(int *client_sockets, int client_socket) {
  int i = 0;
  while (client_sockets[i] != client_socket) 
    i++;

  client_sockets[i] = 0;
}

void send_to_all(char *message, int len) {
  int i = 0;
  while (client_sockets[i] != 0) {
    write(client_sockets[i], message, len);
    i++;
  }
}

/* UTILS - END */

void *listen_msgs(void *data) {
  int socket_desc = *(int *)data;

  char message[2000];
  int msg_size;

  while ((msg_size = recv(socket_desc, message, 2000, 0)) > 0) {
    printf("[MessaCerServer]: Message received: %s\n", message);

    send_to_all(message, msg_size);

    memset(message, 0, sizeof(message));
  }

  if (msg_size == 0) {
    puts("[MessaCerServer]: Client disconnected");
    fflush(stdout);
  } else if (msg_size == -1) {
    perror("[MessaCerServer]: recv failed");
  }

  send_to_all("{\"action\":\"userLeave\"}", 23);

  remove_client(client_sockets, socket_desc);

  return NULL;
}

int main(void) {
	int fd, address, socket_desc;
	struct sockaddr_in server, client;
  struct cthreads_thread thread;

  client_sockets = malloc(sizeof(int) * MAX_CLIENTS);

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

  puts("[MessaCerServer]: Waiting for incoming connections...");

  address = sizeof(struct sockaddr_in);

  while ((socket_desc = accept(fd, (struct sockaddr *)&client, (socklen_t *)&address))) {
    puts("[MessaCerServer]: Connection accepted");

    send_to_all("{\"action\":\"userJoin\"}", 21);

    add_client(client_sockets, socket_desc);

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
