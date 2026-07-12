# Seoul canonical wire protocol (version 1)

This directory is the single cross-language source of truth for every document
that crosses a Seoul process or language boundary. The native C++ runtime
(`native/seoul/`) and the TypeScript Canvas Design Lab
(`apps/canvas-prototype/`) both conform to these schemas; neither side may
author an independent protocol model.

## Contents

| File | Document | Native ground truth |
| --- | --- | --- |
| `semantic-result.schema.json` | Capability outcome: shape + roles + data + provenance + citations + conflicts + streaming/partial state + continuation + unavailable fields + structured errors | `semantic/semantic_types.h`, `semantic/semantic_wire.h` |
| `adaptive-surface.schema.json` | SAUI surface document: components, data entries, actions, provenance | `saui/saui_types.h`, `saui/saui_document.cc` |
| `surface-patch.schema.json` | Incremental surface updates (10 ops, atomic) | `saui/saui_patch.h/.cc` |
| `component-event.schema.json` | Typed Canvas interaction events | `saui/saui_events.h` |
| `task-snapshot.schema.json` | Task state for rendering: receipts, usage, approval | `product/task_service.h`, `tasks/task_types.h` |
| `capability-descriptor.schema.json` | One plannable capability (tool) | `tools/tool_types.h`, `tools/tool_schema.h` |

`fixtures/` holds the shared conformance corpus consumed by **both** the
native C++ unit tests and the TypeScript tests. `ts/types.ts` is **generated**
from the schemas by `scripts/generate-protocol-types.mjs`; do not edit it by
hand. `ts/validate.mjs` is the runtime validator for the JSON Schema subset
below. `component-catalog.json` is **generated from the native sources**
(`saui_catalog.cc`, `saui_limits.h`) by
`scripts/generate-component-catalog.mjs`: component semantics (container-ness,
accepted binding kinds, required props, accessible-name rules) and the
structural limits, consumed by the Design Lab's patch engine so both sides
enforce identical rules from identical data. Both generated files are
drift-gated by `scripts/check-protocol.mjs`.

Semantic field ids may be up to 64 chars while SAUI prop/table keys are capped
at 40. Both compilers use the same collision-free derivation: already-safe keys
remain unchanged; longer or reserved keys receive deterministic schema-local
`field_N` names in lexical order. Composite entry names use bounded ordinal
paths (`p0_...`) and series names use the derived field key, so every valid
semantic schema is representable without truncation or probabilistic hashes.
Human labels and the underlying semantic document retain the original ids.

## Versioning

Protocol version 1. Version fields on the wire:

- `semantic-result`: `schema.schema_version` (const 1, required).
- `adaptive-surface`: `schema_version` (const 1, required). `revision` is
  runtime state and never travels on the wire.
- `task-snapshot`, `capability-descriptor`: `schema_version` (const 1,
  required). A descriptor's `version` field is the *capability's* contract
  version, independent of the protocol version.
- `surface-patch`, `component-event`: unversioned envelopes; they reference a
  versioned surface and are covered by its version.

Compatibility rule: a consumer must reject a document whose version field is
missing (where required) or not equal to 1. `protocol/tests/` and the native
conformance tests both assert this.

## JSON Schema subset

Schemas use JSON Schema draft 2020-12 restricted to: `type`, `enum`, `const`,
`properties`, `required`, `additionalProperties`, `propertyNames` (pattern),
`items`, `oneOf`, `$ref` (`#/$defs/...` and cross-file
`<file>.schema.json#/$defs/...`), `pattern`, `minLength`/`maxLength`,
`minimum`/`maximum`, `minItems`/`maxItems`, `minProperties`/`maxProperties`,
`description`, `title`, `$id`, `$defs`. `ts/validate.mjs` implements exactly
this subset and rejects schemas that stray outside it.

## What the schemas do and do not enforce

The schemas are the **structural floor**: key names, enum spellings, shapes,
and coarse bounds. Each side additionally enforces semantic rules the schema
language cannot express — role coherence per shape, data/schema conformance,
undeclared-key rejection, per-prop-key bounds (url schemes, code lengths,
structured-list item rules), binding-kind compatibility, and patch-target
existence. A document can validate against these schemas and still be rejected
by `ValidateSemanticResult`/`ValidateSurface` (C++) or the Design Lab runtime
checks (TS); that is by design and is asserted by fixtures in
`fixtures/invalid/`.

## Conformance strategy

- **TypeScript**: `protocol/tests/protocol-conformance.test.mjs` validates
  every fixture against its schema with `ts/validate.mjs`, asserts the invalid
  corpus fails, and runs the version-compatibility cases.
  `scripts/check-protocol.mjs` (in `npm run ci`) additionally regenerates
  `ts/types.ts` and fails on drift, and extracts the enum wire names from the
  native sources (`semantic_types.cc`, `saui_catalog.cc`, `saui_types.cc`,
  `task_execution.cc`, `saui_patch.cc`) and fails if schema enums and native
  wire names ever diverge.
- **C++**: `semantic/semantic_wire_unittest.cc`,
  `saui/protocol_fixtures_unittest.cc`, `product/task_snapshot_wire_unittest.cc`
  and `tools/tool_descriptor_wire_unittest.cc` parse the same fixtures with the
  production codecs, round-trip them, and assert the version-compatibility
  rules. `native/scripts/materialize.sh` mirrors `protocol/` to
  `src/seoul/protocol/` so the tests read the identical files. These tests are
  source-complete but require the capable build host to compile and run; see
  `docs/release/seoul-product-readiness.md` for what has actually executed.

## Fixture corpus

`fixtures/semantic/` — one canonical semantic result per case: scalar, record,
collection, table, time-series, intervals, hierarchy, graph, geospatial,
document, citations, form, action-set, diff, code, composite, partial-result,
streaming-update (base + `streaming-update.append.json` rows), source-conflict,
error-result. `fixtures/surface/`, `fixtures/patch/`, `fixtures/event/`,
`fixtures/task/`, `fixtures/capability/` cover the other five schemas.
`fixtures/compat/` holds version-compatibility cases with expected outcomes in
`fixtures/compat/expectations.json`. `fixtures/invalid/` holds documents that
must fail schema validation, with the failing pointer listed in
`fixtures/invalid/expectations.json`. `fixtures/catalog.json` maps capability
descriptors to semantic fixtures and example goals for the Design Lab's
fixture capability catalog; every entry is clearly synthetic demo data.

All fixture content is synthetic, domain-neutral, deterministic (fixed
timestamps), and network-free.
