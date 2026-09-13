#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/async.hpp
 * @brief Umbrella include for the public asynchronous programming API.
 *
 * Include this header to access common coroutine types (`task`, generators,
 * synchronization primitives, runtime utilities, and I/O components) in one
 * place. It also exposes convenience macros for coroutine declarations.
 */

#include <atomic>
#include "async_event.hpp"
#include "async_generator.hpp"
#include "awaitable.hpp"
#include "event_signal.hpp"
#include "fire_and_forget_task.hpp"
#include "generator.hpp"
#include "runtime.hpp"
#include "sync_wait.hpp"
#include "task_completion_source.hpp"
#include "task.hpp"
#include "thread_pool.hpp"
#include "when_all.hpp"
#include "when_any.hpp"
#include <webcraft/async/io/io.hpp>

#define co_async [&]()->::webcraft::async::task<void>
#define async_t(T) ::webcraft::async::task<T>