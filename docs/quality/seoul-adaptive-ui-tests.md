# Seoul Adaptive UI Test Plan

Status: Source complete; not compiled or runtime-verified on the authoring host.

This is the test plan for the SAUI layer in `native/seoul/browser/saui/`. The
unit-level tests named below are authored and check the parser, validator, patch,
and selection logic. They compile and run only on a capable build host (an 8 GiB
host cannot build Chromium), so no run results are recorded here; the tests
exist as source.

## Authored unit tests

### Document and catalog (`saui_document_unittest.cc`)

- Schema parse: `ParsesMinimalSurface`, `SerializationRoundTrips`,
  `ParsesDataEntriesOfEveryKind`.
- Unknown component rejection: `RejectsUnknownComponentType`.
- Forbidden handler/markup keys: `RejectsEventHandlerProps`, plus
  `RejectsHandlerAndMarkupKeys` in `SauiPropKeyTest`.
- Non-http URL rejection: `RejectsNonHttpUrls` and
  `ParsesActionsAndRejectsBadNavigationTarget`.
- Depth and structure bounds: `RejectsExcessiveNestingDepth`,
  `RejectsChildrenOnNonContainer`.
- Data-kind parsing: `ParsesDataEntriesOfEveryKind`,
  `RejectsMalformedSeriesPoints`, `RejectsRaggedTableRows`.
- Schema and identifier rules: `RejectsWrongSchemaVersionAndUnknownKind`,
  `ValidatesCharsetAndBounds` (`SauiIdentifierTest`). The catalog is checked by
  `EveryTypeHasARowInDeclarationOrder`, `NamesAreUniqueAndRoundTrip`, and
  `ChartTypesRequireBindingAndAccessibleName` (`SauiCatalogTest`).

### Validator (`saui_validator_unittest.cc`)

- Binding mismatch and resolution: `RejectsBindingKindMismatch`,
  `RejectsUnresolvedBinding`, `RejectsMissingRequiredBinding`.
- Duplicate ids and action references: `RejectsDuplicateComponentIds`,
  `UnknownActionReferenceIsRejected`.
- Chart honesty: `AcceptsWellFormedChartSurface`,
  `ChartWithoutProvenanceIsRejected`, `ChartWithOnePointIsRejected`,
  `ChartMissingUnitsPropIsRejected`, `ChartMissingAccessibleNameIsRejected`,
  and the eligibility gate `EntryChartEligibilityGate`.
- Truncated axis: `TruncatedBarChartMustSaySo`.
- Title rule: `NonResponseSurfaceRequiresTitle`.

### Incremental patch (`saui_patch_unittest.cc`)

- Patch atomicity: `FailedOpLeavesSurfaceUntouched`,
  `ResultingInvalidSurfaceIsRejected`, `WrongSurfaceIdIsRejected`.
- In-place updates: `AppendSeriesPointsUpdatesInPlace`,
  `SetPropsMergesAndReportsComponent`, `RemoveComponentAndStateChange`,
  `AppendChildRespectsContainerRule`.
- Malicious/untrusted payload rejection: `ParsesUntrustedPatchDocument`,
  `ParseRejectsUnknownOpAndBadIds`.

### Presentation selection (`saui_selection_unittest.cc`)

- `SingleScalarNeverBecomesAChart`, `NoEligibleDataMeansText`,
  `MissingInputsPreferAFormOverQuestions`, `TextOnlyRequestWins`,
  `OneEligibleSeriesIsASingleComponent`, `ComparisonComposesASurface`, and
  `MonitoringAndTaskResultsPersist`.

## Malicious-payload coverage

The forbidden-key, non-http-URL, ragged-table, malformed-series,
excessive-depth, unknown-type, and untrusted-patch tests above are the
structural defense against a hostile generator document. They assert that a
rejected document produces a precise `SauiError` and no partial accept, matching
the parser and patch code.

## Open work requiring a build

Interactive behavior and the accessibility tree cannot be exercised at the unit
level: rendering a validated surface, dispatching real `ComponentEvent`s, and
asserting the resulting accessibility tree require the browser-test harness and a
capable build host. Those browser tests are open. Today the document model,
catalog, validator, patch, and selection logic are covered by the authored unit
tests listed here; the rendered-surface behavior is specified but not yet tested.
