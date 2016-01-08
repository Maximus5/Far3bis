﻿/*
vc_crt_fix_impl.cpp

Workaround for Visual C++ CRT incompatibility with old Windows versions
*/
/*
Copyright © 2011 Far Group
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

THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS' AND ANY EXPRESS OR
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

#include <windows.h>

template<typename T>
inline T GetFunctionPointer(const wchar_t* ModuleName, const char* FunctionName, T Replacement)
{
	const auto Address = GetProcAddress(GetModuleHandleW(ModuleName), FunctionName);
	return Address? reinterpret_cast<T>(Address) : Replacement;
}

#define CREATE_FUNCTION_POINTER(ModuleName, FunctionName)\
static const auto Function = GetFunctionPointer(ModuleName, #FunctionName, &implementation::FunctionName)

static const wchar_t kernel32[] = L"kernel32";

// EncodePointer (VC2010)
extern "C" PVOID WINAPI EncodePointerWrapper(PVOID Ptr)
{
	struct implementation
	{
		static PVOID WINAPI EncodePointer(PVOID Ptr) { return Ptr; }
	};

	CREATE_FUNCTION_POINTER(kernel32, EncodePointer);
	return Function(Ptr);
}

// DecodePointer(VC2010)
extern "C" PVOID WINAPI DecodePointerWrapper(PVOID Ptr)
{
	struct implementation
	{
		static PVOID WINAPI DecodePointer(PVOID Ptr) { return Ptr; }
	};

	CREATE_FUNCTION_POINTER(kernel32, DecodePointer);
	return Function(Ptr);
}

// GetModuleHandleExW (VC2012)
extern "C" BOOL WINAPI GetModuleHandleExWWrapper(DWORD Flags, LPCWSTR ModuleName, HMODULE *Module)
{
	struct implementation
	{
		static BOOL WINAPI GetModuleHandleExW(DWORD Flags, LPCWSTR ModuleName, HMODULE *Module)
		{
			BOOL Result = FALSE;
			if (Flags)
			{
				SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
			}
			else
			{
				*Module = GetModuleHandleW(ModuleName);
				if (*Module)
				{
					Result = TRUE;
				}
			}
			return Result;
		}
	};

	CREATE_FUNCTION_POINTER(kernel32, GetModuleHandleExW);
	return Function(Flags, ModuleName, Module);
}

// InitializeSListHead (VC2015)
extern "C" void WINAPI InitializeSListHeadWrapper(PSLIST_HEADER ListHead)
{
	struct implementation
	{
		static void WINAPI InitializeSListHead(PSLIST_HEADER ListHead)
		{
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		}
	};

	CREATE_FUNCTION_POINTER(kernel32, InitializeSListHead);
	return Function(ListHead);
}

// RtlGenRandom (VC2015)
// RtlGenRandom is used in rand_s implementation in ucrt.
// As long as we don't use the rand_s in the code (which is easy: it's non-standard, requires _CRT_RAND_S to be defined first and not recommended in general)
// this function is never called so it's ok to provide this dummy implementation only to have the _SystemFunction036@8 symbol in binary to make their loader happy.
extern "C" BOOLEAN WINAPI SystemFunction036(PVOID Buffer, ULONG Size)
{
	return TRUE;
}
