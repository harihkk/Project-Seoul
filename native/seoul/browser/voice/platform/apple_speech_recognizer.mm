// Project Seoul voice operating layer - Apple platform adapter.
//
// Objective-C++ implementation over the Speech framework. On-device
// recognition is requested (requiresOnDeviceRecognition) so audio stays local
// by default; microphone input is tapped only between StartCapture and
// Stop/Cancel, and no audio buffer is retained. Compiles only on a capable
// macOS host (Speech + AVFoundation).

#import "seoul/browser/voice/platform/apple_speech_recognizer.h"

#import <AVFoundation/AVFoundation.h>
#import <Speech/Speech.h>

#include <utility>

namespace seoul {

class AppleSpeechRecognizer::Impl {
 public:
  Impl() { recognizer_ = [[SFSpeechRecognizer alloc] init]; }

  void Start(const VoiceSettings& settings, CaptureCallbacks callbacks) {
    callbacks_ = std::move(callbacks);
    // Only proceed once the user has granted speech recognition authorization.
    if ([SFSpeechRecognizer authorizationStatus] !=
        SFSpeechRecognizerAuthorizationStatusAuthorized) {
      if (callbacks_.on_error) {
        std::move(callbacks_.on_error).Run("Speech recognition not authorized.");
      }
      return;
    }
    request_ = [[SFSpeechAudioBufferRecognitionRequest alloc] init];
    request_.shouldReportPartialResults = YES;
    // Keep audio on device when the model supports it.
    if (@available(macOS 13.0, *)) {
      request_.requiresOnDeviceRecognition =
          recognizer_.supportsOnDeviceRecognition;
    }

    engine_ = [[AVAudioEngine alloc] init];
    AVAudioInputNode* input = engine_.inputNode;
    AVAudioFormat* format = [input outputFormatForBus:0];
    __block CaptureCallbacks* cb = &callbacks_;
    [input installTapOnBus:0
                bufferSize:1024
                    format:format
                     block:^(AVAudioPCMBuffer* buffer, AVAudioTime* when) {
                       // Audio buffers are appended to the recognition request
                       // and then dropped; none is stored by Seoul.
                       [request_ appendAudioPCMBuffer:buffer];
                     }];
    [engine_ prepare];
    NSError* start_error = nil;
    if (![engine_ startAndReturnError:&start_error]) {
      if (cb->on_error) {
        std::move(cb->on_error)
            .Run(std::string("Audio engine failed to start: ") +
                 start_error.localizedDescription.UTF8String);
      }
      return;
    }

    task_ = [recognizer_
        recognitionTaskWithRequest:request_
                     resultHandler:^(SFSpeechRecognitionResult* result,
                                     NSError* error) {
                       if (error) {
                         if (cb->on_error) {
                           std::move(cb->on_error)
                               .Run(error.localizedDescription.UTF8String);
                         }
                         return;
                       }
                       if (!result) return;
                       const std::string text =
                           result.bestTranscription.formattedString.UTF8String;
                       if (result.isFinal) {
                         if (cb->on_final) {
                           std::move(cb->on_final).Run(text, /*confidence=*/0.0);
                         }
                       } else if (cb->on_partial) {
                         cb->on_partial.Run(text, /*confidence=*/0.0);
                       }
                     }];
  }

  void Stop() {
    [engine_.inputNode removeTapOnBus:0];
    [engine_ stop];
    [request_ endAudio];
    // The result handler delivers the final transcript; the task is not
    // cancelled here so a pending final result can arrive.
  }

  void Cancel() {
    [engine_.inputNode removeTapOnBus:0];
    [engine_ stop];
    [task_ cancel];
    task_ = nil;
    request_ = nil;
    callbacks_ = CaptureCallbacks();
  }

 private:
  SFSpeechRecognizer* recognizer_ = nil;
  SFSpeechAudioBufferRecognitionRequest* request_ = nil;
  SFSpeechRecognitionTask* task_ = nil;
  AVAudioEngine* engine_ = nil;
  CaptureCallbacks callbacks_;
};

AppleSpeechRecognizer::AppleSpeechRecognizer()
    : impl_(std::make_unique<Impl>()) {}
AppleSpeechRecognizer::~AppleSpeechRecognizer() = default;

std::string AppleSpeechRecognizer::provider_name() const {
  return "apple_speech";
}

SpeechRoute AppleSpeechRecognizer::route() const {
  return SpeechRoute::kLocal;
}

void AppleSpeechRecognizer::StartCapture(const VoiceSettings& settings,
                                         CaptureCallbacks callbacks) {
  impl_->Start(settings, std::move(callbacks));
}

void AppleSpeechRecognizer::StopCapture() {
  impl_->Stop();
}

void AppleSpeechRecognizer::CancelCapture() {
  impl_->Cancel();
}

}  // namespace seoul
