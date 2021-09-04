#include <string>
#include <vector>
#include <tuple>
#include <optional>
#include <functional>
#include <map>

#include <string.h>

#include "lib-common.h"

enum ClippyCommand : uint8_t {
  NONE,
  RETRIEVE_CLIPBOARD,
  SET_CLIPBOARD,
  CLIPBOARD_CONTENTS,
  PING,
  PONG,
  SHOW_NOTIFICATION,
  OPEN_URL,
  LOCAL_CMD,
  LOCAL_CMD_STDIN,
  LOCAL_CMD_STDIN_CLOSE,
  LOCAL_CMD_STDOUT,
  LOCAL_CMD_STDOUT_CLOSE,
  LOCAL_CMD_STDERR,
  LOCAL_CMD_STDERR_CLOSE,
  LOCAL_CMD_STATUS,
};

int execvp(const std::vector<std::string>& args);

std::string Read(int fd, bool& eof, bool& error, size_t max);
// first int is fd that is the stdin of the process, second is a fd that is the stdout of the process, third is stderr of process, fourth a process id
std::tuple<int, int, int, pid_t> ExecRedirected(const std::vector<std::string>& command, const std::vector<int>& closeAfterFork);

bool IsFile(std::string file);
bool IsSocket(std::string file);

std::optional<std::string> GetTMUXVariable(std::string variable, std::vector<int> closeAfterFork);
bool IsLocalSession(std::vector<int> closeAfterFork);

std::string GetUsername();

std::string GetClipboard(std::vector<int> closeAfterFork);
bool SetClipboard(std::string clipboard, std::vector<int> closeAfterFork);
bool ShowNotification(std::string summary, std::string body, std::vector<int> closeAfterFork);
bool OpenURL(std::string url, std::vector<int> closeAfterFork);

using ExpectedLength = std::function<uint32_t(const std::string&)>;
// bool indicates if this channel is still good, and should continue or be closed after output is sent
using Convert = std::function<std::tuple<std::string,bool>(std::string)>;
using ProxyClose = std::function<void()>;
struct ProxyState {
  int in;
  int out;
  ExpectedLength expectedLengthForConvert;
  Convert convertInToOut; // will be called with empty input when EOF of `in` is encountered
  ProxyClose close = []{};
  std::string inBuffer = {}; // not yet converted
  bool good = true;
};
std::map<int,std::string> Proxy(std::vector<ProxyState*> fds, std::function<bool(size_t count)> good2);
uint32_t ProtocolMessageLength(const std::string& buffer);
