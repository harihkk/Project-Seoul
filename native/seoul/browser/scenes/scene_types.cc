// Project Seoul Scenes.

#include "seoul/browser/scenes/scene_types.h"

namespace seoul {

SceneAssistantDefaults::SceneAssistantDefaults() = default;
SceneAssistantDefaults::SceneAssistantDefaults(const SceneAssistantDefaults&) =
    default;
SceneAssistantDefaults::SceneAssistantDefaults(SceneAssistantDefaults&&) =
    default;
SceneAssistantDefaults& SceneAssistantDefaults::operator=(
    const SceneAssistantDefaults&) = default;
SceneAssistantDefaults& SceneAssistantDefaults::operator=(
    SceneAssistantDefaults&&) = default;
SceneAssistantDefaults::~SceneAssistantDefaults() = default;

SceneDefinition::SceneDefinition() = default;
SceneDefinition::SceneDefinition(const SceneDefinition&) = default;
SceneDefinition::SceneDefinition(SceneDefinition&&) = default;
SceneDefinition& SceneDefinition::operator=(const SceneDefinition&) = default;
SceneDefinition& SceneDefinition::operator=(SceneDefinition&&) = default;
SceneDefinition::~SceneDefinition() = default;

}  // namespace seoul
