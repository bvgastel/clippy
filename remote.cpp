#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>

#include "simple-raw.h"
#include "lib.h"
#include "daemon.h"


std::string GetInput(bool &eof, bool& error) {
  return Read(STDIN_FILENO, eof, error, 1024*1024);
}

std::optional<std::string> RemoteSession() {
  // tmux makes things difficult. Creation of the session and the active session are
  // seperate things. So we have to detect if tmux is active, and check the active
  // environment used when `tmux attach` was run.
  std::string socket_path;
  if (getenv("TMUX")) {
    auto clippy = GetTMUXVariable("LC_CLIPPY", {});
    socket_path = clippy ? *clippy : "";
    //std::cerr << "using tmux clippy var: " << socket_path << std::endl;
  } else {
    auto clippy = getenv("LC_CLIPPY");
    socket_path = clippy ? clippy : "";
    //std::cerr << "using direct clippy var: " << socket_path << std::endl;
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
  if (!WriteBinary(fd, uint8_t(ClippyCommand::PING))) {
    close(fd);
    return false;
  }

  bool good = true;
  // FIXME: add time-out?
  uint32_t command = ReadBinary(fd, uint8_t(ClippyCommand::NONE), good);
  bool retval = command == ClippyCommand::PONG;
  close(fd);
  return retval;
}

int main(int argc, char *argv[]) {
  bool get = false;
  bool set = false;
  bool ssh = false;
  bool daemon = false;
  bool notification = false;
  bool openurl = false;
  bool command = false;

  if (argc > 1) {
    get = std::string_view(argv[1]) == "-g" || std::string_view(argv[1]) == "get";
    set = std::string_view(argv[1]) == "-s" || std::string_view(argv[1]) == "set";
    notification = std::string_view(argv[1]) == "-n" || std::string_view(argv[1]) == "notification";
    ssh = std::string_view(argv[1]) == "--ssh" || std::string_view(argv[1]) == "ssh";
    daemon = std::string_view(argv[1]) == "--daemon" || std::string_view(argv[1]) == "daemon";
    openurl = std::string_view(argv[1]) == "openurl";
    command = std::string_view(argv[1]) == "command";
  }

  if (notification && (argc < 3 || argc > 4)) {
    fprintf(stderr, "Usage: %s notification [summary] [body]\n", argv[0]);
    return -1;
  }
  if (openurl && argc != 3) {
    fprintf(stderr, "Usage: %s openurl [url]\n", argv[0]);
    return -1;
  }

  if (daemon) {
    std::string local_socket_path = "/tmp/clipboardlocal." + GetUsername();
    Server(local_socket_path);
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
          Server(local_socket_path);
          // Server() does not return
        }
        std::cerr << "Starting clippy daemon (pid=" << pid << ")" << std::endl;
      }
      args.push_back("-R"); args.push_back(remote_socket_path + ":" + local_socket_path);
    }
    args.push_back("-o"); args.push_back("StreamLocalBindUnlink yes");
    for (int i = 2; i < argc; ++i)
      args.push_back(argv[i]);
    execvp(args);
    return 0;
  }
  auto remoteSession = RemoteSession();
  //std::cerr << "using socket: " << *remoteSession << std::endl;

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
    if (notification) {
      auto summary = argv[2];
      auto body = argc >= 4 ? argv[3] : "";
      ShowNotification(summary, body, {});
      return 0;
    }
    if (openurl) {
      auto url = argv[2];
      OpenURL(url, {});
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
    if (!WriteBinary(fd, uint8_t(ClippyCommand::RETRIEVE_CLIPBOARD)))
      return -1;
    bool good = true;
    uint32_t c = ReadBinary(fd, uint8_t(ClippyCommand::NONE), good);
    if (c == ClippyCommand::CLIPBOARD_CONTENTS) {
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
    if (!WriteBinary(fd, uint8_t(ClippyCommand::SET_CLIPBOARD)))
      return -1;
    bool eof = false;
    bool error = false;
    std::string retval = GetInput(eof, error);
    if (error)
      return -1;
    if (!WriteBinary(fd, retval))
      return -1;
    // FIXME: check message back, if setting the clipboard succeeded
    return 0;
  }
  if (notification) {
    if (!WriteBinary(fd, uint8_t(ClippyCommand::SHOW_NOTIFICATION)))
      return -1;
    if (!WriteBinary(fd, argv[2]))
      return -1;
    if (!WriteBinary(fd, argc >= 4 ? argv[3] : ""))
      return -1;
    // FIXME: check message back, if notification was shown
    return 0;
  }
  if (openurl) {
    if (!WriteBinary(fd, uint8_t(ClippyCommand::OPEN_URL)))
      return -1;
    if (!WriteBinary(fd, argv[2]))
      return -1;
    // FIXME: check message back, if url was opened
    return 0;
  }
  if (command) {
    //std::cerr << "command" << std::endl;
    if (!WriteBinary(fd, uint8_t(ClippyCommand::LOCAL_CMD)))
      return -1;
    std::vector<std::string> c;
    for (int i = 2; i < argc; ++i)
      c.push_back(argv[i]);
    if (!WriteBinary(fd, c))
      return -1;
    //std::cerr << "going into proxy loop with fd=" << fd << std::endl;
    int status = 0;
    bool running = true;
    ProxyState commandIn {STDIN_FILENO, fd, [](const std::string& in) -> uint32_t { return uint32_t(in.size()); }, [](std::string in) -> std::tuple<std::string,bool> {
      uint32_t length = 1 + uint32_t(in.size());
      length = ntoh(length);
      char cmd[2] = {char(in.size() > 0 ? ClippyCommand::LOCAL_CMD_STDIN : ClippyCommand::LOCAL_CMD_STDIN_CLOSE), 0};
      return {std::string(reinterpret_cast<const char*>(&length), sizeof(length)) + std::string(cmd) + in, true};
    }};
    ProxyState commandOut {fd, STDOUT_FILENO, ProtocolMessageLength, [&status, &running](std::string in) -> std::tuple<std::string,bool> {
      //std::cerr << "got message of " << in.size() << std::endl;
      if (in.size() < 5 || in[4] == ClippyCommand::LOCAL_CMD_STDOUT_CLOSE) {
        //std::cerr << "closing stdout from remote connection" << std::endl;
        return {std::string(), true};
      }
      if (in[4] == ClippyCommand::LOCAL_CMD_STDOUT) {
        //std::cerr << "got stdout of " << in.size()-5 << std::endl;
        return {in.substr(5), true};
      } else if (in[4] == ClippyCommand::LOCAL_CMD_STDERR) {
        //std::cerr << "got stderr of " << in.size()-5 << std::endl;
        // we assume STDERR is smaller, and does not need to be buffered
        std::cerr << in.substr(5);
      } else if (in[4] == ClippyCommand::LOCAL_CMD_STATUS) {
        status = uint8_t(in[5]);
        //std::cerr << "got status of remote command: " << status << std::endl;
        running = false;
        return {std::string(), false};
      }
      return {std::string(), true};
    }, [&running]{ if (running) { exit(-1); }}}; // lost connection too early, so error exit
    auto remaining = Proxy({&commandIn, &commandOut}, [&running](size_t count) {
      //std::cerr << "wait loop for " << count << " fds" << std::endl;
      return count >= 1 && running; });
    int s = fcntl(STDOUT_FILENO, F_GETFL, 0);
    ENSURE(fcntl(STDOUT_FILENO, F_SETFL, s & ~O_NONBLOCK) == 0);
    //std::cerr << "writing remaining " << commandOut.converted.size() << " bytes" << std::endl;
    auto& r = remaining[STDOUT_FILENO];
    if (SafeWrite(STDOUT_FILENO, r.data(), r.size()) != r.size())
      return -1;
    exit(status);
  }

  return 0;
}

