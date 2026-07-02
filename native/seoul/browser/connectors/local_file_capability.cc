// Project Seoul connected tools.

#include "seoul/browser/connectors/local_file_capability.h"

#include <utility>

#include "base/uuid.h"

namespace seoul {

SelectedFileRegistry::SelectedFileRegistry() = default;
SelectedFileRegistry::~SelectedFileRegistry() = default;

base::expected<std::string, FileSelectionError> SelectedFileRegistry::Select(
    const std::string& display_name,
    const std::string& mime_type,
    int64_t size_bytes) {
  if (display_name.empty() || display_name.size() > 255 || size_bytes < 0) {
    return base::unexpected(FileSelectionError::kInvalidName);
  }
  if (files_.size() >= kMaxSelectedFiles) {
    return base::unexpected(FileSelectionError::kSelectionFull);
  }
  SelectedFile file;
  file.token = base::Uuid::GenerateRandomV4().AsLowercaseString();
  file.display_name = display_name;
  file.mime_type = mime_type;
  file.size_bytes = size_bytes;
  const std::string token = file.token;
  files_.emplace(token, std::move(file));
  return token;
}

bool SelectedFileRegistry::Revoke(const std::string& token) {
  return files_.erase(token) > 0;
}

void SelectedFileRegistry::Clear() {
  files_.clear();
}

const SelectedFile* SelectedFileRegistry::Find(
    const std::string& token) const {
  auto it = files_.find(token);
  return it == files_.end() ? nullptr : &it->second;
}

std::vector<SelectedFile> SelectedFileRegistry::List() const {
  std::vector<SelectedFile> files;
  for (const auto& [token, file] : files_) {
    files.push_back(file);
  }
  return files;
}

std::vector<ToolDescriptor> BuildLocalFileCapabilities() {
  std::vector<ToolDescriptor> capabilities;

  ToolDescriptor list;
  list.id = ToolId::FromString("files.selection.list");
  list.name = "List selected files";
  list.description =
      "Lists the files the user has explicitly selected for this task.";
  list.provider = "seoul";
  list.risk = RiskCategory::kReadOnly;
  list.sensitivity = DataSensitivity::kPersonal;
  list.idempotency = IdempotencyClass::kIdempotent;
  list.observation_contract = "selected-file listing";
  capabilities.push_back(std::move(list));

  ToolDescriptor read;
  read.id = ToolId::FromString("files.selection.read");
  read.name = "Read a selected file";
  read.description =
      "Reads the contents of one explicitly selected file by its token.";
  read.provider = "seoul";
  read.risk = RiskCategory::kReadOnly;
  read.sensitivity = DataSensitivity::kPersonal;
  read.idempotency = IdempotencyClass::kIdempotent;
  read.observation_contract = "bounded file contents";
  SchemaField token;
  token.name = "token";
  token.kind = SchemaFieldKind::kString;
  token.required = true;
  token.description = "Opaque selection token from files.selection.list.";
  read.input_schema.fields.push_back(std::move(token));
  capabilities.push_back(std::move(read));

  return capabilities;
}

base::expected<const SelectedFile*, FileSelectionError> ResolveReadRequest(
    const SelectedFileRegistry& registry,
    const std::string& token) {
  const SelectedFile* file = registry.Find(token);
  if (!file) {
    return base::unexpected(FileSelectionError::kUnknownToken);
  }
  return file;
}

const char* FileSelectionErrorToString(FileSelectionError error) {
  switch (error) {
    case FileSelectionError::kUnknownToken:
      return "unknown_token";
    case FileSelectionError::kSelectionFull:
      return "selection_full";
    case FileSelectionError::kInvalidName:
      return "invalid_name";
  }
  return "unknown_token";
}

}  // namespace seoul
