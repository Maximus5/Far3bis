﻿/*
platform.env.cpp

*/
/*
Copyright © 2017 Far Group
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

#include "headers.hpp"
#pragma hdrstop

#include "platform.env.hpp"

#include "lasterror.hpp"

namespace os::env
{
	const wchar_t* provider::detail::provider::data() const
	{
		return m_Data;
	}

	//-------------------------------------------------------------------------

	provider::strings::strings()
	{
		m_Data = GetEnvironmentStrings();
	}

	provider::strings::~strings()
	{
		if (m_Data)
		{
			FreeEnvironmentStrings(m_Data);
		}
	}

	//-------------------------------------------------------------------------

	provider::block::block()
	{
		m_Data = nullptr;
		handle TokenHandle;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &ptr_setter(TokenHandle)))
		{
			CreateEnvironmentBlock(reinterpret_cast<void**>(&m_Data), TokenHandle.native_handle(), TRUE);
		}
	}

	provider::block::~block()
	{
		if (m_Data)
		{
			DestroyEnvironmentBlock(m_Data);
		}
	}

	//-------------------------------------------------------------------------

	bool get(const string_view& Name, string& Value)
	{
		GuardLastError ErrorGuard;
		null_terminated C_Name(Name);

		// GetEnvironmentVariable might return 0 not only in case of failure, but also when variable is empty.
		// To recognise this, we set LastError to ERROR_SUCCESS manually and check it after the call,
		// which doesn't change it upon success.
		SetLastError(ERROR_SUCCESS);

		if (detail::ApiDynamicStringReceiver(Value, [&](wchar_t* Buffer, size_t Size)
		{
			return ::GetEnvironmentVariable(C_Name.data(), Buffer, static_cast<DWORD>(Size));
		}))
		{
			return true;
		}

		if (GetLastError() == ERROR_SUCCESS)
		{
			Value.clear();
			return true;
		}

		// Something went wrong, it's better to leave the last error as is
		ErrorGuard.dismiss();
		return false;
	}

	string get(const string_view& Name)
	{
		string Result;
		get(Name, Result);
		return Result;
	}

	bool set(const string_view& Name, const string_view& Value)
	{
		return ::SetEnvironmentVariable(null_terminated(Name).data(), null_terminated(Value).data()) != FALSE;
	}

	bool del(const string_view& Name)
	{
		return ::SetEnvironmentVariable(null_terminated(Name).data(), nullptr) != FALSE;
	}

	string expand(const string_view& Str)
	{
		null_terminated C_Str(Str);

		string Result;
		if (!detail::ApiDynamicStringReceiver(Result, [&](wchar_t* Buffer, size_t Size)
		{
			const auto ReturnedSize = ::ExpandEnvironmentStrings(C_Str.data(), Buffer, static_cast<DWORD>(Size));
			// This pesky function includes a terminating null character even upon success, breaking the usual pattern
			return ReturnedSize <= Size? ReturnedSize - 1 : ReturnedSize;
		}))
		{
			Result = make_string(Str);
		}
		return Result;
	}

	string get_pathext()
	{
		const auto PathExt = get(L"PATHEXT");
		return !PathExt.empty()? PathExt : L".COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC"s;
	}
}
