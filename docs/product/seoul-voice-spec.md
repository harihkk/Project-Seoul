# Seoul Voice Specification

Status: Source complete; not compiled or runtime-verified on the authoring host.

The voice layer gives Seoul one conversational session shared by voice, typed
text, and Canvas interaction. This spec describes the source in
`native/seoul/browser/voice/`. The module stores bounded transcript segments
only; there is no audio buffer type anywhere in it, so raw audio cannot be
persisted by construction.

## Session states

`native/seoul/browser/voice/voice_types.h` defines `VoiceSessionState` with 14
values: `kIdle`, `kMicrophoneRequesting`, `kListening`, `kPartialTranscript`,
`kFinalizingTranscript`, `kUnderstanding`, `kPlanning`, `kAwaitingConfirmation`,
`kExecuting`, `kSpeaking`, `kInterrupted`, `kPaused`, `kCancelled`, and
`kFailed`. `kCancelled` and `kFailed` are terminal (`IsTerminal`).

## Transition rules

`native/seoul/browser/voice/voice_session.cc` validates every transition against
an explicit table. Each turn method returns `kIllegalTransition` and leaves the
state unchanged when called in a state that does not accept it. The main path is:

- `RequestStart` (idle only) moves to microphone-requesting; a null STT provider
  fails immediately with `kProviderUnavailable`.
- `OnMicrophoneGranted` moves to listening; `OnMicrophoneDenied` fails with
  `kMicrophoneDenied`.
- `OnPartialTranscript` appends a partial hypothesis (replacing the previous
  partial for the same utterance) and enters partial-transcript.
- `OnSilenceDetected` or `StopCapture` moves to finalizing.
- `OnFinalTranscript` requires non-empty text; an empty final is treated as
  `kMalformedProviderOutput` and the session fails visibly rather than acting on
  nothing. On success it enters understanding.
- `OnUnderstandingComplete` moves to planning; `OnPlanReady` moves to
  awaiting-confirmation or straight to executing depending on
  `requires_confirmation`; `OnConfirmation(false)` returns to idle.
- `OnExecutionFinished` speaks a summary (if TTS is present and output enabled)
  or returns to listening or idle by input mode; `OnSpeechFinished` does the
  same by input mode.

Terminal or idle sessions accept `ResetToIdle`; `Cancel` is illegal from a
terminal or idle state.

## Push-to-talk and conversation mode

`VoiceInputMode` is `kPushToTalk` (capture while held; manual `StopCapture`
finalizes; the turn ends to idle) or `kToggleConversation` (explicit on/off;
after speaking, the session returns to listening for the follow-up). The mode is
read at `OnExecutionFinished` and `OnSpeechFinished` to decide whether to return
to listening or idle.

## STT and TTS provider seams

`native/seoul/browser/voice/speech_providers.h` defines `SpeechToTextProvider`
and `TextToSpeechProvider`. Each exposes a `provider_name` and a `SpeechRoute`.
Implementations adapt one engine (a platform speech framework, an audited local
model runtime, or an official cloud API). Providers deliver transcripts and
speech events through typed callbacks only; the header states that raw audio
never crosses this boundary into Seoul state. Either provider may be null: a null
STT provider fails `RequestStart`; a null TTS provider skips the speaking state.

## Barge-in

`BargeIn` is valid only from speaking. It calls `StopImmediately` on the TTS
provider (the contract requires output to cease immediately, never at a phrase
boundary) and moves to interrupted. The session comment and code make clear that
task state and already-verified work are untouched: the session does not own the
task, and `active_task_id` is never cleared by interruption.
`ResumeListeningAfterInterrupt` returns to listening.

## Microphone policy

There is no always-on path: capture begins only after `RequestStart` leads
through microphone-requesting and an explicit `OnMicrophoneGranted`. Denial
fails the session. `VoiceSettings.persist_raw_audio` defaults to false, and the
type header notes there is deliberately no code path in this module that stores
audio. Transcript segments are bounded by `kMaxTranscriptLength` (8192) and
`kMaxTranscriptSegments` (200), with the oldest segment dropped past the cap.

## Local and cloud route visibility

`SpeechRoute` (`kLocal` or `kCloud`) is surfaced next to the microphone state via
`VoiceSession::speech_route`. Local transcription is the default;
`VoiceSettings.allow_cloud_speech` defaults to false. Each `TranscriptSegment`
records the `route` that produced it, so the origin of every segment is visible.

## Spoken-reference resolution

`native/seoul/browser/voice/voice_reference_resolver.cc` resolves a spoken phrase
to exactly one `VisibleReferent` id supplied by the Canvas. A referent carries a
stable id (a SAUI component id or data selection key), a `label`, a `kind`
(singular noun class such as "result", "chart", "step", "tab"), a 1-based
`ordinal`, and `selected`/`focused` flags; it never carries screen coordinates.
Resolution is deterministic: deictic words ("this", "that", "it") resolve to the
selected item, then the focused item, then the only item; ordinal words ("second
result", "last one") resolve within an optional kind scope; otherwise an exact
label match wins, then a unique substring match. Failure is precise:
`kAmbiguousReference`, `kUnknownReference`, `kTooManyReferents`, or
`kTranscriptTooLong`.

## Typed text joins the same thread

`SubmitTypedInput` is valid whenever a new turn could begin by voice (idle,
listening, or interrupted). It appends a final segment with confidence 1.0 and
enters understanding directly, so typed and spoken input share one transcript and
one state machine.
