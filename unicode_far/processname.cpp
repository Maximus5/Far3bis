﻿/*
processname.cpp

Обработать имя файла: сравнить с маской, масками, сгенерировать по маске
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

#include "processname.hpp"
#include "pathmix.hpp"
#include "local.hpp"

/* $ 09.10.2000 IS
    Генерация нового имени по маске
    (взял из ShellCopy::ShellCopyConvertWildcards)
*/
// На основе имени файла (Src) и маски (Dest) генерируем новое имя
// SelectedFolderNameLength - длина каталога. Например, есть
// каталог dir1, а в нем файл file1. Нужно сгенерировать имя по маске для dir1.
// Параметры могут быть следующими: Src="dir1", SelectedFolderNameLength=0
// или Src="dir1\\file1", а SelectedFolderNameLength=4 (длина "dir1")
bool ConvertWildcards(const string& SrcName, string &strDest, int SelectedFolderNameLength)
{
	size_t DestNamePos = PointToName(strDest.data()) - strDest.data();

	if (strDest.find_first_of(L"*?", DestNamePos) == string::npos)
	{
		return false;
	}

	const string strWildName = strDest.substr(DestNamePos);
	const string strPartAfterFolderName = SelectedFolderNameLength? SrcName.substr(SelectedFolderNameLength) : string{};

	const string strSrc = SelectedFolderNameLength? SrcName.substr(0, SelectedFolderNameLength) : SrcName;
	const wchar_t *Src = strSrc.data();
	const wchar_t *SrcNamePtr = PointToName(Src);

	strDest.resize(strDest.size() + SrcName.size());

	size_t BeforeNameLength = DestNamePos? 0 : SrcNamePtr-Src;

	const wchar_t *SrcNameDot = wcsrchr(SrcNamePtr, L'.');
	const wchar_t *CurWildPtr = strWildName.data();

	while (*CurWildPtr)
	{
		switch (*CurWildPtr)
		{
		case L'?':
			CurWildPtr++;

			if (*SrcNamePtr)
			{
				strDest[DestNamePos++] = *(SrcNamePtr++);
			}
			break;

		case L'*':
			CurWildPtr++;
			while (*SrcNamePtr)
			{
				if (*CurWildPtr==L'.' && SrcNameDot && !wcschr(CurWildPtr+1,L'.'))
				{
					if (SrcNamePtr==SrcNameDot)
						break;
				}
				else if (*SrcNamePtr==*CurWildPtr)
				{
					break;
				}
				strDest[DestNamePos++] = *(SrcNamePtr++);
			}
			break;

		case L'.':
			CurWildPtr++;
			strDest[DestNamePos++] = L'.';

			if (wcspbrk(CurWildPtr,L"*?"))
				while (*SrcNamePtr)
					if (*(SrcNamePtr++)==L'.')
						break;
			break;

		default:
			strDest[DestNamePos++] = *(CurWildPtr++);
			if (*SrcNamePtr && *SrcNamePtr!=L'.')
				SrcNamePtr++;
			break;
		}
	}
	strDest.resize(DestNamePos);

	if (!strDest.empty() && strDest.back() == L'.')
		strDest.pop_back();

	strDest.insert(0, Src, BeforeNameLength);

	if (SelectedFolderNameLength)
		strDest += strPartAfterFolderName; //BUGBUG???, was src in 1.7x

	return true;
}

bool CmpName(const wchar_t *pattern, const wchar_t *str, bool skippath, bool CmpNameSearchMode)
{
	if (!pattern || !str)
		return false;

	if (skippath)
		str = PointToName(str);

	for (;; ++str)
	{
		/* $ 01.05.2001 DJ
		   используем инлайновые версии
		*/
		wchar_t stringc=Upper(*str);
		wchar_t patternc=Upper(*pattern++);

		switch (patternc)
		{
		case 0:
			return !stringc;

		case L'?':
			if (!stringc)
				return false;
			break;

		case L'*':
			if (!*pattern)
				return true;

			/* $ 01.05.2001 DJ
				оптимизированная ветка работает и для имен с несколькими
				точками
			*/
			if (*pattern==L'.')
			{
				if (pattern[1]==L'*' && !pattern[2])
					return true;

				if (!wcspbrk(pattern, L"*?["))
				{
					const wchar_t *dot = wcsrchr(str, L'.');

					if (!pattern[1])
						return !dot || !dot[1];

					const wchar_t *patdot = wcschr(pattern+1, L'.');

					if (patdot  && !dot)
						return false;

					if (!patdot && dot )
						return !StrCmpI(pattern+1,dot+1);
				}
			}

			do
			{
				if(CmpName(pattern,str,false,CmpNameSearchMode))
					return true;
			}
			while (*str++);
			return false;

		case L'[':
			{
				if (!wcschr(pattern,L']'))
				{
					if (patternc != stringc)
						return false;

					break;
				}

				if (*pattern && *(pattern+1)==L']')
				{
					if (*pattern!=*str)
						return false;

					pattern+=2;
					break;
				}

				int match = 0;
				wchar_t rangec;
				while ((rangec = Upper(*pattern++)) != 0)
				{
					if (rangec == L']')
					{
						if (match)
							break;
						else
							return false;
					}

					if (match)
						continue;

					if (rangec == L'-' && *(pattern - 2) != L'[' && *pattern != L']')
					{
						match = (stringc <= Upper(*pattern) &&
									Upper(*(pattern - 2)) <= stringc);
						pattern++;
					}
					else
						match = (stringc == rangec);
				}

				if (!rangec)
					return false;
			}
			break;

		default:
			if (patternc != stringc)
			{
				if (patternc==L'.' && !stringc && !CmpNameSearchMode)
					return *pattern != L'.' && CmpName(pattern, str, true, CmpNameSearchMode);
				else
					return false;
			}
			break;
		}
	}
}
