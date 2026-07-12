// Project Seoul origin-scoped agent permissions.
//
// Discovery permissions decide which capabilities a planner can see. This
// service is the execution boundary: reusable grants are exact to capability,
// window, tab, main frame, source origin, destination origin, and connector
// service. A broader grant is never inferred from a narrower one.

#ifndef SEOUL_BROWSER_POLICY_AGENT_PERMISSION_SERVICE_H_
#define SEOUL_BROWSER_POLICY_AGENT_PERMISSION_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "seoul/browser/lifecycle/lifecycle_identity.h"
#include "seoul/browser/tools/tool_types.h"
#include "url/origin.h"

namespace seoul {

inline constexpr size_t kMaxAgentPermissionGrants = 1024;
inline constexpr size_t kMaxAgentFrameScopeLength = 80;
inline constexpr size_t kMaxAgentServiceScopeLength = 120;
inline constexpr base::TimeDelta kDefaultAgentGrantDuration = base::Minutes(30);
inline constexpr base::TimeDelta kMaxAgentGrantDuration = base::Hours(24);

struct AgentPermissionRequest {
  ToolId capability;
  ApprovalPolicy approval = ApprovalPolicy::kNeverRequired;
  RiskCategory risk = RiskCategory::kReadOnly;
  DataSensitivity sensitivity = DataSensitivity::kNone;
  LiveWindowKey window;
  LiveTabKey tab;
  // The page agent currently acts only through the primary main frame. This
  // explicit token prevents a future subframe implementation from inheriting
  // a main-frame grant accidentally.
  std::string frame_scope;
  url::Origin source_origin;
  url::Origin destination_origin;
  // Exact connector/provider service identity for non-page capabilities.
  std::string service_scope;

  friend bool operator==(const AgentPermissionRequest&,
                         const AgentPermissionRequest&) = default;
};

enum class AgentPermissionDecisionKind {
  kAllowed,
  kNeedsApproval,
  kDenied,
};

enum class AgentPermissionReason {
  kPolicyAllows,
  kExactGrant,
  kFirstUse,
  kAlwaysRequired,
  kHighRisk,
  kInvalidScope,
};

struct AgentPermissionDecision {
  AgentPermissionDecisionKind kind = AgentPermissionDecisionKind::kDenied;
  AgentPermissionReason reason = AgentPermissionReason::kInvalidScope;
};

class AgentPermissionService {
 public:
  using Clock = base::RepeatingCallback<base::Time()>;

  explicit AgentPermissionService(Clock clock);
  AgentPermissionService(const AgentPermissionService&) = delete;
  AgentPermissionService& operator=(const AgentPermissionService&) = delete;
  ~AgentPermissionService();

  AgentPermissionDecision Evaluate(const AgentPermissionRequest& request) const;

  // Records an exact, session-memory grant after explicit approval. Only
  // first-use/read or reversible requests are reusable; irreversible and
  // external-side-effect requests remain per-step approvals.
  bool GrantFirstUse(const AgentPermissionRequest& request,
                     base::TimeDelta duration = kDefaultAgentGrantDuration);

  void RevokeTab(const LiveTabKey& tab);
  void RevokeWindow(const LiveWindowKey& window);
  void RevokeAll();
  size_t grant_count() const;

  // User-facing, data-minimized approval copy. It names exact scope but never
  // includes paths, query strings, typed values, page text, or credentials.
  std::string Describe(const AgentPermissionRequest& request) const;

 private:
  struct Grant {
    AgentPermissionRequest request;
    base::Time expires_at;
  };

  bool IsValidRequest(const AgentPermissionRequest& request) const;
  bool ScopeMatches(const AgentPermissionRequest& request,
                    const Grant& grant) const;
  void PruneExpired();

  Clock clock_;
  std::vector<Grant> grants_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_POLICY_AGENT_PERMISSION_SERVICE_H_
