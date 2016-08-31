﻿#ifndef FAREXCPT_HPP_F7B85E85_71DD_483D_BD7F_B26B8566AC8E
#define FAREXCPT_HPP_F7B85E85_71DD_483D_BD7F_B26B8566AC8E
#pragma once

/*
farexcpt.hpp

Все про исключения
*/
/*
Copyright © 1996 Eugene Roshal
Copyright © 2000 Far Group
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

class Plugin;

bool ProcessStdException(const std::exception& e, const wchar_t* Function, Plugin* Module = nullptr);
bool ProcessUnknownException(const wchar_t* Function, Plugin* Module = nullptr);

LONG WINAPI FarUnhandledExceptionFilter(EXCEPTION_POINTERS *ExceptionInfo);

void RestoreGPFaultUI();

void RegisterTestExceptionsHook();

bool IsCppException(const EXCEPTION_POINTERS* e);

template<class function, class filter, class handler>
auto seh_invoke(function&& Callable, filter&& Filter, handler&& Handler)
{
#if COMPILER == C_GCC
	// GCC doesn't support these currently
	return Callable();
#else
	__try
	{
		return Callable();
	}
	__except (Filter(GetExceptionCode(), GetExceptionInformation()))
	{
		void ResetStackOverflowIfNeeded();

		ResetStackOverflowIfNeeded();
		return Handler();
	}
#endif
}

template<class function, class handler>
auto seh_invoke_with_ui(function&& Callable, handler&& Handler, const wchar_t* Function, Plugin* Module = nullptr)
{
	int SehFilter(int, EXCEPTION_POINTERS*, const wchar_t*, Plugin*);
	return seh_invoke(std::forward<function>(Callable), [&](auto Code, auto Info) { return SehFilter(Code, Info, Function, Module); }, std::forward<handler>(Handler));
}

template<class function, class handler>
auto seh_invoke_no_ui(function&& Callable, handler&& Handler)
{
	return seh_invoke(std::forward<function>(Callable), [](auto, auto) { return EXCEPTION_EXECUTE_HANDLER; }, std::forward<handler>(Handler));
}

#endif // FAREXCPT_HPP_F7B85E85_71DD_483D_BD7F_B26B8566AC8E
