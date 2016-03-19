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

namespace detail
{
	template <class F> struct return_type;

	template <class R, class... A>
	struct return_type<R(*)(A...)>
	{
		using type = std::remove_const_t<std::remove_reference_t<R>>;
	};

	template <class R, class C, class... A>
	struct return_type<R(C::*)(A...)>
	{
		using type = std::remove_const_t<std::remove_reference_t<R>>;
	};
}

template<class T>
using return_type_t = typename detail::return_type<T>::type;

#define FN_RETURN_TYPE(function_name) return_type_t<decltype(&function_name)>

#endif // FUNCTION_TRAITS_HPP_071DD1DD_F933_40DC_A662_CB85F7BE7F00
