CC ?= gcc
CFLAGS := -Wall -Wextra -Wpedantic -std=c89 -pthread -lpthread -lssl -lcrypto
FILES := main.c

all:
	$(CC) $(CFLAGS) $(FILES) -o server