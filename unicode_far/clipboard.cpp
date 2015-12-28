﻿/*
clipboard.cpp

Работа с буфером обмена.
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

#include "clipboard.hpp"
#include "console.hpp"
#include "strmix.hpp"

enum { NO_FORMAT = 0xffff };
static struct internal_clipboard
{
	UINT Format;
	os::memory::global::ptr Handle;
}
InternalClipboard[] =
{
	// CF_OEMTEXT CF_TEXT CF_UNICODETEXT CF_HDROP
	{ NO_FORMAT },
	{ NO_FORMAT },
	{ NO_FORMAT },
	{ NO_FORMAT },
	{ NO_FORMAT },
};

bool Clipboard::UseInternalClipboard = false;
bool Clipboard::InternalClipboardOpened = false;
bool Clipboard::SystemClipboardOpened = false;

//Sets UseInternalClipboard to State, and returns previous state
bool Clipboard::SetUseInternalClipboardState(bool State)
{
	const bool OldState = UseInternalClipboard;
	UseInternalClipboard = State;
	return OldState;
}

bool Clipboard::GetUseInternalClipboardState()
{
	return UseInternalClipboard;
}

UINT Clipboard::RegisterFormat(FAR_CLIPBOARD_FORMAT Format)
{
	static const simple_pair<unsigned, const wchar_t*> FormatMap[] =
	{
		{ 0xFEB0, L"FAR_VerticalBlock" },
		{ 0xFEB1, L"FAR_VerticalBlock_Unicode" },
		{ 0xFEB2, CFSTR_PREFERREDDROPEFFECT },
		{ 0xFEB3, L"MSDEVColumnSelect" },
		{ 0xFEB4, L"Borland IDE Block Type" }
	};

	static_assert(ARRAYSIZE(FormatMap) == FCF_COUNT, "Wrong size of FormatMap");
	assert(Format < FCF_COUNT);

	return UseInternalClipboard? FormatMap[Format].first : RegisterClipboardFormat(FormatMap[Format].second);
}

bool Clipboard::Open()
{
	if (UseInternalClipboard)
	{
		if (!InternalClipboardOpened)
		{
			InternalClipboardOpened=true;
			return true;
		}

		return false;
	}

	if (!SystemClipboardOpened)
	{
		if (OpenClipboard(Console().GetWindow()))
		{
			SystemClipboardOpened = true;
			return true;
		}
	}
	return false;
}

bool Clipboard::Close()
{
	// Closing already closed buffer is OK

	if (UseInternalClipboard)
	{
		InternalClipboardOpened = false;
		return true;
	}

	if (SystemClipboardOpened)
	{
		if (!CloseClipboard())
			return false;
		SystemClipboardOpened = false;
	}
	return true;
}

bool Clipboard::Clear()
{
	if (UseInternalClipboard)
	{
		if (InternalClipboardOpened)
		{
			std::for_each(RANGE(InternalClipboard, i)
			{
				if (i.Handle)
				{
					i.Handle.reset();
					i.Format = NO_FORMAT;
				}
			});

			return true;
		}

		return false;
	}

	return EmptyClipboard() != FALSE;
}

HANDLE Clipboard::GetData(UINT uFormat)
{
	if (UseInternalClipboard)
	{
		if (InternalClipboardOpened && uFormat != NO_FORMAT)
		{
			const auto ItemIterator = std::find_if(CONST_RANGE(InternalClipboard, i)
			{
				return i.Format == uFormat;
			});

			if (ItemIterator != std::cend(InternalClipboard))
				return ItemIterator->Handle.get();
		}
		return nullptr;
	}

	return GetClipboardData(uFormat);
}

bool Clipboard::SetData(UINT uFormat, HGLOBAL hMem)
{
	if (UseInternalClipboard)
	{
		if (InternalClipboardOpened)
		{
			const auto ItemIterator = std::find_if(RANGE(InternalClipboard, i)
			{
				return !i.Handle;
			});

			if (ItemIterator != std::cend(InternalClipboard))
			{
				ItemIterator->Handle.reset(hMem);
				ItemIterator->Format = uFormat;
				return true;
			}
		}

		return false;
	}

	if (SetClipboardData(uFormat, hMem))
	{
		if (auto Locale = os::memory::global::copy<LCID>(LOCALE_USER_DEFAULT))
		{
			if (SetClipboardData(CF_LOCALE, Locale.get()))
			{
				Locale.release();
			}
			return true;
		}
	}

	return false;
}

bool Clipboard::SetData(UINT uFormat, os::memory::global::ptr&& hMem)
{
	const auto Result = SetData(uFormat, hMem.get());
	if (Result)
	{
		hMem.release();
	}
	return Result;
}

bool Clipboard::IsFormatAvailable(UINT Format)
{
	if (UseInternalClipboard)
	{
		return Format != NO_FORMAT && std::any_of(CONST_RANGE(InternalClipboard, i)
		{
			return i.Format == Format;
		});
	}

	return IsClipboardFormatAvailable(Format) != FALSE;
}

// Перед вставкой производится очистка буфера
bool Clipboard::SetText(const wchar_t *Data, size_t Size)
{
	auto Result = Clear();
	if (Data && Size)
	{
		if (auto hData = os::memory::global::copy(Data, Size))
		{
			Result = SetData(CF_UNICODETEXT, std::move(hData));
		}
		else
		{
			Result = false;
		}
	}
	return Result;
}

// вставка без очистки буфера - на добавление
bool Clipboard::SetFormat(FAR_CLIPBOARD_FORMAT Format, const wchar_t *Data, size_t Size)
{
	const auto FormatType = RegisterFormat(Format);

	if (!FormatType)
		return false;

	bool Result = false;

	if (Data && Size)
	{
		if (auto hData = os::memory::global::copy(Data, Size))
		{
			Result = SetData(FormatType, std::move(hData));
		}
	}

	if (Format == FCF_VERTICALBLOCK_UNICODE)
	{
		// support "Borland IDE Block Type"
		SetData(RegisterFormat(FCF_BORLANDIDEVBLOCK), os::memory::global::copy(L'\2'));
		// support "MSDEVColumnSelect"
		SetData(RegisterFormat(FCF_MSDEVCOLUMNSELECT), nullptr);
	}

	return Result;
}

bool Clipboard::SetHDROP(const string& NamesData, bool bMoved)
{
	bool Result=false;
	if (!NamesData.empty())
	{
		const auto RawDataSize = (NamesData.size() + 1) * sizeof(wchar_t);
		if (auto hMemory = os::memory::global::alloc(GMEM_MOVEABLE, sizeof(DROPFILES) + RawDataSize))
		{
			if (const auto Drop = os::memory::global::lock<LPDROPFILES>(hMemory))
			{
				Drop->pFiles=sizeof(DROPFILES);
				Drop->pt.x=0;
				Drop->pt.y=0;
				Drop->fNC = TRUE;
				Drop->fWide = TRUE;
				memcpy(Drop.get() + 1, NamesData.data(), RawDataSize);
				Clear();
				if(SetData(CF_HDROP, std::move(hMemory)))
				{
					if(bMoved)
					{
						if (auto hMemoryMove = os::memory::global::copy<DWORD>(DROPEFFECT_MOVE))
						{
							if(SetData(RegisterFormat(FCF_CFSTR_PREFERREDDROPEFFECT), std::move(hMemoryMove)))
							{
								Result = true;
							}
						}
					}
					else
						Result = true;
				}
			}
		}
	}
	return Result;
}

bool Clipboard::Get(string& data)
{
	bool Result = false;

	if (auto hClipData = GetData(CF_UNICODETEXT))
	{
		if (const auto ClipAddr = os::memory::global::lock<const wchar_t*>(hClipData))
		{
			Result = true;
			data = ClipAddr.get();
		}
	}
	else if ((hClipData = GetData(CF_HDROP)) != nullptr)
	{
		if (const auto Files = os::memory::global::lock<const DROPFILES*>(hClipData))
		{
			const auto StartA=reinterpret_cast<const char*>(Files.get())+Files->pFiles;
			const auto Start = reinterpret_cast<const wchar_t*>(StartA);
			if(Files->fWide)
			{
				FOR(const auto& i, os::enum_strings(Start))
				{
					data.append(i.data(), i.size()).append(L"\r\n");
				}
			}
			else
			{
				FOR(const auto& i, (os::enum_strings_t<const char*, const char*>(StartA)))
				{
					data.append(wide(std::string(i.data(), i.size()))).append(L"\r\n");
				}
			}
			Result = true;
		}
	}
	return Result;
}

// max - без учета символа конца строки!
bool Clipboard::GetEx(int max, string& data)
{
	bool Result = false;
	if (const auto hClipData = GetData(CF_UNICODETEXT))
	{
		if (const auto ClipAddr = os::memory::global::lock<const wchar_t*>(hClipData))
		{
			data.assign(ClipAddr.get(), std::min(max, StrLength(ClipAddr.get())));
			Result = true;
		}
	}

	return Result;
}

bool Clipboard::GetFormat(FAR_CLIPBOARD_FORMAT Format, string& data)
{
	bool Result = false;

	auto FormatType = RegisterFormat(Format);

	if (!FormatType)
		return false;

	bool ColumnSelect = IsFormatAvailable(RegisterFormat(FCF_MSDEVCOLUMNSELECT));
	if (!ColumnSelect)
	{
		if (const auto hClipData = GetData(RegisterFormat(FCF_BORLANDIDEVBLOCK)))
			if (const auto ClipAddr = os::memory::global::lock<const char*>(hClipData))
				ColumnSelect = (*ClipAddr & 0x02) != 0;
	}

	bool isOEMVBlock = false;

	if (!ColumnSelect)
	{
		if (Format == FCF_VERTICALBLOCK_UNICODE && !IsFormatAvailable(FormatType))
		{
			FormatType = RegisterFormat(FCF_VERTICALBLOCK_OEM);
			isOEMVBlock = true;
		}

		if (!FormatType || !IsFormatAvailable(FormatType))
			return false;
	}

	if (const auto hClipData = GetData(ColumnSelect? CF_UNICODETEXT : FormatType))
	{
		if (const auto ClipAddr = os::memory::global::lock<const wchar_t*>(hClipData))
		{
			data = isOEMVBlock? wide(reinterpret_cast<const char*>(ClipAddr.get())) : ClipAddr.get();
			Result = true;
		}
	}

	return Result;
}

bool Clipboard::InternalCopy(bool FromWin)
{
	bool Ret=false;
	const bool OldUseInternalClipboard = SetUseInternalClipboardState(!FromWin);

	UINT uFormat;
	auto hClipData = GetData(uFormat = CF_UNICODETEXT);

	if (!hClipData)
		hClipData = GetData(uFormat = CF_HDROP);

	if (hClipData)
	{
		SetUseInternalClipboardState(!Clipboard::GetUseInternalClipboardState());
		SetData(uFormat,hClipData);
		Ret=true;
	}

	SetUseInternalClipboardState(OldUseInternalClipboard);

	return Ret;
}

/* ------------------------------------------------------------ */
int SetClipboard(const wchar_t* Data, size_t Size)
{
	Clipboard clip;
	return clip.Open()? clip.SetText(Data, Size) : FALSE;
}

int SetClipboardFormat(FAR_CLIPBOARD_FORMAT Format,const wchar_t *Data, size_t Size)
{
	Clipboard clip;
	return clip.Open()? clip.SetFormat(Format, Data, Size) : FALSE;
}

bool GetClipboard(string& data)
{
	Clipboard clip;
	return clip.Open() ? clip.Get(data) : false;
}

// max - без учета символа конца строки!
bool GetClipboardEx(int max, string& data)
{
	Clipboard clip;
	return clip.Open()? clip.GetEx(max, data) : false;

}

bool GetClipboardFormat(FAR_CLIPBOARD_FORMAT Format, string& data)
{
	Clipboard clip;
	return clip.Open()? clip.GetFormat(Format, data) : false;
}

bool ClearInternalClipboard()
{
	bool OldState = Clipboard::SetUseInternalClipboardState(true);

	Clipboard clip;

	if (!clip.Open())
		return false;

	bool ret = clip.Clear();

	clip.Close();

	Clipboard::SetUseInternalClipboardState(OldState);

	return ret;
}
