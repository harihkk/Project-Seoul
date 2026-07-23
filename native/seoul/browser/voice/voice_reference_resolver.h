// Project Seoul voice operating layer.
// Resolves spoken references to visible items ("this", "that", "the second
// result", "Saturday", "the red line") against stable component and data ids.
// Resolution is deterministic and uses ids, labels, kinds, and ordinals
// supplied by the Canvas; never screen coordinates.

#ifndef SEOUL_BROWSER_VOICE_VOICE_REFERENCE_RESOLVER_H_
#define SEOUL_BROWSER_VOICE_VOICE_REFERENCE_RESOLVER_H_

#include <string>
#include <vector>

#include "seoul/browser/voice/voice_types.h"

namespace seoul {

// One referable visible item, published by the Canvas with the surface. The
// id is the stable SAUI component id (or a data selection key), never a
// coordinate.
struct VisibleReferent {
  VisibleReferent();
  VisibleReferent(const VisibleReferent&);
  VisibleReferent(VisibleReferent&&);
  VisibleReferent& operator=(const VisibleReferent&);
  VisibleReferent& operator=(VisibleReferent&&);
  ~VisibleReferent();

  std::string id;
  std::string label;  // user-visible label ("Saturday", "Apple", "red line")
  std::string kind;   // singular noun class ("result", "chart", "step",
                      // "task", "card", "tab")
  int ordinal = 0;    // 1-based position within its kind, display order
  bool selected = false;
  bool focused = false;
};

// Resolves `phrase` to exactly one referent id. Failure is precise:
// kAmbiguousReference when several match equally, kUnknownReference when none
// match, kTooManyReferents/kTranscriptTooLong on bound violations.
VoiceResult<std::string> ResolveVoiceReference(
    const std::string& phrase,
    const std::vector<VisibleReferent>& referents);

}  // namespace seoul

#endif  // SEOUL_BROWSER_VOICE_VOICE_REFERENCE_RESOLVER_H_
