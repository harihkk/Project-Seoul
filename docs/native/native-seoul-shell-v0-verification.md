# Native Seoul Shell V0 verification

Machine: 8 GiB RAM - no GN, Ninja, compilation, or test execution.

## VERIFIED NOW

- Projection acceptance audit (`docs/research/workspace-projection-v0-acceptance-audit.md`)
- Accessibility repair: `GetViewAccessibility().SetIsIgnored`
- Shell architecture research (`docs/research/native-shell-integration-audit.md`)
- Product spec (`docs/product/native-seoul-shell-v0-spec.md`)
- Pure shell view model (`shell_view_model.*`)
- Per-window shell controller (`shell_controller.*`)
- Shell service wired in `SeoulOrganizationService`
- Production Views: header, footer, workspace menu, command launcher
- Patch registers `ShellService` in `VerticalTabStripRegionView`
- GN targets: `shell_core`, `shell_chromium`, unit/browser test targets
- Tests authored: `seoul_shell_core_unittests`, `shell_browser_tests`
- Patch manifest SHA256 updated

## NOT VERIFIED UNTIL CAPABLE HOST

- GN generation, compilation, linking
- Unit/browser test execution
- Actual shell appearance and layout
- Focus, accessibility tree, keyboard shortcuts
- Context menus, collapse animation, expand-on-hover
- Real tab actions and workspace switching UX
- Performance and memory

## Recommended next milestone

**Runtime integration verification on a capable host**: compile `shell_chromium`, run `seoul_shell_core_unittests` and browser tests, manually validate workspace switch + Essentials + projection filter together.
