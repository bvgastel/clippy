#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "simple-raw.h"
#include "lib.h"

#define LISTEN_BACKLOG 5

void ProcessExit(int) {
  int status = 0; 
  while (true) {
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid == 0) // avoid blocking
      break;
    if (pid < 0 && errno == ECHILD) // no unwaited-for childs
      break;
    // on success or error, try again
  }
}

void Ignore(int) {
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
      // std::cerr << "show notification: " << summary << " / " << body << std::endl;
      ShowNotification(summary, body, {cfd});
    } else if (command == ClippyCommand::OPEN_URL) {
      std::string url = ReadBinary(cfd, "", good);
      if (!good)
        break;
      // std::cerr << "opening url: " << url << std::endl;
      OpenURL(url, {cfd});
    } else if (command == ClippyCommand::LOCAL_CMD) {
      std::vector<std::string> cmd = ReadBinary(cfd, std::vector<std::string>(), good);
      if (!good || cmd.empty())
        break;
      //std::cerr << "executing local command: " << cmd[0] << std::endl;
      int rfd = -1, wfd = -1, efd = -1;
      pid_t pid = 0;
      signal(SIGCHLD, Ignore);
      std::tie(wfd, rfd, efd, pid) = ExecRedirected(cmd, {cfd});
      int status = 0;
      bool running = true;
      auto allLengthsAreFine = [](const std::string& in) -> uint32_t { return uint32_t(in.size()); };
      auto processRead = [](std::string in) -> std::tuple<std::string,bool> {
        uint32_t length = 1 + uint32_t(in.size());
        length = ntoh(length);
        char c[2] = {char(in.size() > 0 ? ClippyCommand::LOCAL_CMD_STDOUT : ClippyCommand::LOCAL_CMD_STDOUT_CLOSE), 0};
        //std::cerr << "sending stdout of " << in.size() << " bytes: " << in << std::endl;
        return {std::string(reinterpret_cast<const char*>(&length), sizeof(length)) + std::string(c) + in, true};
      };
      ProxyState commandOut {rfd, cfd, allLengthsAreFine, processRead};
      auto processError = [](std::string in) -> std::tuple<std::string,bool> {
        uint32_t length = 1 + uint32_t(in.size());
        length = ntoh(length);
        char c[2] = {char(in.size() > 0 ? ClippyCommand::LOCAL_CMD_STDERR : ClippyCommand::LOCAL_CMD_STDERR_CLOSE), 0};
        //std::cerr << "sending stderr of " << in.size() << " bytes: " << in << std::endl;
        return {std::string(reinterpret_cast<const char*>(&length), sizeof(length)) + std::string(c) + in, true};
      };
      ProxyState commandError {efd, cfd, allLengthsAreFine, processError};
      auto processWrite = [](std::string in) -> std::tuple<std::string,bool> {
        ENSURE(in.size() >= 5);
        if (in[4] == ClippyCommand::LOCAL_CMD_STDIN) {
          return {in.substr(5), true};
        }
        if (in[4] == ClippyCommand::LOCAL_CMD_STDIN_CLOSE) {
          return {{}, false};
        }
        return {{}, true};
      };
      ProxyClose commandInClose = [&wfd] {
          close(wfd);
          wfd = -1;
      };
      ProxyState commandIn {cfd, wfd, ProtocolMessageLength, processWrite, commandInClose};
      auto remaining = Proxy({&commandError, &commandOut, &commandIn}, [&running, &status, pid](size_t count) -> bool {
        pid_t rc = waitpid(pid, &status, WNOHANG);
        running = running && rc == 0;
        if (rc == -1)
          perror("waitpid");
        //std::cerr << "running " << pid << ": " << running << " -> " << rc << "/" << errno << "/" << status << std::endl;
        return running || count >= 2;
      });
      //std::cerr << "finished executing local command: " << cmd[0] << " -> " << running << "/" << status << std::endl;
      int s = fcntl(cfd, F_GETFL, 0);
      ENSURE(fcntl(cfd, F_SETFL, s & ~O_NONBLOCK) == 0);
      //std::cerr << "writing remaining " << commandOut.converted.size() << " bytes" << std::endl;
      auto& r = remaining[STDOUT_FILENO];
      if (SafeWrite(cfd, r.data(), r.size()) != r.size())
        break;
      // FIXME: check read end for additional content
      if (rfd >= 0)
        close(rfd);
      if (wfd >= 0)
        close(wfd);
      if (efd >= 0)
        close(efd);
      if (running) {
        // FIXME: check if there is a force close command (other side terminates)
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR);
      }
      if (!WriteBinary(cfd, uint32_t(2)))
        break;
      if (!WriteBinary(cfd, ClippyCommand::LOCAL_CMD_STATUS))
        break;
      if (!WriteBinary(cfd, uint8_t(WEXITSTATUS(status))))
        break;
      //std::cerr << "finished handling local command: " << cmd[0] << " -> " << status << std::endl;
      // this command takes over the connection, different wire format (due to non-blockign handling of connections, the length of messages is sent up front)
      break;
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
