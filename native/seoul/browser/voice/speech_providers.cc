// Project Seoul voice.

#include "seoul/browser/voice/speech_providers.h"

namespace seoul {

SpeechToTextProvider::CaptureCallbacks::CaptureCallbacks() = default;
SpeechToTextProvider::CaptureCallbacks::CaptureCallbacks(CaptureCallbacks&&) =
    default;
SpeechToTextProvider::CaptureCallbacks&
SpeechToTextProvider::CaptureCallbacks::operator=(CaptureCallbacks&&) = default;
SpeechToTextProvider::CaptureCallbacks::~CaptureCallbacks() = default;

TextToSpeechProvider::SpeakCallbacks::SpeakCallbacks() = default;
TextToSpeechProvider::SpeakCallbacks::SpeakCallbacks(SpeakCallbacks&&) = default;
TextToSpeechProvider::SpeakCallbacks&
TextToSpeechProvider::SpeakCallbacks::operator=(SpeakCallbacks&&) = default;
TextToSpeechProvider::SpeakCallbacks::~SpeakCallbacks() = default;

}  // namespace seoul
