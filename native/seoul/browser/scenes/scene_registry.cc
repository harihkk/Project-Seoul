// Project Seoul Scenes.

#include "seoul/browser/scenes/scene_registry.h"

#include <set>
#include <utility>

namespace seoul {

namespace {

bool ValidSlug(const std::string& slug, size_t max_length) {
  if (slug.empty() || slug.size() > max_length) {
    return false;
  }
  if (slug[0] < 'a' || slug[0] > 'z') {
    return false;
  }
  for (char c : slug) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-')) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool HasDuplicates(const std::vector<T>& values) {
  std::set<T> seen;
  for (const T& value : values) {
    if (!seen.insert(value).second) {
      return true;
    }
  }
  return false;
}

}  // namespace

SceneRegistry::SceneRegistry(SceneResolvers resolvers)
    : resolvers_(std::move(resolvers)) {}

SceneRegistry::~SceneRegistry() = default;

SceneStatusResult SceneRegistry::Validate(const SceneDefinition& scene) const {
  if (scene.schema_version != kSceneSchemaVersion) {
    return base::unexpected(SceneError::kUnsupportedSchema);
  }
  if (!ValidSlug(scene.id, kMaxSceneNameLength)) {
    return base::unexpected(SceneError::kInvalidId);
  }
  if (scene.name.empty() || scene.name.size() > kMaxSceneNameLength) {
    return base::unexpected(SceneError::kInvalidName);
  }
  if (scene.workspace_id.empty()) {
    return base::unexpected(SceneError::kMissingWorkspace);
  }
  if (scene.site_layer_ids.size() > kMaxSceneSiteLayers ||
      scene.workflow_shortcut_ids.size() > kMaxSceneWorkflowShortcuts ||
      scene.assistant.default_connectors.size() > kMaxSceneContextTools) {
    return base::unexpected(SceneError::kTooManySceneItems);
  }
  if (HasDuplicates(scene.site_layer_ids) ||
      HasDuplicates(scene.routing_rule_ids) ||
      HasDuplicates(scene.workflow_shortcut_ids)) {
    return base::unexpected(SceneError::kDuplicateReference);
  }
  if (scene.lifecycle.idle_archive_minutes < 1 ||
      scene.lifecycle.idle_archive_minutes > 10080) {
    return base::unexpected(SceneError::kInvalidLifecyclePolicy);
  }
  if (resolvers_.workspace_exists &&
      !resolvers_.workspace_exists.Run(scene.workspace_id)) {
    return base::unexpected(SceneError::kUnknownWorkspace);
  }
  if (!scene.theme_id.empty() && resolvers_.theme_exists &&
      !resolvers_.theme_exists.Run(scene.theme_id)) {
    return base::unexpected(SceneError::kUnknownTheme);
  }
  if (resolvers_.site_layer_exists) {
    for (const std::string& layer_id : scene.site_layer_ids) {
      if (!resolvers_.site_layer_exists.Run(layer_id)) {
        return base::unexpected(SceneError::kUnknownSiteLayer);
      }
    }
  }
  return base::ok();
}

SceneStatusResult SceneRegistry::Upsert(SceneDefinition scene) {
  if (auto valid = Validate(scene); !valid.has_value()) {
    return valid;
  }
  const bool exists = scenes_.find(scene.id) != scenes_.end();
  if (!exists && scenes_.size() >= kMaxScenes) {
    return base::unexpected(SceneError::kLimitExceeded);
  }
  const std::string id = scene.id;
  scenes_[id] = std::move(scene);
  return base::ok();
}

SceneStatusResult SceneRegistry::Remove(const std::string& scene_id) {
  if (scenes_.erase(scene_id) == 0) {
    return base::unexpected(SceneError::kUnknownScene);
  }
  return base::ok();
}

const SceneDefinition* SceneRegistry::Find(const std::string& scene_id) const {
  auto it = scenes_.find(scene_id);
  return it == scenes_.end() ? nullptr : &it->second;
}

std::vector<const SceneDefinition*> SceneRegistry::List() const {
  std::vector<const SceneDefinition*> result;
  for (const auto& [id, scene] : scenes_) {
    result.push_back(&scene);
  }
  return result;
}

SceneResult<std::vector<SceneActivationStep>>
SceneRegistry::BuildActivationPlan(const std::string& scene_id) const {
  const SceneDefinition* scene = Find(scene_id);
  if (!scene) {
    return base::unexpected(SceneError::kUnknownScene);
  }
  // Re-validate at activation time: referenced entities may have been removed
  // since the Scene was stored.
  if (auto valid = Validate(*scene); !valid.has_value()) {
    return base::unexpected(valid.error());
  }
  std::vector<SceneActivationStep> plan;
  plan.push_back(
      {SceneActivationStep::Kind::kSwitchWorkspace, scene->workspace_id});
  if (!scene->theme_id.empty()) {
    plan.push_back({SceneActivationStep::Kind::kApplyTheme, scene->theme_id});
  }
  for (const std::string& layer_id : scene->site_layer_ids) {
    plan.push_back({SceneActivationStep::Kind::kEnableSiteLayer, layer_id});
  }
  for (const std::string& rule_id : scene->routing_rule_ids) {
    plan.push_back({SceneActivationStep::Kind::kInstallRoutingRule, rule_id});
  }
  plan.push_back(
      {SceneActivationStep::Kind::kApplyLifecyclePolicy, std::string()});
  plan.push_back(
      {SceneActivationStep::Kind::kApplyAssistantDefaults, std::string()});
  plan.push_back(
      {SceneActivationStep::Kind::kSetCompactPreference, std::string()});
  return plan;
}

const char* SceneErrorToString(SceneError error) {
  switch (error) {
    case SceneError::kInvalidName:
      return "invalid_name";
    case SceneError::kInvalidId:
      return "invalid_id";
    case SceneError::kMissingWorkspace:
      return "missing_workspace";
    case SceneError::kTooManySceneItems:
      return "too_many_scene_items";
    case SceneError::kDuplicateReference:
      return "duplicate_reference";
    case SceneError::kUnknownScene:
      return "unknown_scene";
    case SceneError::kUnknownWorkspace:
      return "unknown_workspace";
    case SceneError::kUnknownTheme:
      return "unknown_theme";
    case SceneError::kUnknownSiteLayer:
      return "unknown_site_layer";
    case SceneError::kInvalidLifecyclePolicy:
      return "invalid_lifecycle_policy";
    case SceneError::kUnsupportedSchema:
      return "unsupported_schema";
    case SceneError::kLimitExceeded:
      return "limit_exceeded";
  }
  return "unknown_scene";
}

}  // namespace seoul
