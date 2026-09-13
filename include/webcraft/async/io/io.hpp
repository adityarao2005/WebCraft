#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/io/io.hpp
 * @brief Umbrella include for WebCraft async I/O components.
 *
 * Include this header to access stream concepts, adaptors, filesystem streams,
 * and socket streams from a single entry point.
 */

#include "core.hpp"
#include "adaptors.hpp"
#include "fs.hpp"
#include "socket.hpp"

// #define WEBCRAFT_UDP_MOCK