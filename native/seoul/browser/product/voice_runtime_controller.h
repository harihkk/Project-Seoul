// Project Seoul product runtime - voice/task bridge.
//
// Owns the product-level voice turn: starts the speech provider, drives the
// VoiceSession state machine, and submits final transcripts into TaskService
// through the same StartGoal path as typed Canvas turns. This is deliberately
// provider-neutral; platform audio capture lives behind SpeechToTextProvider.

#ifndef SEOUL_BROWSER_PRODUCT_VOICE_RUNTIME_CONTROLLER_H_
#define SEOUL_BROWSER_PRODUCT_VOICE_RUNTIME_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "seoul/browser/lifecycle/lifecycle_identity.h"
#include "seoul/browser/product/task_service.h"
#include "seoul/browser/tasks/task_types.h"
#include "seoul/browser/voice/speech_providers.h"
#include "seoul/browser/voice/voice_session.h"
#include "seoul/browser/voice/voice_types.h"

namespace seoul {

struct VoiceRuntimeSnapshot {
  VoiceSessionState state = VoiceSessionState::kIdle;
  VoiceError last_error = VoiceError::kIllegalTransition;
  SpeechRoute route = SpeechRoute::kLocal;
  std::string active_task_id;
  std::string last_final_transcript;
  bool speech_provider_available = false;
  bool speech_output_available = false;
};

class VoiceRuntimeController : public TaskServiceObserver {
 public:
  using StartGoalCallback =
      base::RepeatingCallback<TaskId(const std::string& goal,
                                     const LiveWindowKey& window)>;

  VoiceRuntimeController(TaskService* tasks,
                         SpeechToTextProvider* speech_to_text,
                         TextToSpeechProvider* text_to_speech,
                         StartGoalCallback start_goal,
                         base::RepeatingCallback<base::Time()> clock);
  VoiceRuntimeController(const VoiceRuntimeController&) = delete;
  VoiceRuntimeController& operator=(const VoiceRuntimeController&) = delete;
  ~VoiceRuntimeController() override;

  VoiceStatusResult StartVoice(const LiveWindowKey& window);
  VoiceStatusResult StopVoice();
  VoiceStatusResult CancelVoice();

  VoiceRuntimeSnapshot Snapshot() const;

  // TaskServiceObserver:
  void OnTaskUpdated(const TaskId& task_id) override;
  void OnTaskFinished(const TaskId& task_id) override;

  VoiceSession* session_for_testing() { return &session_; }

 private:
  void OnPartialTranscript(const std::string& text, double confidence);
  void OnFinalTranscript(const std::string& text, double confidence);
  void OnSilenceDetected();
  void OnProviderError(const std::string& message);
  void AdvanceFromTaskSnapshot(const TaskId& task_id);
  std::string SpokenSummaryFor(const TaskSnapshot& snapshot) const;

  raw_ptr<TaskService> tasks_;
  raw_ptr<SpeechToTextProvider> speech_to_text_;
  raw_ptr<TextToSpeechProvider> text_to_speech_;
  StartGoalCallback start_goal_;
  VoiceSession session_;
  LiveWindowKey active_window_;
  std::string last_final_transcript_;
  bool observing_tasks_ = false;
  base::WeakPtrFactory<VoiceRuntimeController> weak_factory_{this};
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_VOICE_RUNTIME_CONTROLLER_H_
