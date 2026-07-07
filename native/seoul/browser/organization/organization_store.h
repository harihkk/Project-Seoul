// Project Seoul native organization engine.
// Bounded, versioned serialization of the organization snapshot to and from a
// base::DictValue. This is the on-disk shape that the KeyedService persists
// into a single profile preference. It stores ONLY organization metadata: no
// page content, history, cookies, passwords, form values, downloads, model
// prompts, or tokens. Deserialization is strict: unsupported future schema
// versions are rejected (never downgraded), malformed required fields are
// rejected, and missing optional fields default safely. Deep invariant checks
// live in OrganizationModel::LoadSnapshot; this layer is structural plus
// version/size.

#ifndef SEOUL_BROWSER_ORGANIZATION_ORGANIZATION_STORE_H_
#define SEOUL_BROWSER_ORGANIZATION_ORGANIZATION_STORE_H_

#include "base/values.h"
#include "seoul/browser/organization/organization_errors.h"
#include "seoul/browser/organization/organization_types.h"

namespace seoul {

// Deterministic serialization: the same snapshot always produces the same dict.
base::DictValue SerializeSnapshot(const OrganizationSnapshot& snapshot);

// Strict structural deserialization. Returns kUnsupportedSchema for an unknown
// version and kCorruptState for malformed required structure. The returned
// snapshot must still pass OrganizationModel::LoadSnapshot for semantic
// validity.
MutationResult<OrganizationSnapshot> DeserializeSnapshot(
    const base::DictValue& dict);

// True if the serialized JSON of `dict` is within kMaxSerializedBytes.
bool SerializedSizeWithinLimit(const base::DictValue& dict);

}  // namespace seoul

#endif  // SEOUL_BROWSER_ORGANIZATION_ORGANIZATION_STORE_H_
