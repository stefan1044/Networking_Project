#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#define SALT "tempword"
#define PORT 2004

using namespace std;

int serverSocketDescriptor, clientSocketDescriptor;
struct sockaddr_in server;
struct sockaddr_in from;

string operators[] = {"&&", "||", "2>", "1>", "|", ">", "<"};
string outputString;

int retStatus;

int waitForClient();
int pingClient(string& temp);
int receivePing(string& buffer);
void sigpipeMask(int sig);
void processInput(string input);
void processString(string input);
string readReddir();
vector<string> separateStringOnOperators(string input);

string encrypt(string str);
string decrypt(string str);

int main() {
  signal(SIGPIPE, sigpipeMask);

  if ((serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Error creating the socket.\n");
    return errno;
  }
  bzero(&server, sizeof(server));
  bzero(&from, sizeof(from));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(PORT);

  // Server setup
  if (bind(serverSocketDescriptor, (struct sockaddr*)&server,
           sizeof(struct sockaddr)) == -1) {
    perror("Error binding.\n");
    return errno;
  }
  if (listen(serverSocketDescriptor, 1) == -1) {
    perror("Error when starting to listen.\n");
    return errno;
  }

  if ((retStatus = waitForClient())) {
    return retStatus;
  } else {
    cout << "Client connected!\n";
  }

  fd_set descriptorSet;
  FD_ZERO(&descriptorSet);
  FD_SET(clientSocketDescriptor, &descriptorSet);

  timeval tv = {0, 0};
  int conStatus, inputStatus, socketCount;
  while (true) {
    // cout << "DEBUG: In while!\n";
    fd_set copySet = descriptorSet;

    // Can Write
    socketCount =
        select(clientSocketDescriptor + 1, nullptr, &copySet, nullptr, &tv);
    if (socketCount == 1) {
      conStatus = pingClient(outputString);
      // Conection is still up
      if (conStatus == 0) {
        // cout << "DEBUG: Connection is still up!\n";
      }
    }
    this_thread::sleep_for(chrono::milliseconds(200));

    // Can Read
    copySet = descriptorSet;
    socketCount =
        select(clientSocketDescriptor + 1, &copySet, nullptr, nullptr, &tv);
    string inputString;
    if (socketCount == 1) {
      inputStatus = receivePing(inputString);
      if (inputStatus == 5005) {  // pinged successfully
      } else if (inputStatus == 5006) {
        // cout << inputString;
        processInput(inputString);

      } else {
        // client probably disconted
        this_thread::sleep_for(chrono::milliseconds(100));
      }
    }

    this_thread::sleep_for(chrono::milliseconds(200));
  }

  return 0;
}

int waitForClient() {
  socklen_t lenght = sizeof(from);
  printf("Waiting on port:%d...\n", PORT);
  fflush(stdout);
  clientSocketDescriptor =
      accept(serverSocketDescriptor, (struct sockaddr*)&from, &lenght);

  if (clientSocketDescriptor == -1) {
    perror("Error at accept!\n");
    return errno;
  }

  return 0;
}
int pingClient(string& temp) {
  long unsigned length = temp.length();
  if (length > 0) {
    write(clientSocketDescriptor, encrypt(temp).c_str(), 4096);
    // write(STDOUT_FILENO, "DEBUG: Wrote", 13);
    temp = "";
  } else if (write(clientSocketDescriptor, "0", 4096) <= 0) {
    cout << "Error when trying to send keepAlive!\n";
    return errno;
  }

  return 0;
}
int receivePing(string& buffer) {
  char* buff = new char[4096];
  if (read(clientSocketDescriptor, buff, 4096) <= 0) {
    cout << "Error when receiving ping!Client Probably disconneted\n";
    return errno;
  }

  if (buff[0] == '0') {
    return 5005;  // Pinged successfully!
  } else {
    string temp(buff);
    buffer = decrypt(temp);
    printf("Message from client%s\n", buffer.c_str());
    // write(STDOUT_FILENO, "DEBUG: Read", 11);
    return 5006;  // Received input!
  }

  return -1;  // FailSafe
}

void sigpipeMask(int sig) {
  int retStatus;
  if ((retStatus = waitForClient())) {
  } else {
    cout << "Client connected!\n";
  }
}

// separtes commands by ; and passes each to processString to be run
void processInput(string input) {
  // DEBUG:
  // write(STDIN_FILENO, input.c_str(), input.length() + 1);
  // cout << '\n';

  vector<string> vec;
  size_t pos = 0;
  string del(";");

  while ((pos = input.find(del)) != string::npos) {
    vec.push_back(input.substr(0, pos));
    input.erase(0, pos + 1);
  }
  if (input.size() > 0) {
    vec.push_back(input);
  }
  // cout << "DEBUG: " << vec.size() << '\n';
  for (auto var : vec) {
    // write(STDIN_FILENO, var.c_str(), var.length() + 1);
    // write(STDIN_FILENO, "\n", 1);
    processString(var);
  }
  // cout << "DEBUG ended\n";
}
void processString(string input) {
  // DEBUG
  // write(STDIN_FILENO, input.c_str(), input.length() + 1);
  // write(STDIN_FILENO, "\n", 1);
  // DEBUG ended

  // string redirect(" > reddir.txt");
  // input += redirect;
  // system(input.c_str());

  separateStringOnOperators(input);

  string temp = readReddir();

  outputString = temp;
  // DEBUG
  // write(STDIN_FILENO, temp.c_str(), temp.length() + 1);
  // write(STDIN_FILENO, "\n", 1);
  // DEBUG ENDED
}
string readReddir() {
  ifstream fin("reddir.txt");
  string ret, temp;

  while (getline(fin, temp)) {
    ret += temp;
    ret += '\n';
  }

  fin.close();
  return ret;
}

vector<string> separateStringOnOperators(string input) {
  vector<string> vec;
  size_t pos = 0;

  bool ok = true;
  while (ok) {
    bool finishedOps = true;
    for (auto op : operators) {
      pos = input.find(op);

      // DEBUG
      //  write(STDOUT_FILENO, op.c_str(), op.length() + 1);
      //  write(STDOUT_FILENO, "\n\0", 2);
      //  printf("Pos is %ld\n", pos);
      //  write(STDOUT_FILENO, "\n\0", 2);
      // END DEBUG

      if (pos == string::npos) {
        continue;
      } else {
        finishedOps = false;
      }
      vec.push_back(input.substr(0, pos));
      vec.push_back(op);
      input.erase(0, pos + op.length());
    }
    if (finishedOps == true) ok = false;
  }
  if (input.size() > 0) {
    vec.push_back(input);
  }

  // DEBUG
  for (auto var : vec) {
    write(STDOUT_FILENO, var.c_str(), var.length() + 1);
    write(STDOUT_FILENO, "\n\0", 2);
  }
  // END DEBUG

  return vec;
}

string encrypt(string str) {
  unsigned len = str.size();

  string cstr = str;
  for (unsigned i = 0; i < len - len % 2 - 1; i += 2) {
    swap(cstr[i], cstr[i + 1]);
    cstr[i] = cstr[i] + 4;
    cstr[i + 1] = cstr[i + 1] + 4;
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