# native/seoul/config

Seoul-owned build and feature configuration inputs: GN snippets that compile the
Seoul source under `../browser/`, `../webui/`, and `../resources/`, plus any
Seoul-owned feature/config declarations.

Contract:
- Materialized to `$SEOUL_CHROMIUM_ROOT/src/seoul/config/` (and referenced from
  the Seoul GN targets).
- These are inputs to the build; they are not the development baseline GN args.
  The baseline component-build args live in `../../gn/macos-arm64-baseline.gn` and
  are not the shipping configuration.
- Upstream `BUILD.gn` files are only touched through integration patches in
  `../../patches/chromium/` (for example, to add the Seoul target as a dependency).

Empty in this milestone by design. Do not add placeholder config.
