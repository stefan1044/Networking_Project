#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#define SALT "tempword"
#define PORT 2004

int socketDescriptor;
struct sockaddr_in server;
struct sockaddr_in from;

int main() {
  if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Eroare la socket().\n");
    return errno;
  }

  bzero (&server, sizeof (server));
  bzero (&from, sizeof (from));

  return 0;
}
