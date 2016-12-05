﻿#ifndef COMMON_HPP_1BD5AB87_3379_4AFE_9F63_DB850DCF72B4
#define COMMON_HPP_1BD5AB87_3379_4AFE_9F63_DB850DCF72B4
#pragma once

/*
common.hpp

Some useful classes, templates && macros.

*/
/*
Copyright © 2013 Far Group
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// TODO: use separately where required

#include "common/compiler.hpp"
#include "common/preprocessor.hpp"
#include "common/movable.hpp"
#include "common/exception.hpp"
#include "common/noncopyable.hpp"
#include "common/swapable.hpp"
#include "common/rel_ops.hpp"
#include "common/conditional.hpp"
#include "common/scope_exit.hpp"
#include "common/function_traits.hpp"
#include "common/smart_ptr.hpp"
#include "common/any.hpp"
#include "common/null_iterator.hpp"
#include "common/enumerator.hpp"
#include "common/iterator_range.hpp"
#include "common/algorithm.hpp"
#include "common/monitored.hpp"
#include "common/enum_substrings.hpp"
#include "common/string_literal.hpp"
#include "common/string_utils.hpp"
#include "common/zip_view.hpp"
#include "common/blob_view.hpp"

// TODO: clean up & split

template<class T>
void resize_nomove(T& container, size_t size)
{
	T Tmp(size);
	using std::swap;
	swap(container, Tmp);
}

template<class T>
void clear_and_shrink(T& container)
{
	T Tmp;
	using std::swap;
	swap(container, Tmp);
}

template<class T>
void node_swap(T& Container, const typename T::const_iterator& a, const typename T::const_iterator& b)
{
	const auto NextA = std::next(a), NextB = std::next(b);
	Container.splice(NextA, Container, b);
	Container.splice(NextB, Container, a);
}

template <typename T>
bool CheckStructSize(const T* s)
{
	return s && (s->StructSize >= sizeof(T));
}

template <typename T>
bool CheckNullOrStructSize(const T* s)
{
	return !s || CheckStructSize(s);
}

template<typename T, size_t N>
void ClearArray(T(&a)[N]) noexcept
{
	std::fill_n(a, N, T{});
}

template<class T>
auto NullToEmpty(const T* Str)
{
	static constexpr T empty{};
	return Str? Str : &empty;
}

template<class T>
auto EmptyToNull(const T* Str)
{
	return (Str && !*Str)? nullptr : Str;
}

template<class T>
auto make_hash(const T& value)
{
	return std::hash<T>{}(value);
}

template <class T>
T Round(const T &a, const T &b)
{
	return a / b + ((a % b * 2 > b)? 1 : 0);
}

inline void* ToPtr(intptr_t Value)
{
	return reinterpret_cast<void*>(Value);
}

template<class T, class Y>
bool InRange(const T& from, const Y& what, const T& to)
{
	return from <= what && what <= to;
}

template<typename owner, typename acquire, typename release>
auto make_raii_wrapper(owner* Owner, const acquire& Acquire, const release& Release)
{
	std::invoke(Acquire, Owner);
	auto&& Releaser = [&Release](owner* Owner){ std::invoke(Release, Owner); };
	return std::unique_ptr<owner, std::remove_reference_t<decltype(Releaser)>>(Owner, std::move(Releaser));
}

template<typename T>
auto as_unsigned(T Value)
{
	return static_cast<std::make_unsigned_t<T>>(Value);
}

template<typename T>
auto as_underlying_type(T Value)
{
	return static_cast<std::underlying_type_t<T>>(Value);
}

namespace enum_helpers
{
	template<class O, class R = void, class T>
	constexpr auto operation(T a, T b)
	{
		return static_cast<std::conditional_t<std::is_same<R, void>::value, T, R>>(O()(as_underlying_type(a), as_underlying_type(b)));
	}
}

#ifdef _DEBUG
#define SELF_TEST(code) \
namespace \
{ \
	struct SelfTest \
	{ \
		SelfTest() \
		{ \
			code; \
		} \
	} _SelfTest; \
}
#else
#define SELF_TEST(code)
#endif

constexpr auto bit(size_t Number)
{
	return 1 << Number;
}

#define SIGN_UNICODE    0xFEFF
#define SIGN_REVERSEBOM 0xFFFE
#define SIGN_UTF8       0xBFBBEF
#define EOL_STR L"\r\n"

constexpr size_t aligned_size(size_t Size, size_t Alignment = MEMORY_ALLOCATION_ALIGNMENT)
{
	return (Size + (Alignment - 1)) & ~(Alignment - 1);
}

template<class T, int Alignment = MEMORY_ALLOCATION_ALIGNMENT>
struct aligned_sizeof
{
	enum
	{
		value = aligned_size(sizeof(T), Alignment)
	};
};

#endif // COMMON_HPP_1BD5AB87_3379_4AFE_9F63_DB850DCF72B4
