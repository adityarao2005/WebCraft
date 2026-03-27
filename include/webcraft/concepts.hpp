#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file concepts.hpp
 * @brief Small shared concepts used by WebCraft templates.
 *
 * This header provides lightweight concept helpers used as constraints
 * throughout the public API. Include it when writing generic utilities that
 * integrate with WebCraft template components.
 */

#include <concepts>

namespace webcraft
{
    template <typename T, typename R>
    concept not_same_as = !std::same_as<T, R>;

    template <bool T>
    concept not_true = !T;
}