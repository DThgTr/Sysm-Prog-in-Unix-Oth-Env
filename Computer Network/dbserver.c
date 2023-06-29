
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "msg.h"
#include <pthread.h>

#define DB_FILENAME "db.bin"  //database file name

void Usage(char *progname);

int  Listen(char *portnum, int *sock_family);

//------------Handle client------------
struct clientInfo {
  int c_fd;
  struct sockaddr *addr;
  size_t addrlen;
  int sock_family;
};
// Handle incoming connection
void* handleClient(void *listen_fd);
// Handle request over connection
void* handleRequest(void *c_fd);
// self-explanatory
void handlePUT(int c_fd, struct record clientRec, struct msg *resMsg /*neccessary params*/);
void handleGET(int c_fd, struct record clientRec, struct msg *resMsg/*neccessary params*/);
//------------DB operations-----------
int writeRecord(const struct record *rec);
int findRecord(uint32_t id, struct record *returnRec);

//=========================---------MAIN----------=======================
int 
main(int argc, char **argv) 
{
  // Expect the port number as a command line argument.
  if (argc != 2) {
    Usage(argv[0]);
  }
  // Listen to the socket
  int sock_family;
  int listen_fd = Listen(argv[1], &sock_family);
  if (listen_fd <= 0) {
    // We failed to bind/listen to a socket.  Quit with failure.
    printf("Couldn't bind to any addresses.\n");
    return EXIT_FAILURE;
  }

  printf("\nPre CLI Thread\n");
  pthread_t clientThread;
  if (pthread_create(&clientThread, NULL, handleClient, &listen_fd)) {
    perror("pthread_create failed");
    exit(EXIT_FAILURE);
  }
  pthread_join(clientThread, NULL);
  printf("\nPost CLI Thread\n");

  return 0;
}

void Usage(char *progname) {
  printf("usage: %s port \n", progname);
  exit(EXIT_FAILURE);
}

int 
Listen(char *portnum, int *sock_family) {

  // Populate the "hints" addrinfo structure for getaddrinfo().
  // ("man addrinfo")
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;       // IPv6 (also handles IPv4 clients)
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  // getaddrinfo() returns a list of
  // address structures via the output parameter "result".
  struct addrinfo *result;
  int res = getaddrinfo(NULL, portnum, &hints, &result);

  // Did addrinfo() fail?
  if (res != 0) {
	printf( "getaddrinfo failed: %s", gai_strerror(res));
    return -1;
  }

  // Loop through the returned address structures until we are able
  // to create a socket and bind to one.  The address structures are
  // linked in a list through the "ai_next" field of result.
  int listen_fd = -1;
  struct addrinfo *rp;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    listen_fd = socket(rp->ai_family,
                       rp->ai_socktype,
                       rp->ai_protocol);
    if (listen_fd == -1) {
      // Creating this socket failed.  So, loop to the next returned
      // result and try again.
      printf("socket() failed:%s \n ", strerror(errno));
      listen_fd = -1;
      continue;
    }

    // Configure the socket; we're setting a socket "option."  In
    // particular, we set "SO_REUSEADDR", which tells the TCP stack
    // so make the port we bind to available again as soon as we
    // exit, rather than waiting for a few tens of seconds to recycle it.
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    // Try binding the socket to the address and port number returned
    // by getaddrinfo().
    if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      // Bind worked!  Print out the information about what
      // we bound to.
      //PrintOut(listen_fd, rp->ai_addr, rp->ai_addrlen);

      // Return to the caller the address family.
      *sock_family = rp->ai_family;
      break;
    }

    // The bind failed.  Close the socket, then loop back around and
    // try the next address/port returned by getaddrinfo().
    close(listen_fd);
    listen_fd = -1;
  }

  // Free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // If we failed to bind, return failure.
  if (listen_fd == -1)
    return listen_fd;

  // Success. Tell the OS that we want this to be a listening socket.
  if (listen(listen_fd, SOMAXCONN) != 0) {
    printf("Failed to mark socket as listening:%s \n ", strerror(errno));
    close(listen_fd);
    return -1;
  }

  // Return to the client the listening file descriptor.
  return listen_fd;
}

//============!!=============!!==========================================!!=============!!=============
//============!!=============!!============HANDLE CLIENT FUNC============!!=============!!=============
//============!!=============!!==========================================!!=============!!=============
//=====================HANDLE CLIENT===================
void* 
handleClient(void *listen_fd) {
  // Loop forever, accepting a connection from a client and doing
  // an echo trick to it.
  while (1) {
    printf("\nWaiting for Request\n");
    struct sockaddr_storage caddr;
    socklen_t caddr_len = sizeof(caddr);
    int client_fd = accept(*(int*)listen_fd,
                           (struct sockaddr *)(&caddr),
                           &caddr_len);
    if (client_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
        continue;
      printf("Failure on accept:%s \n ", strerror(errno));
      break;
    }

    //-------------Create thread to handle request-----------
    pthread_t requestThread;
    if (pthread_create(&requestThread, NULL, handleRequest, &client_fd)) {
      perror("pthread_create failed");
      exit(EXIT_FAILURE);
    }
    // Detach thread to continue listening
    if (pthread_detach(requestThread) != 0) {
      perror("pthread_detach failed");
      exit(EXIT_FAILURE);
    }
    /*
    HandleClient(client_fd,
                 (struct sockaddr *)(&caddr),
                 caddr_len,
                 sock_family);
    */
  }
  printf("\nSTOPPED WAITING\n");
  //--------------Close socket-------------
  close(*(int*) listen_fd);
  
  //------------Exit Thread-----------
  pthread_exit(NULL);
}

//====================HANDLE REQUEST===================
void* 
handleRequest(void *c_fd) 
{
  /*
  A handler thread terminates only when its client closes the connection. 

  A handler thread must process each request from a dbclient and send an appropriate response back to the client. 
  There are two types of request messages from dbclient:
  */
  /*
  // Print out information about the client.
  printf("\nNew client connection \n" );
  PrintOut(c_fd, addr, addrlen);
  PrintReverseDNS(addr, addrlen);
  PrintServerSide(c_fd, sock_family);
  */

  // Loop, reading data and echo'ing it back, until the client
  // closes the connection.
  while (1) {
    printf("\nListening to Client\n");
    //------------Read Client Message----------
    struct msg clientMsg;
    ssize_t res = read(*(int*) c_fd, &clientMsg, sizeof(clientMsg));
    if (res == 0) {
      printf("[The client disconnected.] \n");
      break;
    }
    // Handle errors
    if (res == -1) {
      printf(" Error on client socket:%s \n ", strerror(errno));
      break;
    }
    //--------------Process Message--------------
    /*
    A handler thread must process each request from a dbclient and send an appropriate response back to the client. 
    */
    printf("\nProcess Request\n");
    struct msg resMsg;
    resMsg.type = FAIL;
    struct record clientRec = clientMsg.rd; // Get record from client
    // Get message type
    switch(clientMsg.type) {
      case PUT:
        handlePUT(*(int*) c_fd, clientRec, &resMsg/*neccessary param*/);
        break;
      case GET:
        handleGET(*(int*) c_fd, clientRec, &resMsg/*neccessary param*/);
        break;
      default:
        printf("Cannot discern client message type\n");
    }

    //==!!==!!==WRITE==!==!==
    printf("\nResponse to Client\n");
    if (write(*(int *)c_fd, &resMsg, sizeof(resMsg)) < 0) {
      perror("write failed");
      break;
    }
    printf("\nFinished Response\n");
  }
  printf ("\nStopped listening to Client\n");
  //-------Close Connection------
  close(*(int*) c_fd);

  //--------Exit Thread-------
  pthread_exit(NULL);
}
//=====================handle PUT & GET======================
/*
A handler thread must process each request from a dbclient and send an appropriate response back to the client. 
There are two types of request messages from dbclient. 
*/
//-----handle PUT-----
void handlePUT(int c_fd, struct record clientRec, struct msg *resMsg/*neccessary params*/) {
  /*
  1. PUT: This message contains the data record that needs to be stored in the database. 
    On receiving this message, handler appends the record to the database file. 
    (Do not store the entire message. Store only the client data record.) 
    =>  If the write is successful, it will send SUCCESS message to the client. 
        Otherwise the handler sends FAIL message.
  */

  //
  //
  //PUT IMPLEMENTATION
  resMsg->type = FAIL;
  if (writeRecord(&clientRec) > 0) {
    resMsg->type = SUCCESS;
  }
}
//-----hanlde GET-----
void handleGET(int c_fd, struct record clientRec, struct msg *resMsg/*neccessary params*/) {
  /*
  2. GET: This request message will contain the id of the record that needs to be fetched. 
    On receiving this message, handler searches the database to find a matching record 
    (record with id field that matches the id in the get message). 
    =>  If a matching record is found, the handler sends SUCCESS message with the matching record. 
        Otherwise the handler sends FAIL message.
  */

  //
  //
  //GET IMPLEMENTATION
  struct record resRec;
  resMsg->type = FAIL;
  if (findRecord(clientRec.id, &resRec) > 0) {
    resMsg->rd = resRec;
    resMsg->type = SUCCESS;
  }
}

//============!!=============!!==========================================!!=============!!=============
//============!!=============!!============DATABASE OPERATIONS===========!!=============!!=============
//============!!=============!!==========================================!!=============!!=============
// Write to database
int writeRecord(const struct record *rec) {
  FILE* fp;
  fp = fopen(DB_FILENAME, "ab");
  if (fp == NULL) {
    perror("Error opening database file");
    exit(EXIT_FAILURE);
  }

  if (fwrite(rec, sizeof(struct record), 1, fp) == 1) {
    fclose(fp);
    return 1;
  }
  perror("Error writing record to database");
  fclose(fp);
  return 0;
}

// Find record given id
int findRecord(uint32_t id, struct record *returnRec) {
  FILE* fp;
  fp = fopen(DB_FILENAME, "rb");
  if (fp == NULL) {
    perror("Error opening database file");
    exit(EXIT_FAILURE);
  }

  if (fseek(fp, -sizeof(struct record), SEEK_END) != 0) {
    perror("Error seeking to end of file");
    fclose(fp);
    return 0;
  }

  while (1) {
    //if (fread(returnRec, sizeof(struct record), 1, fp) != 1) {
    size_t items_read = fread(returnRec, sizeof(struct record), 1, fp);
    if (items_read != 1) {
      printf("/nItems read: %zu\n", items_read);
      perror("Error reading record from database");
      break;
    }
    if (feof(fp)) {
      printf("\nEof database\n");
      break;
    }
    if (returnRec->id == id) {
      fclose(fp);
      return 1;
    }
    if (fseek(fp, -2 * sizeof(struct record), SEEK_CUR) != 0) {
      break;
    }
  }
  fclose(fp);
  return 0;
}
