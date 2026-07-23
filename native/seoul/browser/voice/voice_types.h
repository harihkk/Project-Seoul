// Project Seoul voice operating layer.
// Session states, settings, and transcript records. The voice layer stores
// transcripts only: there is no audio buffer type anywhere in this module, so
// raw audio cannot be persisted by construction. Whether speech recognition
// runs locally or in the cloud is always explicit and user-visible.

#ifndef SEOUL_BROWSER_VOICE_VOICE_TYPES_H_
#define SEOUL_BROWSER_VOICE_VOICE_TYPES_H_

#include <string>

#include "base/time/time.h"
#include "base/types/expected.h"

namespace seoul {

// Explicit voice session states (one active input turn plus the response).
enum class VoiceSessionState {
  kIdle,
  kMicrophoneRequesting,
  kListening,
  kPartialTranscript,
  kFinalizingTranscript,
  kUnderstanding,
  kPlanning,
  kAwaitingConfirmation,
  kExecuting,
  kSpeaking,
  kInterrupted,
  kPaused,
  kCancelled,
  kFailed,
};

const char* VoiceSessionStateToString(VoiceSessionState state);

enum class VoiceInputMode {
  kPushToTalk,          // capture while held; manual stop finalizes
  kToggleConversation,  // explicit on/off; returns to listening after speech
};

// Where speech processing runs. Surfaced in the shell and Task Deck.
enum class SpeechRoute {
  kLocal,
  kCloud,
};

enum class VoiceError {
  kIllegalTransition,
  kMicrophoneDenied,
  kEmptyTranscript,
  kTranscriptTooLong,
  kProviderUnavailable,
  kMalformedProviderOutput,
  kTooManyReferents,
  kAmbiguousReference,
  kUnknownReference,
};

const char* VoiceErrorToString(VoiceError error);

template <typename T>
using VoiceResult = base::expected<T, VoiceError>;

using VoiceStatusResult = base::expected<void, VoiceError>;

inline VoiceStatusResult VoiceOk() {
  return base::ok();
}

inline base::unexpected<VoiceError> VoiceErr(VoiceError error) {
  return base::unexpected(error);
}

inline constexpr size_t kMaxTranscriptLength = 8192;
inline constexpr size_t kMaxTranscriptSegments = 200;
inline constexpr size_t kMaxReferencePhraseLength = 512;
inline constexpr size_t kMaxVisibleReferents = 256;

struct TranscriptSegment {
  TranscriptSegment();
  TranscriptSegment(const TranscriptSegment&);
  TranscriptSegment(TranscriptSegment&&);
  TranscriptSegment& operator=(const TranscriptSegment&);
  TranscriptSegment& operator=(TranscriptSegment&&);
  ~TranscriptSegment();

  std::string text;
  bool is_final = false;
  double confidence = 0.0;  // provider-reported, [0, 1]; 0 when unknown
  SpeechRoute route = SpeechRoute::kLocal;
  base::Time captured_at;

  friend bool operator==(const TranscriptSegment&,
                         const TranscriptSegment&) = default;
};

struct VoiceSettings {
  VoiceSettings();
  VoiceSettings(const VoiceSettings&);
  VoiceSettings(VoiceSettings&&);
  VoiceSettings& operator=(const VoiceSettings&);
  VoiceSettings& operator=(VoiceSettings&&);
  ~VoiceSettings();

  VoiceInputMode input_mode = VoiceInputMode::kPushToTalk;
  std::string input_device_id;  // empty selects the system default
  base::TimeDelta silence_timeout = base::Seconds(2);
  // Raw audio is never persisted by default. There is deliberately no code
  // path in this module that stores audio even when a future setting enables
  // provider-side retention.
  bool persist_raw_audio = false;
  bool allow_cloud_speech = false;  // local transcription is the default
  bool speech_output_enabled = true;
  std::string voice_id;  // empty selects the platform default voice
  double speech_rate = 1.0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_VOICE_VOICE_TYPES_H_
