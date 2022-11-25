#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#define SALT "tempword"
#define PORT 2004

int serverSocketDescriptor, clientSocketDescriptor;
struct sockaddr_in server;
struct sockaddr_in from;

int main() {
  if ((serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Eroare la socket().\n");
    return errno;
  }

  bzero(&server, sizeof(server));
  bzero(&from, sizeof(from));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(PORT);

  if (bind(serverSocketDescriptor, (struct sockaddr *)&server,
           sizeof(struct sockaddr)) == -1) {
    perror("Eroare la bind().\n");
    return errno;
  }

  if (listen(serverSocketDescriptor, 1) == -1) {
    perror("Eroare la listen().\n");
    return errno;
  }

  while (1) {
    socklen_t lenght = sizeof(from);
    printf("Waiting on port:%d...\n", PORT);
    fflush(stdout);
    clientSocketDescriptor =
        accept(serverSocketDescriptor, (struct sockaddr *)&from, &lenght);

    if (clientSocketDescriptor == -1) {
      perror("Error at accept!\n");
      return errno;
    }
  }
  return 0;
}
