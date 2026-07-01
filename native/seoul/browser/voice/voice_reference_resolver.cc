// Project Seoul voice operating layer.

#include "seoul/browser/voice/voice_reference_resolver.h"

#include <algorithm>
#include <string_view>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace seoul {

namespace {

struct OrdinalWord {
  std::string_view word;
  int ordinal;
};

constexpr OrdinalWord kOrdinalWords[] = {
    {"first", 1}, {"1st", 1},    {"second", 2},  {"2nd", 2},    {"third", 3},
    {"3rd", 3},   {"fourth", 4}, {"4th", 4},     {"fifth", 5},  {"5th", 5},
    {"sixth", 6}, {"6th", 6},    {"seventh", 7}, {"7th", 7},    {"eighth", 8},
    {"8th", 8},   {"ninth", 9},  {"9th", 9},     {"tenth", 10}, {"10th", 10},
};

bool IsDeicticWord(std::string_view token) {
  return token == "this" || token == "that" || token == "it";
}

// "results" matches kind "result"; exact match also accepted.
bool TokenNamesKind(std::string_view token, std::string_view kind) {
  if (token == kind) {
    return true;
  }
  return token.size() == kind.size() + 1 && token.back() == 's' &&
         token.substr(0, kind.size()) == kind;
}

}  // namespace

VoiceResult<std::string> ResolveVoiceReference(
    const std::string& phrase,
    const std::vector<VisibleReferent>& referents) {
  if (phrase.size() > kMaxReferencePhraseLength) {
    return base::unexpected(VoiceError::kTranscriptTooLong);
  }
  if (referents.size() > kMaxVisibleReferents) {
    return base::unexpected(VoiceError::kTooManyReferents);
  }
  if (referents.empty()) {
    return base::unexpected(VoiceError::kUnknownReference);
  }

  const std::string lowered = base::ToLowerASCII(phrase);
  const std::vector<std::string> tokens = base::SplitString(
      lowered, " ,.?!'\"", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty()) {
    return base::unexpected(VoiceError::kUnknownReference);
  }

  // 1. Deictic reference ("this", "that", "it"): the selected item, else the
  // focused item, else the only item.
  const bool deictic = std::any_of(
      tokens.begin(), tokens.end(),
      [](const std::string& token) { return IsDeicticWord(token); });

  // 2. Ordinal reference ("the second result", "first chart", "the last
  // one"), optionally scoped by a kind word.
  int ordinal = 0;
  bool wants_last = false;
  std::string kind_scope;
  for (const std::string& token : tokens) {
    for (const OrdinalWord& word : kOrdinalWords) {
      if (token == word.word) {
        ordinal = word.ordinal;
      }
    }
    if (token == "last") {
      wants_last = true;
    }
    for (const VisibleReferent& referent : referents) {
      if (!referent.kind.empty() && TokenNamesKind(token, referent.kind)) {
        kind_scope = referent.kind;
      }
    }
  }

  if (ordinal > 0 || wants_last) {
    std::vector<const VisibleReferent*> scoped;
    for (const VisibleReferent& referent : referents) {
      if (kind_scope.empty() || referent.kind == kind_scope) {
        scoped.push_back(&referent);
      }
    }
    if (scoped.empty()) {
      return base::unexpected(VoiceError::kUnknownReference);
    }
    std::sort(scoped.begin(), scoped.end(),
              [](const VisibleReferent* a, const VisibleReferent* b) {
                return a->ordinal < b->ordinal;
              });
    if (wants_last) {
      return scoped.back()->id;
    }
    for (const VisibleReferent* referent : scoped) {
      if (referent->ordinal == ordinal) {
        return referent->id;
      }
    }
    return base::unexpected(VoiceError::kUnknownReference);
  }

  // 3. Label match: exact label wins, then unique substring containment.
  std::vector<const VisibleReferent*> exact;
  std::vector<const VisibleReferent*> partial;
  for (const VisibleReferent& referent : referents) {
    if (referent.label.empty()) {
      continue;
    }
    const std::string label = base::ToLowerASCII(referent.label);
    if (label == lowered) {
      exact.push_back(&referent);
      continue;
    }
    if (lowered.find(label) != std::string::npos ||
        label.find(lowered) != std::string::npos) {
      partial.push_back(&referent);
    }
  }
  if (exact.size() == 1) {
    return exact.front()->id;
  }
  if (exact.size() > 1) {
    return base::unexpected(VoiceError::kAmbiguousReference);
  }
  if (partial.size() == 1) {
    return partial.front()->id;
  }
  if (partial.size() > 1) {
    return base::unexpected(VoiceError::kAmbiguousReference);
  }

  if (deictic) {
    for (const VisibleReferent& referent : referents) {
      if (referent.selected) {
        return referent.id;
      }
    }
    for (const VisibleReferent& referent : referents) {
      if (referent.focused) {
        return referent.id;
      }
    }
    if (referents.size() == 1) {
      return referents.front().id;
    }
    return base::unexpected(VoiceError::kAmbiguousReference);
  }

  return base::unexpected(VoiceError::kUnknownReference);
}

}  // namespace seoul
