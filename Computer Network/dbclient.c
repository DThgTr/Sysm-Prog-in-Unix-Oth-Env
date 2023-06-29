#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "msg.h"

#define BUF 256

void Usage(char *progname);

int LookupName(char *name,
                unsigned short port,
                struct sockaddr_storage *ret_addr,
                size_t *ret_addrlen);

int Connect(const struct sockaddr_storage *addr,
             const size_t addrlen,
             int *ret_fd);

//--------PUT & GET REQUEST--------
void PUTRequest(int sock_fd);
void GETRequest(int sock_fd);

int 
main(int argc, char **argv) {
  if (argc != 3) {
    Usage(argv[0]);
  }

  unsigned short port = 0;
  if (sscanf(argv[2], "%hu", &port) != 1) {
    Usage(argv[0]);
  }

  // Get an appropriate sockaddr structure.
  struct sockaddr_storage addr;
  size_t addrlen;
  if (!LookupName(argv[1], port, &addr, &addrlen)) {
    Usage(argv[0]);
  }

  // Connect to the remote host.
  int socket_fd;
  if (!Connect(&addr, addrlen, &socket_fd)) {
    Usage(argv[0]);
  }

  //==========================PROMPTS=====================
  /*
  The client application is interactive (you may want to reuse some of the code you wrote for A5). 
  First, it sets up a connection with the server. 
  It then prompts the user to choose one of the operations: put, get, quit. 
  Below are the actions taken by dbclient for these operations.
  
  */
  while (1) {
    // Get user choice
    int choice = -1;
    printf("Enter your choice (1 to put, 2 to get, 0 to quit): ");
    if (scanf("%d", &choice) != 1) { // Handle read fail
      printf("User's Choice: Read fail\n");
      continue;
    }
    else if(choice < 0 || choice > 2) { // Handle invalid input
      printf("Invalid choice\n");
      continue;
    }

    // Handle action
    switch (choice) {
      case 0: // QUIT 
        close(socket_fd);
        return EXIT_SUCCESS;
      case PUT: // PUT
        PUTRequest(socket_fd);
        break;
      case GET: // GET
        GETRequest(socket_fd);
        break;
      default:
        printf("Invalid choice\n");
    }
  }

  // Clean up.
  close(socket_fd);
  return EXIT_SUCCESS;
}
//================================HANDLE CHOICE=====================================
//--------------------PUT REQUEST-----------------
void PUTRequest (int sock_fd) {
  /*
  PUT: If the user chooses put, then prompt the user for name and id. 
        Send a put message to dbserver (fill name and id fields of the record), and wait for the response. 
        If the response is SUCCESS, print "put success" message. Otherwise print "put failed" message.
  return:
    SUCCESS
    FAIL
  */
  char name[MAX_NAME_LENGTH];
  uint32_t id;
  printf("Enter the name: ");
  if (scanf("%s", name) != 1) { // Handle read fail
    printf("PUT: Read name fail\n");
    return;
  }
  printf("Enter the id: ");
  if (scanf("%u", &id) != 1) { // Handle read fail
    printf("PUT: Read id fail\n");
    return;
  }

  //-------Prepare message for sending------
  struct record recHolder;
  // Create record for sending
  strncpy(recHolder.name, name, MAX_NAME_LENGTH);
  recHolder.id = id;
  // Create msg to be sent
  struct msg msgHolder = {PUT, recHolder};

  //-------Send & Wait-Receive-------
  // Sent PUT message to dbserver
  if (write(sock_fd, &msgHolder, sizeof(msgHolder)) < 0) {
    perror("write failed");
    return;
  }

  // Wait and receive response from dbserver
  //if (recv(sock_fd, &msgHolder, sizeof(msgHolder), 0) < 0) {
  if (read(sock_fd, &msgHolder, sizeof(msgHolder)) < 0) {
    perror("read failed");
    return;
  }

  //-------Process server Response------
  // Check server response
  switch(msgHolder.type) {
    case SUCCESS:
      printf("put success\n");
      break;
    case FAIL:
      printf("put failed\n");
      break;
    default:
      printf("Unknown response");
  }
}

//--------------------GET REQUEST-----------------
void GETRequest(int sock_fd) {
  /*
  GET: If the user chooses get, then prompt the user for id. 
        Send a get message to server (only fill id field of the record), and wait for the response. 
        If the response is SUCCESS, print name and id. Otherwise print "get failed" message.
  return:
    SUCCESS
    FAIL
  */
  uint32_t id;
  printf("Enter the id: ");
  if (scanf("%u", &id) != 1) { // Handle read fail
    printf("GET: Read id fail\n");
    return;
  }

  //-------Prepare message for sending------
  struct record recHolder;  // Create dummy record
  recHolder.id = id;        // id to be sent
  // Create msg to be sent
  struct msg msgHolder = {GET, recHolder};

  //-------Send & Wait-Receive-------
  // Sent PUT message to dbserver
  if (write(sock_fd, &msgHolder, sizeof(msgHolder)) < 0) {
    perror("write failed");
    return;
  }

  // Wait and receive response from dbserver
  if (read(sock_fd, &msgHolder, sizeof(msgHolder)) < 0) {
    perror("read failed");
    return;
  }
  //
  
  // Check server response
  switch(msgHolder.type) {
    case SUCCESS:
      recHolder = msgHolder.rd;
      printf("name: %s\n", recHolder.name);
      printf("id: %d\n", recHolder.id);
      break;
    case FAIL:
      printf("get failed\n");
      break;
    default:
      printf("Unknown response");
  }
}

//================================THE REST OF DEM=====================================
void 
Usage(char *progname) {
  printf("usage: %s  hostname port \n", progname);
  exit(EXIT_FAILURE);
}

int 
LookupName(char *name,
                unsigned short port,
                struct sockaddr_storage *ret_addr,
                size_t *ret_addrlen) {
  struct addrinfo hints, *results;
  int retval;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // Do the lookup by invoking getaddrinfo().
  if ((retval = getaddrinfo(name, NULL, &hints, &results)) != 0) {
    printf("getaddrinfo failed: %s", gai_strerror(retval));
    return 0;
  }

  // Set the port in the first result.
  if (results->ai_family == AF_INET) {
    struct sockaddr_in *v4addr =
            (struct sockaddr_in *) (results->ai_addr);
    v4addr->sin_port = htons(port);
  } else if (results->ai_family == AF_INET6) {
    struct sockaddr_in6 *v6addr =
            (struct sockaddr_in6 *)(results->ai_addr);
    v6addr->sin6_port = htons(port);
  } else {
    printf("getaddrinfo failed to provide an IPv4 or IPv6 address \n");
    freeaddrinfo(results);
    return 0;
  }

  // Return the first result.
  assert(results != NULL);
  memcpy(ret_addr, results->ai_addr, results->ai_addrlen);
  *ret_addrlen = results->ai_addrlen;

  // Clean up.
  freeaddrinfo(results);
  return 1;
}

int 
Connect(const struct sockaddr_storage *addr,
             const size_t addrlen,
             int *ret_fd) {
  // Create the socket.
  int socket_fd = socket(addr->ss_family, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    printf("socket() failed: %s", strerror(errno));
    return 0;
  }

  // Connect the socket to the remote host.
  int res = connect(socket_fd,
                    (const struct sockaddr *)(addr),
                    addrlen);
  if (res == -1) {
    printf("connect() failed: %s", strerror(errno));
    return 0;
  }

  *ret_fd = socket_fd;
  return 1;
}
