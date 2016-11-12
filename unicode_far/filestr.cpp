﻿/*
filestr.cpp

Класс GetFileString
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

#include "filestr.hpp"
#include "nsUniversalDetectorEx.hpp"
#include "config.hpp"
#include "codepage_selection.hpp"
#include "strmix.hpp"

static constexpr size_t DELTA = 1024;
static constexpr size_t ReadBufCount = 0x2000;

enum EolType
{
	FEOL_NONE,
	// \r\n
	FEOL_WINDOWS,
	// \n
	FEOL_UNIX,
	// \r
	FEOL_MAC,
	// \r\r (это не реальное завершение строки, а состояние парсера)
	FEOL_MAC2,
	// \r\r\n (появление таких концов строк вызвано багом Notepad-а)
	FEOL_NOTEPAD
};

bool IsTextUTF8(const char* Buffer, size_t Length, bool& PureAscii)
{
	bool Ascii=true;
	size_t Octets=0;
	size_t LastOctetsPos = 0;
	const size_t MaxCharSize = 4;

	for (size_t i=0; i<Length; i++)
	{
		BYTE c=Buffer[i];

		if (c&0x80)
			Ascii=false;

		if (Octets)
		{
			if ((c&0xC0)!=0x80)
				return false;

			Octets--;
		}
		else
		{
			LastOctetsPos = i;

			if (c&0x80)
			{
				while (c&0x80)
				{
					c <<= 1;
					Octets++;
				}

				Octets--;

				if (!Octets)
					return false;
			}
		}
	}

	PureAscii = Ascii;
	return (!Octets || Length - LastOctetsPos < MaxCharSize) && !Ascii;
}

GetFileString::GetFileString(os::fs::file& SrcFile, uintptr_t CodePage) :
	SrcFile(SrcFile),
	m_CodePage(CodePage),
	ReadPos(0),
	ReadSize(0),
	Peek(false),
	LastLength(0),
	LastString(nullptr),
	LastResult(false),
	m_Eol(m_CodePage),
	SomeDataLost(false),
	bCrCr(false)
{
	if (m_CodePage == CP_UNICODE || m_CodePage == CP_REVERSEBOM)
		m_wReadBuf.resize(ReadBufCount);
	else
		m_ReadBuf.resize(ReadBufCount);

	m_wStr.reserve(DELTA);
}

bool GetFileString::PeekString(LPWSTR* DestStr, size_t& Length)
{
	if(!Peek)
	{
		LastResult = GetString(DestStr, Length);
		Peek = true;
		LastString = *DestStr;
		LastLength = Length;
	}
	else
	{
		*DestStr = LastString;
		Length = LastLength;
	}
	return LastResult;
}

bool GetFileString::GetString(string& str)
{
	wchar_t* s;
	size_t len;
	if (GetString(&s, len))
	{
		str.assign(s, len);
		return true;
	}
	return false;
}

bool GetFileString::GetString(LPWSTR* DestStr, size_t& Length)
{
	if(Peek)
	{
		Peek = false;
		*DestStr = LastString;
		Length = LastLength;
		return LastResult;
	}

	switch (m_CodePage)
	{
	case CP_UNICODE:
	case CP_REVERSEBOM:
		if (GetTString(m_wReadBuf, m_wStr, m_CodePage == CP_REVERSEBOM))
		{
			*DestStr = m_wStr.data();
			Length = m_wStr.size() - 1;
			return true;
		}
		return false;

	case CP_UTF8:
	case CP_UTF7:
		{
			std::vector<char> CharStr;
			CharStr.reserve(DELTA);
			bool ExitCode = GetTString(m_ReadBuf, CharStr);

			if (ExitCode)
			{
				Utf::errors errs;
				const auto len = Utf::get_chars(m_CodePage, CharStr.data(), CharStr.size() - 1, m_wStr.data(), m_wStr.size(), &errs);

				SomeDataLost = SomeDataLost || errs.Conversion.Error;
				if (len > m_wStr.size())
				{
					resize_nomove(m_wStr, len + 1);
					Utf::get_chars(m_CodePage, CharStr.data(), CharStr.size() - 1, m_wStr.data(), len, nullptr);
				}

				m_wStr.resize(len+1);
				m_wStr[len] = L'\0';
				*DestStr = m_wStr.data();
				Length = m_wStr.size() - 1;
			}
			return ExitCode;
		}

	default:
		{
			std::vector<char> CharStr;
			CharStr.reserve(DELTA);
			bool ExitCode = GetTString(m_ReadBuf, CharStr);

			if (ExitCode)
			{
				DWORD Result = ERROR_SUCCESS;
				size_t nResultLength = 0;
				bool bGet = false;
				m_wStr.resize(CharStr.size());

				if (!SomeDataLost)
				{
					nResultLength = MultiByteToWideChar(m_CodePage, SomeDataLost ? 0 : MB_ERR_INVALID_CHARS, CharStr.data(), static_cast<int>(CharStr.size()), m_wStr.data(), static_cast<int>(m_wStr.size()));

					if (!nResultLength)
					{
						Result = GetLastError();
						if (Result == ERROR_NO_UNICODE_TRANSLATION || (Result == ERROR_INVALID_FLAGS && IsNoFlagsCodepage(m_CodePage)))
						{
							SomeDataLost = true;
							bGet = true;
						}
					}
				}
				else
				{
					bGet = true;
				}
				if (bGet)
				{
					nResultLength = encoding::get_chars(m_CodePage, CharStr, m_wStr);
					if (nResultLength > m_wStr.size())
					{
						Result = ERROR_INSUFFICIENT_BUFFER;
					}
				}
				if (Result == ERROR_INSUFFICIENT_BUFFER)
				{
					nResultLength = encoding::get_chars_count(m_CodePage, CharStr);
					m_wStr.resize(nResultLength);
					encoding::get_chars(m_CodePage, CharStr, m_wStr);
				}

				m_wStr.resize(nResultLength);

				*DestStr = m_wStr.data();
				Length = m_wStr.size() - 1;
				ExitCode = !m_wStr.empty();
			}

			return ExitCode;
		}
	}
}

template<class T>
bool GetFileString::GetTString(std::vector<T>& From, std::vector<T>& To, bool bBigEndian)
{
	bool ExitCode = true;
	T* ReadBufPtr = ReadPos < ReadSize ? From.data() + ReadPos / sizeof(T) : nullptr;

	To.clear();

	// Обработка ситуации, когда у нас пришёл двойной \r\r, а потом не было \n.
	// В этом случаем считаем \r\r двумя MAC окончаниями строк.
	if (bCrCr)
	{
		To.emplace_back(m_Eol.cr<T>());
		bCrCr = false;
	}
	else
	{
		EolType Eol = FEOL_NONE;
		for (;;)
		{
			if (ReadPos >= ReadSize)
			{
				if (!(SrcFile.Read(From.data(), ReadBufCount*sizeof(T), ReadSize) && ReadSize))
				{
					if (To.empty())
					{
						ExitCode = false;
					}
					break;
				}

				if (bBigEndian && sizeof(T) != 1)
				{
					swap_bytes(From.data(), From.data(), ReadSize);
				}

				ReadPos = 0;
				ReadBufPtr = From.data();
			}
			if (Eol == FEOL_NONE)
			{
				// UNIX
				if (*ReadBufPtr == m_Eol.lf<T>())
				{
					Eol = FEOL_UNIX;
				}
				// MAC / Windows? / Notepad?
				else if (*ReadBufPtr == m_Eol.cr<T>())
				{
					Eol = FEOL_MAC;
				}
			}
			else if (Eol == FEOL_MAC)
			{
				// Windows
				if (*ReadBufPtr == m_Eol.lf<T>())
				{
					Eol = FEOL_WINDOWS;
				}
				// Notepad?
				else if (*ReadBufPtr == m_Eol.cr<T>())
				{
					Eol = FEOL_MAC2;
				}
				else
				{
					break;
				}
			}
			else if (Eol == FEOL_WINDOWS || Eol == FEOL_UNIX)
			{
				break;
			}
			else if (Eol == FEOL_MAC2)
			{
				// Notepad
				if (*ReadBufPtr == m_Eol.lf<T>())
				{
					Eol = FEOL_NOTEPAD;
				}
				else
				{
					// Пришёл \r\r, а \n не пришёл, поэтому считаем \r\r двумя MAC окончаниями строк
					To.pop_back();
					bCrCr = true;
					break;
				}
			}
			else
			{
				break;
			}

			ReadPos += sizeof(T);

			To.emplace_back(*ReadBufPtr);
			++ReadBufPtr;
		}
	}
	To.emplace_back(0);
	return ExitCode;
}

bool GetFileFormat(
	os::fs::file& file, uintptr_t& nCodePage, bool* pSignatureFound, bool bUseHeuristics, bool* pPureAscii)
{
	DWORD dwTemp = 0;
	bool bSignatureFound = false;
	bool bDetect = false;
	bool bPureAscii = false;

	size_t Readed = 0;
	if (file.Read(&dwTemp, sizeof(dwTemp), Readed) && Readed > 1 ) // minimum signature size is 2 bytes
	{
		if (LOWORD(dwTemp) == SIGN_UNICODE)
		{
			nCodePage = CP_UNICODE;
			file.SetPointer(2, nullptr, FILE_BEGIN);
			bSignatureFound = true;
		}
		else if (LOWORD(dwTemp) == SIGN_REVERSEBOM)
		{
			nCodePage = CP_REVERSEBOM;
			file.SetPointer(2, nullptr, FILE_BEGIN);
			bSignatureFound = true;
		}
		else if ((dwTemp & 0x00FFFFFF) == SIGN_UTF8)
		{
			nCodePage = CP_UTF8;
			file.SetPointer(3, nullptr, FILE_BEGIN);
			bSignatureFound = true;
		}
		else
		{
			file.SetPointer(0, nullptr, FILE_BEGIN);
		}
	}

	if (bSignatureFound)
	{
		bDetect = true;
	}
	else if (bUseHeuristics)
	{
		file.SetPointer(0, nullptr, FILE_BEGIN);
		size_t Size = 0x8000; // BUGBUG. TODO: configurable
		char_ptr Buffer(Size);
		size_t ReadSize = 0;
		bool ReadResult = file.Read(Buffer.get(), Size, ReadSize);
		file.SetPointer(0, nullptr, FILE_BEGIN);

		bPureAscii = ReadResult && !ReadSize; // empty file == pure ascii

		if (ReadResult && ReadSize)
		{
			// BUGBUG MSDN documents IS_TEXT_UNICODE_BUFFER_TOO_SMALL but there is no such thing
			if (ReadSize > 1)
			{
				int test = IS_TEXT_UNICODE_UNICODE_MASK | IS_TEXT_UNICODE_REVERSE_MASK | IS_TEXT_UNICODE_NOT_UNICODE_MASK | IS_TEXT_UNICODE_NOT_ASCII_MASK;

				IsTextUnicode(Buffer.get(), static_cast<int>(ReadSize), &test); // return value is ignored, it's ok.

				if (!(test & IS_TEXT_UNICODE_NOT_UNICODE_MASK) && (test & IS_TEXT_UNICODE_NOT_ASCII_MASK))
				{
					if (test & IS_TEXT_UNICODE_UNICODE_MASK)
					{
						nCodePage = CP_UNICODE;
						bDetect = true;
					}
					else if (test & IS_TEXT_UNICODE_REVERSE_MASK)
					{
						nCodePage = CP_REVERSEBOM;
						bDetect = true;
					}
				}

				if (!bDetect && IsTextUTF8(Buffer.get(), ReadSize, bPureAscii))
				{
					nCodePage = CP_UTF8;
					bDetect = true;
				}
			}

			if (!bDetect && !bPureAscii)
			{
				int cp = GetCpUsingUniversalDetector(Buffer.get(), ReadSize);
				// This whole block shouldn't be here
				if ( cp >= 0 )
				{
					if (Global->Opt->strNoAutoDetectCP.Get() == L"-1")
					{
						if ( Global->Opt->CPMenuMode )
						{
							if ( static_cast<UINT>(cp) != GetACP() && static_cast<UINT>(cp) != GetOEMCP() )
							{
								long long selectType = Codepages().GetFavorite(cp);
								if (0 == (selectType & CPST_FAVORITE))
									cp = -1;
							}
						}
					}
					else
					{
						const auto BannedCpList = split<std::vector<string>>(Global->Opt->strNoAutoDetectCP, STLF_UNIQUE);

						if (std::find(ALL_CONST_RANGE(BannedCpList), str(cp)) != BannedCpList.cend())
						{
							cp = -1;
						}
					}
				}

				if (cp != -1)
				{
					nCodePage = cp;
					bDetect = true;
				}
			}
		}
	}

	if (pSignatureFound)
		*pSignatureFound = bSignatureFound;

	if (pPureAscii)
		*pPureAscii = bPureAscii;

	return bDetect;
}
