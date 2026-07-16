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
};

void WorkerThread();
void EventListenerLoopThread();
void AuxPipeEventListenerLoopThread();
void ToTsfPipeEventListenerLoopThread();
void ToTsfWorkerThreadPipeEventListenerLoopThread();

void PrepareCandidateList(uint64_t client_id, uint64_t activation_epoch);
void ClearState();
void RegisterStatusSnapshotWindow(HWND toolbar_window);
void EnqueueCloudCandidate(const std::string &candidate, const std::string &pinyin, uint64_t generation);
void EnqueueEnglishCandidates(std::vector<WordItem> candidates, const std::string &input, uint64_t generation);
void EnqueueCandidateUiAction(CandidateUiAction action, int one_based_index);
void EnqueuePipeSessionInvalidatedTask(uint64_t client_id, uint64_t invalidation_epoch);
void EnqueueReloadInputSessionTask();
void EnqueueApplyCandidatePageSizeTask();
void EnqueueResetInputSessionCacheTask();
} // namespace FanyNamedPipe
