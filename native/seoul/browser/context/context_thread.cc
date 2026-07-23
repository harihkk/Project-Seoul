// Project Seoul context threads.

#include "seoul/browser/context/context_thread.h"

#include <utility>
#include <vector>

namespace seoul {

ContextItem::ContextItem() = default;
ContextItem::ContextItem(const ContextItem&) = default;
ContextItem::ContextItem(ContextItem&&) = default;
ContextItem& ContextItem::operator=(const ContextItem&) = default;
ContextItem& ContextItem::operator=(ContextItem&&) = default;
ContextItem::~ContextItem() = default;

CloudContextScope::CloudContextScope() = default;
CloudContextScope::CloudContextScope(const CloudContextScope&) = default;
CloudContextScope::CloudContextScope(CloudContextScope&&) = default;
CloudContextScope& CloudContextScope::operator=(const CloudContextScope&) =
    default;
CloudContextScope& CloudContextScope::operator=(CloudContextScope&&) = default;
CloudContextScope::~CloudContextScope() = default;

namespace {

bool ValidName(const std::string& name) {
  return !name.empty() && name.size() <= kMaxContextTitleLength;
}

// Kind-specific well-formedness: references and citations need a reference;
// excerpts and notes need bounded text.
ContextStatusResult ValidateItem(const ContextItem& item) {
  if (item.title.size() > kMaxContextTitleLength) {
    return base::unexpected(ContextError::kInvalidTitle);
  }
  switch (item.kind) {
    case ContextItemKind::kTabReference:
    case ContextItemKind::kFileReference:
    case ContextItemKind::kSurfaceReference:
    case ContextItemKind::kTaskOutput:
    case ContextItemKind::kWorkflowReference:
    case ContextItemKind::kCitation:
      if (item.reference.empty()) {
        return base::unexpected(ContextError::kInvalidItem);
      }
      break;
    case ContextItemKind::kExcerpt:
      if (item.text.empty()) {
        return base::unexpected(ContextError::kInvalidItem);
      }
      if (item.text.size() > kMaxContextExcerptLength) {
        return base::unexpected(ContextError::kExcerptTooLong);
      }
      break;
    case ContextItemKind::kNote:
    case ContextItemKind::kDecision:
      if (item.text.empty() || item.text.size() > kMaxContextNoteLength) {
        return base::unexpected(ContextError::kInvalidItem);
      }
      break;
  }
  return base::ok();
}

}  // namespace

ContextThread::ContextThread(std::string id, std::string name)
    : id_(std::move(id)), name_(std::move(name)) {}

ContextThread::~ContextThread() = default;

ContextStatusResult ContextThread::SetName(const std::string& name) {
  if (!ValidName(name)) {
    return base::unexpected(ContextError::kInvalidTitle);
  }
  name_ = name;
  return base::ok();
}

ContextResult<std::string> ContextThread::AddItem(ContextItem item,
                                                  base::Time now) {
  // The core guarantee: anything the caller could not vouch for as
  // non-sensitive is refused. Passwords, cookies, tokens, raw audio, and
  // unrelated form contents arrive here only if a caller mislabels them, and
  // even then the flag stops them.
  if (item.flagged_sensitive) {
    return base::unexpected(ContextError::kSensitiveItemRejected);
  }
  if (auto valid = ValidateItem(item); !valid.has_value()) {
    return base::unexpected(valid.error());
  }
  if (items_.size() >= kMaxContextItems) {
    return base::unexpected(ContextError::kLimitExceeded);
  }
  item.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  item.added_at = now;
  const std::string id = item.id;
  items_.push_back(std::move(item));
  return id;
}

ContextStatusResult ContextThread::RemoveItem(const std::string& item_id) {
  const size_t before = items_.size();
  std::erase_if(items_, [&item_id](const ContextItem& item) {
    return item.id == item_id;
  });
  if (items_.size() == before) {
    return base::unexpected(ContextError::kUnknownItem);
  }
  return base::ok();
}

CloudContextScope MinimizeForCloud(const ContextThread& thread,
                                   bool include_bodies,
                                   size_t max_bytes) {
  CloudContextScope scope;
  if (thread.archived()) {
    return scope;  // archived threads are never sent
  }
  for (const ContextItem& item : thread.items()) {
    if (item.flagged_sensitive) {
      continue;  // defense in depth; should be impossible in a thread
    }
    ContextItem minimized = item;
    if (!include_bodies) {
      // Strip excerpt/note bodies; keep the reference so the model can ask
      // for a specific source rather than receiving everything.
      if (minimized.kind == ContextItemKind::kExcerpt ||
          minimized.kind == ContextItemKind::kNote ||
          minimized.kind == ContextItemKind::kDecision) {
        minimized.text.clear();
      }
    }
    const size_t item_bytes = minimized.title.size() +
                              minimized.reference.size() +
                              minimized.origin.size() + minimized.text.size();
    if (scope.approximate_bytes + item_bytes > max_bytes) {
      break;  // budget reached; the rest is dropped rather than truncated
    }
    scope.approximate_bytes += item_bytes;
    scope.items.push_back(std::move(minimized));
  }
  return scope;
}

const char* ContextItemKindToString(ContextItemKind kind) {
  switch (kind) {
    case ContextItemKind::kTabReference:
      return "tab_reference";
    case ContextItemKind::kExcerpt:
      return "excerpt";
    case ContextItemKind::kFileReference:
      return "file_reference";
    case ContextItemKind::kNote:
      return "note";
    case ContextItemKind::kSurfaceReference:
      return "surface_reference";
    case ContextItemKind::kTaskOutput:
      return "task_output";
    case ContextItemKind::kCitation:
      return "citation";
    case ContextItemKind::kWorkflowReference:
      return "workflow_reference";
    case ContextItemKind::kDecision:
      return "decision";
  }
  return "note";
}

const char* ContextErrorToString(ContextError error) {
  switch (error) {
    case ContextError::kInvalidItem:
      return "invalid_item";
    case ContextError::kSensitiveItemRejected:
      return "sensitive_item_rejected";
    case ContextError::kUnknownItem:
      return "unknown_item";
    case ContextError::kLimitExceeded:
      return "limit_exceeded";
    case ContextError::kInvalidTitle:
      return "invalid_title";
    case ContextError::kExcerptTooLong:
      return "excerpt_too_long";
  }
  return "invalid_item";
}

}  // namespace seoul
