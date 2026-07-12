// Project Seoul Preview lifecycle manager.

#include "seoul/browser/preview/preview_manager.h"

#include <utility>

#include "base/check.h"

namespace seoul {

PreviewManager::PreviewManager(Clock clock, IdGenerator id_generator)
    : clock_(std::move(clock)), id_generator_(std::move(id_generator)) {}

PreviewManager::~PreviewManager() = default;

PreviewResult<PreviewOpenResult> PreviewManager::Open(
    LiveWindowKey window,
    LiveTabKey parent_tab,
    const GURL& url) {
  if (!window.is_valid() || !parent_tab.is_valid()) {
    return base::unexpected(PreviewError::kInvalidParent);
  }
  if (!IsSafePreviewUrl(url)) {
    return base::unexpected(PreviewError::kUnsafeUrl);
  }

  PreviewRecord record;
  record.id = id_generator_.Run();
  if (!record.id.is_valid() || previews_.contains(record.id)) {
    return base::unexpected(PreviewError::kInvalidId);
  }

  PreviewOpenResult result;
  if (const PreviewRecord* existing = FindForWindow(window)) {
    if (existing->state == PreviewState::kPromoting) {
      return base::unexpected(PreviewError::kInvalidState);
    }
    result.replaced = existing->id;
    Remove(existing->id);
  } else if (previews_.size() >= kMaxConcurrentPreviews) {
    return base::unexpected(PreviewError::kLimitExceeded);
  }

  record.window = window;
  record.parent_tab = parent_tab;
  record.initial_url = url;
  record.current_url = url;
  record.created_at = clock_.Run();
  record.navigation_count = 1;
  result.id = record.id;
  preview_by_window_.emplace(window, record.id);
  previews_.emplace(record.id, std::move(record));
  return result;
}

PreviewStatusResult PreviewManager::MarkLoading(const PreviewId& id) {
  PreviewRecord* record = FindMutable(id);
  if (!record) {
    return base::unexpected(PreviewError::kUnknownPreview);
  }
  if (record->state == PreviewState::kPromoting) {
    return base::unexpected(PreviewError::kInvalidState);
  }
  record->state = PreviewState::kLoading;
  return base::ok();
}

PreviewStatusResult PreviewManager::DidNavigate(const PreviewId& id,
                                                const GURL& url) {
  PreviewRecord* record = FindMutable(id);
  if (!record) {
    return base::unexpected(PreviewError::kUnknownPreview);
  }
  if (record->state == PreviewState::kPromoting) {
    return base::unexpected(PreviewError::kInvalidState);
  }
  if (!IsSafePreviewUrl(url)) {
    return base::unexpected(PreviewError::kUnsafeUrl);
  }
  if (record->navigation_count >= kMaxPreviewNavigations) {
    return base::unexpected(PreviewError::kNavigationLimitExceeded);
  }
  record->current_url = url;
  ++record->navigation_count;
  record->state = PreviewState::kLoading;
  return base::ok();
}

PreviewStatusResult PreviewManager::MarkReady(const PreviewId& id) {
  PreviewRecord* record = FindMutable(id);
  if (!record) {
    return base::unexpected(PreviewError::kUnknownPreview);
  }
  if (record->state != PreviewState::kLoading) {
    return base::unexpected(PreviewError::kInvalidState);
  }
  record->state = PreviewState::kReady;
  return base::ok();
}

PreviewStatusResult PreviewManager::MarkFailed(const PreviewId& id) {
  PreviewRecord* record = FindMutable(id);
  if (!record) {
    return base::unexpected(PreviewError::kUnknownPreview);
  }
  if (record->state == PreviewState::kPromoting) {
    return base::unexpected(PreviewError::kInvalidState);
  }
  record->state = PreviewState::kFailed;
  return base::ok();
}

PreviewStatusResult PreviewManager::BeginPromotion(
    const PreviewId& id,
    PreviewPromotionTarget target) {
  PreviewRecord* record = FindMutable(id);
  if (!record) {
    return base::unexpected(PreviewError::kUnknownPreview);
  }
  if (record->state != PreviewState::kReady) {
    return base::unexpected(PreviewError::kInvalidState);
  }
  record->state = PreviewState::kPromoting;
  record->promotion_target = target;
  return base::ok();
}

PreviewResult<PreviewRecord> PreviewManager::CommitPromotion(
    const PreviewId& id) {
  const PreviewRecord* record = Find(id);
  if (!record) {
    return base::unexpected(PreviewError::kUnknownPreview);
  }
  if (record->state != PreviewState::kPromoting ||
      !record->promotion_target.has_value()) {
    return base::unexpected(PreviewError::kInvalidState);
  }
  return Remove(id);
}

PreviewStatusResult PreviewManager::AbortPromotion(const PreviewId& id) {
  PreviewRecord* record = FindMutable(id);
  if (!record) {
    return base::unexpected(PreviewError::kUnknownPreview);
  }
  if (record->state != PreviewState::kPromoting) {
    return base::unexpected(PreviewError::kInvalidState);
  }
  record->state = PreviewState::kReady;
  record->promotion_target.reset();
  return base::ok();
}

PreviewResult<PreviewRecord> PreviewManager::Dismiss(
    const PreviewId& id,
    PreviewDismissReason /*reason*/) {
  const PreviewRecord* record = Find(id);
  if (!record) {
    return base::unexpected(PreviewError::kUnknownPreview);
  }
  if (record->state == PreviewState::kPromoting) {
    return base::unexpected(PreviewError::kInvalidState);
  }
  return Remove(id);
}

size_t PreviewManager::DismissForParent(LiveTabKey parent_tab) {
  std::vector<PreviewId> matches;
  for (const auto& [id, record] : previews_) {
    if (record.parent_tab == parent_tab) {
      matches.push_back(id);
    }
  }
  for (const PreviewId& id : matches) {
    Remove(id);
  }
  return matches.size();
}

size_t PreviewManager::DismissForWindow(LiveWindowKey window) {
  const PreviewRecord* record = FindForWindow(window);
  if (!record) {
    return 0;
  }
  Remove(record->id);
  return 1;
}

const PreviewRecord* PreviewManager::Find(const PreviewId& id) const {
  auto it = previews_.find(id);
  return it == previews_.end() ? nullptr : &it->second;
}

const PreviewRecord* PreviewManager::FindForWindow(
    LiveWindowKey window) const {
  auto it = preview_by_window_.find(window);
  return it == preview_by_window_.end() ? nullptr : Find(it->second);
}

std::vector<const PreviewRecord*> PreviewManager::List() const {
  std::vector<const PreviewRecord*> result;
  result.reserve(previews_.size());
  for (const auto& [id, record] : previews_) {
    result.push_back(&record);
  }
  return result;
}

bool PreviewManager::IsSafePreviewUrl(const GURL& url) const {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS() && url.has_host() &&
         !url.has_username() && !url.has_password();
}

PreviewRecord* PreviewManager::FindMutable(const PreviewId& id) {
  auto it = previews_.find(id);
  return it == previews_.end() ? nullptr : &it->second;
}

PreviewRecord PreviewManager::Remove(const PreviewId& id) {
  auto it = previews_.find(id);
  CHECK(it != previews_.end());
  PreviewRecord removed = std::move(it->second);
  preview_by_window_.erase(removed.window);
  previews_.erase(it);
  return removed;
}

}  // namespace seoul
