﻿/*
platform.cpp

Враперы вокруг некоторых WinAPI функций
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

#include "headers.hpp"
#pragma hdrstop

#include "imports.hpp"
#include "pathmix.hpp"
#include "string_utils.hpp"

namespace os
{
	namespace detail
	{
		void handle_closer::operator()(HANDLE Handle) const { CloseHandle(Handle); }
		void printer_handle_closer::operator()(HANDLE Handle) const { ClosePrinter(Handle); }
	}


NTSTATUS GetLastNtStatus()
{
	return Imports().RtlGetLastNtStatus? Imports().RtlGetLastNtStatus() : STATUS_SUCCESS;
}

bool WNetGetConnection(const string& LocalName, string &RemoteName)
{
	wchar_t_ptr_n<MAX_PATH> Buffer(MAX_PATH);
	// MSDN says that call can fail with ERROR_NOT_CONNECTED or ERROR_CONNECTION_UNAVAIL if calling application
	// is running in a different logon session than the application that made the connection.
	// However, it may fail with ERROR_NOT_CONNECTED for non-network too, in this case Buffer will not be initialised.
	// Deliberately initialised with empty string to fix that.
	Buffer[0] = L'\0';
	auto Size = static_cast<DWORD>(Buffer.size());
	auto Result = ::WNetGetConnection(LocalName.data(), Buffer.get(), &Size);

	while (Result == ERROR_MORE_DATA)
	{
		Buffer.reset(Size);
		Result = ::WNetGetConnection(LocalName.data(), Buffer.get(), &Size);
	}

	const auto& IsReceived = [](int Code) { return Code == NO_ERROR || Code == ERROR_NOT_CONNECTED || Code == ERROR_CONNECTION_UNAVAIL; };

	if (IsReceived(Result) && *Buffer)
	{
		// Size isn't updated if the buffer is large enough
		RemoteName = Buffer.get();
		return true;
	}

	return false;
}

void EnableLowFragmentationHeap()
{
	// Starting with Windows Vista, the system uses the low-fragmentation heap (LFH) as needed to service memory allocation requests.
	// Applications do not need to enable the LFH for their heaps.
	if (IsWindowsVistaOrGreater())
		return;

	if (!Imports().HeapSetInformation)
		return;

	std::vector<HANDLE> Heaps(10);
	for (;;)
	{
		const auto NumberOfHeaps = ::GetProcessHeaps(static_cast<DWORD>(Heaps.size()), Heaps.data());
		const auto Received = NumberOfHeaps <= Heaps.size();
		Heaps.resize(NumberOfHeaps);
		if (Received)
			break;
	}

	for (const auto i: Heaps)
	{
		ULONG HeapFragValue = 2;
		Imports().HeapSetInformation(i, HeapCompatibilityInformation, &HeapFragValue, sizeof(HeapFragValue));
	}
}

string GetLocaleValue(LCID lcid, LCTYPE id)
{
	string Result;
	return detail::ApiDynamicErrorBasedStringReceiver(ERROR_INSUFFICIENT_BUFFER, Result, [&](wchar_t* Buffer, size_t Size)
	{
		const auto ReturnedSize = ::GetLocaleInfo(lcid, id, Buffer, static_cast<int>(Size));
		return ReturnedSize? ReturnedSize - 1 : 0;
	})?
	Result : L""s;
}

string GetPrivateProfileString(const string& AppName, const string& KeyName, const string& Default, const string& FileName)
{
	wchar_t_ptr Buffer(NT_MAX_PATH);
	DWORD size = ::GetPrivateProfileString(AppName.data(), KeyName.data(), Default.data(), Buffer.get(), static_cast<DWORD>(Buffer.size()), FileName.data());
	return string(Buffer.get(), size);
}

bool GetWindowText(HWND Hwnd, string& Text)
{
	return detail::ApiDynamicStringReceiver(Text, [&](wchar_t* Buffer, size_t Size)
	{
		const size_t Length = ::GetWindowTextLength(Hwnd);

		if (!Length)
			return Length;

		if (Length + 1 > Size)
			return Length + 1;

		return static_cast<size_t>(::GetWindowText(Hwnd, Buffer, static_cast<int>(Size)));
	});
}

bool IsWow64Process()
{
#ifdef _WIN64
	return false;
#else
	static const auto Wow64Process = []{ BOOL Value = FALSE; return Imports().IsWow64Process(GetCurrentProcess(), &Value) && Value; }();
	return Wow64Process;
#endif
}

DWORD GetAppPathsRedirectionFlag()
{
	static const auto RedirectionFlag = []
	{
		// App Paths key is shared in Windows 7 and above
		if (!IsWindows7OrGreater())
		{
#ifdef _WIN64
			return KEY_WOW64_32KEY;
#else
			if (IsWow64Process())
			{
				return KEY_WOW64_64KEY;
			}
#endif
		}
		return 0;
	}();
	return RedirectionFlag;
}

bool GetDefaultPrinter(string& Printer)
{
	return detail::ApiDynamicErrorBasedStringReceiver(ERROR_INSUFFICIENT_BUFFER, Printer, [&](wchar_t* Buffer, size_t Size)
	{
		DWORD dwSize = static_cast<DWORD>(Size);
		return ::GetDefaultPrinter(Buffer, &dwSize)? dwSize - 1 : 0;
	});
}

bool GetComputerName(string& Name)
{
	wchar_t Buffer[MAX_COMPUTERNAME_LENGTH + 1];
	auto Size = static_cast<DWORD>(std::size(Buffer));
	if (!::GetComputerName(Buffer, &Size))
		return false;

	Name.assign(Buffer, Size);
	return true;
}

bool GetComputerNameEx(COMPUTER_NAME_FORMAT NameFormat, string& Name)
{
	return detail::ApiDynamicStringReceiver(Name, [&](wchar_t* Buffer, size_t Size)
	{
		auto dwSize = static_cast<DWORD>(Size);
		if (!::GetComputerNameEx(NameFormat, Buffer, &dwSize) && GetLastError() != ERROR_MORE_DATA)
			return 0ul;
		return dwSize;
	});
}

bool GetUserName(string& Name)
{
	wchar_t Buffer[UNLEN + 1];
	auto Size = static_cast<DWORD>(std::size(Buffer));
	if (!::GetUserName(Buffer, &Size))
		return false;

	Name.assign(Buffer, Size - 1);
	return true;
}

bool GetUserNameEx(EXTENDED_NAME_FORMAT NameFormat, string& Name)
{
	return detail::ApiDynamicStringReceiver(Name, [&](wchar_t* Buffer, size_t Size)
	{
		auto dwSize = static_cast<DWORD>(Size);
		if (!::GetUserNameEx(NameFormat, Buffer, &dwSize) && GetLastError() != ERROR_MORE_DATA)
			return 0ul;
		return dwSize;
	});
}

handle OpenCurrentThread()
{
	HANDLE Handle;
	return os::handle(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &Handle, 0, FALSE, DUPLICATE_SAME_ACCESS) ? Handle : nullptr);
}

handle OpenConsoleInputBuffer()
{
	return handle(fs::low::create_file(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
}

handle OpenConsoleActiveScreenBuffer()
{
	return handle(fs::low::create_file(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
}

	namespace rtdl
	{
		void module::module_deleter::operator()(HMODULE Module) const
		{
			FreeLibrary(Module);
		}

		HMODULE module::get_module() const
		{
			if (!m_tried && !m_module && !m_name.empty())
			{
				m_tried = true;
				m_module.reset(LoadLibrary(m_name.data()));

				if (!m_module && m_AlternativeLoad && IsAbsolutePath(m_name))
				{
					m_module.reset(LoadLibraryEx(m_name.data(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
				}
				// TODO: log if nullptr
			}
			return m_module.get();
		}
	}

	namespace security
	{
		bool is_admin()
		{
			static const auto Result = []
			{
				SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
				const auto AdministratorsGroup = make_sid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS);
				if (!AdministratorsGroup)
					return false;

				BOOL IsMember;
				return CheckTokenMembership(nullptr, AdministratorsGroup.get(), &IsMember) && IsMember;
			}();

			return Result;
		}
	}
}

UUID CreateUuid()
{
	UUID Uuid;
	UuidCreate(&Uuid);
	return Uuid;
}

string GuidToStr(const GUID& Guid)
{
	string result;
	RPC_WSTR str;
	// declared as non-const in GCC headers :(
	if (UuidToString(const_cast<GUID*>(&Guid), &str) == RPC_S_OK)
	{
		SCOPE_EXIT{ RpcStringFree(&str); };
		result = reinterpret_cast<const wchar_t*>(str);
	}
	return upper(result);
}

bool StrToGuid(const wchar_t* Value, GUID& Guid)
{
	return UuidFromString(reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(Value)), &Guid) == RPC_S_OK;
}
