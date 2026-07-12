// Project Seoul Preview lifecycle manager.
// Pure state owner for ephemeral previews. WebContents and Views ownership live
// in the Chromium host; this class makes implicit promotion unrepresentable.

#ifndef SEOUL_BROWSER_PREVIEW_PREVIEW_MANAGER_H_
#define SEOUL_BROWSER_PREVIEW_PREVIEW_MANAGER_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "seoul/browser/preview/preview_types.h"

namespace seoul {

class PreviewManager {
 public:
  using Clock = base::RepeatingCallback<base::Time()>;
  using IdGenerator = base::RepeatingCallback<PreviewId()>;

  PreviewManager(Clock clock, IdGenerator id_generator);
  PreviewManager(const PreviewManager&) = delete;
  PreviewManager& operator=(const PreviewManager&) = delete;
  ~PreviewManager();

  PreviewResult<PreviewOpenResult> Open(LiveWindowKey window,
                                        LiveTabKey parent_tab,
                                        const GURL& url);
  // A navigation start immediately makes promotion unavailable. The host
  // records the committed URL separately through DidNavigate().
  PreviewStatusResult MarkLoading(const PreviewId& id);
  PreviewStatusResult DidNavigate(const PreviewId& id, const GURL& url);
  PreviewStatusResult MarkReady(const PreviewId& id);
  PreviewStatusResult MarkFailed(const PreviewId& id);

  // Promotion is two-phase. The Chromium host calls BeginPromotion before it
  // transfers ownership of the WebContents. It commits only after insertion
  // succeeds, or aborts back to ready on failure.
  PreviewStatusResult BeginPromotion(const PreviewId& id,
                                     PreviewPromotionTarget target);
  PreviewResult<PreviewRecord> CommitPromotion(const PreviewId& id);
  PreviewStatusResult AbortPromotion(const PreviewId& id);

  PreviewResult<PreviewRecord> Dismiss(const PreviewId& id,
                                       PreviewDismissReason reason);
  size_t DismissForParent(LiveTabKey parent_tab);
  size_t DismissForWindow(LiveWindowKey window);

  const PreviewRecord* Find(const PreviewId& id) const;
  const PreviewRecord* FindForWindow(LiveWindowKey window) const;
  std::vector<const PreviewRecord*> List() const;
  size_t size() const { return previews_.size(); }

 private:
  bool IsSafePreviewUrl(const GURL& url) const;
  PreviewRecord* FindMutable(const PreviewId& id);
  PreviewRecord Remove(const PreviewId& id);

  Clock clock_;
  IdGenerator id_generator_;
  std::map<PreviewId, PreviewRecord> previews_;
  std::map<LiveWindowKey, PreviewId> preview_by_window_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PREVIEW_PREVIEW_MANAGER_H_
