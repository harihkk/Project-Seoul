// Project Seoul Preview lifecycle types.

#include "seoul/browser/preview/preview_types.h"

namespace seoul {

PreviewId PreviewId::GenerateNew() {
  PreviewId id;
  id.uuid_ = base::Uuid::GenerateRandomV4();
  return id;
}

PreviewId PreviewId::FromString(std::string_view value) {
  PreviewId id;
  id.uuid_ = base::Uuid::ParseLowercase(value);
  return id;
}

std::string PreviewId::value() const {
  return is_valid() ? uuid_.AsLowercaseString() : std::string();
}

const char* PreviewErrorToString(PreviewError error) {
  switch (error) {
    case PreviewError::kInvalidId:
      return "invalid_id";
    case PreviewError::kInvalidParent:
      return "invalid_parent";
    case PreviewError::kUnsafeUrl:
      return "unsafe_url";
    case PreviewError::kUnknownPreview:
      return "unknown_preview";
    case PreviewError::kInvalidState:
      return "invalid_state";
    case PreviewError::kLimitExceeded:
      return "limit_exceeded";
    case PreviewError::kNavigationLimitExceeded:
      return "navigation_limit_exceeded";
  }
  return "invalid_state";
}

}  // namespace seoul
