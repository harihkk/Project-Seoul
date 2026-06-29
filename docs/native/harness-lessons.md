# Harness lessons for the native implementation

The Manifest V3 browser-control harness (`apps/browser-harness/`) is frozen. It is
a protocol and safety reference only and receives no new features. These are the
lessons it surfaced, to carry into the native Seoul subsystems (see
`../product/native-architecture.md`). They are design guidance, not changes to the
harness.

## 1. One safety-policy implementation, not several

The harness ended up expressing the same safety rules (sensitive-field refusal,
navigation/URL allow-list, snapshot binding) in more than one place (background and
content). Native Seoul must have a single authoritative permission/risk-policy
module that all action paths consult. Do not duplicate the policy across the UI,
the action executor, and the page-observation layer; duplicated policy drifts and
creates gaps.

## 2. Bounded retention for session/task records

The harness stores control-session records and timelines in `chrome.storage.local`.
Native session/task records must have explicit, bounded retention: a maximum record
count and/or age, deterministic eviction, and never unbounded growth. Persist only
non-sensitive metadata (state, kind, timestamps) and never page content, typed text,
input values, page HTML, or secrets.

## 3. Distinguish transport failure from action failure

The harness learned (the hard way, via forced worker loss) that "the message port
closed" is a transport failure and is not the same as "the action failed" or "the
action succeeded." Native Seoul must keep these distinct: a dropped channel, a dead
worker, or a timeout is transport-unknown; only an explicit result from the page is
action success/failure. Mapping transport failures onto action failures causes
false negatives and unsafe retries.

## 4. Never replay an action with an unknown outcome

When an action was in flight and the outcome cannot be confirmed (worker
terminated mid-execution), the harness records `ACTION_OUTCOME_UNKNOWN` and never
replays. Native Seoul must adopt the same rule: an action whose outcome is unknown
is reconciled to a stopped/safe state and is never automatically retried. Replaying
an unknown-outcome action can double-submit forms, payments, or messages.

## Cross-references

- Permission and risk policy, credential mediation, and task state/recovery
  subsystems: `../product/native-architecture.md`.
- Permissions/approvals, credential isolation, action audit trail, and
  pause/resume/no-replay product behavior: `../product/feature-matrix.md`.
