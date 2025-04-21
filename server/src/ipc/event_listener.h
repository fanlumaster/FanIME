#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include "ime_engine/shuangpin/dictionary.h"

inline std::condition_variable queueCv;
inline std::atomic_bool running = true;
inline std::shared_ptr<DictionaryUlPb> g_dictQuery;

void WorkerThread();
void EventListenerLoopThread();