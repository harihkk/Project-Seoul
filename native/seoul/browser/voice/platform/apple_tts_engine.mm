// Project Seoul voice operating layer - Apple platform adapter.
//
// Objective-C++ implementation over AVSpeechSynthesizer. StopImmediately uses
// AVSpeechBoundaryImmediate so barge-in halts output at once. Compiles only on
// a capable macOS host (AVFoundation).

#import "seoul/browser/voice/platform/apple_tts_engine.h"

#import <AVFoundation/AVFoundation.h>

#include <utility>

namespace seoul {

// A delegate that bridges AVSpeechSynthesizer callbacks to SpeakCallbacks.
}  // namespace seoul

@interface SeoulTtsDelegate : NSObject <AVSpeechSynthesizerDelegate>
@end

@implementation SeoulTtsDelegate {
 @public
  seoul::TextToSpeechProvider::SpeakCallbacks _callbacks;
}
- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    didStartSpeechUtterance:(AVSpeechUtterance*)utterance {
  if (_callbacks.on_started) {
    std::move(_callbacks.on_started).Run();
  }
}
- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    didFinishSpeechUtterance:(AVSpeechUtterance*)utterance {
  if (_callbacks.on_finished) {
    std::move(_callbacks.on_finished).Run();
  }
}
@end

namespace seoul {

class AppleTtsEngine::Impl {
 public:
  Impl() {
    synthesizer_ = [[AVSpeechSynthesizer alloc] init];
    delegate_ = [[SeoulTtsDelegate alloc] init];
    synthesizer_.delegate = delegate_;
  }

  void Speak(const std::string& text,
             const VoiceSettings& settings,
             SpeakCallbacks callbacks) {
    delegate_->_callbacks = std::move(callbacks);
    AVSpeechUtterance* utterance = [AVSpeechUtterance
        speechUtteranceWithString:[NSString stringWithUTF8String:text.c_str()]];
    utterance.rate =
        AVSpeechUtteranceDefaultSpeechRate * static_cast<float>(settings.speech_rate);
    if (!settings.voice_id.empty()) {
      AVSpeechSynthesisVoice* voice = [AVSpeechSynthesisVoice
          voiceWithIdentifier:[NSString stringWithUTF8String:settings.voice_id.c_str()]];
      if (voice) utterance.voice = voice;
    }
    [synthesizer_ speakUtterance:utterance];
  }

  void StopImmediately() {
    // Barge-in: halt at once, not at the next utterance boundary.
    [synthesizer_ stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
  }

 private:
  AVSpeechSynthesizer* synthesizer_ = nil;
  SeoulTtsDelegate* delegate_ = nil;
};

AppleTtsEngine::AppleTtsEngine() : impl_(std::make_unique<Impl>()) {}
AppleTtsEngine::~AppleTtsEngine() = default;

std::string AppleTtsEngine::provider_name() const {
  return "apple_tts";
}

SpeechRoute AppleTtsEngine::route() const {
  return SpeechRoute::kLocal;
}

void AppleTtsEngine::Speak(const std::string& text,
                           const VoiceSettings& settings,
                           SpeakCallbacks callbacks) {
  impl_->Speak(text, settings, std::move(callbacks));
}

void AppleTtsEngine::StopImmediately() {
  impl_->StopImmediately();
}

}  // namespace seoul
