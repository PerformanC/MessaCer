# MessaCer

A decentralized messaging system

## Installation

The installation of MessaCer will require you to have the following dependencies installed on your system:

- make
- any compiler that supports C89
- git

After you have installed the dependencies, you can install MessaCer by following the instructions below.

### Server

```command
$ git clone https://github.com/PerformanC/MessaCer.git
$ cd MessaCer/server
$ make
```

### Terminal based client

```command
$ git clone https://github.com/PerformanC/MessaCer.git
$ cd MessaCer/client
$ make
```

### Web-client

```command
$ git clone https://github.com/PerformanC/MessaCer.git
$ cd MessaCer/webclient
$ npm install
```

## Usage

### Server

The server listens for connections from clients, and it will broadcast the messages sent by the clients to all the other clients connected to the server, guaranteeing the integrity of the information about the clients.

By default, the password to access the server is 1234, but that can be easily modified in the main.c editing the `serverPassword` variable.

After that, you can start the server by executing the following command:

```command
$ ./server
```

All done, your server should be running, if not, pay attention to the logging.

### Client

The client connects to the server, and it will send the messages to the server, which will be broadcasted to all the other clients connected to the server.

To start the client, you can execute the following command:

```command
$ ./client
```

It will ask you a few questions, make sure to answer them correctly, and you should be able to connect to the server. (The default password is 1234, and the username can be any string with the limitation of 16 letters maximum)

### Web-client

The web-client does everything the client does but in a browser with an interface.

To start the server, you can execute the following command:

```command
$ node .
```

Once the webclient prints out `[http]: Running on port 8080.` you can go to https://localhost:8080 to access the interface.

## License

MessaCer is licensed under PerformanC's License, which is a modified version of the MIT License, focusing on the protection of the source code and the rights of the PerformanC team over the source code.

If you wish to use some part of the source code, you must contact us first, and if we agree, you can use the source code, but you must give us credit for the source code you use.

## Credits

- [ThePedroo](https://github.com/ThePedroo) - PerformanC developer, server developer
- [AverageFemale](https://github.com/AverageFemale) - PerformanC developer, web-client developer
- [HackerSmacker](https://github.com/HackerSmacker) - Cogmasters organization member (gave the idea of sockets)

## Warning

This project is still in development, and it's not ready for production use, which misses authentication, encryption, and many other features. Feel free to use it, but don't expect it to be secure.
