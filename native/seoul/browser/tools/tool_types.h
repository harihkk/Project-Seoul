// Project Seoul general-purpose operator: tool layer.
// Typed tool identities and descriptors. Every action Seoul can take is a
// registered tool with a stable id, typed input/output schemas, declared risk
// and sensitivity, an approval policy, and an observation contract. A planner
// receives only registered permitted tools and can never invent one.

#ifndef SEOUL_BROWSER_TOOLS_TOOL_TYPES_H_
#define SEOUL_BROWSER_TOOLS_TOOL_TYPES_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "seoul/browser/tools/tool_schema.h"

namespace seoul {

enum class ToolError {
  kInvalidId,
  kDuplicateId,
  kUnknownTool,
  kInvalidDescriptor,
  kReservedNamespace,
  kProviderMismatch,
  kLimitExceeded,
};

const char* ToolErrorToString(ToolError error);

template <typename T>
using ToolResult = base::expected<T, ToolError>;

using ToolStatusResult = base::expected<void, ToolError>;

inline constexpr size_t kMaxRegisteredTools = 1024;
inline constexpr size_t kMaxToolIdLength = 128;
inline constexpr size_t kMaxCapabilityTags = 16;

// Namespaced dotted identifier, for example "browser.tabs.open" or
// "connector.calendar.list_events". Segments are [a-z][a-z0-9_]*; two to four
// segments.
class ToolId {
 public:
  ToolId() = default;
  static ToolId FromString(std::string_view value);
  static bool IsValidString(std::string_view value);

  bool is_valid() const { return !value_.empty(); }
  const std::string& value() const { return value_; }
  // The first segment ("browser", "connector", ...).
  std::string root_namespace() const;

  friend bool operator==(const ToolId& a, const ToolId& b) {
    return a.value_ == b.value_;
  }
  friend bool operator<(const ToolId& a, const ToolId& b) {
    return a.value_ < b.value_;
  }

 private:
  std::string value_;
};

// How sensitive the data a tool touches is. Permission contexts cap this.
enum class DataSensitivity {
  kNone,                // no user data (arithmetic, formatting)
  kOrganization,        // workspace/tab metadata
  kPageContent,         // visible page content
  kPersonal,            // user documents, calendar, mail
  kCredentialAdjacent,  // touches authenticated sessions or submissions
};

enum class RiskCategory {
  kReadOnly,
  kReversibleMutation,    // undoable browser/organization change
  kIrreversibleMutation,  // hard to reverse (send, submit, delete)
  kExternalSideEffect,    // leaves the browser (email sent, order placed)
};

enum class ApprovalPolicy {
  kNeverRequired,
  kFirstUsePerScope,  // ask once per (tool, origin/service) pair
  kAlwaysRequired,
};

enum class IdempotencyClass {
  kIdempotent,
  kNotIdempotent,
};

struct ToolDescriptor {
  ToolId id;
  std::string name;         // short human name
  std::string description;  // planner-facing behavior contract
  std::string provider;     // "seoul" for builtins; connector id otherwise
  ToolSchema input_schema;
  ToolSchema output_schema;
  std::vector<std::string> capability_tags;
  bool requires_network = false;
  DataSensitivity sensitivity = DataSensitivity::kNone;
  RiskCategory risk = RiskCategory::kReadOnly;
  ApprovalPolicy approval = ApprovalPolicy::kNeverRequired;
  base::TimeDelta timeout = base::Seconds(30);
  bool cancellable = true;
  IdempotencyClass idempotency = IdempotencyClass::kNotIdempotent;
  // What observable state change proves the tool ran (consumed by the
  // execution layer's observe-verify step).
  std::string observation_contract;
  // Registered verifier used to confirm the outcome; empty means the output
  // schema itself is the only check.
  std::string verifier_id;
  // Optional SAUI component wire name best suited to render results.
  std::string preferred_component;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_TOOLS_TOOL_TYPES_H_
