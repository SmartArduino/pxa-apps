/* SPDX-License-Identifier: MIT
 * Narrow, independently implemented value-type adapter for the PXA port.
 * No SDL, JSON, file, window, or original nomlib renderer implementation.
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#define NOM_SCAST(Type, value) static_cast<Type>(value)
#define NOM_ASSERT(expr) assert(expr)
#define NOM_ASSERT_INVALID_PATH() assert(!"invalid upstream control path")
#define NOM_LOG_CATEGORY_CUSTOM 0
#define NOM_LOG_PRIORITY_VERBOSE 0
#define NOM_LOG_TRACE(...) ((void)0)
#define NOM_LOG_TRACE_PRIO(...) ((void)0)
#define NOM_LOG_DEBUG(...) ((void)0)
#define NOM_LOG_INFO(...) ((void)0)
#define NOM_LOG_ERR(...) ((void)0)
#define NOM_LOG_CRIT(...) ((void)0)
constexpr int NONE=0;
namespace nom {
using int8=std::int8_t;using uint8=std::uint8_t;
using int16=std::int16_t;using uint16=std::uint16_t;
using int32=std::int32_t;using uint32=std::uint32_t;
using int64=std::int64_t;using uint64=std::uint64_t;
using real32=float;using real64=double;using size_type=std::size_t;
class Value;
struct RenderTarget {};
struct IDrawable {using RenderTarget=nom::RenderTarget;};
}

