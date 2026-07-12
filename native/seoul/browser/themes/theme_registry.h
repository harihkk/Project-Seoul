// Project Seoul theme registry.
// Stores only fully validated themes by stable id. The registry owns no UI or
// browser mutation; Scene validation and Studio inspect it through narrow APIs.

#ifndef SEOUL_BROWSER_THEMES_THEME_REGISTRY_H_
#define SEOUL_BROWSER_THEMES_THEME_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

#include "base/values.h"
#include "seoul/browser/themes/theme_validation.h"

namespace seoul {

class ThemeRegistry {
 public:
  ThemeRegistry();
  ThemeRegistry(const ThemeRegistry&) = delete;
  ThemeRegistry& operator=(const ThemeRegistry&) = delete;
  ~ThemeRegistry();

  ThemeStatusResult Upsert(Theme theme);
  ThemeStatusResult Remove(const std::string& theme_id);
  const Theme* Find(const std::string& theme_id) const;
  bool Exists(const std::string& theme_id) const;
  std::vector<const Theme*> List() const;  // id-ordered
  size_t size() const { return themes_.size(); }

  base::DictValue TakePersistedState() const;
  void RestorePersistedState(const base::DictValue& state);

 private:
  std::map<std::string, Theme> themes_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_THEMES_THEME_REGISTRY_H_
