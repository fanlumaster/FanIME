#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include "MetasequoiaImeEngine/shuangpin/dictionary.h"

inline std::condition_variable pipe_queueCv;
inline std::atomic_bool pipe_running = true;
inline std::shared_ptr<DictionaryUlPb> g_dictQuery;

namespace FanyNamedPipe
{
void WorkerThread();
void EventListenerLoopThread();
void AuxPipeEventListenerLoopThread();
void ToTsfPipeEventListenerLoopThread();

void PrepareCandidateList();
} // namespace FanyNamedPipe