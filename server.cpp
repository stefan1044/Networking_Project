#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
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
int serverSocketDescriptor, currentPid, debugDescriptor;
int fileDescriptors[3];
struct sockaddr_in server;
struct sockaddr_in from;

string operators[] = {"&&", "||", "2>", "1>", "|", ">", "<"};
string outputString;
string currentDirectory;

int retStatus;

int waitForClient();
int pingClient(string& temp, int sd);
int receivePing(string& buffer, bool& loggedIn, int sd);
void sigpipeMask(int sig);
void processInput(string input);
void processString(string input);
string readOutput();
string readError();
vector<string> separateStringOnOperators(string input);
int executeCommand(string command);
int operatorErrReddir(string command, string fileName);
int operatorOutputReddir(string command, string fileName);
int operatorInputReddir(string command, string fileName);
int operatorPipe(string command1, string command2);

string encrypt(string str);
string decrypt(string str);
void writeDebug(string str);

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

  // Server loop
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
      // individual client fork
      close(serverSocketDescriptor);
      char wd[200];
      getcwd(wd, 200);
      printf("Working directory: %s\n", wd);
      currentDirectory = ""s + wd;
      currentPid = getpid();
      string inName = "stdin"s + to_string(currentPid);
      string outName = "stdout"s + to_string(currentPid);
      string errName = "stderr"s + to_string(currentPid);

      fileDescriptors[0] =
          open(inName.c_str(), O_RDWR | O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU);
      fileDescriptors[1] =
          open(outName.c_str(), O_RDWR | O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU);
      fileDescriptors[2] =
          open(errName.c_str(), O_RDWR | O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU);
      dup2(fileDescriptors[0], STDIN_FILENO);
      dup2(fileDescriptors[1], STDOUT_FILENO);
      dup2(fileDescriptors[2], STDERR_FILENO);

      debugDescriptor = open("debug.txt", O_RDWR | O_APPEND | O_CREAT,
                             S_IRWXO | S_IRWXG | S_IRWXU);
      // driver code
      fd_set descriptorSet;
      FD_ZERO(&descriptorSet);
      FD_SET(clientSocketDescriptor, &descriptorSet);

      timeval tv = {0, 0};
      int conStatus, inputStatus, socketCount;

      bool loggedIn = false;

      while (connectionUp) {
        fd_set copySet = descriptorSet;

        // Can Write
        socketCount =
            select(clientSocketDescriptor + 1, nullptr, &copySet, nullptr, &tv);
        if (socketCount == 1) {
          conStatus = pingClient(outputString, clientSocketDescriptor);
          // Conection is still up
          if (conStatus == 0) {
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
          if (inputStatus == 5005) {         // pinged successfully
          } else if (inputStatus == 5006) {  // received command
            processInput(inputString);
          } else if (inputStatus == 5007) {  // logged in
            loggedIn = true;
            outputString = "Successfully logged in!\n";
          } else if (inputStatus == 5008) {  // logged out
            loggedIn = false;
            outputString = "Logged out!\n";
          } else if (inputStatus == 5009) {  // sent message without login
            outputString = "You need to login!\n";
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
      writeDebug(closeString);
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
    temp = "";
  } else if (write(sd, "0", 4096) <= 0) {  // send keepalive
    connectionUp = false;
    writeDebug("Error when trying to send keepAlive!"s);
    return errno;
  }

  return 0;
}
int receivePing(string& buffer, bool& loggedIn, int sd) {
  char* buff = new char[4096];
  int nrRead = read(sd, buff, 4096);
  if (nrRead <= 0) {
    connectionUp = false;
    writeDebug("Error when receiving ping!Client Probably disconneted"s);
    return errno;
  }

  if (buff[0] == '0' && ((""s + buff) == "0"s)) {
    return 5005;  // Pinged successfully!
  } else {
    string temp(buff);
    buffer = decrypt(temp);
    // attempt to login
    if (loggedIn == false) {
      if (buffer.find("login "s) == 0) {
        ifstream in("database.txt");
        char fileStr[128];
        string subString = buffer.substr(6);
        while (in.getline(fileStr, 128)) {
          if (subString == string(fileStr)) {
            writeDebug("Read: "s + (""s + fileStr));
            in.close();
            return 5007;
          }
        }
        in.close();
      }

      return 5009;
    }

    if (buffer == "logout"s) {
      buffer = "";
      return 5008;
    }
    writeDebug("Message from client: "s + buffer);

    return 5006;  // Received input!
  }

  return -1;  // FailSafe
}

void sigpipeMask(int sig) {
  writeDebug("Disconected!"s);
  connectionUp = false;
}

// separtes commands by ; and passes each to processString to be run
void processInput(string input) {
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
    processString(var);
  }
  // cout << "DEBUG ended\n";
}
// Bulk of the work. Executes commands and operators
void processString(string input) {
  // Clear contents of files
  fseek(stdin, 0, SEEK_SET);
  ftruncate(fileDescriptors[0], 0);
  fseek(stdout, 0, SEEK_SET);
  ftruncate(fileDescriptors[1], 0);
  fseek(stderr, 0, SEEK_SET);
  ftruncate(fileDescriptors[2], 0);
  // Separate commands and operators
  vector<string> vec = separateStringOnOperators(input);
  int currentStatus = 0;

  // sanitize pipe
  executeCommand("rm piepfile");
  fseek(stderr, 0, SEEK_SET);
  ftruncate(fileDescriptors[2], 0);

  if (vec.size() == 1) {
    currentStatus = executeCommand(vec[0].c_str());
    writeDebug(to_string(currentStatus));
    if (currentStatus == 127 || currentStatus == -1 || currentStatus == -127) {
      outputString = "Could not execute command!\n";
      return;
    } else if (currentStatus >= 0) {
      string temp = readOutput();
      temp.append(""s + readError());
      outputString = temp;
      writeDebug("Outputs is: "s + outputString);
      return;
    }
    return;
  }

  string lastOutput, errStream, lastOp = "NULL"s;
  // iterate over commands and operators
  bool pipeLine = false;
  for (unsigned i = 0; i < vec.size(); i += 2) {
    if (pipeLine == true)
      pipeLine = false;
    else {
      executeCommand("rm pipefile"s);
      fseek(stdout, 0, SEEK_SET);
      ftruncate(fileDescriptors[1], 0);
      fseek(stderr, 0, SEEK_SET);
      ftruncate(fileDescriptors[2], 0);
    }
    string op;

    // Verify operator
    if (i + 1 >= vec.size()) {
      op = "NULL";
    } else
      op = vec[i + 1];

    if (op == "NULL"s) {
      int retStat;
      retStat = executeCommand(vec[i]);

      if (retStat == 0) {
        outputString.append(readOutput());
      } else {
        outputString.append(readError());
      }
    } else if (op == "&&"s) {
      int retStat;
      retStat = executeCommand(vec[i]);
      writeDebug("Status before operator "s + op + " : "s + to_string(retStat));
      if (retStat == 0) {
        outputString.append(readOutput());
        retStat = executeCommand(vec[i + 2]);
        writeDebug("Status after operator: " + to_string(retStat));
        // if (retStat == 0) {
        //   outputString.append(readOutput());
        // } else {
        //   outputString.append(readError());
        // }
      } else {
        outputString.append(readError());
        break;
      }
    } else if (op == "||"s) {
      int retStat;
      retStat = executeCommand(vec[i]);
      writeDebug("Status before operator "s + op + " : "s + to_string(retStat));
      if (retStat == 0) {
        outputString.append(readOutput());
        break;
      } else {
        //   outputString.append(readError());
        //   retStat = executeCommand(vec[i + 2]);
        //   writeDebug("Status after operator: " + to_string(retStat));
        //   if (retStat == 0) {
        //     outputString.append(readOutput());
        //   }
        // else {
        outputString.append(readError());
        //}
      }
    } else if (op == "2>"s) {
      int retStat;
      retStat = operatorErrReddir(vec[i], vec[i + 2]);

      if (retStat == -1) writeDebug("Error opening the file!"s);
    } else if (op == "1>"s) {
      int retStat;
      retStat = operatorOutputReddir(vec[i], vec[i + 2]);

      if (retStat == -1) writeDebug("Error opening the file!"s);
    } else if (op == "|"s) {
      int retStat;
      retStat = operatorPipe(vec[i], vec[i + 2]);

      if (retStat != 0) {
        outputString = readError();
      } else {
        pipeLine = true;
        outputString = readOutput();
      }
    } else if (op == ">"s) {
      int retStat;
      retStat = operatorOutputReddir(vec[i], vec[i + 2]);

      if (retStat == -1) writeDebug("Error opening the file!"s);
    } else if (op == "<"s) {
      int retStat;
      retStat = operatorInputReddir(vec[i], vec[i + 2]);

      writeDebug("RetStat is: "s + to_string(retStat));
      if (retStat == -1)
        writeDebug("Error opening the file!"s);
      else {
        outputString = readOutput();
      }
    } else {
      // Unknown Operator
    }
    lastOutput = outputString;
  }
}
string readOutput() {
  char buff[4096];
  bzero(buff, 4096);
  int nrRead = 0;
  fseek(stdout, 0, SEEK_SET);
  if ((nrRead = read(STDOUT_FILENO, buff, 4095)) < 0) {
    writeDebug("Error reading from output!"s);
  } else {
    writeDebug("Bytes Read: "s + to_string(nrRead));
    writeDebug("Readoutput read: "s + buff);
  }

  ftruncate(fileDescriptors[1], 0);

  return (""s + buff);
}
string readError() {
  char buff[4096];
  bzero(buff, 4096);
  int nrRead = 0;
  fseek(stderr, 0, SEEK_SET);
  if ((nrRead = read(fileDescriptors[2], buff, 4095)) < 0) {
    writeDebug("Error reading from stderr!"s);
  } else {
    writeDebug("Bytes Read: "s + to_string(nrRead));
    writeDebug("ReadError read: "s + buff);
  }

  ftruncate(fileDescriptors[2], 0);

  return (""s + buff);
}

vector<string> separateStringOnOperators(string input) {
  vector<string> vec;
  size_t pos = 0;

  bool ok = true;
  while (ok) {
    bool finishedOps = true;
    for (auto op : operators) {
      pos = input.find(op);

      if (pos == string::npos) {
        continue;
      } else {
        finishedOps = false;
      }
      vec.push_back(input.substr(0, pos));
      vec.push_back(op);
      input.erase(0, pos + op.length());
      break;
    }
    if (finishedOps == true) ok = false;
  }
  if (input.size() > 0) {
    vec.push_back(input);
  }

  // DEBUG
  for (auto var : vec) {
    writeDebug(var);
  }
  // END DEBUG

  return vec;
}

int executeCommand(string command) {
  int retStat = -1;
  fseek(stdin, 0, SEEK_SET);
  writeDebug("Executing command: "s + command);

  command = "cd " + currentDirectory + " && " + command;
  retStat = WEXITSTATUS(system(command.c_str()));

  // Success
  if (retStat == 0) {
    // get from reddir

    return retStat;
  }
  // Command Error
  else if (retStat > 0) {
    return retStat;
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

int operatorErrReddir(string command, string fileName) {
  int retStat = -1;
  retStat = executeCommand(command);

  string err = readError();
  int startPos = fileName.find_first_not_of(' ');
  int lastPos = fileName.find_last_not_of(' ');
  fileName = fileName.substr(startPos, lastPos - startPos + 1);

  int fd =
      open(fileName.c_str(), O_RDWR | O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU);
  if (fd < 0) {
    writeDebug("Error opening file!"s);
    close(fd);
    return -1;
  } else {
    write(fd, err.c_str(), err.length() + 1);
  }

  close(fd);
  return retStat;
}
int operatorOutputReddir(string command, string fileName) {
  int retStat = -1;
  retStat = executeCommand(command);

  string output = readOutput();
  int startPos = fileName.find_first_not_of(' ');
  int lastPos = fileName.find_last_not_of(' ');
  fileName = fileName.substr(startPos, lastPos - startPos + 1);

  int fd =
      open(fileName.c_str(), O_RDWR | O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU);
  if (fd < 0) {
    writeDebug("Error opening file!"s);
    close(fd);
    return -1;
  } else {
    write(fd, output.c_str(), output.length() + 1);
  }

  close(fd);
  return retStat;
}
int operatorInputReddir(string command, string fileName) {
  int retStat = -1;

  int startPos = fileName.find_first_not_of(' ');
  int lastPos = fileName.find_last_not_of(' ');
  fileName = fileName.substr(startPos, lastPos - startPos + 1);
  executeCommand("touch temp");
  executeCommand("chmod 777 temp");
  int fd = open("temp", O_RDWR, S_IRWXO | S_IRWXG | S_IRWXU);
  if (fd < 0) {
    writeDebug("Error opening file!"s);
    close(fd);
    executeCommand("rm temp");
    return -1;
  }
  executeCommand("cat "s + fileName + " >temp"s);

  char buff[8192];
  int nrRead = read(fd, buff, 8191);

  if (nrRead < 0) {
    writeDebug("Error reading from file!"s);
    close(fd);
    executeCommand("rm temp");
    return -1;
  }
  write(fileDescriptors[0], buff, strlen(buff));

  close(fd);
  // executeCommand("rm temp");
  retStat = executeCommand(command);

  return retStat;
}
int operatorPipe(string command1, string command2) {
  int retStat;
  bool pipeExist = false;
  ftruncate(fileDescriptors[2], 0);

  // Check if pipealready exists
  retStat = executeCommand("cat pipefile"s);
  writeDebug("Ret status is " + to_string(retStat));
  if (retStat == 0) pipeExist = true;
  writeDebug("Pipe exists? : "s + to_string(pipeExist));

  // Sanitize after commands
  fseek(stdout, 0, SEEK_SET);
  ftruncate(fileDescriptors[1], 0);
  fseek(stderr, 0, SEEK_SET);
  ftruncate(fileDescriptors[2], 0);

  if (pipeExist == false) {
    executeCommand("touch pipefile");
    executeCommand("chmod 777 pipefile");

    retStat = operatorOutputReddir(command1, "pipefile"s);
    writeDebug("Return status of output redirection: " + to_string(retStat));

    if (retStat != 0) {
      writeDebug("Returned before pipe"s);
      executeCommand("rm pipefile");
      return retStat;
    }

    fseek(stdout, 0, SEEK_SET);
    ftruncate(fileDescriptors[1], 0);
  }

  auto fp = fopen("pipefile", "r");
  fseek(fp, 0, SEEK_SET);

  retStat = operatorInputReddir(command2, "pipefile"s);
  writeDebug("Return status of input redirection: " + to_string(retStat));

  if (retStat != 0) {
    writeDebug("Returned after pipe"s);
    executeCommand("rm pipefile");
    return retStat;
  }

  // executeCommand("rm pipefile");

  return retStat;
}

string encrypt(string str) {
  unsigned len = str.size();

  string cstr = str;

  if (len == 1) {
    cstr[0] += 4;
    return cstr;
  }

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

  if (len == 1) {
    cstr[0] -= 4;
    return cstr;
  }

  for (unsigned i = 0; i < len - len % 2 - 1; i += 2) {
    swap(cstr[i], cstr[i + 1]);
    cstr[i] -= 4;
    cstr[i + 1] -= 4;
  }

  return cstr;
}
void writeDebug(string str) {
  write(debugDescriptor, (str + "\n"s).c_str(), str.length() + 1);
}