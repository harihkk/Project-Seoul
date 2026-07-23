// Project Seoul product runtime - the browser-owned page agent.

#include "seoul/browser/product/browser/page_agent.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/gurl.h"

namespace seoul {

namespace {

// Snapshot bounds: enough for real pages, capped so a hostile document cannot
// grow memory or stall the observation.
constexpr size_t kMaxSnapshotNodes = 5000;
constexpr base::TimeDelta kSnapshotTimeout = base::Seconds(5);
constexpr size_t kMaxObservedElements = 400;

// Roles that are worth reporting as actionable/observable semantic elements.
bool IsInterestingRole(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kComboBoxSelect:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kHeading:
      return true;
    default:
      return false;
  }
}

bool RoleIsEditable(ax::mojom::Role role) {
  return role == ax::mojom::Role::kTextField ||
         role == ax::mojom::Role::kTextFieldWithComboBox ||
         role == ax::mojom::Role::kSearchBox;
}

bool IsValueMutation(PageActionKind kind) {
  return kind == PageActionKind::kType || kind == PageActionKind::kClear ||
         kind == PageActionKind::kSelectOption;
}

const char* RoleName(ax::mojom::Role role) {
  return ui::ToString(role);
}

}  // namespace

PageObservation::PageObservation() = default;
PageObservation::PageObservation(const PageObservation&) = default;
PageObservation::PageObservation(PageObservation&&) = default;
PageObservation& PageObservation::operator=(const PageObservation&) = default;
PageObservation& PageObservation::operator=(PageObservation&&) = default;
PageObservation::~PageObservation() = default;

PageActionRequest::PageActionRequest() = default;
PageActionRequest::PageActionRequest(const PageActionRequest&) = default;
PageActionRequest::PageActionRequest(PageActionRequest&&) = default;
PageActionRequest& PageActionRequest::operator=(const PageActionRequest&) =
    default;
PageActionRequest& PageActionRequest::operator=(PageActionRequest&&) = default;
PageActionRequest::~PageActionRequest() = default;

PageAgent::PageAgent(WebContentsResolver resolver)
    : resolver_(std::move(resolver)) {}

PageAgent::~PageAgent() = default;

void PageAgent::Observe(
    const LiveTabKey& tab,
    base::OnceCallback<void(std::optional<PageObservation>)> callback) {
  content::WebContents* contents = resolver_.Run(tab);
  if (!contents) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  // A fresh observation expires every prior handle for this tab. Capture the
  // generation this request belongs to; the async snapshot below must be
  // rejected if another Observe() or InvalidateTab() bumps the generation
  // before it returns, so a superseded tree can never register handles.
  TabGeneration& generation = tabs_[tab];
  ++generation.generation;
  generation.handles.clear();
  const uint64_t expected_generation = generation.generation;

  ui::AXMode snapshot_mode = ui::kAXModeWebContentsOnly;
  // Password protection is an AX state, while payment and one-time-code
  // semantics are standards-defined autocomplete tokens. Request HTML
  // attributes for this bounded one-shot snapshot, then project only the
  // reviewed fields below; raw attributes never enter PageObservation.
  snapshot_mode.set_mode(ui::AXMode::kHTML, true);
  contents->RequestAXTreeSnapshot(
      base::BindOnce(
          [](base::WeakPtr<PageAgent> agent, LiveTabKey tab,
             uint64_t expected_generation,
             base::OnceCallback<void(std::optional<PageObservation>)> callback,
             ui::AXTreeUpdate& update) {
            if (agent) {
              agent->OnSnapshot(tab, expected_generation, std::move(callback),
                                &update);
            }
          },
          weak_factory_.GetWeakPtr(), tab, expected_generation,
          std::move(callback)),
      snapshot_mode, kMaxSnapshotNodes, kSnapshotTimeout,
      content::WebContents::AXTreeSnapshotPolicy::kSameOriginDirectDescendants);
}

void PageAgent::OnSnapshot(
    LiveTabKey tab,
    uint64_t expected_generation,
    base::OnceCallback<void(std::optional<PageObservation>)> callback,
    void* update_ptr) {
  content::WebContents* contents = resolver_.Run(tab);
  if (!contents) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  TabGeneration& generation = tabs_[tab];
  // Another observation or a navigation superseded this snapshot; drop it
  // rather than register stale nodes under the current generation.
  if (generation.generation != expected_generation) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  ui::AXTreeUpdate& update = *static_cast<ui::AXTreeUpdate*>(update_ptr);

  PageObservation observation;
  observation.tab = tab;
  observation.generation = generation.generation;
  // Report scheme://host[:port]/path with no query, fragment, or credentials,
  // so the observation never carries tracking parameters or secrets.
  const GURL& committed = contents->GetLastCommittedURL();
  if (committed.is_valid()) {
    GURL::Replacements strip;
    strip.ClearQuery();
    strip.ClearRef();
    strip.ClearUsername();
    strip.ClearPassword();
    observation.url = committed.ReplaceComponents(strip).spec();
  }
  observation.title = base::UTF16ToUTF8(contents->GetTitle());
  observation.loaded = !contents->IsLoading();

  for (const ui::AXNodeData& node : update.nodes) {
    if (observation.elements.size() >= kMaxObservedElements) {
      break;
    }
    if (!IsInterestingRole(node.role)) {
      continue;
    }
    const std::string name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    if (name.empty() && !RoleIsEditable(node.role)) {
      continue;  // unnamed non-inputs are noise
    }
    const std::string handle = "n" +
                               base::NumberToString(generation.generation) +
                               "-" + base::NumberToString(node.id);
    PageObservation::Element element;
    element.handle = handle;
    element.role = RoleName(node.role);
    element.name = name;
    element.enabled =
        node.GetRestriction() != ax::mojom::Restriction::kDisabled;
    element.focusable = node.HasState(ax::mojom::State::kFocusable);
    element.editable = RoleIsEditable(node.role);
    PageFieldSafetyDescriptor field;
    field.protected_state = node.IsPasswordField();
    field.input_type =
        node.GetStringAttribute(ax::mojom::StringAttribute::kInputType);
    field.autocomplete = node.GetHtmlAttribute("autocomplete");
    element.sensitivity = ClassifyPageField(field);
    element.agent_writable =
        element.editable && AllowsModelValueMutation(element.sensitivity);

    NodeBinding binding;
    binding.ax_node_id = node.id;
    binding.role = element.role;
    binding.editable = element.editable;
    binding.enabled = element.enabled;
    binding.sensitivity = element.sensitivity;
    generation.handles[handle] = std::move(binding);
    observation.elements.push_back(std::move(element));
  }

  std::move(callback).Run(std::move(observation));
}

PageActionStatus PageAgent::PerformAction(const LiveTabKey& tab,
                                          const PageActionRequest& request) {
  content::WebContents* contents = resolver_.Run(tab);
  if (!contents) {
    return PageActionStatus::kUnknownTab;
  }
  auto tab_it = tabs_.find(tab);
  if (tab_it == tabs_.end()) {
    return PageActionStatus::kExpiredHandle;
  }
  auto handle_it = tab_it->second.handles.find(request.handle);
  if (handle_it == tab_it->second.handles.end()) {
    // Either never captured, or expired by a newer observation/navigation.
    return PageActionStatus::kExpiredHandle;
  }
  const NodeBinding& binding = handle_it->second;

  if (IsValueMutation(request.kind) &&
      !AllowsModelValueMutation(binding.sensitivity)) {
    // Browser autofill and direct user input remain available. The model path
    // cannot supply, clear, or select a credential/payment/OTP value.
    return PageActionStatus::kSensitiveField;
  }

  if (IsValueMutation(request.kind) && !binding.editable) {
    return PageActionStatus::kNotActionable;
  }
  if (!binding.enabled) {
    return PageActionStatus::kNotActionable;
  }

  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  if (!frame) {
    return PageActionStatus::kElementGone;
  }

  ui::AXActionData action;
  action.target_node_id = binding.ax_node_id;
  switch (request.kind) {
    case PageActionKind::kClick:
    case PageActionKind::kToggle:
      action.action = ax::mojom::Action::kDoDefault;
      break;
    case PageActionKind::kType:
      action.action = ax::mojom::Action::kSetValue;
      action.value = request.value;
      break;
    case PageActionKind::kClear:
      action.action = ax::mojom::Action::kSetValue;
      action.value = std::string();
      break;
    case PageActionKind::kSelectOption:
      action.action = ax::mojom::Action::kSetValue;
      action.value = request.value;
      break;
    case PageActionKind::kScrollToElement:
      action.action = ax::mojom::Action::kScrollToMakeVisible;
      break;
    case PageActionKind::kFocus:
      action.action = ax::mojom::Action::kFocus;
      break;
  }
  frame->AccessibilityPerformAction(action);
  return PageActionStatus::kOk;
}

void PageAgent::InvalidateTab(const LiveTabKey& tab) {
  auto it = tabs_.find(tab);
  if (it != tabs_.end()) {
    // Bump the generation so any outstanding handle is now stale, but keep
    // the entry so the generation counter keeps climbing monotonically.
    ++it->second.generation;
    it->second.handles.clear();
  }
}


PageObservation::Element::Element() = default;
PageObservation::Element::Element(const Element&) = default;
PageObservation::Element::Element(Element&&) = default;
PageObservation::Element& PageObservation::Element::operator=(const Element&) = default;
PageObservation::Element& PageObservation::Element::operator=(Element&&) = default;
PageObservation::Element::~Element() = default;

PageAgent::TabGeneration::TabGeneration() = default;
PageAgent::TabGeneration::TabGeneration(const TabGeneration&) = default;
PageAgent::TabGeneration::TabGeneration(TabGeneration&&) = default;
PageAgent::TabGeneration& PageAgent::TabGeneration::operator=(const TabGeneration&) = default;
PageAgent::TabGeneration& PageAgent::TabGeneration::operator=(TabGeneration&&) = default;
PageAgent::TabGeneration::~TabGeneration() = default;

}  // namespace seoul
