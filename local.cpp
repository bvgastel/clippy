#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include "simple-raw.h"
#include "lib.h"

const char *socket_path = "/tmp/clipboardlocal";
//char *socket_path = "\0hidden";
#define LISTEN_BACKLOG 5

int main(int argc, char *argv[]) {

  if (argc > 1)
    socket_path = argv[1];

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket error");
    exit(-1);
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (*socket_path == '\0') {
    *addr.sun_path = '\0';
    strncpy(addr.sun_path+1, socket_path+1, sizeof(addr.sun_path)-2);
  } else {
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
    unlink(socket_path);
  }

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
    printf("accepted connection\n");

    bool good = true;
    while (good) {
      uint32_t command = ReadBinary(cfd, ClippyCommand::NONE, good);
      if (command == ClippyCommand::SET_CLIPBOARD) {
        std::string s = ReadBinary(cfd, "", good);
        if (!good)
          break;
        std::cerr << "receiving new clipboard contents: " << s << std::endl;
        SetClipboard(s, {fd, cfd});
      } else if (command == ClippyCommand::RETRIEVE_CLIPBOARD) {
        if (!WriteBinary(cfd, ClippyCommand::CLIPBOARD_CONTENTS))
          break;
        std::string clipboard = GetClipboard({fd, cfd});
        std::cerr << "sending clipboard to remote: " << clipboard << std::endl;
        if (!WriteBinary(cfd, clipboard))
          break;
      }
    }
    std::cerr << "closing connection" << std::endl;
    close(cfd);
  }

  return 0;
}

