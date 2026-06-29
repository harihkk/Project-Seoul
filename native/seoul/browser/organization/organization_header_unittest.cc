// Header self-containment regression: organization_types.h must pull in limits.
#include "seoul/browser/organization/organization_types.h"

namespace seoul {
static_assert(kOrganizationSchemaVersion == 1, "schema constant visible");
}  // namespace seoul
