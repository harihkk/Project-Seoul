# native/seoul/resources

Static, Seoul-owned resources: icons, image assets, localized strings, and the
GRD/GRDP inputs that package them.

Contract:
- Materialized to `$SEOUL_CHROMIUM_ROOT/src/seoul/resources/`.
- Resource packs are registered with the build via `../config/` GN and, where an
  upstream registration point must be touched, via an integration patch.
- All assets are original to Seoul or appropriately licensed for redistribution.
  No proprietary assets or branding from any other browser.

Empty in this milestone by design. Do not add placeholder assets.
