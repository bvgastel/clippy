#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "simple-raw.h"
#include "lib.h"

#define LISTEN_BACKLOG 5

void ProcessExit(int) {
  int status = 0; 
  while (pid_t pid = waitpid(-1, &status, WNOHANG));
}

void Connection(int cfd) {
  bool good = true;
  while (good) {
    uint32_t command = ReadBinary(cfd, ClippyCommand::NONE, good);
    if (command == ClippyCommand::SET_CLIPBOARD) {
      std::string s = ReadBinary(cfd, "", good);
      if (!good)
        break;
      // std::cerr << "receiving new clipboard contents: " << s << std::endl;
      SetClipboard(s, {cfd});
    } else if (command == ClippyCommand::RETRIEVE_CLIPBOARD) {
      if (!WriteBinary(cfd, ClippyCommand::CLIPBOARD_CONTENTS))
        break;
      std::string clipboard = GetClipboard({cfd});
      // std::cerr << "sending clipboard to remote: " << clipboard << std::endl;
      if (!WriteBinary(cfd, clipboard))
        break;
    } else if (command == ClippyCommand::PING) {
      if (!WriteBinary(cfd, ClippyCommand::PONG))
        break;
    } else if (command == ClippyCommand::SHOW_NOTIFICATION) {
      std::string summary = ReadBinary(cfd, "", good);
      std::string body = ReadBinary(cfd, "", good);
      if (!good)
        break;
      // std::cerr << "receiving new clipboard contents: " << s << std::endl;
      ShowNotification(summary, body, {cfd});
    } else {
      break;
    }
  }
}
[[noreturn]] void Server(std::string socket_path) {
  // good thing to close these file descriptors, to not interfere with stdin/stdout of e.g. ssh
  close(STDIN_FILENO);
  close(STDOUT_FILENO);

#ifdef __FreeBSD__
  setproctitle("local");
#endif

  signal(SIGCHLD, ProcessExit);

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket error");
    exit(-1);
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path)-1);
  unlink(socket_path.c_str());

  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("bind error");
    exit(-1);
  }

  if (listen(fd, LISTEN_BACKLOG) == -1) {
    perror("listen error");
    exit(-1);
  }

  // std::string clipboard = GetClipboard({fd});
  // std::cerr << "Clipboard: " << clipboard << std::endl;

  // SetClipboard("foobar3", {fd});

  while (true) {
    int cfd = accept(fd, NULL, NULL);
    if (cfd < 0) {
      perror("accept error");
      continue;
    }
    //fprintf(stderr, "accepted connection on %i: %i\n", fd, cfd);

    pid_t child = fork();
    if (child == 0) {
      close(fd);
      Connection(cfd);
      close(cfd);
      exit(0);
    }
    // std::cerr << "closing connection" << std::endl;
    close(cfd);
  }
}
