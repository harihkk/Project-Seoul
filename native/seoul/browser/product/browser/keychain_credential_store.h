// Project Seoul product runtime - macOS credential store.
// The concrete CredentialStore over the macOS Keychain (Security framework).
// Secrets live only in the keychain under a Seoul service namespace; they are
// never written to prefs, never sent to the Canvas WebUI, and never logged.
// Reads distinguish "absent" from "keychain unavailable" via last_status().

#ifndef SEOUL_BROWSER_PRODUCT_BROWSER_KEYCHAIN_CREDENTIAL_STORE_H_
#define SEOUL_BROWSER_PRODUCT_BROWSER_KEYCHAIN_CREDENTIAL_STORE_H_

#include <optional>
#include <string>

#include "seoul/browser/intelligence/credential_store.h"

namespace seoul {

class KeychainCredentialStore : public CredentialStore {
 public:
  enum class StoreStatus {
    kOk,
    kNotFound,
    kLocked,       // keychain present but interaction not allowed
    kUnavailable,  // keychain services errored
  };

  // `profile_namespace` isolates credentials per profile: the keychain
  // account is "<profile_namespace>/<account_key>".
  explicit KeychainCredentialStore(const std::string& profile_namespace);
  KeychainCredentialStore(const KeychainCredentialStore&) = delete;
  KeychainCredentialStore& operator=(const KeychainCredentialStore&) = delete;
  ~KeychainCredentialStore() override;

  // CredentialStore:
  std::optional<std::string> Get(const std::string& account_key) override;
  bool Set(const std::string& account_key, const std::string& secret) override;
  bool Delete(const std::string& account_key) override;

  // The outcome of the most recent operation, for user-visible provider
  // states (locked / unavailable / missing). Never carries secret content.
  StoreStatus last_status() const { return last_status_; }

 private:
  std::string QualifiedAccount(const std::string& account_key) const;

  const std::string profile_namespace_;
  StoreStatus last_status_ = StoreStatus::kOk;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_BROWSER_KEYCHAIN_CREDENTIAL_STORE_H_
