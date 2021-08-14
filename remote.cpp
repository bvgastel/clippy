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

// wrapper around the C execvp so it can be called with C++ strings (easier to work with)
// always start with the command itself
int execvp(const std::vector<std::string>& args) {
	// build argument list
	const char** c_args = new const char*[args.size()+1];
	for (size_t i = 0; i < args.size(); ++i) {
		c_args[i] = args[i].c_str();
	}
	c_args[args.size()] = nullptr;
	// replace current process with new process as specified
	::execvp(c_args[0], const_cast<char**>(c_args));
	// if we got this far, there must be an error
	int retval = errno;
	// in case of failure, clean up memory
	delete[] c_args;
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
    std::string local_socket_path = "/tmp/clipboardlocal." + GetUsername();
    std::string remote_socket_path = "/tmp/clipboardremote." + GetUsername() + "." + std::to_string(getpid());
    std::vector<std::string> args;
    args.push_back("ssh");
    args.push_back("-o"); args.push_back("SetEnv LC_CLIPPY=" + remote_socket_path);
    args.push_back("-R"); args.push_back(remote_socket_path + ":" + local_socket_path);
    args.push_back("-o"); args.push_back("StreamLocalBindUnlink yes");
    for (int i = 2; i < argc; ++i)
      args.push_back(argv[i]);
    execvp(args);
    return 0;
  }
  // std::cerr << "using socket: " << socket_path << std::endl;
  std::string socket_path = getenv("LC_CLIPPY");

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

