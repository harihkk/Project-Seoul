// Project Seoul voice operating layer.
// The voice session state machine. Voice, typed text, and Canvas interaction
// share one session: every transition is validated against an explicit table,
// illegal transitions are rejected without changing state, and barge-in stops
// speech output immediately while preserving the active task. The session
// stores bounded transcript segments only; it has no audio storage.

#ifndef SEOUL_BROWSER_VOICE_VOICE_SESSION_H_
#define SEOUL_BROWSER_VOICE_VOICE_SESSION_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "seoul/browser/voice/speech_providers.h"
#include "seoul/browser/voice/voice_types.h"

namespace seoul {

class VoiceSessionObserver : public base::CheckedObserver {
 public:
  virtual void OnVoiceSessionStateChanged(VoiceSessionState old_state,
                                          VoiceSessionState new_state) = 0;
  virtual void OnTranscriptUpdated(const TranscriptSegment& segment) {}
};

class VoiceSession {
 public:
  // Providers outlive the session (owned by the voice service). Either may be
  // null: a null STT provider fails RequestStart with kProviderUnavailable; a
  // null TTS provider skips the speaking state.
  VoiceSession(const VoiceSettings& settings,
               SpeechToTextProvider* speech_to_text,
               TextToSpeechProvider* text_to_speech,
               base::RepeatingCallback<base::Time()> clock);
  VoiceSession(const VoiceSession&) = delete;
  VoiceSession& operator=(const VoiceSession&) = delete;
  ~VoiceSession();

  void AddObserver(VoiceSessionObserver* observer);
  void RemoveObserver(VoiceSessionObserver* observer);

  VoiceSessionState state() const { return state_; }
  const VoiceSettings& settings() const { return settings_; }
  // The route of the transcription provider in use, shown next to the
  // microphone state.
  SpeechRoute speech_route() const;
  const std::vector<TranscriptSegment>& transcript() const {
    return transcript_;
  }
  // The task this session is driving, if any. Barge-in and interruption never
  // clear it; only the task layer completes or cancels a task.
  const std::string& active_task_id() const { return active_task_id_; }
  void set_active_task_id(const std::string& id) { active_task_id_ = id; }
  VoiceError last_error() const { return last_error_; }

  // Turn lifecycle. Each returns kIllegalTransition (state unchanged) when
  // invoked in a state that does not accept it.
  VoiceStatusResult RequestStart();
  VoiceStatusResult OnMicrophoneGranted();
  VoiceStatusResult OnMicrophoneDenied();
  VoiceStatusResult OnPartialTranscript(const std::string& text,
                                        double confidence);
  VoiceStatusResult OnSilenceDetected();
  VoiceStatusResult StopCapture();  // manual stop (push-to-talk release)
  VoiceStatusResult OnFinalTranscript(const std::string& text,
                                      double confidence);
  // Typed input joins the same thread: valid whenever a new turn could start
  // by voice; enters kUnderstanding directly with an is_final segment.
  VoiceStatusResult SubmitTypedInput(const std::string& text);
  VoiceStatusResult OnUnderstandingComplete();
  VoiceStatusResult OnPlanReady(bool requires_confirmation);
  VoiceStatusResult OnConfirmation(bool approved);
  VoiceStatusResult OnExecutionFinished(const std::string& spoken_summary);
  VoiceStatusResult OnSpeechFinished();
  // Barge-in: user speaks or explicitly interrupts while Seoul is speaking.
  // Speech output stops immediately; verified work and the active task are
  // preserved; the session is ready to capture the interruption.
  VoiceStatusResult BargeIn();
  VoiceStatusResult ResumeListeningAfterInterrupt();
  VoiceStatusResult Pause();
  VoiceStatusResult Resume();
  VoiceStatusResult Cancel();
  // Returns a terminal session (cancelled/failed) or an idle one to idle.
  VoiceStatusResult ResetToIdle();

 private:
  void SetState(VoiceSessionState next);
  VoiceStatusResult AppendSegment(const std::string& text,
                                  bool is_final,
                                  double confidence);
  bool IsTerminal(VoiceSessionState state) const;
  void FailWith(VoiceError error);

  VoiceSettings settings_;
  raw_ptr<SpeechToTextProvider> speech_to_text_;
  raw_ptr<TextToSpeechProvider> text_to_speech_;
  base::RepeatingCallback<base::Time()> clock_;
  VoiceSessionState state_ = VoiceSessionState::kIdle;
  std::vector<TranscriptSegment> transcript_;
  std::string active_task_id_;
  VoiceError last_error_ = VoiceError::kIllegalTransition;
  base::ObserverList<VoiceSessionObserver> observers_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_VOICE_VOICE_SESSION_H_
