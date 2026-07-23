// Project Seoul connected tools.
// Local file access as typed capabilities over EXPLICITLY user-selected
// files. A selection produces an opaque token; capabilities operate on tokens
// only, so no path (and no file outside the selection) is ever reachable by a
// planner. Actual file I/O lives behind a read seam in the platform layer.

#ifndef SEOUL_BROWSER_CONNECTORS_LOCAL_FILE_CAPABILITY_H_
#define SEOUL_BROWSER_CONNECTORS_LOCAL_FILE_CAPABILITY_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "seoul/browser/tools/tool_types.h"

namespace seoul {

inline constexpr size_t kMaxSelectedFiles = 64;

struct SelectedFile {
  SelectedFile();
  SelectedFile(const SelectedFile&);
  SelectedFile(SelectedFile&&);
  SelectedFile& operator=(const SelectedFile&);
  SelectedFile& operator=(SelectedFile&&);
  ~SelectedFile();

  std::string token;         // opaque, generated at selection time
  std::string display_name;  // shown to the user; carries no directory path
  std::string mime_type;
  int64_t size_bytes = 0;

  friend bool operator==(const SelectedFile&, const SelectedFile&) = default;
};

enum class FileSelectionError {
  kUnknownToken,
  kSelectionFull,
  kInvalidName,
};

const char* FileSelectionErrorToString(FileSelectionError error);

// The user-approved file selection for one profile. Selection happens only
// through the platform file chooser (never a model); revocation removes the
// token immediately.
class SelectedFileRegistry {
 public:
  SelectedFileRegistry();
  SelectedFileRegistry(const SelectedFileRegistry&) = delete;
  SelectedFileRegistry& operator=(const SelectedFileRegistry&) = delete;
  ~SelectedFileRegistry();

  // Registers a user-chosen file and returns its opaque token.
  base::expected<std::string, FileSelectionError> Select(
      const std::string& display_name,
      const std::string& mime_type,
      int64_t size_bytes);
  bool Revoke(const std::string& token);
  void Clear();

  const SelectedFile* Find(const std::string& token) const;
  std::vector<SelectedFile> List() const;
  size_t size() const { return files_.size(); }

 private:
  std::map<std::string, SelectedFile> files_;
};

// Capability descriptors for the selection: files.selection.list (read-only,
// no arguments) and files.selection.read (token argument validated against
// the registry before any I/O). Provider "seoul"; the files.* namespace is
// Seoul-owned.
std::vector<ToolDescriptor> BuildLocalFileCapabilities();

// Validates a files.selection.read argument set against the registry.
base::expected<const SelectedFile*, FileSelectionError>
ResolveReadRequest(const SelectedFileRegistry& registry,
                   const std::string& token);

}  // namespace seoul

#endif  // SEOUL_BROWSER_CONNECTORS_LOCAL_FILE_CAPABILITY_H_
