// Project Seoul native organization engine.

#include "seoul/browser/organization/seoul_organization_service_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "seoul/browser/organization/seoul_organization_service.h"

namespace seoul {

// static
SeoulOrganizationService* SeoulOrganizationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SeoulOrganizationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
SeoulOrganizationServiceFactory*
SeoulOrganizationServiceFactory::GetInstance() {
  static base::NoDestructor<SeoulOrganizationServiceFactory> instance;
  return instance.get();
}

SeoulOrganizationServiceFactory::SeoulOrganizationServiceFactory()
    : ProfileKeyedServiceFactory(
          "SeoulOrganizationService",
          // Eligible regular profiles only. kOriginalOnly excludes
          // off-the-record (incognito); guest, system, and Ash-internal
          // profiles are excluded.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

SeoulOrganizationServiceFactory::~SeoulOrganizationServiceFactory() = default;

std::unique_ptr<KeyedService>
SeoulOrganizationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SeoulOrganizationService>(profile,
                                                    profile->GetPrefs());
}

void SeoulOrganizationServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SeoulOrganizationService::RegisterProfilePrefs(registry);
}

bool SeoulOrganizationServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Create eagerly for eligible profiles so the default workspace is
  // initialized and prefs are loaded at profile startup. No UI and no tab
  // behavior is touched.
  return true;
}

}  // namespace seoul
