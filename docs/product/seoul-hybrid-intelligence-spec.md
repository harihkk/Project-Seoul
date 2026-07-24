# Seoul Hybrid Intelligence Specification

Status: Current compile and runtime evidence is maintained in the product
readiness report.

Seoul's intelligence layer is provider-neutral and deterministic-first. This spec
describes the source in `native/seoul/browser/intelligence/`. Core logic never
depends on a specific model SDK, deterministic reasoning never reaches a model,
and Seoul must remain useful with no model configured.

## Provider-neutral ModelProvider interface

`native/seoul/browser/intelligence/model_provider.h` defines the `ModelProvider`
seam. A provider reports a `provider_id` (for example "local:llama-3.2-3b") and
`ModelCapabilities`: `text_generation`, `structured_generation`, `tool_planning`,
`vision`, `streaming`, `max_context_tokens`, a `ModelLocality` (`kLocal` or
`kCloud`), a `RetentionPolicy`, a measured `planning_quality` (0 to 100, set from
benchmarks not marketing), and per-mega-token input/output costs in microdollars
(0 for local). `RetentionPolicy` (`kNoRetention`, `kEphemeralRetention`,
`kRetainedNotTrained`, `kUnknown`) records the provider's data-retention
posture as metadata. Providers expose `Generate`, `Cancel`, and a
pre-flight `EstimateCostMicrodollars`. Local runtimes and official cloud APIs both
implement this one interface.

## Deterministic-first rule

`ReasoningKind` in `reasoning_router.h` splits reasoning into deterministic-only
and model-eligible kinds. `IsDeterministicKind` returns true for
`kExactBrowserCommand`, `kKnownSceneOperation`, `kArithmetic`, `kSorting`,
`kFiltering`, `kChartRendering`, `kWorkflowExecution`,
`kStructuredDataFormatting`, and `kAlreadySelectedToolCall`. The header states a
model call for any of these is a routing bug. The model-eligible kinds are
`kGeneralPlanning`, `kSummarization`, `kClassification`, `kOpenEndedGeneration`,
and `kVisionUnderstanding`.

## Best-qualifying-route logic

`RouteReasoning` in `reasoning_router.cc` picks the best route that meets the
quality threshold: deterministic first, then the best qualifying model. If the
kind is deterministic, it returns `kDeterministic` immediately. Otherwise it
evaluates `LocalQualifies` and `CloudQualifies` and applies these rules in
order:

- Local when preferred or alone: when local qualifies and either the optional
  `prefer_local` preference is set (local is free) or cloud does not qualify,
  it routes local.
- Best route by default: `prefer_local` is off by default, so when cloud
  qualifies it routes cloud with the estimated cost, reasoning
  `kCloudNeededForVision` or `kCloudNeededForQuality`.
- Local fallback: if local qualifies but was not preferred and cloud is
  unavailable, it still routes local rather than failing.
- Unavailable: when nothing qualifies, it returns `kUnavailable` with the most
  specific `RouteReason`.

`LocalQualifies` requires `local_available`, a non-null provider, vision when
`needs_vision`, and `planning_quality >= required_quality`. `CloudQualifies` adds
the ceilings below.

## Budget ceiling and vision requirement

`CloudQualifies` returns false when cloud is disabled or the provider is null,
when the provider lacks vision but the step needs it, when the provider's
`planning_quality` is below `required_quality`, or when the estimated cost
exceeds `remaining_budget_microdollars`.

## Official APIs only, BYOK, no consumer-chat scraping

The `model_provider.h` header states consumer chat subscriptions and website
scraping are explicitly out of scope; providers are local runtimes or official
cloud APIs. Bring-your-own-key credential storage lives in a secure store outside
this module, so no keys appear here. The router explains every choice through
`RouteDecision` and `RouteReason` string forms.

## Useful with no model configured

Because deterministic kinds route to `kDeterministic` without any provider, and
because `RouteReasoning` accepts null `local` and `cloud` providers, Seoul
operates in deterministic mode with no model configured: exact browser commands,
known scene operations, arithmetic, sorting, filtering, chart rendering, workflow
execution, structured formatting, and already-selected tool calls all resolve
without a model.
