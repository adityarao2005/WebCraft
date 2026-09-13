#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file webcraft.hpp
 * @brief Top-level include for WebCraft runtime bootstrap utilities.
 *
 * Include this header when you only need the runtime lifecycle API
 * (for example, to construct and manage `webcraft::async::runtime`).
 * For the broader async surface, include `webcraft/async/async.hpp`.
 */

#include <webcraft/async/runtime.hpp>