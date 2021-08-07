#include <string>
#include <vector>
#include <tuple>

#define ENSURE(x) if (!(x)) { ::abort(); }
#define USING(x) ((void)(x))
enum ClippyCommand : uint32_t {
  NONE,
  RETRIEVE_CLIPBOARD,
  SET_CLIPBOARD,
  CLIPBOARD_CONTENTS
};

std::string Contents(int fd, bool& eof, size_t max);
// first int is fd to read from, second is a fd to write to, third is a process id
std::tuple<int, int, pid_t> ExecRedirected(const std::vector<std::string>& command, bool redirectError, const std::vector<int>& closeAfterFork);

bool IsFile(std::string file);
bool IsSocket(std::string file);

std::string GetClipboard(std::vector<int> closeAfterFork);
bool SetClipboard(std::string clipboard, std::vector<int> closeAfterFork);
