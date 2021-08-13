#include "lib.h"
#include "simple-raw.h"

#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <stdexcept>
#include <fstream>

bool IsFile(std::string file) {
  struct stat st;
  if (stat(file.c_str(), &st) != 0) {
    if (errno == ENOENT)
      return false;
    throw std::invalid_argument(std::string("File::IsFile: could not stat() file ") + file);
  }
  return S_ISREG(st.st_mode) || S_ISLNK(st.st_mode);
}
bool IsSocket(std::string file) {
  struct stat st;
  if (stat(file.c_str(), &st) != 0) {
    if (errno == ENOENT)
      return false;
    throw std::invalid_argument(std::string("File::IsSocket: could not stat() file ") + file);
  }
  return S_ISSOCK(st.st_mode);
}
std::string Contents(int fd, bool& eof, bool& error, size_t max) {
  std::string retval;
  eof = false;
  error = false;
  if (fd < 0)
    return retval;

  char buffer[16384];
  while (retval.size() < max) {
    auto bytes = read(fd, buffer, std::min(max - retval.size(), sizeof(buffer)));
    //fprintf(stderr, "File::Contents: %lli -> %i/%s\n", int64_t(bytes), errno, strerror(errno));
    if (bytes < 0 && errno == EINTR)
      continue;
    // if marked non-blocking, it is ok
    if (bytes == 0 || (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
      eof = bytes == 0;
      break;
    }
    if (bytes < 0) {
      error = true;
      eof = true;
      // DO NOT throw, because that might cause resource leaks (file descriptors not closed)
      //throw std::invalid_argument(std::string() + "File::Contents error for fd=" + std::to_string(fd) + "; error=" + std::to_string(errno) + "/" + strerror(errno));
      return {};
    }
    assert(bytes > 0);
    retval.append(buffer, bytes);
  }
  return retval;
}

std::tuple<int, int, pid_t> ExecRedirected(const std::vector<std::string>& command, bool redirectError, const std::vector<int>& closeAfterFork) {
	//std::cout << "executing " << command[0] << std::endl;
	int pipeForInput[2]; // the input of the child process is going here, so the parent can write to it
	ENSURE(pipe(pipeForInput) == 0);
	int pipeForOutput[2]; // the output of the child process is directed to this, so the parent can read from it
	ENSURE(pipe(pipeForOutput) == 0);

	pid_t child = fork();
	if (child == 0) {
		// child
		close(pipeForInput[1]); // close write end
		close(pipeForOutput[0]); // close read end
		if (redirectError)
			dup2(pipeForOutput[1], STDERR_FILENO);
		dup2(pipeForOutput[1], STDOUT_FILENO);
		dup2(pipeForInput[0], STDIN_FILENO);
		close(pipeForInput[0]); // close read end
		close(pipeForOutput[1]); // close write end
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
		closefrom(STDERR_FILENO + 1);
    USING(closeAfterFork);
#else
    for (int fd : closeAfterFork)
      close(fd);
#endif
		char** args = static_cast<char**>(alloca(sizeof(const char*) * (command.size() + 1)));
		for (size_t i = 0; i < command.size(); ++i)
			args[i] = const_cast<char*>(command[i].c_str());
		args[command.size()] = nullptr;
		execvp(command[0].c_str(), args);
		exit(-1);
	}
	close(pipeForInput[0]); // close read end
	close(pipeForOutput[1]); // close write end
	return std::make_tuple(pipeForOutput[0], pipeForInput[1], child);
}

std::string GetUsername() {
	struct passwd _pw;
	struct passwd *pw;
	char buffer[256];
  auto err = getpwuid_r(getuid(), &_pw, buffer, sizeof(buffer), &pw);
  if (err == 0 && pw)
    return pw->pw_name;
  return {};
}

static std::ifstream OpenFile(std::string filename) {
  std::ifstream infile;
  infile.open(filename.c_str(), std::ios::binary);
  if (!infile.is_open())
    throw std::invalid_argument("could not open file: " + filename);

  // pos == max long on Linux if reading a directory
  if (!infile.good())
    throw std::invalid_argument("could not get size of file (possibly a dir): " + filename);
  return infile;
}

std::string FileContents(std::string filename) {
  // FIXME: replace with Contents(fd, ...) version
  // so there is less code
  auto infile = OpenFile(filename);

  std::string retval;
  char buffer[16384];

  do {
    infile.read(buffer, sizeof(buffer));
    retval.append(buffer, 0, infile.gcount());
  } while (infile.good());

  if (infile.bad())
    throw std::invalid_argument("could not read file: " + filename);

  infile.close();
  return retval;
}

bool IsOnWSL() {
  std::string version = FileContents("/proc/version");
  return version.find("icrosoft") != std::string::npos;
}

bool IsLocalSession(std::vector<int> closeAfterFork) {
  // if TMUX is active (TMUX env var) first query tmux (so the variables are copied from the environment driving starting tmx)
  if (getenv("TMUX")) {
    std::vector<std::string> getClipboardCommand = {"tmux", "show-environment", "DISPLAY"};
    auto [rfd, wfd, pid] = ExecRedirected(getClipboardCommand, false, closeAfterFork);
    close(wfd);
    bool eof = false;
    bool error = false;
    std::string retval = Contents(rfd, eof, error, 1024*1024);
    error |= retval.size() == 1024*1024 && !eof;
    close(rfd);
    // if empty, then command is assumed to have failed
    if (!error && !retval.empty()) {
      // tmux outputs "-DISPLAY" if variable is not found and "DISPLAY=foobar" if variable is found
      return retval.substr(0, 1) != "-";
    }
  }
  // detect if it is a X11 session by checking if DISPLAY is set
  if (getenv("DISPLAY"))
    return true;
  // if (getenv("WAYLAND_DISPLAY"))
  //   return true;
  return false;
}


std::string GetClipboard(std::vector<int> closeAfterFork) {
#if defined(__APPLE__)
  std::vector<std::string> getClipboardCommand = {"pbpaste"};
#else
  std::vector<std::string> getClipboardCommand = {"xsel", "--clipboard", "--output"};
  bool wsl = IsOnWSL();
  if (wsl) {
    getClipboardCommand = {"powershell.exe", "Get-Clipboard"};
  }
#endif
  auto [rfd, wfd, pid] = ExecRedirected(getClipboardCommand, false, closeAfterFork);
  close(wfd);
  bool eof = false;
  bool error = false;
  std::string retval = Contents(rfd, eof, error, 1024*1024);
  error |= retval.size() == 1024*1024 && !eof;
  close(rfd);
  if (wsl) {
    // `powershell.exe Get-Clipboard` appends \r\n to output, get rid of it
    retval = retval.size() >= 2 ? retval.substr(0, retval.size()-2) : std::string();
  }
  // better to return nothing than half an clipboard
  // this way the user knows something went wrong
  return !error ? retval : std::string();
}

bool SetClipboard(std::string clipboard, std::vector<int> closeAfterFork) {
#if defined(__APPLE__)
  std::vector<std::string> setClipboardCommand = {"pbcopy"};
#else
  std::vector<std::string> setClipboardCommand = {"xsel", "--clipboard", "--input"};
  if (IsOnWSL()) {
    setClipboardCommand = {"clip.exe"};
  }
#endif
  auto [rfd, wfd, pid] = ExecRedirected(setClipboardCommand, false, closeAfterFork);
  close(rfd);
  auto bytes = SafeWrite(wfd, clipboard.c_str(), clipboard.size());
  close(wfd);
  return bytes == clipboard.size();
}
