// Project Seoul product runtime - voice/task bridge.

#include "seoul/browser/product/voice_runtime_controller.h"

#include <tuple>
#include <utility>

namespace seoul {

namespace {

bool IsTerminalTaskState(TaskState state) {
  return state == TaskState::kCompleted || state == TaskState::kFailed ||
         state == TaskState::kCancelled;
}

}  // namespace

VoiceRuntimeSnapshot::VoiceRuntimeSnapshot() = default;
VoiceRuntimeSnapshot::VoiceRuntimeSnapshot(const VoiceRuntimeSnapshot&) =
    default;
VoiceRuntimeSnapshot::VoiceRuntimeSnapshot(VoiceRuntimeSnapshot&&) = default;
VoiceRuntimeSnapshot& VoiceRuntimeSnapshot::operator=(
    const VoiceRuntimeSnapshot&) = default;
VoiceRuntimeSnapshot& VoiceRuntimeSnapshot::operator=(VoiceRuntimeSnapshot&&) =
    default;
VoiceRuntimeSnapshot::~VoiceRuntimeSnapshot() = default;

VoiceRuntimeController::VoiceRuntimeController(
    TaskService* tasks,
    SpeechToTextProvider* speech_to_text,
    TextToSpeechProvider* text_to_speech,
    StartGoalCallback start_goal,
    base::RepeatingCallback<base::Time()> clock)
    : tasks_(tasks),
      speech_to_text_(speech_to_text),
      text_to_speech_(text_to_speech),
      start_goal_(std::move(start_goal)),
      session_(VoiceSettings(), speech_to_text, text_to_speech,
               std::move(clock)) {
  if (tasks_) {
    tasks_->AddObserver(this);
    observing_tasks_ = true;
  }
}

VoiceRuntimeController::~VoiceRuntimeController() {
  if (observing_tasks_ && tasks_) {
    tasks_->RemoveObserver(this);
  }
}

VoiceStatusResult VoiceRuntimeController::StartVoice(
    const LiveWindowKey& window) {
  if (!window.is_valid()) {
    return VoiceErr(VoiceError::kIllegalTransition);
  }
  if (!speech_to_text_) {
    return session_.RequestStart();
  }
  if (session_.state() == VoiceSessionState::kSpeaking) {
    if (auto barge = session_.BargeIn(); !barge.has_value()) {
      return barge;
    }
    if (auto resume = session_.ResumeListeningAfterInterrupt();
        !resume.has_value()) {
      return resume;
    }
  } else {
    if (auto start = session_.RequestStart(); !start.has_value()) {
      return start;
    }
  }

  active_window_ = window;

  SpeechToTextProvider::CaptureCallbacks callbacks;
  callbacks.on_partial = base::BindRepeating(
      &VoiceRuntimeController::OnPartialTranscript,
      weak_factory_.GetWeakPtr());
  callbacks.on_final = base::BindOnce(
      &VoiceRuntimeController::OnFinalTranscript, weak_factory_.GetWeakPtr());
  callbacks.on_silence = base::BindOnce(
      &VoiceRuntimeController::OnSilenceDetected, weak_factory_.GetWeakPtr());
  callbacks.on_error = base::BindOnce(&VoiceRuntimeController::OnProviderError,
                                      weak_factory_.GetWeakPtr());

  speech_to_text_->StartCapture(session_.settings(), std::move(callbacks));
  if (session_.state() == VoiceSessionState::kMicrophoneRequesting) {
    return session_.OnMicrophoneGranted();
  }
  return session_.state() == VoiceSessionState::kFailed
             ? VoiceErr(session_.last_error())
             : VoiceOk();
}

VoiceStatusResult VoiceRuntimeController::StopVoice() {
  return session_.StopCapture();
}

VoiceStatusResult VoiceRuntimeController::CancelVoice() {
  return session_.Cancel();
}

VoiceRuntimeSnapshot VoiceRuntimeController::Snapshot() const {
  VoiceRuntimeSnapshot snapshot;
  snapshot.state = session_.state();
  snapshot.last_error = session_.last_error();
  snapshot.route = session_.speech_route();
  snapshot.active_task_id = session_.active_task_id();
  snapshot.last_final_transcript = last_final_transcript_;
  snapshot.speech_provider_available = speech_to_text_ != nullptr;
  snapshot.speech_output_available = text_to_speech_ != nullptr;
  return snapshot;
}

void VoiceRuntimeController::OnTaskUpdated(const TaskId& task_id) {
  AdvanceFromTaskSnapshot(task_id);
}

void VoiceRuntimeController::OnTaskFinished(const TaskId& task_id) {
  AdvanceFromTaskSnapshot(task_id);
}

void VoiceRuntimeController::OnPartialTranscript(const std::string& text,
                                                 double confidence) {
  if (session_.state() == VoiceSessionState::kMicrophoneRequesting) {
    std::ignore = session_.OnMicrophoneGranted();
  }
  std::ignore = session_.OnPartialTranscript(text, confidence);
}

void VoiceRuntimeController::OnFinalTranscript(const std::string& text,
                                               double confidence) {
  if (session_.state() == VoiceSessionState::kMicrophoneRequesting) {
    std::ignore = session_.OnMicrophoneGranted();
  }
  if (auto accepted = session_.OnFinalTranscript(text, confidence);
      !accepted.has_value()) {
    return;
  }
  last_final_transcript_ = text;

  if (!active_window_.is_valid() || start_goal_.is_null()) {
    std::ignore = session_.Cancel();
    return;
  }

  const TaskId task_id = start_goal_.Run(text, active_window_);
  if (!task_id.is_valid()) {
    std::ignore = session_.Cancel();
    return;
  }
  session_.set_active_task_id(task_id.value());
  if (session_.state() == VoiceSessionState::kUnderstanding) {
    std::ignore = session_.OnUnderstandingComplete();
  }
  AdvanceFromTaskSnapshot(task_id);
}

void VoiceRuntimeController::OnSilenceDetected() {
  if (auto silenced = session_.OnSilenceDetected(); silenced.has_value() &&
      speech_to_text_) {
    speech_to_text_->StopCapture();
  }
}

void VoiceRuntimeController::OnProviderError(const std::string&) {
  if (session_.state() == VoiceSessionState::kMicrophoneRequesting) {
    std::ignore = session_.OnMicrophoneDenied();
    return;
  }
  std::ignore = session_.Cancel();
}

void VoiceRuntimeController::AdvanceFromTaskSnapshot(const TaskId& task_id) {
  if (!tasks_ || !task_id.is_valid() ||
      task_id.value() != session_.active_task_id()) {
    return;
  }
  std::optional<TaskSnapshot> snapshot = tasks_->Snapshot(task_id);
  if (!snapshot.has_value()) {
    return;
  }

  if (session_.state() == VoiceSessionState::kUnderstanding) {
    std::ignore = session_.OnUnderstandingComplete();
  }
  if (session_.state() == VoiceSessionState::kPlanning &&
      snapshot->state != TaskState::kPlanning &&
      snapshot->state != TaskState::kDraft) {
    std::ignore = session_.OnPlanReady(snapshot->state == TaskState::kAwaitingApproval);
  }
  if (snapshot->state == TaskState::kAwaitingApproval) {
    return;
  }
  if (snapshot->state == TaskState::kCompleted &&
      session_.state() == VoiceSessionState::kExecuting) {
    std::ignore = session_.OnExecutionFinished(SpokenSummaryFor(*snapshot));
    return;
  }
  if (IsTerminalTaskState(snapshot->state) &&
      session_.state() != VoiceSessionState::kIdle &&
      session_.state() != VoiceSessionState::kCancelled &&
      session_.state() != VoiceSessionState::kFailed) {
    std::ignore = session_.Cancel();
  }
}

std::string VoiceRuntimeController::SpokenSummaryFor(
    const TaskSnapshot& snapshot) const {
  for (auto it = snapshot.receipts.rbegin(); it != snapshot.receipts.rend();
       ++it) {
    if (!it->observed_summary.empty()) {
      return it->observed_summary;
    }
  }
  return std::string();
}

}  // namespace seoul
