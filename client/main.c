#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../libs/jsmn.h"
#include "../libs/jsmn-find.h"
#include "../libs/json-build.h"
#include "../libs/cthreads.h"

char password[16];

struct informations {
  int socket;
  char *username;
  int exitval;
  int running;
};

void unscrambleString(char *str, size_t strLength, char *password) {
  size_t i = 0;
  size_t passwordLength = strlen(password);

  while (i < strLength) {
    str[i] ^= password[i % passwordLength];

    i++;
  }
}

void *messString(char *str, char *password) {
  size_t i = 0;
  size_t strLength = strlen(str);
  size_t passwordLength = strlen(password);

  while (i < strLength) {
    str[i] ^= password[i % passwordLength];

    i++;
  }
}

static void *on_text(void *data) {
  int sock = *(int *)data, r;
  size_t fieldSize;
  jsmn_parser parser;
  jsmntok_t tokens[256];
  jsmnf_loader loader;
  jsmnf_pair pairs[256];
  jsmnf_pair *op, *msg, *author;

  char payload[4000], newMessage[4000], message[3900], opStr[16];

  size_t payloadSize;

  while ((payloadSize = recv(sock, payload, 4000, 0)) != 0) {
    unscrambleString(payload, payloadSize, password);

    printf("[MessaCer]: Received message: %s\n", payload);
    jsmn_init(&parser);
    r = jsmn_parse(&parser, payload, payloadSize, tokens, 256);
    if (r < 0) {
      puts("[MessaCer]: Failed to parse JSON, ignoring.");

      memset(payload, 0, sizeof(payload));

      continue;
    }

    jsmnf_init(&loader);
    r = jsmnf_load(&loader, payload, tokens, parser.toknext, pairs, 256);
    if (r < 0) {
      puts("[MessaCer]: Failed to load JSMNF, ignoring.");

      memset(payload, 0, sizeof(payload));

      continue;
    }

    op = jsmnf_find(pairs, payload, "op", 2);

    sprintf(opStr, "%.*s", (int)op->v.len, payload + op->v.pos);

    if (strcmp(opStr, "msg") == 0) {
      msg = jsmnf_find(pairs, payload, "msg", 3);
      author = jsmnf_find(pairs, payload, "author", 6);

      fieldSize = sprintf(message, "%.*s", (int)msg->v.len, payload + msg->v.pos);

      r = jsmnf_unescape(newMessage, sizeof(newMessage), message, fieldSize);
      if (r < 0) {
        puts("[MessaCer]: Failed to unescape payload, ignoring.");

        memset(payload, 0, sizeof(payload));

        continue;
      }

      printf("\r%.*s: %s\n", (int)author->v.len, payload + author->v.pos, newMessage);

      memset(newMessage, 0, r);
    }
    if (strcmp(opStr, "userJoin") == 0) {
      printf("\n[MessaCer]: Someone connected to the server.\n");
    }
    if (strcmp(opStr, "userLeave") == 0) {
      printf("\n[MessaCer]: Someone disconnected from the server.\n");
    }

    memset(payload, 0, payloadSize);
  }

  puts("[MessaCer]: Server disconnected, exiting.");

  exit(1);

  return NULL;
}

int main(void) {
  int fd;
  jsonb b;
	struct sockaddr_in server; struct cthreads_thread thread;
  size_t payloadSize;
  char url[512], username[32], message[3900], payload[4000];

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

  puts("[MessaCer]: Successfully connected to host, caution, it has access to everything you send and may be stored.\n");

  payloadSize = sprintf(payload, "{\"op\":\"auth\",\"password\":\"%s\",\"username\":\"%s\"}", password, username);
  messString(payload, password);

  if (send(fd, payload, payloadSize, 0) < 0) {
    perror("[MessaCer]: Failed to send auth to server.");
    return 1;
  }

  memset(payload, 0, sizeof(payload));

  struct cthreads_args args;
  cthreads_thread_create(&thread, NULL, on_text, (void *)&fd, &args);

  while (1) {
    fgets(message, sizeof(message), stdin);
    strtok(message, "\n");

    printf("\033[1A%s: %s\n", username, message);

    jsonb_init(&b);
    jsonb_object(&b, payload, sizeof(payload));
    {
      jsonb_key(&b, payload, sizeof(payload), "op", sizeof("op") - 1);
      jsonb_string(&b, payload, sizeof(payload), "msg", sizeof("msg") - 1);
      jsonb_key(&b, payload, sizeof(payload), "msg", sizeof("msg") - 1);
      jsonb_string(&b, payload, sizeof(payload), message, strlen(message));
      jsonb_object_pop(&b, payload, sizeof(payload));
    }

    messString(payload, password);

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
