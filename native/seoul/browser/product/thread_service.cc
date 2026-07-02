// Project Seoul product runtime: the Context Thread service.

#include "seoul/browser/product/thread_service.h"

#include <utility>

#include "base/strings/string_number_conversions.h"

namespace seoul {

namespace {

const char* ItemKindKey(ContextItemKind kind) {
  return ContextItemKindToString(kind);
}

std::optional<ContextItemKind> ItemKindFromKey(const std::string& key) {
  static constexpr ContextItemKind kAll[] = {
      ContextItemKind::kTabReference,     ContextItemKind::kExcerpt,
      ContextItemKind::kFileReference,    ContextItemKind::kNote,
      ContextItemKind::kSurfaceReference, ContextItemKind::kTaskOutput,
      ContextItemKind::kCitation,         ContextItemKind::kWorkflowReference,
      ContextItemKind::kDecision,
  };
  for (const ContextItemKind kind : kAll) {
    if (key == ContextItemKindToString(kind)) {
      return kind;
    }
  }
  return std::nullopt;
}

}  // namespace

ThreadService::ThreadService(base::RepeatingCallback<base::Time()> clock)
    : clock_(std::move(clock)) {}

ThreadService::~ThreadService() = default;

std::string ThreadService::CreateThread(const std::string& name) {
  if (threads_.size() >= kMaxThreads || name.empty() ||
      name.size() > kMaxContextTitleLength) {
    return std::string();
  }
  const std::string id = "thread-" + base::NumberToString(next_id_++);
  threads_[id] = std::make_unique<ContextThread>(id, name);
  return id;
}

bool ThreadService::RenameThread(const std::string& thread_id,
                                 const std::string& name) {
  auto it = threads_.find(thread_id);
  return it != threads_.end() && it->second->SetName(name).has_value();
}

bool ThreadService::ArchiveThread(const std::string& thread_id) {
  auto it = threads_.find(thread_id);
  if (it == threads_.end()) {
    return false;
  }
  it->second->Archive();
  return true;
}

bool ThreadService::ReopenThread(const std::string& thread_id) {
  auto it = threads_.find(thread_id);
  if (it == threads_.end()) {
    return false;
  }
  it->second->Restore();
  return true;
}

bool ThreadService::DeleteThread(const std::string& thread_id) {
  return threads_.erase(thread_id) > 0;
}

ContextResult<std::string> ThreadService::AttachItem(
    const std::string& thread_id,
    ContextItem item) {
  auto it = threads_.find(thread_id);
  if (it == threads_.end()) {
    return base::unexpected(ContextError::kUnknownItem);
  }
  return it->second->AddItem(std::move(item), clock_.Run());
}

bool ThreadService::DetachItem(const std::string& thread_id,
                               const std::string& item_id) {
  auto it = threads_.find(thread_id);
  return it != threads_.end() && it->second->RemoveItem(item_id).has_value();
}

const ContextThread* ThreadService::FindThread(
    const std::string& thread_id) const {
  auto it = threads_.find(thread_id);
  return it != threads_.end() ? it->second.get() : nullptr;
}

std::vector<ThreadSummary> ThreadService::Summaries() const {
  std::vector<ThreadSummary> out;
  out.reserve(threads_.size());
  for (const auto& [id, thread] : threads_) {
    ThreadSummary summary;
    summary.id = id;
    summary.name = thread->name();
    summary.archived = thread->archived();
    summary.item_count = thread->items().size();
    out.push_back(std::move(summary));
  }
  return out;
}

base::Value::Dict ThreadService::TakePersistedState() const {
  base::Value::Dict state;
  state.Set("next_id", static_cast<double>(next_id_));
  base::Value::List threads;
  for (const auto& [id, thread] : threads_) {
    base::Value::Dict entry;
    entry.Set("id", id);
    entry.Set("name", thread->name());
    entry.Set("archived", thread->archived());
    base::Value::List items;
    for (const ContextItem& item : thread->items()) {
      base::Value::Dict item_value;
      item_value.Set("id", item.id);
      item_value.Set("kind", ItemKindKey(item.kind));
      item_value.Set("title", item.title);
      item_value.Set("reference", item.reference);
      item_value.Set("origin", item.origin);
      item_value.Set("text", item.text);
      items.Append(std::move(item_value));
    }
    entry.Set("items", std::move(items));
    threads.Append(std::move(entry));
  }
  state.Set("threads", std::move(threads));
  return state;
}

void ThreadService::RestorePersistedState(const base::Value::Dict& state) {
  next_id_ = static_cast<uint64_t>(state.FindDouble("next_id").value_or(1.0));
  const base::Value::List* threads = state.FindList("threads");
  if (!threads) {
    return;
  }
  for (const base::Value& entry : *threads) {
    const base::Value::Dict* dict = entry.GetIfDict();
    if (!dict) {
      continue;
    }
    const std::string* id = dict->FindString("id");
    const std::string* name = dict->FindString("name");
    if (!id || !name || id->empty() || name->empty() ||
        threads_.size() >= kMaxThreads) {
      continue;
    }
    auto thread = std::make_unique<ContextThread>(*id, *name);
    if (dict->FindBool("archived").value_or(false)) {
      thread->Archive();
    }
    if (const base::Value::List* items = dict->FindList("items")) {
      for (const base::Value& item_entry : *items) {
        const base::Value::Dict* item_dict = item_entry.GetIfDict();
        if (!item_dict) {
          continue;
        }
        const std::string* kind_key = item_dict->FindString("kind");
        const std::optional<ContextItemKind> kind =
            kind_key ? ItemKindFromKey(*kind_key) : std::nullopt;
        if (!kind.has_value()) {
          continue;  // unknown kinds are dropped, never guessed at
        }
        ContextItem item;
        item.kind = kind.value();
        if (const std::string* title = item_dict->FindString("title")) {
          item.title = *title;
        }
        if (const std::string* reference = item_dict->FindString("reference")) {
          item.reference = *reference;
        }
        if (const std::string* origin = item_dict->FindString("origin")) {
          item.origin = *origin;
        }
        if (const std::string* text = item_dict->FindString("text")) {
          item.text = *text;
        }
        thread->AddItem(std::move(item), clock_.Run());
      }
    }
    threads_[*id] = std::move(thread);
  }
}

}  // namespace seoul
