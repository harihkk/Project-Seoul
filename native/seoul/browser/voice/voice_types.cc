// Project Seoul voice operating layer.
// Out-of-line special members for the voice value types. These structs hold
// non-trivial members (std::string), so their constructors and destructors are
// defined here rather than being implicit or inline.

#include "seoul/browser/voice/voice_types.h"

namespace seoul {

TranscriptSegment::TranscriptSegment() = default;
TranscriptSegment::TranscriptSegment(const TranscriptSegment&) = default;
TranscriptSegment::TranscriptSegment(TranscriptSegment&&) = default;
TranscriptSegment& TranscriptSegment::operator=(const TranscriptSegment&) =
    default;
TranscriptSegment& TranscriptSegment::operator=(TranscriptSegment&&) = default;
TranscriptSegment::~TranscriptSegment() = default;

VoiceSettings::VoiceSettings() = default;
VoiceSettings::VoiceSettings(const VoiceSettings&) = default;
VoiceSettings::VoiceSettings(VoiceSettings&&) = default;
VoiceSettings& VoiceSettings::operator=(const VoiceSettings&) = default;
VoiceSettings& VoiceSettings::operator=(VoiceSettings&&) = default;
VoiceSettings::~VoiceSettings() = default;

}  // namespace seoul
