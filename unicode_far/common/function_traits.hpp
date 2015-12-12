﻿#ifndef FUNCTION_TRAITS_HPP_071DD1DD_F933_40DC_A662_CB85F7BE7F00
#define FUNCTION_TRAITS_HPP_071DD1DD_F933_40DC_A662_CB85F7BE7F00
#pragma once

/*
Copyright © 2014 Far Group
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

template <class F> struct return_type;
#define DEFINE_R_TYPE { typedef typename std::remove_const<typename std::remove_reference<R>::type>::type type; };
#ifdef NO_VARIADIC_TEMPLATES

template <class R>
struct return_type<R(*)()> DEFINE_R_TYPE

template <class R, class C>
struct return_type<R(C::*)()> DEFINE_R_TYPE

#define RETURN_TYPE_FREE_VTE(TYPENAME_LIST, ARG_LIST, REF_ARG_LIST, FWD_ARG_LIST) \
template<typename R, TYPENAME_LIST> \
struct return_type<R(*)(ARG_LIST)> DEFINE_R_TYPE

#define RETURN_TYPE_MF_VTE(TYPENAME_LIST, ARG_LIST, REF_ARG_LIST, FWD_ARG_LIST) \
template<typename R, class C, TYPENAME_LIST> \
struct return_type<R(C::*)(ARG_LIST)> DEFINE_R_TYPE

#include "common/variadic_emulation_helpers_begin.hpp"
VTE_GENERATE(RETURN_TYPE_FREE_VTE)
VTE_GENERATE(RETURN_TYPE_MF_VTE)
#include "common/variadic_emulation_helpers_end.hpp"

#undef RETURN_TYPE_MF_VTE
#undef RETURN_TYPE_FREE_VTE

#else
template <class R, class... A>
struct return_type<R(*)(A...)> DEFINE_R_TYPE

template <class R, class C, class... A>
struct return_type<R(C::*)(A...)> DEFINE_R_TYPE

#endif
#undef DEFINE_R_TYPE
#define FN_RETURN_TYPE(function_name) return_type<decltype(&function_name)>::type

#endif // FUNCTION_TRAITS_HPP_071DD1DD_F933_40DC_A662_CB85F7BE7F00
