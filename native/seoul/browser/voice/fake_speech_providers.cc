// Project Seoul voice operating layer.

#include "seoul/browser/voice/fake_speech_providers.h"

#include <utility>

namespace seoul {

FakeSpeechToTextProvider::FakeSpeechToTextProvider(SpeechRoute route)
    : route_(route) {}

FakeSpeechToTextProvider::~FakeSpeechToTextProvider() = default;

std::string FakeSpeechToTextProvider::provider_name() const {
  return "fake_stt";
}

SpeechRoute FakeSpeechToTextProvider::route() const {
  return route_;
}

void FakeSpeechToTextProvider::StartCapture(const VoiceSettings& settings,
                                            CaptureCallbacks callbacks) {
  capturing_ = true;
  callbacks_ = std::move(callbacks);
}

void FakeSpeechToTextProvider::StopCapture() {
  capturing_ = false;
  ++stop_count_;
}

void FakeSpeechToTextProvider::CancelCapture() {
  capturing_ = false;
  ++cancel_count_;
  callbacks_ = CaptureCallbacks();
}

void FakeSpeechToTextProvider::EmitPartial(const std::string& text,
                                           double confidence) {
  if (callbacks_.on_partial) {
    callbacks_.on_partial.Run(text, confidence);
  }
}

void FakeSpeechToTextProvider::EmitFinal(const std::string& text,
                                         double confidence) {
  if (callbacks_.on_final) {
    std::move(callbacks_.on_final).Run(text, confidence);
  }
}

void FakeSpeechToTextProvider::EmitSilence() {
  if (callbacks_.on_silence) {
    std::move(callbacks_.on_silence).Run();
  }
}

void FakeSpeechToTextProvider::EmitError(const std::string& message) {
  if (callbacks_.on_error) {
    std::move(callbacks_.on_error).Run(message);
  }
}

FakeTextToSpeechProvider::FakeTextToSpeechProvider() = default;

FakeTextToSpeechProvider::~FakeTextToSpeechProvider() = default;

std::string FakeTextToSpeechProvider::provider_name() const {
  return "fake_tts";
}

SpeechRoute FakeTextToSpeechProvider::route() const {
  return SpeechRoute::kLocal;
}

void FakeTextToSpeechProvider::Speak(const std::string& text,
                                     const VoiceSettings& settings,
                                     SpeakCallbacks callbacks) {
  speaking_ = true;
  last_spoken_text_ = text;
  if (callbacks.on_started) {
    std::move(callbacks.on_started).Run();
  }
}

void FakeTextToSpeechProvider::StopImmediately() {
  speaking_ = false;
  ++immediate_stop_count_;
}

}  // namespace seoul
