#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#define SALT "tempword"
#define PORT 2004

using namespace std;

bool connectionUp = true;
int serverSocketDescriptor, currentPid;
int fileSocketDescriptors[3];
struct sockaddr_in server;
struct sockaddr_in from;

string operators[] = {"&&", "||", "2>", "1>", "|", ">", "<"};
string outputString;

int retStatus;

int waitForClient();
int pingClient(string& temp, int sd);
int receivePing(string& buffer, bool& loggedIn, int sd);
void sigpipeMask(int sig);
void processInput(string input);
void processString(string input);
string readReddir();
vector<string> separateStringOnOperators(string input);
int executeCommand(string command);

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

  while (true) {
    int clientSocketDescriptor;

    if ((clientSocketDescriptor = waitForClient()) < 0) {
      return retStatus;
    } else {
      cout << "Client connected!\n";
    }

    int pid;

    if ((pid = fork()) == -1) {
      close(clientSocketDescriptor);
      continue;
    } else if (pid > 0) {
      close(clientSocketDescriptor);
      string openString =
          "Running client on PID "s + to_string(pid).c_str() + "\n";
      write(STDOUT_FILENO, openString.c_str(), openString.length() + 1);
      while (waitpid(-1, NULL, WNOHANG))
        ;
      continue;
    } else if (pid == 0) {
      close(serverSocketDescriptor);
      currentPid = getpid();

      // driver code
      fd_set descriptorSet;
      FD_ZERO(&descriptorSet);
      FD_SET(clientSocketDescriptor, &descriptorSet);

      timeval tv = {0, 0};
      int conStatus, inputStatus, socketCount;

      bool loggedIn = false;

      while (connectionUp) {
        // cout << "DEBUG: In while!\n";
        fd_set copySet = descriptorSet;

        // Can Write
        socketCount =
            select(clientSocketDescriptor + 1, nullptr, &copySet, nullptr, &tv);
        if (socketCount == 1) {
          conStatus = pingClient(outputString, clientSocketDescriptor);
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
          inputStatus =
              receivePing(inputString, loggedIn, clientSocketDescriptor);
          if (inputStatus == 5005) {  // pinged successfully
          } else if (inputStatus == 5006) {
            // cout << inputString;
            processInput(inputString);
          } else if (inputStatus == 5007) {
            loggedIn = true;
            outputString = "Successfully logged in!\n";
          } else if (inputStatus == 5008) {
            loggedIn = false;
            outputString = "Logged out!\n";
          } else {
            // client probably disconted
            this_thread::sleep_for(chrono::milliseconds(100));
          }
        }

        this_thread::sleep_for(chrono::milliseconds(200));
      }

      // cleanup
      close(clientSocketDescriptor);
      string closeString =
          "Client ran on PID "s + to_string(currentPid).c_str() + " closed\n"s;
      write(STDIN_FILENO, closeString.c_str(), closeString.length() + 1);
      exit(0);
    }
  }

  return 0;
}

int waitForClient() {
  socklen_t lenght = sizeof(from);
  printf("Waiting on port:%d...\n", PORT);
  fflush(stdout);
  int clientSocketDescriptor =
      accept(serverSocketDescriptor, (struct sockaddr*)&from, &lenght);

  if (clientSocketDescriptor == -1) {
    perror("Error at accept!\n");
    return errno;
  }

  return clientSocketDescriptor;
}
int pingClient(string& temp, int sd) {
  long unsigned length = temp.length();
  if (length > 0) {
    // has something to write
    write(sd, encrypt(temp).c_str(), 4096);
    // write(STDOUT_FILENO, "DEBUG: Wrote", 13);
    temp = "";
  } else if (write(sd, "0", 4096) <= 0) {  // send keepalive
    connectionUp = false;
    cout << "Error when trying to send keepAlive!\n";
    return errno;
  }

  return 0;
}
int receivePing(string& buffer, bool& loggedIn, int sd) {
  char* buff = new char[4096];
  if (read(sd, buff, 4096) <= 0) {
    cout << "Error when receiving ping!Client Probably disconneted\n";
    connectionUp = false;
    return errno;
  }

  if (buff[0] == '0') {
    return 5005;  // Pinged successfully!
  } else {
    string temp(buff);
    buffer = decrypt(temp);
    // attempt to login
    if (loggedIn == false) {
      if (buffer.find("login "s) == 0) {
        // printf("%s\n", buffer.substr(6).c_str());

        ifstream in("database.txt");
        char fileStr[128];
        string subString = buffer.substr(6);
        // printf("SubStr: %s\n", subString.c_str());
        while (in.getline(fileStr, 128)) {
          // printf("Read: %s\n", fileStr);
          if (subString == string(fileStr)) {
            printf("Read: %s\n", fileStr);
            return 5007;
          }
        }
      }

      printf("Message from client: %s\n", buffer.c_str());

      buffer = "";
      return 5005;
    }

    if (buffer == "logout"s) {
      buffer = "";
      return 5008;
    }
    printf("Message from client: %s\n", buffer.c_str());
    // write(STDOUT_FILENO, "DEBUG: Read", 11);
    return 5006;  // Received input!
  }

  return -1;  // FailSafe
}

void sigpipeMask(int sig) {
  write(STDOUT_FILENO, "Disconected\n", 12);
  connectionUp = false;
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
    // write(STDOUT_FILENO, var.c_str(), var.length() + 1);
    // write(STDOUT_FILENO, "\n", 1);
    processString(var);
  }
  // cout << "DEBUG ended\n";
}
void processString(string input) {
  // DEBUG
  // write(STDOUT_FILENO, input.c_str(), input.length() + 1);
  // write(STDOUT_FILENO, "\n", 1);
  // DEBUG ended

  // string redirect(" > reddir.txt");
  // input += redirect;
  // system(input.c_str());

  // printf("Here\n");

  vector<string> vec = separateStringOnOperators(input);
  bool currentStatus = true;
  string inName = "stdin"s + to_string(currentPid);
  string outName = "stdout"s + to_string(currentPid);
  string errName = "stderr"s + to_string(currentPid);

  fileSocketDescriptors[0] = open(inName.c_str(), O_RDWR | O_CREAT);
  fileSocketDescriptors[1] = open(outName.c_str(), O_RDWR | O_CREAT);
  fileSocketDescriptors[2] = open(errName.c_str(), O_RDWR | O_CREAT);

  if (vec.size() == 1) {
    executeCommand(vec[0].c_str());
    string temp = readReddir();
    outputString = temp;
    return;
  }

  for (unsigned i = 0; i < vec.size(); i += 3) {
    string op;
    // Verify operator
    if (i + 1 >= vec.size()) {
      op = "NULL";
    } else
      op = vec[i + 1];

    if (op == "&&"s) {
      int retStat;
      retStat = executeCommand(vec[i]);
      if (retStat > 0) {
        retStat = executeCommand(vec[i + 2]);
      }
    } else if (op == "||"s) {
    } else if (op == "2>"s) {
    } else if (op == "1>"s) {
    } else if (op == "|"s) {
    } else if (op == ">"s) {
    } else if (op == "<"s) {
    } else {
      // Unknown Operator
    }
  }

  string temp = readReddir();

  outputString = temp;
  // DEBUG
  // write(STDOUT_FILENO, temp.c_str(), temp.length() + 1);
  // write(STDOUT_FILENO, "\n", 1);
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

int executeCommand(string command) {
  string redirect(" > reddir.txt");
  string temp = command + redirect;
  int retStat = 0;

  retStat = system(temp.c_str());

  int pid;

  if ((pid=fork()) == -1){
    perror("Error when forking for command");
    exit(0);
  }

  if(pid == 0){

  }
  else{





    exit(0);
  }

  // Success
  if (retStat >= 0) {
    // get from reddir

    return 1;
  }
  // System fork error
  else if (retStat == -1) {
    return -1;
  }
  // Could not execute shell command
  else if (retStat == 127) {
    return 127;
  }
  // Unknown error
  else {
    return -127;
  }
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