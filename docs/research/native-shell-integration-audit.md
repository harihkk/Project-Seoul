# Native shell integration audit (pinned M149)

Pinned revision: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`.

## Selected integration seam

| Component | File | Ownership | Seoul action |
| --- | --- | --- | --- |
| `VerticalTabStripRegionView` | `chrome/browser/ui/views/frame/vertical_tab_strip_region_view.*` | `BrowserView` child; FlexLayout vertical | Insert Seoul header/footer siblings; register services at `InitializeTabStrip` |
| `VerticalTabStripView` | via `RootTabCollectionNode::Initialize` | Chromium tab collection | **Not replaced** - remains tab row source |
| `VerticalTabStripTopContainer` | region ctor child | Chromium | Left intact (window controls / top actions) |
| `VerticalTabStripBottomContainer` | region ctor child | Chromium | Anchor for Seoul footer insertion |
| `VerticalTabStripStateController` | browser feature | Chromium | Seoul reads collapse via `OnCollapseStateChanged` patch hook |
| `SeoulOrganizationService` | profile KeyedService | Seoul | Owns `ShellService` + `ProjectionService` |

## Rejected alternatives

| Alternative | Reason |
| --- | --- |
| WebUI shell | Violates product constraint; adds bundle weight |
| Duplicate tab list views | Would break drag/focus/model indexing |
| Inject section views inside `TabCollectionNode` | Risks breaking model/view 1:1 mapping |
| Second width/collapse controller | Conflicts with `VerticalTabStripStateController` |

## Seoul insertion order (V0)

```text
VerticalTabStripTopContainer        (Chromium)
top_button_separator
SeoulShellHeaderView                (Seoul)
VerticalTabStripView                (Chromium / RootTabCollectionNode)
SeoulShellFooterView                (Seoul)
VerticalTabStripBottomContainer     (Chromium)
```

## Patch surface

- `InitializeTabStrip` - register `ShellService` + `ProjectionService`
- destructor - unregister both
- `OnCollapseStateChanged` - `ShellService::OnCollapseStateChanged`
- narrow accessors: `GetSeoulShellSeparatorAnchor`, `GetSeoulShellFooterAnchor`, `GetSeoulTabStripView`
- `BUILD.gn` - `//seoul/browser/shell:shell_chromium` circular include

## Section separation limitation (V0)

Retained vs temporary tabs are **not** physically split inside `VerticalTabStripView`. V0 uses section labels in `SeoulShellHeaderView` driven by projection metadata. Chromium tab nodes remain authoritative.

## Compile uncertainty

Full link of `shell_chromium` against `//chrome/browser` not verified on 8 GiB host.
