// Project Seoul voice operating layer.
// Deterministic fake speech providers for tests. Tests drive scripted
// transcript sequences (the unit-level stand-in for recorded audio fixtures;
// real audio round trips run on the capable host) and observe hard-stop
// behavior for barge-in. Test support only; never linked into production.

#ifndef SEOUL_BROWSER_VOICE_FAKE_SPEECH_PROVIDERS_H_
#define SEOUL_BROWSER_VOICE_FAKE_SPEECH_PROVIDERS_H_

#include <string>

#include "seoul/browser/voice/speech_providers.h"

namespace seoul {

class FakeSpeechToTextProvider : public SpeechToTextProvider {
 public:
  explicit FakeSpeechToTextProvider(SpeechRoute route = SpeechRoute::kLocal);
  ~FakeSpeechToTextProvider() override;

  std::string provider_name() const override;
  SpeechRoute route() const override;
  void StartCapture(const VoiceSettings& settings,
                    CaptureCallbacks callbacks) override;
  void StopCapture() override;
  void CancelCapture() override;

  // Test drivers.
  void EmitPartial(const std::string& text, double confidence);
  void EmitFinal(const std::string& text, double confidence);
  void EmitSilence();
  void EmitError(const std::string& message);

  bool capturing() const { return capturing_; }
  int stop_count() const { return stop_count_; }
  int cancel_count() const { return cancel_count_; }

 private:
  SpeechRoute route_;
  bool capturing_ = false;
  int stop_count_ = 0;
  int cancel_count_ = 0;
  CaptureCallbacks callbacks_;
};

class FakeTextToSpeechProvider : public TextToSpeechProvider {
 public:
  FakeTextToSpeechProvider();
  ~FakeTextToSpeechProvider() override;

  std::string provider_name() const override;
  SpeechRoute route() const override;
  void Speak(const std::string& text,
             const VoiceSettings& settings,
             SpeakCallbacks callbacks) override;
  void StopImmediately() override;

  bool speaking() const { return speaking_; }
  int immediate_stop_count() const { return immediate_stop_count_; }
  const std::string& last_spoken_text() const { return last_spoken_text_; }

 private:
  bool speaking_ = false;
  int immediate_stop_count_ = 0;
  std::string last_spoken_text_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_VOICE_FAKE_SPEECH_PROVIDERS_H_
