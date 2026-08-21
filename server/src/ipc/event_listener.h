#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <cstdint>
#include "session/input_session.h"
#include "english/english_ime.h"

inline std::condition_variable pipe_queueCv;
inline std::atomic_bool pipe_running = true;
inline std::shared_ptr<IInputSession> g_inputSession;

namespace FanyNamedPipe
{
enum class CandidateUiAction
{
    Commit,
    Pin,
    Delete,
    FixPosition,
    ClearPosition,
};

void WorkerThread();
void EventListenerLoopThread();
void AuxPipeEventListenerLoopThread();
void ToTsfPipeEventListenerLoopThread();
void ToTsfWorkerThreadPipeEventListenerLoopThread();

// Delivers an activation that arrived before the candidate window existed. Call
// once the window is available; a no-op when nothing was deferred.
void ReplayDeferredClientActivation();

void PrepareCandidateList(uint64_t client_id, uint64_t activation_epoch);
void ClearState();
void RegisterStatusSnapshotWindow(HWND toolbar_window);
void EnqueueCloudCandidate(const std::string &candidate, const std::string &pinyin, uint64_t generation);
void EnqueueAiCandidate(const std::string &candidate, const std::string &identity, uint64_t generation);
void CancelCloudCandidateRequest();
void EnqueueEnglishCandidates(std::vector<WordItem> candidates, const std::string &input, uint64_t generation);
void EnqueueCandidateTranslations(std::vector<EnglishIme::TranslationResult> results, uint64_t generation);
void EnqueueEmojiCandidates(std::vector<WordItem> candidates, const std::string &input, uint64_t generation);
void EnqueueKaomojiCandidates(std::vector<WordItem> candidates, const std::string &input, uint64_t generation);
void EnqueueCandidateUiAction(CandidateUiAction action, int one_based_index, int fixed_position = 0);
void EnqueuePipeSessionInvalidatedTask(uint64_t client_id, uint64_t invalidation_epoch);
void EnqueueReloadInputSessionTask();
void EnqueueApplyCandidatePageSizeTask();
void EnqueueRefreshCandidatePageTask();
void EnqueueResetInputSessionCacheTask();
void EnqueueExitEnglishInputModeTask();
} // namespace FanyNamedPipe
