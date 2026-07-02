// Project Seoul voice operating layer - Apple platform adapter.
// A real on-device speech-to-text path backed by the Speech framework
// (SFSpeechRecognizer). It implements the provider-neutral SpeechToTextProvider
// seam so the voice session machine is unchanged. Microphone capture requires
// an explicit permission grant and is never always-on; no raw audio is
// persisted (the adapter holds only the live recognition request/task and
// forwards transcripts). This is macOS platform glue (Objective-C++) and
// compiles only on a capable host with the Speech and AVFoundation frameworks.

#ifndef SEOUL_BROWSER_VOICE_PLATFORM_APPLE_SPEECH_RECOGNIZER_H_
#define SEOUL_BROWSER_VOICE_PLATFORM_APPLE_SPEECH_RECOGNIZER_H_

#include <memory>
#include <string>

#include "seoul/browser/voice/speech_providers.h"

namespace seoul {

// Opaque implementation holder so the Objective-C++ types (SFSpeechRecognizer,
// AVAudioEngine) do not leak into C++ translation units.
class AppleSpeechRecognizer : public SpeechToTextProvider {
 public:
  AppleSpeechRecognizer();
  AppleSpeechRecognizer(const AppleSpeechRecognizer&) = delete;
  AppleSpeechRecognizer& operator=(const AppleSpeechRecognizer&) = delete;
  ~AppleSpeechRecognizer() override;

  std::string provider_name() const override;
  SpeechRoute route() const override;  // kLocal: on-device recognition
  void StartCapture(const VoiceSettings& settings,
                    CaptureCallbacks callbacks) override;
  void StopCapture() override;
  void CancelCapture() override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_VOICE_PLATFORM_APPLE_SPEECH_RECOGNIZER_H_
