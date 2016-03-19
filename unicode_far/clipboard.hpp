﻿#ifndef CLIPBOARD_HPP_989E040C_4D10_4D7C_88C0_5EF499171878
#define CLIPBOARD_HPP_989E040C_4D10_4D7C_88C0_5EF499171878
#pragma once

/*
clipboard.hpp

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

class default_clipboard_mode
{
public:
	enum mode { system, internal };
	static void set(mode Mode);
	static mode get();

private:
	static mode m_Mode;
};

enum FAR_CLIPBOARD_FORMAT: int;

class Clipboard
{
public:
	static Clipboard& GetInstance(default_clipboard_mode::mode Mode);
	virtual ~Clipboard() {}

	virtual bool Open() = 0;
	virtual bool Close() = 0;
	virtual bool Clear() = 0;

	bool SetText(const wchar_t *Data, size_t Size);
	bool SetText(const wchar_t *Data) { return SetText(Data, wcslen(Data)); }
	bool SetText(const string& Data) { return SetText(Data.data(), Data.size()); }

	bool SetVText(const wchar_t *Data, size_t Size);
	bool SetVText(const wchar_t *Data) { return SetVText(Data, wcslen(Data)); }
	bool SetVText(const string& Data) { return SetVText(Data.data(), Data.size()); }

	bool SetHDROP(const string& NamesData, bool bMoved);

	bool GetText(string& data) const;
	bool GetVText(string& data) const;

protected:
	Clipboard(): m_Opened() {}

	bool m_Opened;

private:
	virtual bool SetData(UINT uFormat, os::memory::global::ptr&& hMem) = 0;
	virtual HANDLE GetData(UINT uFormat) const = 0;
	virtual UINT RegisterFormat(FAR_CLIPBOARD_FORMAT Format) const = 0;
	virtual bool IsFormatAvailable(UINT Format) const = 0;

	bool GetHDROPAsText(string& data) const;
};

class clipboard_accessor:noncopyable
{
public:
	clipboard_accessor(default_clipboard_mode::mode Mode = default_clipboard_mode::get()): m_Mode(Mode) {}
	Clipboard* operator->() const { return &Clipboard::GetInstance(m_Mode); }
	~clipboard_accessor() { Clipboard::GetInstance(m_Mode).Close(); }

private:
	default_clipboard_mode::mode m_Mode;
};


bool SetClipboardText(const wchar_t* Data, size_t Size);
inline bool SetClipboardText(const wchar_t* Data) { return SetClipboardText(Data, wcslen(Data)); }
inline bool SetClipboardText(const string& Data) { return SetClipboardText(Data.data(), Data.size()); }

bool SetClipboardVText(const wchar_t *Data, size_t Size);
inline bool SetClipboardVText(const wchar_t* Data) { return SetClipboardVText(Data, wcslen(Data)); }
inline bool SetClipboardVText(const string& Data) { return SetClipboardVText(Data.data(), Data.size()); }

bool GetClipboardText(string& data);
bool GetClipboardVText(string& data);

bool ClearInternalClipboard();

bool CopyData(const clipboard_accessor& From, clipboard_accessor& To);

#endif // CLIPBOARD_HPP_989E040C_4D10_4D7C_88C0_5EF499171878
