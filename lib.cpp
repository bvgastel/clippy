#include "lib.h"
#include "simple-raw.h"

#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include "simple-raw.h"

#include <chrono>

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iomanip>

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
std::string Read(int fd, bool& eof, bool& error, size_t max) {
  std::string retval;
  eof = false;
  error = false;
  if (fd < 0)
    return retval;

  char buffer[16384];
  while (retval.size() < max) {
    auto bytes = read(fd, buffer, std::min(max - retval.size(), sizeof(buffer)));
    //fprintf(stderr, "File::Read: %lli -> %i/%s\n", int64_t(bytes), errno, strerror(errno));
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
      //throw std::invalid_argument(std::string() + "File::Read: error for fd=" + std::to_string(fd) + "; error=" + std::to_string(errno) + "/" + strerror(errno));
      return {};
    }
    assert(bytes > 0);
    retval.append(buffer, size_t(bytes));
  }
  return retval;
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

bool CheckFD(int fd) {
  int rc = fcntl(fd, F_GETFD);
  return rc >= 0 || errno != EBADF;
}

std::tuple<int, int, int, pid_t> ExecRedirected(const std::vector<std::string>& command, const std::vector<int>& closeAfterFork) {
  //std::cout << "executing " << command[0] << std::endl;
  int pipeForInput[2]; // the input of the child process is going here, so the parent can write to it
  ENSURE(pipe(pipeForInput) == 0);
  int pipeForOutput[2]; // the output of the child process is directed to this, so the parent can read from it
  ENSURE(pipe(pipeForOutput) == 0);
  int pipeForError[2]; // the stderr of the child process is directed to this, so the parent can read from it
  ENSURE(pipe(pipeForError) == 0);

  pid_t child = fork();
  if (child == 0) {
    // child
    close(pipeForInput[1]); // close write end
    close(pipeForOutput[0]); // close read end
    close(pipeForError[0]); // close read end

    dup2(pipeForError[1], STDERR_FILENO);
    close(pipeForError[1]); // close write end
    dup2(pipeForOutput[1], STDOUT_FILENO);
    close(pipeForOutput[1]); // close write end

    // if fd=0 was not used
    // pipeForInput[0] will be allocated on fd=0
    if (pipeForInput[0] != STDIN_FILENO) {
      dup2(pipeForInput[0], STDIN_FILENO);
      close(pipeForInput[0]); // close read end
    }

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    closefrom(STDERR_FILENO + 1);
    USING(closeAfterFork);
#else
    for (int fd : closeAfterFork) {
      // especially useful check if early on in the program stdin/stdout are closed, and
      // there are file descriptors opened (which are assigned fd 0 and 1).
      if (fd > STDERR_FILENO)
        close(fd);
    }
#endif
    //std::cerr << command[0] << ": " << CheckFD(STDIN_FILENO) << " / " << CheckFD(STDOUT_FILENO) << " / " << CheckFD(STDERR_FILENO) << std::endl;;

    execvp(command);
    exit(-1);
  }
  close(pipeForInput[0]); // close read end
  close(pipeForOutput[1]); // close write end
  close(pipeForError[1]); // close stderr end
  return std::make_tuple(pipeForInput[1], pipeForOutput[0], pipeForError[0], child);
}

pid_t ExecWithInOut(const std::vector<std::string>& command, int fd, const std::vector<int>& closeAfterFork) {
  pid_t child = fork();
  if (child == 0) {
    if (fd != STDIN_FILENO)
      dup2(fd, STDIN_FILENO);
    if (fd != STDOUT_FILENO)
      dup2(fd, STDOUT_FILENO);

    if (fd != STDOUT_FILENO && fd != STDIN_FILENO)
      close(fd);

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    closefrom(STDERR_FILENO + 1);
    USING(closeAfterFork);
#else
    for (int f : closeAfterFork) {
      // especially useful check if early on in the program stdin/stdout are closed, and
      // there are file descriptors opened (which are assigned fd 0 and 1).
      if (f > STDOUT_FILENO)
        close(f);
    }
#endif
    //std::cerr << command[0] << ": " << CheckFD(STDIN_FILENO) << " / " << CheckFD(STDOUT_FILENO) << " / " << CheckFD(STDERR_FILENO) << std::endl;;

    execvp(command);
    exit(-1);
  }
  return child;
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

bool IsOnWSL() {
  // detect interop: https://docs.microsoft.com/en-us/windows/wsl/interop
  return IsFile("/proc/sys/fs/binfmt_misc/WSLInterop");
}

std::optional<std::string> GetTMUXVariable(std::string variable, std::vector<int> closeAfterFork) {
  std::vector<std::string> getClipboardCommand = {"tmux", "show-environment", variable};
  auto [wfd, rfd, efd, pid] = ExecRedirected(getClipboardCommand, closeAfterFork);
  close(wfd);
  close(efd);
  bool eof = false;
  bool error = false;
  std::string retval = Read(rfd, eof, error, 1024*1024);
  error |= retval.size() == 1024*1024 && !eof;
  close(rfd);

  if (!error && eof) {
    // tmux outputs "-DISPLAY" if variable is not found, and "DISPLAY=foobar" if variable is found
    auto pos = retval.find("=");
    if (pos != retval.npos) {
      // do not copy trailing '\n' from tmux
      retval = retval.substr(pos+1, retval.size()-pos-2);
      return {retval};
    }
  }
  return {};
}

bool IsLocalSession(std::vector<int> closeAfterFork) {
  // if TMUX is active (TMUX env var) first query tmux (so the variables are copied from the environment driving starting tmx)
  if (getenv("TMUX")) {
    // if empty, then command is assumed to have failed
    auto display = GetTMUXVariable("DISPLAY", closeAfterFork);
    return display && display->size() > 0;
  }
  // detect if it is a X11 session by checking if DISPLAY is set
  auto display = getenv("DISPLAY");
  if (display && strlen(display) > 0)
    return true;
  // if (getenv("WAYLAND_DISPLAY"))
  //   return true;
  return false;
}

// from https://stackoverflow.com/questions/2896600/how-to-replace-all-occurrences-of-a-character-in-string
std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}

std::string GetClipboard(std::vector<int> closeAfterFork) {
  bool wsl [[maybe_unused]] = false;
#if defined(__APPLE__)
  std::vector<std::string> getClipboardCommand = {"pbpaste"};
  bool wsl = false;
#else
  std::vector<std::string> getClipboardCommand = {"xsel", "--clipboard", "--output"};
  wsl = IsOnWSL();
  if (wsl) {
    getClipboardCommand = {"powershell.exe", "Get-Clipboard"};
  }
#endif
  auto [wfd, rfd, efd, pid] = ExecRedirected(getClipboardCommand, closeAfterFork);
  close(wfd);
  close(efd);
  bool eof = false;
  bool error = false;
  std::string retval = Read(rfd, eof, error, 1024*1024);
  error |= retval.size() == 1024*1024 && !eof;
  close(rfd);
  if (wsl) {
    // `powershell.exe Get-Clipboard` appends \r\n to output, get rid of it
    retval = retval.size() >= 2 ? retval.substr(0, retval.size()-2) : std::string();
  }
  if (wsl) {
    retval = ReplaceAll(retval, "\r\n", "\n");
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
  auto [wfd, rfd, efd, pid] = ExecRedirected(setClipboardCommand, closeAfterFork);
  close(rfd);
  close(efd);
  auto bytes = SafeWrite(wfd, clipboard.c_str(), clipboard.size());
  close(wfd);
  return bytes == clipboard.size();
}

bool ShowNotification(std::string summary, std::string body, std::vector<int> closeAfterFork) {
#if defined(__APPLE__)
  std::stringstream ss;
  ss << "display notification ";
  ss << std::quoted(body);
  ss << " with title ";
  ss << std::quoted(summary);
  std::vector<std::string> command = {"osascript", "-e", ss.str()};
#else
  std::vector<std::string> command = {"notify-send", summary, body};
  if (IsOnWSL()) {
    // if this does not work, user should execute
    // Set-ExecutionPolicy -ExecutionPolicy Unrestricted -Scope CurrentUser
    std::stringstream ss;
    // two times creating a notify icon is needed apparantly, because otherwise the notification is not shown
    if (body.empty())
      body = "-";
    ss << "Add-Type -AssemblyName System.Windows.Forms; $balmsg = New-Object System.Windows.Forms.NotifyIcon; $balmsg = New-Object System.Windows.Forms.NotifyIcon; $path = (Get-Process -id $pid).Path; $balmsg.Icon = [System.Drawing.Icon]::ExtractAssociatedIcon($path); $balmsg.BalloonTipIcon = [System.Windows.Forms.ToolTipIcon]::Warning; $balmsg.BalloonTipText = ";
    ss << std::quoted(body);
    ss << "; $balmsg.BalloonTipTitle = ";
    ss << std::quoted(summary);
    ss << "; $balmsg.Visible = $true; $balmsg.ShowBalloonTip(10000)";
    command = {"powershell.exe", ss.str()};
  }
#endif
  auto [wfd, rfd, efd, pid] = ExecRedirected(command, closeAfterFork);
  close(wfd);
  close(rfd);
  close(efd);
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR);
  return status == 0;
}

bool OpenURL(std::string url, std::vector<int> closeAfterFork) {
#if defined(__APPLE__)
  std::vector<std::string> command = {"open", url};
#else
  std::vector<std::string> command = {"xdg-open", url};
  if (IsOnWSL()) {
    command = {"cmd.exe", "/C", "start", url};
  }
#endif
  auto [wfd, rfd, efd, pid] = ExecRedirected(command, closeAfterFork);
  close(wfd);
  close(rfd);
  close(efd);
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR);
  return status == 0;
}

void Closed(int signal) {
  std::cerr << "got SIGPIPE signal: " << signal << std::endl;
  //exit(-1);
}
// fds = <in, out>
std::map<int,std::string> Proxy(std::vector<ProxyState*> fds, std::function<bool(size_t)> good2) {
  signal(SIGPIPE, Closed);

  using namespace std::chrono_literals;
  std::chrono::milliseconds timeout = 100ms; // sample rate for exit of process
  // set non-blocking
  for (auto& state : fds) {
    int s = fcntl(state->in, F_GETFL, 0);
    ENSURE(fcntl(state->in, F_SETFL, s | O_NONBLOCK) == 0);
    s = fcntl(state->out, F_GETFL, 0);
    ENSURE(fcntl(state->out, F_SETFL, s | O_NONBLOCK) == 0);
  }
  // multiplex
  fd_set fdsetRead;
  fd_set fdsetWrite;
  std::map<int,std::string> buffers;
  while (fds.size() > 0 && good2(fds.size())) {
    int max = 0;
    FD_ZERO(&fdsetRead);
    FD_ZERO(&fdsetWrite);
    for (auto& state : fds) {
      auto& converted = buffers[state->out];
      if (converted.empty()) {
        OLD_STYLE_CAST(SIGN_CONVERSION(FD_SET(state->in, &fdsetRead)));
        max = std::max(max, state->in);
      } else {
        OLD_STYLE_CAST(SIGN_CONVERSION(FD_SET(state->out, &fdsetWrite)));
        max = std::max(max, state->out);
      }
    }
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::chrono::milliseconds remaining = std::chrono::duration_cast<std::chrono::milliseconds>(start - std::chrono::steady_clock::now()) + timeout;
    struct timeval to = {long(remaining.count()) / 1000, int(remaining.count()) % 1000};
    int nr = select(max + 1, &fdsetRead, &fdsetWrite, nullptr, &to);
    if (nr == 0) {
      // time limit reached
      continue;
    }
    for (auto it = fds.begin(); it != fds.end(); ) {
      auto& state = *it;
      auto& converted = buffers[state->out];
      if (converted.empty()) {
        if (OLD_STYLE_CAST(SIGN_CONVERSION(FD_ISSET(state->in, &fdsetRead)))) {
          char b[16384];
          auto [bytes, err] = SafeRead(state->in, b, sizeof(b));
          //std::cerr << "fd=" << state->in << " bytes=" << bytes << " and err=" << err << " (good=" << state->good << "; fds=" << fds.size() << ")" << std::endl;
          if (bytes == 0 && err != EAGAIN) {
            // EOF
            //std::cerr << "EOF in fd=" << state->in << std::endl;
            std::tie(converted,std::ignore) = state->convertInToOut(std::string());
            state->good = false;
          } else {
            state->inBuffer += std::string(b, bytes);
          }
        }
      } else {
        if (OLD_STYLE_CAST(SIGN_CONVERSION(FD_ISSET(state->out, &fdsetWrite)))) {
        }
      }

      while (state->inBuffer.size() > 0) {
        auto expectedLength = state->expectedLengthForConvert(state->inBuffer);
        //std::cerr << "got " << state->inBuffer.size() << " but expected " << expectedLength << std::endl;
        if (state->inBuffer.size() == 0 || state->inBuffer.size() < expectedLength)
          break;
        auto [c,ok] = state->convertInToOut(state->inBuffer.substr(0, expectedLength));
        converted += c;
        state->good = state->good && ok;
        state->inBuffer = state->inBuffer.substr(expectedLength);
        //std::cerr << "current state: good=" << state->good << " converted=" << state->converted.size() << std::endl;
      }

      if (converted.size() > 0) {
        auto bytes = SafeWrite(state->out, converted.data(), converted.size());
        if (bytes == 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
          // EOF
          //std::cerr << "EOF out fd=" << state->out << std::endl;
          //perror("EOF out");
          state->good = false;
        } else {
          if (bytes > 0)
            converted = converted.substr(bytes);
        }
      }

      if (converted.empty() && !state->good) {
        state->close();
        it = fds.erase(it);
      } else {
        ++it;
      }
    }
  }
  return buffers;
}

uint32_t ProtocolMessageLength(const std::string& buffer) {
  uint32_t n = 0;
  if (buffer.size() < sizeof(n)) {
    return std::numeric_limits<uint32_t>::max();
  }
  memcpy(&n, buffer.data(), sizeof(n));
  n = ntoh(n);
  return n + sizeof(n);
}
