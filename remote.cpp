#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "simple-raw.h"
#include "lib.h"


std::string GetInput(bool &eof, bool& error) {
  return Contents(STDIN_FILENO, eof, error, 1024*1024);
}

int main(int argc, char *argv[]) {
  bool get = false;
  bool set = false;
  //if (argc > 1) socket_path=argv[1];
  if (argc > 1) {
    get = std::string_view(argv[1]) == "-g";
    set = std::string_view(argv[1]) == "-s";
  }

  std::string socket_path = "/tmp/clipboardremote." + GetUsername();
  // std::cerr << "using socket: " << socket_path << std::endl;

  // fallback to local clipboard if remote connection is not available
  if (!IsSocket(socket_path.c_str()) || IsLocalSession({})) {
    // std::cerr << "Remote socket is not available, guessing this is a local session." << std::endl;
    if (get) {
      std::string clipboard = GetClipboard({});
      std::cout << clipboard;
      std::flush(std::cout);
      return 0;
    }
    if (set) {
      bool eof = false;
      bool error = false;
      std::string retval = GetInput(eof, error);
      if (!error)
        SetClipboard(retval, {});
      return 0;
    }
    return -1;
  }
  //std::cerr << "Remote socket found" << std::endl;

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket error");
    exit(-1);
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path)-1);

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("connect error");
    exit(-1);
  }

  // get clipboard
  if (get) {
    if (!WriteBinary(fd, ClippyCommand::RETRIEVE_CLIPBOARD))
      return -1;
    bool good = true;
    uint32_t command = ReadBinary(fd, ClippyCommand::NONE, good);
    if (command == ClippyCommand::CLIPBOARD_CONTENTS) {
      std::string clipboard = ReadBinary(fd, std::string(), good);
      if (!good)
        return -1;
      // std::cerr << "setting clipboard to " << clipboard << std::endl;
      // SetClipboard(clipboard, {fd});
      std::cout << clipboard;
      std::flush(std::cout);
    }
    return 0;
  }
  if (set) {
    // set clipboard
    if (!WriteBinary(fd, ClippyCommand::SET_CLIPBOARD))
      return -1;
    bool eof = false;
    bool error = false;
    std::string retval = GetInput(eof, error);
    if (error)
      return -1;
    if (!WriteBinary(fd, retval))
      return -1;
    return 0;
  }

  return 0;
}

