// Project Seoul native lifecycle bridge.
// Identity types that keep LIVE (runtime) and PERSISTED (restorable) references
// separate, and that never embed a raw pointer, index, process id, routing id,
// or renderer id. All four are derived from a Chromium SessionID value (an
// int), but are distinct types so a window ref can never be used as a tab ref
// and a live key can never be persisted where a persisted ref is required.
//
// Chromium guarantees verified in the pinned source:
//  - Browser::session_id() (SessionID) identifies a window and is restorable.
//  - sessions::SessionTabHelper::IdForTab(WebContents*) gives a tab SessionID.
//  - TabStripModelChange::RemovedTab carries std::optional<SessionID>.
// RESEARCH REQUIRED (resolve at the build host): whether a tab's SessionID is
//  preserved across WebContents discard/replacement and across full session
//  restore at every point Seoul needs it. Where it is not guaranteed, the
//  coordinator keeps the reference unresolved rather than fabricating identity.

#ifndef SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_IDENTITY_H_
#define SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_IDENTITY_H_

#include <compare>
#include <string>
#include <string_view>

#include "base/strings/string_number_conversions.h"

namespace seoul {

// Why a Seoul-model mutation happened. Exposed during observer notification so
// the future outbound command layer never reacts to its own changes as new user
// events. The smallest mechanism that prevents feedback loops.
enum class MutationOrigin {
  kChromiumObservation,      // inbound: a real Chromium event
  kStartupReconciliation,    // inbound: reconciling against restored state
  kUserOrganizationCommand,  // future outbound: an explicit Seoul command
  kSystemRecovery,           // recovery/repair
};

// A SessionID-backed strong id. `prefix` namespaces the string form so a window
// key string can never be mistaken for a tab key string.
#define SEOUL_SESSION_ID(TypeName, PREFIX)                                   \
  class TypeName {                                                           \
   public:                                                                   \
    TypeName() = default;                                                    \
    static TypeName FromSessionId(int session_id) {                          \
      TypeName k;                                                            \
      k.value_ = session_id;                                                 \
      return k;                                                              \
    }                                                                        \
    static TypeName Parse(std::string_view s) {                              \
      TypeName k;                                                            \
      std::string_view p = PREFIX;                                           \
      if (s.size() > p.size() && s.substr(0, p.size()) == p) {               \
        int v = 0;                                                           \
        if (base::StringToInt(s.substr(p.size()), &v) && v != 0) {           \
          k.value_ = v;                                                      \
        }                                                                    \
      }                                                                      \
      return k;                                                              \
    }                                                                        \
    bool is_valid() const {                                                  \
      return value_ != 0;                                                    \
    }                                                                        \
    int session_id() const {                                                 \
      return value_;                                                         \
    }                                                                        \
    std::string value() const {                                              \
      return is_valid() ? std::string(PREFIX) + base::NumberToString(value_) \
                        : std::string();                                     \
    }                                                                        \
    friend bool operator==(const TypeName& a, const TypeName& b) {           \
      return a.value_ == b.value_;                                           \
    }                                                                        \
    friend bool operator<(const TypeName& a, const TypeName& b) {            \
      return a.value_ < b.value_;                                            \
    }                                                                        \
                                                                             \
   private:                                                                  \
    int value_ = 0;                                                          \
  }

// Live (runtime) keys and persisted (restorable) refs are distinct TYPES even
// though both are SessionID-backed. The engine stores the string form of the
// live key as its opaque tab_key/window_key; the persisted ref is the
// reconciliation handle written/read across restart.
SEOUL_SESSION_ID(LiveWindowKey, "w-");
SEOUL_SESSION_ID(PersistedWindowRef, "wref-");
SEOUL_SESSION_ID(LiveTabKey, "t-");
SEOUL_SESSION_ID(PersistedTabRef, "tref-");

#undef SEOUL_SESSION_ID

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_IDENTITY_H_
