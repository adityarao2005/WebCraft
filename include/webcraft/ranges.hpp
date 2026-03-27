#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file ranges.hpp
 * @brief Range adaptor helpers for collecting range pipelines.
 *
 * The main utility is `webcraft::ranges::to<T>`, which can be used in a
 * pipeline to materialize an input range into a target container type.
 */

#include <ranges>

namespace webcraft::ranges
{

    /**
     * @brief Range adaptor closure that materializes a range into container `T`.
     * @tparam T Target container type.
     */
    template <typename T>
    class to : public std::ranges::range_adaptor_closure<to<T>>
    {
    public:
        /**
         * @brief Collects all values from an input range into `T`.
         */
        constexpr T operator()(std::ranges::input_range auto &&range) const
        {
            T result;

            for (auto &&item : range)
            {
                result.insert(result.end(), std::forward<decltype(item)>(item));
            }

            return std::move(result);
        }
    };
}