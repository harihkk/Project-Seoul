// Project Seoul voice operating layer.
// Provider seams for speech-to-text and text-to-speech. Implementations adapt
// one engine (platform speech framework, an audited local model runtime, or an
// official cloud API) to these interfaces. Providers deliver transcripts and
// speech events only; raw audio never crosses this boundary into Seoul state.

#ifndef SEOUL_BROWSER_VOICE_SPEECH_PROVIDERS_H_
#define SEOUL_BROWSER_VOICE_SPEECH_PROVIDERS_H_

#include <string>

#include "base/functional/callback.h"
#include "seoul/browser/voice/voice_types.h"

namespace seoul {

class SpeechToTextProvider {
 public:
  virtual ~SpeechToTextProvider() = default;

  virtual std::string provider_name() const = 0;
  virtual SpeechRoute route() const = 0;

  struct CaptureCallbacks {
    // Fired repeatedly with the evolving hypothesis for the current utterance.
    base::RepeatingCallback<void(const std::string& text, double confidence)>
        on_partial;
    // Fired once per utterance with the final transcript.
    base::OnceCallback<void(const std::string& text, double confidence)>
        on_final;
    // Provider-detected end of speech (silence).
    base::OnceCallback<void()> on_silence;
    base::OnceCallback<void(const std::string& message)> on_error;
  };

  virtual void StartCapture(const VoiceSettings& settings,
                            CaptureCallbacks callbacks) = 0;
  // Stops capture and requests the final transcript for what was heard.
  virtual void StopCapture() = 0;
  // Abandons the utterance; no final transcript follows.
  virtual void CancelCapture() = 0;
};

class TextToSpeechProvider {
 public:
  virtual ~TextToSpeechProvider() = default;

  virtual std::string provider_name() const = 0;
  virtual SpeechRoute route() const = 0;

  struct SpeakCallbacks {
    base::OnceCallback<void()> on_started;
    base::OnceCallback<void()> on_finished;
    base::OnceCallback<void(const std::string& message)> on_error;
  };

  virtual void Speak(const std::string& text,
                     const VoiceSettings& settings,
                     SpeakCallbacks callbacks) = 0;
  // Hard stop for barge-in: output must cease immediately, not at a phrase
  // boundary. No on_finished follows a stop.
  virtual void StopImmediately() = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_VOICE_SPEECH_PROVIDERS_H_
