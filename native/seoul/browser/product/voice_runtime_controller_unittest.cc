// Project Seoul product runtime: voice provider -> task bridge tests.
//
// These tests prove the product seam, not speech accuracy: fake providers emit
// deterministic transcripts, and the controller must submit only final text
// into the same StartGoal path as typed Canvas turns.

#include "seoul/browser/product/voice_runtime_controller.h"

#include <string>

#include "base/functional/bind.h"
#include "seoul/browser/voice/fake_speech_providers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

base::Time TestNow() {
  return base::Time::UnixEpoch();
}

LiveWindowKey TestWindow() {
  return LiveWindowKey::FromSessionId(42);
}

TaskId RecordGoal(std::string* recorded_goal,
                  LiveWindowKey* recorded_window,
                  TaskId task_id,
                  const std::string& goal,
                  const LiveWindowKey& window) {
  *recorded_goal = goal;
  *recorded_window = window;
  return task_id;
}

}  // namespace

TEST(VoiceRuntimeControllerTest, StartsCaptureAndSubmitsFinalTranscript) {
  FakeSpeechToTextProvider speech;
  std::string submitted_goal;
  LiveWindowKey submitted_window;
  const TaskId returned_task = TaskId::GenerateNew();

  VoiceRuntimeController controller(
      /*tasks=*/nullptr, &speech, /*text_to_speech=*/nullptr,
      base::BindRepeating(&RecordGoal, base::Unretained(&submitted_goal),
                          base::Unretained(&submitted_window), returned_task),
      base::BindRepeating(&TestNow));

  ASSERT_TRUE(controller.StartVoice(TestWindow()).has_value());
  EXPECT_TRUE(speech.capturing());
  EXPECT_EQ(controller.Snapshot().state, VoiceSessionState::kListening);

  speech.EmitPartial("open the", 0.4);
  EXPECT_EQ(controller.Snapshot().state, VoiceSessionState::kPartialTranscript);

  ASSERT_TRUE(controller.StopVoice().has_value());
  EXPECT_EQ(speech.stop_count(), 1);
  EXPECT_EQ(controller.Snapshot().state,
            VoiceSessionState::kFinalizingTranscript);

  speech.EmitFinal("open the release checklist", 0.9);
  EXPECT_EQ(submitted_goal, "open the release checklist");
  EXPECT_EQ(submitted_window, TestWindow());
  EXPECT_EQ(controller.Snapshot().active_task_id, returned_task.value());
  EXPECT_EQ(controller.Snapshot().last_final_transcript,
            "open the release checklist");
  EXPECT_EQ(controller.Snapshot().state, VoiceSessionState::kPlanning);
}

TEST(VoiceRuntimeControllerTest, MissingProviderFailsVisibly) {
  VoiceRuntimeController controller(
      /*tasks=*/nullptr, /*speech_to_text=*/nullptr,
      /*text_to_speech=*/nullptr, VoiceRuntimeController::StartGoalCallback(),
      base::BindRepeating(&TestNow));

  const VoiceStatusResult result = controller.StartVoice(TestWindow());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), VoiceError::kProviderUnavailable);
  EXPECT_FALSE(controller.Snapshot().speech_provider_available);
  EXPECT_EQ(controller.Snapshot().state, VoiceSessionState::kFailed);
  EXPECT_EQ(controller.Snapshot().last_error, VoiceError::kProviderUnavailable);
}

TEST(VoiceRuntimeControllerTest, SilenceRequestsFinalTranscript) {
  FakeSpeechToTextProvider speech;
  VoiceRuntimeController controller(
      /*tasks=*/nullptr, &speech, /*text_to_speech=*/nullptr,
      VoiceRuntimeController::StartGoalCallback(),
      base::BindRepeating(&TestNow));

  ASSERT_TRUE(controller.StartVoice(TestWindow()).has_value());
  speech.EmitSilence();

  EXPECT_EQ(speech.stop_count(), 1);
  EXPECT_EQ(controller.Snapshot().state,
            VoiceSessionState::kFinalizingTranscript);
}

TEST(VoiceRuntimeControllerTest, InvalidWindowDoesNotStartCapture) {
  FakeSpeechToTextProvider speech;
  VoiceRuntimeController controller(
      /*tasks=*/nullptr, &speech, /*text_to_speech=*/nullptr,
      VoiceRuntimeController::StartGoalCallback(),
      base::BindRepeating(&TestNow));

  const VoiceStatusResult result = controller.StartVoice(LiveWindowKey());

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), VoiceError::kIllegalTransition);
  EXPECT_FALSE(speech.capturing());
  EXPECT_EQ(controller.Snapshot().state, VoiceSessionState::kIdle);
}

}  // namespace seoul
