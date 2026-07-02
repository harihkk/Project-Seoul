# Domain hardcoding removal audit

This audit records every domain-specific symbol removed from or generalized in
the core runtime, per the capability-driven architecture correction. After this
pass the production core contains no weather, market, shopping, travel,
calendar-domain, or research-domain contracts; capability results flow through
the generic semantic fabric (`native/seoul/browser/semantic/`) and render
through the shape-and-role adaptive interface compiler
(`native/seoul/browser/saui/interface_compiler.*`). A static neutrality check
(`scripts/check-domain-neutrality.mjs`) fails CI if forbidden domain symbols
are reintroduced into core modules.

Status: source-level removal complete; not compiled on the authoring host.

## 1. Deleted files (domain contracts in data_core)

Treatment: removed entirely (option 4: no value beyond generic schemas). Their
information content is now expressible as semantic shapes plus roles, so no
fixture package was needed to preserve them; evaluation fixtures use held-out
schemas defined inside tests only.

- `native/seoul/browser/data/weather_types.h`: WeatherCondition,
  TemperatureUnit, WeatherLocation, WeatherObservation, DailyForecast,
  WeatherAlert, WeatherReport. Generic replacement: kTimeSeries /
  kEventStream / kComposite shapes with kTimestamp, kMeasure (unit metadata),
  kCategory, kIntervalStart/kIntervalEnd roles.
- `native/seoul/browser/data/weather_provider.h`: WeatherRequest,
  WeatherProvider. Replacement: any capability returning a SemanticResult.
- `native/seoul/browser/data/market_types.h`: MarketStatus, SeriesInterval,
  SeriesAdjustment, DecimalAmount, InstrumentIdentity, MarketQuote, SeriesBar,
  PriceSeries. Generic replacement: kTimeSeries with kOpen/kHigh/kLow/kClose
  roles (generic interval summaries), kMoney role with currency_code field
  metadata, provenance freshness for delayed-vs-live.
- `native/seoul/browser/data/market_data_provider.h`: InstrumentQuery,
  SeriesRequest, MarketDataProvider.
- `native/seoul/browser/data/product_types.h`: StockState, RatingSummary,
  ProductOffer, PriceHistoryAvailability, PriceHistoryPoint,
  ProductPriceHistory, ProductSearchResult. Generic replacement:
  kEntityCollection with kIdentifier/kName/kMoney/kUrl/kStatus roles;
  provider-supplied history is just another kTimeSeries.
- `native/seoul/browser/data/shopping_provider.h`: ProductSearchRequest,
  ShoppingProvider.

## 2. Generalized files (data_core)

- `native/seoul/browser/data/data_validation.{h,cc}`: removed
  ValidateWeatherReport, ValidateMarketQuote, ValidatePriceSeriesForChart,
  ValidateProductOffer, ValidateProductPriceHistory and the violation values
  kMissingUnits, kInsufficientPoints, kNonMonotonicTimestamps,
  kNonFiniteValue, kMissingCurrency, kMissingMerchant, kMissingSourceUrl,
  kNegativePrice, kInconsistentCurrency, kMissingLocation, kEmptyResult.
  Retained: ValidateProvenance and the provenance/freshness/data-error string
  functions. Chart eligibility now lives generically in
  `semantic/semantic_inspection.cc` (ChartWouldMislead) and
  `saui/saui_validator.cc` (EntryChartEligible).
- `native/seoul/browser/data/data_errors.h`: removed kUnknownLocation and
  kUnknownInstrument; added generic kUnresolvedEntity.
- `native/seoul/browser/data/data_validation_unittest.cc`: domain fixtures
  removed; retains provenance tests only.
- `native/seoul/browser/data/BUILD.gn`: sources reduced accordingly.

## 3. Removed SAUI component types (saui_types.h + saui_catalog.cc)

`ComponentCategory::kDomain` removed. Component enum values removed, with the
generic primitive that replaces each:

- kWeatherCurrent, kWeatherHourly, kWeatherDaily, kWeatherAlert -> scalar
  metrics, kLineChart/kSortableTable over time series, kTimeline for alerts
  (interval/event shapes).
- kSecurityQuote -> kMetric plus kEntityCard over a record with kMoney role.
- kWatchlistControl -> generic kButton/kFilterChips actions.
- kProductCard, kSellerCard, kLocationCard, kFlightCard, kHotelCard,
  kSourceCard, kEventCard, kFileCard -> kEntityCard (one generic entity
  presentation).
- kPriceComparison, kEvidenceTable, kClaimComparison, kExtractionTable,
  kApiResultTable -> kTable/kSortableTable/kComparisonMatrix.
- kItineraryCard, kAgendaView, kAvailabilityGrid -> kTimeline/kTable over
  interval/event shapes.
- kRouteCard -> kMap + kGeoLayer over kRoute shape.
- kCitationGraph -> kNetworkGraph (generic).
- kOutline -> kTree/kList.
- kReportPreview, kDocumentViewer -> kDocument.
- kAttachmentList -> kList/kFile.
- kImageGallery -> kMedia.
- kParameterForm -> kSchemaForm (dynamic form from a typed schema).

Added generic primitives (all domain-independent): kSchemaForm, kEntityCard,
kDocument, kMedia, kFile, kHeatMap, kNetworkGraph, kGeoLayer, kWorkflowEdge.

Retained with justification as generic primitives: kCandlestickChart (an
open/high/low/close interval-summary presentation over any measure; the OHLC
roles in the semantic fabric are domain-neutral), kComparisonMatrix,
kTimeline, kMap, kTree, kSparkline.

## 4. Domain-specific validation and catalog logic removed

- Catalog rows for all removed types deleted; the catalog static_assert bound
  moved from kApiResultTable to kFileTree.
- No validator rule references a domain; chart honesty rules (axes, units,
  provenance, indicated truncation, bounded pie slices) are unchanged and
  generic.
- The interface compiler contains no domain conditionals; its rules are
  documented shape-and-role rules, exercised by held-out unseen-schema tests
  in `saui/interface_compiler_unittest.cc` (telemetry, standings, listings,
  tallies, org chart, dependency graph, citations - fixtures that exist only
  in tests).

## 5. Documentation treatment

Production docs that presented weather/market/product as core contracts
(`seoul-adaptive-ui-spec.md` domain-composition section, product-definition
module table rows, readiness sections 10-12) are superseded by this audit and
the updated readiness report; the specs remain accurate for the generic
protocol and are updated where they named domain widgets as core.

## 6. Enforcement

`scripts/check-domain-neutrality.mjs` scans core production sources (tests,
fixtures, and docs excluded) for forbidden industry symbols and
phrase-routing patterns and fails when any reappear. Wired into `npm run ci`.
