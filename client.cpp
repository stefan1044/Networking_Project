#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#define PORT 2004

using namespace std;

class safeString {
 private:
  mutex mut;
  string str;

 public:
  safeString(){};
  void create(const string s) {
    const lock_guard<mutex> lock(mut);
    this->str = s;
  }
  string use() {
    const lock_guard<mutex> lock(mut);
    string temp;
    temp = this->str;
    this->str = "";
    return temp;
  }
};

int serverSocketDescriptor;
bool connectionUp = true;
struct sockaddr_in server;

int pingServer(string temp);
int receivePing(string& buffer);
void sigpipeMask(int sig);

string encrypt(string str);
string decrypt(string str);

int main() {
  signal(SIGPIPE, sigpipeMask);

  if ((serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Eroare when creating socket.\n");
    return errno;
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr("127.0.0.1");
  server.sin_port = htons(PORT);

  // client setup
  if (connect(serverSocketDescriptor, (struct sockaddr*)&server,
              sizeof(struct sockaddr)) == -1) {
    perror("Error connecting.\n");
    return errno;
  }

  cout << "Connected to server!\n";

  fd_set descriptorSet;
  FD_ZERO(&descriptorSet);
  FD_SET(serverSocketDescriptor, &descriptorSet);

  timeval tv = {0, 0};

  int conStatus, inputStatus, socketCount;
  safeString inputString;
  string incomingString;

  // Console thread
  auto writingThread = thread([&] {
    string input;
    while (connectionUp && getline(cin, input, '\n')) {
      inputString.create(input);
      // cout << "DEBUG:Received Input\n";
    }
    cout << "DEBUG:Writing thread ended!\n";
  });

  while (connectionUp) {
    // cout << "DEBUG: In while!\n";
    // string temp = inputString.use();
    // cout << temp;
    fd_set copySet = descriptorSet;

    //  Can Read
    socketCount =
        select(serverSocketDescriptor + 1, &copySet, nullptr, nullptr, &tv);
    if (socketCount == 1) {
      inputStatus = receivePing(incomingString);
      if (inputStatus == 5005) {  // pinged successfully
      } else if (inputStatus == 5006) {
        cout << incomingString;
      } else {
        // Error reading Ping
        perror("Error receiving ping!");
        exit(-1);
        return inputStatus;
      }
    }
    this_thread::sleep_for(chrono::milliseconds(200));

    // Can Write
    copySet = descriptorSet;
    socketCount =
        select(serverSocketDescriptor + 1, nullptr, &copySet, nullptr, &tv);
    if (socketCount == 1) {
      // Conection is still up
      string temp = inputString.use();
      conStatus = pingServer(temp);

      // write(STDIN_FILENO, "HERE\n", 5);

      if (conStatus == 0) {
        // cout << "DEBUG: Connection is still up!\n";
      } else {
        perror("Error sending ping!");
        exit(-1);
      }
    }

    this_thread::sleep_for(chrono::milliseconds(200));
  }

  close(serverSocketDescriptor);
  return 0;
}

int pingServer(string temp) {
  long unsigned length = temp.length();
  if (length > 0) {
    // cout << "DEBUG: Need to write: ";

    // Hack to write to stdin since cout is taken by the
    // other thread
    // char* str;
    // str = new char[length + 1];
    // for (long unsigned i = 0; i < length; i++) {
    //   str[i] = temp[i];
    // }
    // str[length + 1] = '\0';
    // write(STDIN_FILENO, str, length + 1);

    write(serverSocketDescriptor, encrypt(temp).c_str(), 4096);
  } else if (write(serverSocketDescriptor, "0", 4096) <= 0) {
    perror("Error when trying to send keepAlive!\n");
    return errno;
  }

  return 0;
}
int receivePing(string& buffer) {
  char* buff = new char[4096];
  if (read(serverSocketDescriptor, buff, 4096) <= 0) {
    cout << "Error when receiving ping!\n";
    return errno;
  }

  if (buff[0] == '0') {
    return 5005;  // Pinged successfully!
  } else {
    string temp(buff);
    buffer = decrypt(temp);
    return 5006;  // Received input!
  }

  return -1;  //"FailSafe"
}
void sigpipeMask(int sig) {
  cout << "Server disconnected!\n";
  connectionUp = false;
}

string encrypt(string str) {
  unsigned len = str.size();

  string cstr = str;
  for (unsigned i = 0; i < len - len % 2 - 1; i += 2) {
    swap(cstr[i], cstr[i + 1]);
    cstr[i] += 4;
    cstr[i + 1] += 4;
  }

  return cstr;
}
string decrypt(string str) {
  unsigned len = str.size();

  string cstr = str;
  for (unsigned i = 0; i < len - len % 2 - 1; i += 2) {
    swap(cstr[i], cstr[i + 1]);
    cstr[i] -= 4;
    cstr[i + 1] -= 4;
  }

  return cstr;
}