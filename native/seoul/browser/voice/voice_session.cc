// Project Seoul voice operating layer.

#include "seoul/browser/voice/voice_session.h"

#include <utility>

namespace seoul {

VoiceSession::VoiceSession(const VoiceSettings& settings,
                           SpeechToTextProvider* speech_to_text,
                           TextToSpeechProvider* text_to_speech,
                           base::RepeatingCallback<base::Time()> clock)
    : settings_(settings),
      speech_to_text_(speech_to_text),
      text_to_speech_(text_to_speech),
      clock_(std::move(clock)) {}

VoiceSession::~VoiceSession() {
  if (state_ == VoiceSessionState::kSpeaking && text_to_speech_) {
    text_to_speech_->StopImmediately();
  }
  observers_.Clear();
}

void VoiceSession::AddObserver(VoiceSessionObserver* observer) {
  observers_.AddObserver(observer);
}

void VoiceSession::RemoveObserver(VoiceSessionObserver* observer) {
  observers_.RemoveObserver(observer);
}

SpeechRoute VoiceSession::speech_route() const {
  return speech_to_text_ ? speech_to_text_->route() : SpeechRoute::kLocal;
}

void VoiceSession::SetState(VoiceSessionState next) {
  if (next == state_) {
    return;
  }
  const VoiceSessionState old_state = state_;
  state_ = next;
  for (VoiceSessionObserver& observer : observers_) {
    observer.OnVoiceSessionStateChanged(old_state, next);
  }
}

bool VoiceSession::IsTerminal(VoiceSessionState state) const {
  return state == VoiceSessionState::kCancelled ||
         state == VoiceSessionState::kFailed;
}

void VoiceSession::FailWith(VoiceError error) {
  last_error_ = error;
  SetState(VoiceSessionState::kFailed);
}

VoiceStatusResult VoiceSession::AppendSegment(const std::string& text,
                                              bool is_final,
                                              double confidence) {
  if (text.size() > kMaxTranscriptLength) {
    return VoiceErr(VoiceError::kTranscriptTooLong);
  }
  TranscriptSegment segment;
  segment.text = text;
  segment.is_final = is_final;
  segment.confidence = confidence;
  segment.route = speech_route();
  segment.captured_at = clock_.Run();
  // A partial hypothesis replaces the previous partial for the same
  // utterance; finals are appended.
  if (!is_final && !transcript_.empty() && !transcript_.back().is_final) {
    transcript_.back() = segment;
  } else {
    transcript_.push_back(segment);
    if (transcript_.size() > kMaxTranscriptSegments) {
      transcript_.erase(transcript_.begin());
    }
  }
  for (VoiceSessionObserver& observer : observers_) {
    observer.OnTranscriptUpdated(transcript_.back());
  }
  return VoiceOk();
}

VoiceStatusResult VoiceSession::RequestStart() {
  if (state_ != VoiceSessionState::kIdle) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  if (!speech_to_text_) {
    FailWith(VoiceError::kProviderUnavailable);
    return VoiceErr(VoiceError::kProviderUnavailable);
  }
  SetState(VoiceSessionState::kMicrophoneRequesting);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnMicrophoneGranted() {
  if (state_ != VoiceSessionState::kMicrophoneRequesting) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  SetState(VoiceSessionState::kListening);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnMicrophoneDenied() {
  if (state_ != VoiceSessionState::kMicrophoneRequesting) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  FailWith(VoiceError::kMicrophoneDenied);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnPartialTranscript(const std::string& text,
                                                    double confidence) {
  if (state_ != VoiceSessionState::kListening &&
      state_ != VoiceSessionState::kPartialTranscript) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  if (auto result = AppendSegment(text, /*is_final=*/false, confidence);
      !result.has_value()) {
    return result;
  }
  SetState(VoiceSessionState::kPartialTranscript);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnSilenceDetected() {
  if (state_ != VoiceSessionState::kListening &&
      state_ != VoiceSessionState::kPartialTranscript) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  SetState(VoiceSessionState::kFinalizingTranscript);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::StopCapture() {
  if (state_ != VoiceSessionState::kListening &&
      state_ != VoiceSessionState::kPartialTranscript) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  if (speech_to_text_) {
    speech_to_text_->StopCapture();
  }
  SetState(VoiceSessionState::kFinalizingTranscript);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnFinalTranscript(const std::string& text,
                                                  double confidence) {
  if (state_ != VoiceSessionState::kFinalizingTranscript &&
      state_ != VoiceSessionState::kListening &&
      state_ != VoiceSessionState::kPartialTranscript) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  if (text.empty()) {
    // A provider returning an empty final transcript is malformed output; the
    // session fails visibly instead of acting on nothing.
    FailWith(VoiceError::kMalformedProviderOutput);
    return VoiceErr(VoiceError::kMalformedProviderOutput);
  }
  if (auto result = AppendSegment(text, /*is_final=*/true, confidence);
      !result.has_value()) {
    FailWith(result.error());
    return result;
  }
  SetState(VoiceSessionState::kUnderstanding);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::SubmitTypedInput(const std::string& text) {
  if (state_ != VoiceSessionState::kIdle &&
      state_ != VoiceSessionState::kListening &&
      state_ != VoiceSessionState::kInterrupted) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  if (text.empty()) {
    return VoiceErr(VoiceError::kEmptyTranscript);
  }
  if (auto result = AppendSegment(text, /*is_final=*/true,
                                  /*confidence=*/1.0);
      !result.has_value()) {
    return result;
  }
  SetState(VoiceSessionState::kUnderstanding);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnUnderstandingComplete() {
  if (state_ != VoiceSessionState::kUnderstanding) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  SetState(VoiceSessionState::kPlanning);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnPlanReady(bool requires_confirmation) {
  if (state_ != VoiceSessionState::kPlanning) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  SetState(requires_confirmation ? VoiceSessionState::kAwaitingConfirmation
                                 : VoiceSessionState::kExecuting);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnConfirmation(bool approved) {
  if (state_ != VoiceSessionState::kAwaitingConfirmation) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  SetState(approved ? VoiceSessionState::kExecuting : VoiceSessionState::kIdle);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnExecutionFinished(
    const std::string& spoken_summary) {
  if (state_ != VoiceSessionState::kExecuting) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  if (!spoken_summary.empty() && settings_.speech_output_enabled &&
      text_to_speech_) {
    SetState(VoiceSessionState::kSpeaking);
    TextToSpeechProvider::SpeakCallbacks callbacks;
    text_to_speech_->Speak(spoken_summary, settings_, std::move(callbacks));
    return VoiceOk();
  }
  SetState(settings_.input_mode == VoiceInputMode::kToggleConversation
               ? VoiceSessionState::kListening
               : VoiceSessionState::kIdle);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::OnSpeechFinished() {
  if (state_ != VoiceSessionState::kSpeaking) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  // Conversation mode keeps listening for the follow-up; push-to-talk ends
  // the turn.
  SetState(settings_.input_mode == VoiceInputMode::kToggleConversation
               ? VoiceSessionState::kListening
               : VoiceSessionState::kIdle);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::BargeIn() {
  if (state_ != VoiceSessionState::kSpeaking) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  // Stop output NOW; never at a phrase boundary. Task state and already
  // verified work are untouched (the session does not own the task).
  if (text_to_speech_) {
    text_to_speech_->StopImmediately();
  }
  SetState(VoiceSessionState::kInterrupted);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::ResumeListeningAfterInterrupt() {
  if (state_ != VoiceSessionState::kInterrupted) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  SetState(VoiceSessionState::kListening);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::Pause() {
  if (state_ != VoiceSessionState::kListening &&
      state_ != VoiceSessionState::kPartialTranscript) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  if (speech_to_text_) {
    speech_to_text_->CancelCapture();
  }
  SetState(VoiceSessionState::kPaused);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::Resume() {
  if (state_ != VoiceSessionState::kPaused) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  SetState(VoiceSessionState::kListening);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::Cancel() {
  if (IsTerminal(state_) || state_ == VoiceSessionState::kIdle) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  if (state_ == VoiceSessionState::kSpeaking && text_to_speech_) {
    text_to_speech_->StopImmediately();
  }
  if ((state_ == VoiceSessionState::kListening ||
       state_ == VoiceSessionState::kPartialTranscript ||
       state_ == VoiceSessionState::kFinalizingTranscript) &&
      speech_to_text_) {
    speech_to_text_->CancelCapture();
  }
  SetState(VoiceSessionState::kCancelled);
  return VoiceOk();
}

VoiceStatusResult VoiceSession::ResetToIdle() {
  if (!IsTerminal(state_) && state_ != VoiceSessionState::kIdle) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  SetState(VoiceSessionState::kIdle);
  return VoiceOk();
}

const char* VoiceSessionStateToString(VoiceSessionState state) {
  switch (state) {
    case VoiceSessionState::kIdle:
      return "idle";
    case VoiceSessionState::kMicrophoneRequesting:
      return "microphone_requesting";
    case VoiceSessionState::kListening:
      return "listening";
    case VoiceSessionState::kPartialTranscript:
      return "partial_transcript";
    case VoiceSessionState::kFinalizingTranscript:
      return "finalizing_transcript";
    case VoiceSessionState::kUnderstanding:
      return "understanding";
    case VoiceSessionState::kPlanning:
      return "planning";
    case VoiceSessionState::kAwaitingConfirmation:
      return "awaiting_confirmation";
    case VoiceSessionState::kExecuting:
      return "executing";
    case VoiceSessionState::kSpeaking:
      return "speaking";
    case VoiceSessionState::kInterrupted:
      return "interrupted";
    case VoiceSessionState::kPaused:
      return "paused";
    case VoiceSessionState::kCancelled:
      return "cancelled";
    case VoiceSessionState::kFailed:
      return "failed";
  }
  return "idle";
}

const char* VoiceErrorToString(VoiceError error) {
  switch (error) {
    case VoiceError::kIllegalTransition:
      return "illegal_transition";
    case VoiceError::kMicrophoneDenied:
      return "microphone_denied";
    case VoiceError::kEmptyTranscript:
      return "empty_transcript";
    case VoiceError::kTranscriptTooLong:
      return "transcript_too_long";
    case VoiceError::kProviderUnavailable:
      return "provider_unavailable";
    case VoiceError::kMalformedProviderOutput:
      return "malformed_provider_output";
    case VoiceError::kTooManyReferents:
      return "too_many_referents";
    case VoiceError::kAmbiguousReference:
      return "ambiguous_reference";
    case VoiceError::kUnknownReference:
      return "unknown_reference";
  }
  return "illegal_transition";
}

}  // namespace seoul
