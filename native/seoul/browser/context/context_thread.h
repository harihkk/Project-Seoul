// Project Seoul context threads.
// A Context Thread holds only user-approved working context. By construction
// it can hold tabs (by reference), excerpts, files, notes, surfaces, task
// outputs, citations, workflow references, and decisions - and nothing from
// the forbidden classes (full history, all tabs, passwords, cookies, tokens,
// unrelated form contents, raw microphone audio). The item type has no field
// capable of carrying those, and AddItem rejects any item flagged sensitive.

#ifndef SEOUL_BROWSER_CONTEXT_CONTEXT_THREAD_H_
#define SEOUL_BROWSER_CONTEXT_CONTEXT_THREAD_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"

namespace seoul {

inline constexpr size_t kMaxContextItems = 200;
inline constexpr size_t kMaxContextTitleLength = 200;
inline constexpr size_t kMaxContextExcerptLength = 20000;
inline constexpr size_t kMaxContextNoteLength = 8000;

// The only item classes a thread may contain. There is deliberately no
// kPassword/kCookie/kToken/kRawAudio/kFullHistory member: forbidden content
// is not representable.
enum class ContextItemKind {
  kTabReference,       // a tab by stable key + title + origin (no page dump)
  kExcerpt,            // a user-selected passage
  kFileReference,      // a user-chosen file by path/handle
  kNote,               // a user note
  kSurfaceReference,   // a pinned SAUI surface id
  kTaskOutput,         // a task receipt/result reference
  kCitation,           // a source url + title
  kWorkflowReference,  // a saved workflow id
  kDecision,           // a recorded user decision
};

const char* ContextItemKindToString(ContextItemKind kind);

struct ContextItem {
  std::string id;  // generated
  ContextItemKind kind = ContextItemKind::kNote;
  std::string title;
  std::string reference;  // tab key / file handle / url / surface id / etc.
  std::string origin;     // display origin for tab/citation
  std::string text;       // excerpt or note body (bounded)
  base::Time added_at;
  // Set by the caller when the source is known to contain sensitive data
  // (a password field, an auth cookie, raw audio). Such items are rejected.
  bool flagged_sensitive = false;

  friend bool operator==(const ContextItem&, const ContextItem&) = default;
};

enum class ContextError {
  kInvalidItem,
  kSensitiveItemRejected,
  kUnknownItem,
  kLimitExceeded,
  kInvalidTitle,
  kExcerptTooLong,
};

const char* ContextErrorToString(ContextError error);

template <typename T>
using ContextResult = base::expected<T, ContextError>;

using ContextStatusResult = base::expected<void, ContextError>;

class ContextThread {
 public:
  ContextThread(std::string id, std::string name);
  ContextThread(const ContextThread&) = delete;
  ContextThread& operator=(const ContextThread&) = delete;
  ~ContextThread();

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  bool archived() const { return archived_; }
  const std::vector<ContextItem>& items() const { return items_; }

  ContextStatusResult SetName(const std::string& name);
  void Archive() { archived_ = true; }
  void Restore() { archived_ = false; }

  // Adds `item` after validating kind-specific requirements and rejecting
  // anything flagged sensitive. Returns the assigned id on success.
  ContextResult<std::string> AddItem(ContextItem item, base::Time now);
  ContextStatusResult RemoveItem(const std::string& item_id);
  void Clear() { items_.clear(); }

 private:
  std::string id_;
  std::string name_;
  bool archived_ = false;
  std::vector<ContextItem> items_;
};

// The scope actually sent to a cloud provider, minimized from a thread. It
// drops archived items, sensitive-flagged items (already impossible in a
// thread, re-checked here), and strips excerpt/note bodies when
// `include_bodies` is false, keeping references and citations. This is the
// only path by which thread content reaches a cloud model.
struct CloudContextScope {
  std::vector<ContextItem> items;
  size_t approximate_bytes = 0;
};

CloudContextScope MinimizeForCloud(const ContextThread& thread,
                                   bool include_bodies,
                                   size_t max_bytes);

}  // namespace seoul

#endif  // SEOUL_BROWSER_CONTEXT_CONTEXT_THREAD_H_
