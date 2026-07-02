// Project Seoul voice operating layer - Apple platform adapter.
// A real text-to-speech path backed by AVSpeechSynthesizer, implementing the
// provider-neutral TextToSpeechProvider seam. StopImmediately halts output at
// once (word boundary), which the voice session uses for barge-in. macOS
// platform glue (Objective-C++); compiles only on a capable host with
// AVFoundation.

#ifndef SEOUL_BROWSER_VOICE_PLATFORM_APPLE_TTS_ENGINE_H_
#define SEOUL_BROWSER_VOICE_PLATFORM_APPLE_TTS_ENGINE_H_

#include <memory>
#include <string>

#include "seoul/browser/voice/speech_providers.h"

namespace seoul {

class AppleTtsEngine : public TextToSpeechProvider {
 public:
  AppleTtsEngine();
  AppleTtsEngine(const AppleTtsEngine&) = delete;
  AppleTtsEngine& operator=(const AppleTtsEngine&) = delete;
  ~AppleTtsEngine() override;

  std::string provider_name() const override;
  SpeechRoute route() const override;
  void Speak(const std::string& text,
             const VoiceSettings& settings,
             SpeakCallbacks callbacks) override;
  void StopImmediately() override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_VOICE_PLATFORM_APPLE_TTS_ENGINE_H_
