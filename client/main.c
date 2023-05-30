#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "cthreads.h"
#include "jsmn.h"
#include "jsmn-find.h"

struct informations {
  int socket;
  char *username;
  int exitval;
  int running;
};

static void *on_text(void *data) {
  int sock = *(int *)data, r;
  jsmn_parser parser;
  jsmntok_t tokens[256];
  jsmnf_loader loader;
  jsmnf_pair pairs[256];
  jsmnf_pair *action, *msg, *author;

  char message[4000], actionStr[16];

  size_t messageSize;

  while ((messageSize = recv(sock, message, 4000, 0)) != 0) {
    jsmn_init(&parser);
    r = jsmn_parse(&parser, message, messageSize, tokens, 256);
    if (r < 0) {
      puts("[MessaCer]: Failed to parse JSON, ignoring.");
      memset(message, 0, sizeof(message));
      continue;
    }

    jsmnf_init(&loader);
    r = jsmnf_load(&loader, message, tokens, parser.toknext, pairs, 256);
    if (r < 0) {
      puts("[MessaCer]: Failed to load JSMNF, ignoring.");
      memset(message, 0, sizeof(message));
      continue;
    }

    action = jsmnf_find(pairs, message, "action", 6);

    sprintf(actionStr, "%.*s", (int)action->v.len, message + action->v.pos);

    if (strcmp(actionStr, "msg") == 0) {
      msg = jsmnf_find(pairs, message, "msg", 3);
      author = jsmnf_find(pairs, message, "author", 6);

      printf("\r%.*s: %.*s\n", (int)author->v.len, message + author->v.pos, (int)msg->v.len, message + msg->v.pos);
    }
    if (strcmp(actionStr, "userJoin") == 0) {
      printf("\n[MessaCer]: Someone connected to the server.\n");
    }
    if (strcmp(actionStr, "userLeave") == 0) {
      printf("\n[MessaCer]: Someone disconnected from the server.\n");
    }

    memset(message, 0, messageSize);
  }

  puts("[MessaCer]: Server disconnected, exiting.");

  exit(1);

  return NULL;
}

int main(void) {
  int fd;
	struct sockaddr_in server; struct cthreads_thread thread;
  char url[512], password[16], username[32], message[3900], payload[4000];

  printf("URL of host server: ");
  fgets(url, sizeof(url), stdin);
  strtok(url, "\n");

  printf("Password: ");
  fgets(password, sizeof(password), stdin);
  strtok(password, "\n");

  printf("Username: ");
  fgets(username, sizeof(username), stdin);
  strtok(username, "\n");

  fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd == -1) {
    perror("[MessaCer]: Could not create connection to server.");
  }

  server.sin_addr.s_addr = inet_addr(url);
  server.sin_family = AF_INET;
  server.sin_port = htons(8888);

  if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("[MessaCer]: Could not connect to server");
    return 1;
  }

  printf("[MessaCer]: Successfully connected to host, caution, it has access to everything you send and may be stored.\n");

  sprintf(payload, "{\"op\":\"auth\",\"password\":\"%s\",\"username\":\"%s\"}", password, username);

  if (send(fd, payload, strlen(payload), 0) < 0) {
    perror("[MessaCer]: Failed to send auth to server.");
    return 1;
  }

  memset(payload, 0, sizeof(payload));

  cthreads_thread_create(&thread, NULL, on_text, (void *)&fd);

  while (1) {
    fgets(message, sizeof(message), stdin);
    strtok(message, "\n");

    printf("\033[1A%s: %s\n", username, message);

    sprintf(payload, "{\"op\":\"msg\",\"msg\":\"%s\"}", message);

    if (send(fd, payload, strlen(payload), 0) < 0) {
      perror("[MessaCer]: Failed to send data to server.");
      return 1;
    }

    memset(message, 0, sizeof(message));
    memset(payload, 0, sizeof(payload));
  }

  if (close(fd) < 0) {
    perror("[MessaCer]: Failed to close connection to server.");
    return 1;
  }

  return 0;
}
