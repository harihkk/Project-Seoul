// Project Seoul native lifecycle bridge.
// Identity types that keep LIVE (runtime) and PERSISTED (restorable) references
// separate. All four are derived from a Chromium SessionID value (an int), but
// are distinct types so a window ref can never be used as a tab ref.
//
// Verified at pinned M149 (6a7b3dbec3b2ca25877c2553b5473b2f277ef644):
//  - BrowserWindowInterface::GetSessionID() identifies a browser window.
//  - sessions::SessionTabHelper::IdForTab(WebContents*) gives a tab SessionID.
//  - TabStripModelChange::RemovedTab carries std::optional<SessionID>.
//
// NOT guaranteed across every boundary Seoul cares about (honest limits):
//  - WebContents discard/replacement: SessionID is preserved for the logical
//    tab, but WebContents identity changes; Seoul keys off SessionID only.
//  - Tab movement between windows: SessionID is preserved; membership follows
//    the pending-transfer handshake rather than a new membership.
//  - Normal restart / crash restoration: window and tab SessionIDs are restored
//    from session storage when Chromium restores the session; until a valid ID
//    appears, Seoul leaves the reference unresolved (no fabrication).
//  - Recently closed restoration: may assign a new SessionID; Seoul treats it
//    as a genuinely new tab unless an explicit future reconciliation rule
//    matches persisted metadata without reopening URLs.

#ifndef SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_IDENTITY_H_
#define SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_IDENTITY_H_

#include <compare>
#include <string>
#include <string_view>

#include "base/strings/string_number_conversions.h"

namespace seoul {

enum class MutationOrigin {
  kChromiumObservation,
  kStartupReconciliation,
  kUserOrganizationCommand,
  kSystemRecovery,
};

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

SEOUL_SESSION_ID(LiveWindowKey, "w-");
SEOUL_SESSION_ID(PersistedWindowRef, "wref-");
SEOUL_SESSION_ID(LiveTabKey, "t-");
SEOUL_SESSION_ID(PersistedTabRef, "tref-");

#undef SEOUL_SESSION_ID

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_IDENTITY_H_
