// Project Seoul native organization engine.
// Profile-keyed factory for SeoulOrganizationService. It restricts the service
// to eligible regular profiles and excludes incognito/off-the-record, guest,
// and system profiles, and registers the bounded organization preference.

#ifndef SEOUL_BROWSER_ORGANIZATION_SEOUL_ORGANIZATION_SERVICE_FACTORY_H_
#define SEOUL_BROWSER_ORGANIZATION_SEOUL_ORGANIZATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace seoul {

class SeoulOrganizationService;

class SeoulOrganizationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the service for `profile`, or nullptr for an ineligible profile
  // (incognito, guest, system).
  static SeoulOrganizationService* GetForProfile(Profile* profile);
  static SeoulOrganizationServiceFactory* GetInstance();

  SeoulOrganizationServiceFactory(const SeoulOrganizationServiceFactory&) =
      delete;
  SeoulOrganizationServiceFactory& operator=(
      const SeoulOrganizationServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SeoulOrganizationServiceFactory>;

  SeoulOrganizationServiceFactory();
  ~SeoulOrganizationServiceFactory() override;

  // ProfileKeyedServiceFactory / BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_ORGANIZATION_SEOUL_ORGANIZATION_SERVICE_FACTORY_H_
