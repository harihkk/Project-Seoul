// Project Seoul hybrid intelligence.
// Secret credential seam. Concrete implementation is the macOS Keychain
// (ObjC++ glue, a capable-host target). Secrets are addressed by an opaque
// account key; they are never returned to the Canvas, never logged, and never
// written to product state or the repository. The cloud provider fetches a key
// through this seam immediately before a request and never retains it.

#ifndef SEOUL_BROWSER_INTELLIGENCE_CREDENTIAL_STORE_H_
#define SEOUL_BROWSER_INTELLIGENCE_CREDENTIAL_STORE_H_

#include <optional>
#include <string>

namespace seoul {

// STATE OWNERSHIP: secret platform-managed state.
//   owner:        the platform secret store (Keychain); Seoul holds no copy.
//   lifetime:     managed by the OS keychain; not tied to any Seoul object.
//   persistence:  secret platform-managed state (never Seoul product state).
//   recovery:     re-read from the keychain on demand.
//   teardown:     Delete removes the item; no in-memory secret survives a call.
//   isolation:    per macOS user account and keychain access policy.
class CredentialStore {
 public:
  virtual ~CredentialStore() = default;

  // Returns the secret for `account_key`, or nullopt if none is stored. The
  // returned string is the caller's to use once and drop; implementations must
  // not cache it.
  virtual std::optional<std::string> Get(const std::string& account_key) = 0;
  virtual bool Set(const std::string& account_key,
                   const std::string& secret) = 0;
  virtual bool Delete(const std::string& account_key) = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_INTELLIGENCE_CREDENTIAL_STORE_H_
