// Project Seoul voice operating layer.
// Unit tests for the voice session state machine: push-to-talk, conversation
// mode, partial/final transcripts, silence, cancellation, barge-in, illegal
// transitions, provider misbehavior, and the no-audio-persistence default.

#include "seoul/browser/voice/voice_session.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "seoul/browser/voice/fake_speech_providers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class RecordingObserver : public VoiceSessionObserver {
 public:
  void OnVoiceSessionStateChanged(VoiceSessionState old_state,
                                  VoiceSessionState new_state) override {
    states.push_back(new_state);
  }
  void OnTranscriptUpdated(const TranscriptSegment& segment) override {
    segments.push_back(segment);
  }
  std::vector<VoiceSessionState> states;
  std::vector<TranscriptSegment> segments;
};

class VoiceSessionTest : public testing::Test {
 protected:
  VoiceSessionTest() { Rebuild(VoiceSettings()); }

  void Rebuild(const VoiceSettings& settings) {
    session_ = std::make_unique<VoiceSession>(
        settings, &stt_, &tts_,
        base::BindLambdaForTesting([this]() { return clock_; }));
    session_->AddObserver(&observer_);
  }

  // Drives idle -> executing through a full understood turn.
  void DriveToExecuting(bool requires_confirmation = false) {
    ASSERT_TRUE(session_->RequestStart().has_value());
    ASSERT_TRUE(session_->OnMicrophoneGranted().has_value());
    ASSERT_TRUE(session_->OnPartialTranscript("show the", 0.5).has_value());
    ASSERT_TRUE(
        session_->OnPartialTranscript("show the weather", 0.7).has_value());
    ASSERT_TRUE(session_->OnSilenceDetected().has_value());
    ASSERT_TRUE(
        session_->OnFinalTranscript("show the weather", 0.9).has_value());
    ASSERT_TRUE(session_->OnUnderstandingComplete().has_value());
    ASSERT_TRUE(session_->OnPlanReady(requires_confirmation).has_value());
    if (requires_confirmation) {
      ASSERT_EQ(session_->state(), VoiceSessionState::kAwaitingConfirmation);
      ASSERT_TRUE(session_->OnConfirmation(true).has_value());
    }
    ASSERT_EQ(session_->state(), VoiceSessionState::kExecuting);
  }

  base::Time clock_ = base::Time::UnixEpoch() + base::Days(20000);
  FakeSpeechToTextProvider stt_;
  FakeTextToSpeechProvider tts_;
  RecordingObserver observer_;
  std::unique_ptr<VoiceSession> session_;
};

TEST_F(VoiceSessionTest, DefaultsAreSafe) {
  const VoiceSettings defaults;
  EXPECT_FALSE(defaults.persist_raw_audio);
  EXPECT_FALSE(defaults.allow_cloud_speech);
  EXPECT_EQ(defaults.input_mode, VoiceInputMode::kPushToTalk);
  EXPECT_EQ(session_->speech_route(), SpeechRoute::kLocal);
}

TEST_F(VoiceSessionTest, PushToTalkTurnEndsIdle) {
  DriveToExecuting();
  ASSERT_TRUE(session_->OnExecutionFinished("It is 21 degrees.").has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kSpeaking);
  EXPECT_TRUE(tts_.speaking());
  EXPECT_EQ(tts_.last_spoken_text(), "It is 21 degrees.");
  ASSERT_TRUE(session_->OnSpeechFinished().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kIdle);
}

TEST_F(VoiceSessionTest, ConversationModeReturnsToListening) {
  VoiceSettings settings;
  settings.input_mode = VoiceInputMode::kToggleConversation;
  Rebuild(settings);
  DriveToExecuting();
  ASSERT_TRUE(session_->OnExecutionFinished("Done.").has_value());
  ASSERT_TRUE(session_->OnSpeechFinished().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kListening);
}

TEST_F(VoiceSessionTest, PartialReplacesPartialAndFinalAppends) {
  ASSERT_TRUE(session_->RequestStart().has_value());
  ASSERT_TRUE(session_->OnMicrophoneGranted().has_value());
  ASSERT_TRUE(session_->OnPartialTranscript("compare", 0.4).has_value());
  ASSERT_TRUE(session_->OnPartialTranscript("compare apple", 0.6).has_value());
  ASSERT_EQ(session_->transcript().size(), 1u);
  EXPECT_EQ(session_->transcript()[0].text, "compare apple");
  EXPECT_FALSE(session_->transcript()[0].is_final);
  ASSERT_TRUE(
      session_->OnFinalTranscript("compare apple and nvidia", 0.9).has_value());
  ASSERT_EQ(session_->transcript().size(), 2u);
  EXPECT_TRUE(session_->transcript()[1].is_final);
  EXPECT_EQ(session_->state(), VoiceSessionState::kUnderstanding);
}

TEST_F(VoiceSessionTest, ManualStopFinalizesLikePushToTalkRelease) {
  ASSERT_TRUE(session_->RequestStart().has_value());
  ASSERT_TRUE(session_->OnMicrophoneGranted().has_value());
  ASSERT_TRUE(session_->OnPartialTranscript("open a tab", 0.5).has_value());
  ASSERT_TRUE(session_->StopCapture().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kFinalizingTranscript);
  EXPECT_EQ(stt_.stop_count(), 1);
}

TEST_F(VoiceSessionTest, MicrophoneDenialFailsVisibly) {
  ASSERT_TRUE(session_->RequestStart().has_value());
  ASSERT_TRUE(session_->OnMicrophoneDenied().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kFailed);
  EXPECT_EQ(session_->last_error(), VoiceError::kMicrophoneDenied);
  ASSERT_TRUE(session_->ResetToIdle().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kIdle);
}

TEST_F(VoiceSessionTest, IllegalTransitionsAreRejectedWithoutStateChange) {
  EXPECT_EQ(session_->OnMicrophoneGranted().error(),
            VoiceError::kIllegalTransition);
  EXPECT_EQ(session_->OnPartialTranscript("x", 0.5).error(),
            VoiceError::kIllegalTransition);
  EXPECT_EQ(session_->BargeIn().error(), VoiceError::kIllegalTransition);
  EXPECT_EQ(session_->OnSpeechFinished().error(),
            VoiceError::kIllegalTransition);
  EXPECT_EQ(session_->state(), VoiceSessionState::kIdle);
  EXPECT_TRUE(observer_.states.empty());
}

TEST_F(VoiceSessionTest, BargeInStopsSpeechImmediatelyAndPreservesTask) {
  DriveToExecuting();
  session_->set_active_task_id("task-123");
  ASSERT_TRUE(session_->OnExecutionFinished("A long answer...").has_value());
  ASSERT_EQ(session_->state(), VoiceSessionState::kSpeaking);

  ASSERT_TRUE(session_->BargeIn().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kInterrupted);
  EXPECT_FALSE(tts_.speaking());
  EXPECT_EQ(tts_.immediate_stop_count(), 1);
  // The interruption never discards the driven task or the transcript.
  EXPECT_EQ(session_->active_task_id(), "task-123");
  EXPECT_FALSE(session_->transcript().empty());

  ASSERT_TRUE(session_->ResumeListeningAfterInterrupt().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kListening);
  ASSERT_TRUE(
      session_->OnPartialTranscript("actually hourly", 0.6).has_value());
}

TEST_F(VoiceSessionTest, ConfirmationGateAndVoiceRejection) {
  DriveToExecuting(/*requires_confirmation=*/true);
  // Approved path covered by DriveToExecuting; now the rejected path.
  Rebuild(VoiceSettings());
  ASSERT_TRUE(session_->RequestStart().has_value());
  ASSERT_TRUE(session_->OnMicrophoneGranted().has_value());
  ASSERT_TRUE(session_->OnFinalTranscript("submit the form", 0.9).has_value());
  ASSERT_TRUE(session_->OnUnderstandingComplete().has_value());
  ASSERT_TRUE(session_->OnPlanReady(true).has_value());
  ASSERT_TRUE(session_->OnConfirmation(false).has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kIdle);
}

TEST_F(VoiceSessionTest, CancelStopsCaptureAndSpeech) {
  ASSERT_TRUE(session_->RequestStart().has_value());
  ASSERT_TRUE(session_->OnMicrophoneGranted().has_value());
  ASSERT_TRUE(session_->Cancel().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kCancelled);
  EXPECT_EQ(stt_.cancel_count(), 1);

  Rebuild(VoiceSettings());
  DriveToExecuting();
  ASSERT_TRUE(session_->OnExecutionFinished("speaking now").has_value());
  ASSERT_TRUE(session_->Cancel().has_value());
  EXPECT_EQ(tts_.immediate_stop_count(), 1);
  EXPECT_EQ(session_->state(), VoiceSessionState::kCancelled);
}

TEST_F(VoiceSessionTest, PauseAndResume) {
  ASSERT_TRUE(session_->RequestStart().has_value());
  ASSERT_TRUE(session_->OnMicrophoneGranted().has_value());
  ASSERT_TRUE(session_->Pause().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kPaused);
  EXPECT_EQ(stt_.cancel_count(), 1);  // capture released while paused
  ASSERT_TRUE(session_->Resume().has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kListening);
}

TEST_F(VoiceSessionTest, EmptyFinalTranscriptIsMalformedProviderOutput) {
  ASSERT_TRUE(session_->RequestStart().has_value());
  ASSERT_TRUE(session_->OnMicrophoneGranted().has_value());
  ASSERT_TRUE(session_->OnSilenceDetected().has_value());
  EXPECT_EQ(session_->OnFinalTranscript("", 0.9).error(),
            VoiceError::kMalformedProviderOutput);
  EXPECT_EQ(session_->state(), VoiceSessionState::kFailed);
}

TEST_F(VoiceSessionTest, TypedInputJoinsTheSameThread) {
  ASSERT_TRUE(session_->SubmitTypedInput("open my research scene").has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kUnderstanding);
  ASSERT_EQ(session_->transcript().size(), 1u);
  EXPECT_TRUE(session_->transcript()[0].is_final);
  EXPECT_EQ(session_->transcript()[0].confidence, 1.0);
}

TEST_F(VoiceSessionTest, SpeechDisabledSkipsSpeakingState) {
  VoiceSettings settings;
  settings.speech_output_enabled = false;
  Rebuild(settings);
  DriveToExecuting();
  ASSERT_TRUE(session_->OnExecutionFinished("summary").has_value());
  EXPECT_EQ(session_->state(), VoiceSessionState::kIdle);
  EXPECT_FALSE(tts_.speaking());
}

TEST_F(VoiceSessionTest, CloudRouteIsVisible) {
  FakeSpeechToTextProvider cloud_stt{SpeechRoute::kCloud};
  VoiceSession session(VoiceSettings(), &cloud_stt, &tts_,
                       base::BindLambdaForTesting([this]() { return clock_; }));
  EXPECT_EQ(session.speech_route(), SpeechRoute::kCloud);
}

TEST_F(VoiceSessionTest, MissingProviderFailsStart) {
  VoiceSession session(VoiceSettings(), nullptr, &tts_,
                       base::BindLambdaForTesting([this]() { return clock_; }));
  EXPECT_EQ(session.RequestStart().error(), VoiceError::kProviderUnavailable);
  EXPECT_EQ(session.state(), VoiceSessionState::kFailed);
}

}  // namespace
}  // namespace seoul
