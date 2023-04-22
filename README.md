# MessaCer

MessaCer is a decentralized messaging system that uses sockets to communicate between clients and servers. [ALPHA]

## Installation

### Server

```command
$ git clone https://github.com/PerformanC/MessaCer.git
$ cd MessaCer/server
$ make
```

### Client

```command
$ git clone https://github.com/PerformanC/MessaCer.git
$ cd MessaCer/client
$ make
```

## Usage

### Server

The server will be listening on the 8888 port by default, and you can change that by editing the main.c file.

```command
$ ./server
```

### Client

The client will connect to the server on the 8888 port by default, and you can change that by editing the main.c file.

```command
$ ./client
```

It will ask you for the address of the server, and then your username, after that you'll be able to send messages to the server.

## License

MessaCer is licensed under PerformanC's License, which is a modified version of the MIT License, focusing on the protection of the source code and the rights of the PerformanC team over the source code.

If you wish to use some part of the source code, you must contact us first, and if we agree, you can use the source code, but you must give us credit for the source code you use.

## Credits

* [ThePedroo](https://github.com/ThePedroo) - PerformanC lead developer
- [HackerSmacker](https://github.com/HackerSmacker) - Gave us the idea for using sockets to communicate between clients and servers.

## Warning

This project is still in development, and it's not ready for production use, which misses authentication, encryption, and many other features. Feel free to use it, but don't expect it to be secure.
