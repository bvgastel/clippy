#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "simple-raw.h"
#include "lib.h"
#include "daemon.h"


std::string GetInput(bool &eof, bool& error) {
  return Contents(STDIN_FILENO, eof, error, 1024*1024);
}

std::optional<std::string> RemoteSession() {
  // tmux makes things difficult. Creation of the session and the active session are
  // seperate things. So we have to detect if tmux is active, and check the active
  // environment used when `tmux attach` was run.
  std::string socket_path;
  if (getenv("TMUX")) {
    auto clippy = GetTMUXVariable("LC_CLIPPY", {});
    socket_path = clippy ? *clippy : "";
  } else {
    auto clippy = getenv("LC_CLIPPY");
    socket_path = clippy ? clippy : "";
  }
  if (!IsSocket(socket_path.c_str()))
    return {};
  return {std::move(socket_path)};
}

bool Active(std::string local_socket_path) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    // should not occur, can not test if there is an active connection this way
    perror("socket error");
    exit(-1);
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, local_socket_path.c_str(), sizeof(addr.sun_path)-1);

  // FIXME: add time-out?
  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    //perror("connect error");
    close(fd);
    return false;
  }

  // FIXME: add time-out?
  if (!WriteBinary(fd, ClippyCommand::PING)) {
    close(fd);
    return false;
  }

  bool good = true;
  // FIXME: add time-out?
  uint32_t command = ReadBinary(fd, ClippyCommand::NONE, good);
  bool retval = command == ClippyCommand::PONG;
  close(fd);
  return retval;
}

int main(int argc, char *argv[]) {
  bool get = false;
  bool set = false;
  bool ssh = false;

  if (argc > 1) {
    get = std::string_view(argv[1]) == "-g";
    set = std::string_view(argv[1]) == "-s";
    ssh = std::string_view(argv[1]) == "--ssh";
  }

  if (ssh) {
    auto nestedRemoteSession = RemoteSession();
    std::string remote_socket_path = "/tmp/clipboardremote." + GetUsername() + "." + std::to_string(getpid());

    std::vector<std::string> args;
    args.push_back("ssh");
    args.push_back("-o"); args.push_back("SetEnv LC_CLIPPY=" + remote_socket_path);
    // nested session
    if (nestedRemoteSession) {
      args.push_back("-R"); args.push_back(remote_socket_path + ":" + *nestedRemoteSession);
    } else {
      // regular
      std::string local_socket_path = "/tmp/clipboardlocal." + GetUsername();

      // if no clippy daemon is active on local host, start one
      if (!IsSocket(local_socket_path) || !Active(local_socket_path)) {
        assert(IsLocalSession({}));
        pid_t pid = fork();
        if (pid == 0) {
          setsid(); // creates new session so that this daemon persists after ssh connection
          // FIXME: close does not work (xsel gives back file descriptor on stdin and stdout)
          // probably a good thing to close these file descriptors, to not interfere with stdin/stdout of ssh
          // close(STDIN_FILENO);
          // close(STDOUT_FILENO);
          Server(local_socket_path);
          return 0;
        }
      }
      args.push_back("-R"); args.push_back(remote_socket_path + ":" + local_socket_path);
    }
    args.push_back("-o"); args.push_back("StreamLocalBindUnlink yes");
    for (int i = 2; i < argc; ++i)
      args.push_back(argv[i]);
    execvp(args);
    return 0;
  }
  // std::cerr << "using socket: " << socket_path << std::endl;
  auto remoteSession = RemoteSession();

  // fallback to local clipboard if remote connection is not available
  if (!remoteSession) {
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
  strncpy(addr.sun_path, remoteSession->c_str(), sizeof(addr.sun_path)-1);

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

