#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

inline std::condition_variable queueCv;
inline std::atomic_bool running = true;

void WorkerThread();
void EventListenerLoopThread();