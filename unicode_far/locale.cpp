﻿/*
locale.cpp

*/
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

#include "headers.hpp"
#pragma hdrstop

#include "locale.hpp"
#include "config.hpp"

int locale::GetDateFormat()
{
	int Result = 0;
	GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_IDATE|LOCALE_RETURN_NUMBER,reinterpret_cast<LPWSTR>(&Result),sizeof(Result)/sizeof(WCHAR));
	return Result;
}

wchar_t locale::GetDateSeparator()
{
	wchar_t Info[100];
	GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_SDATE, Info, static_cast<int>(std::size(Info)));
	return *Info;
}

wchar_t locale::GetTimeSeparator()
{
	wchar_t Info[100];
	GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_STIME, Info, static_cast<int>(std::size(Info)));
	return *Info;
}

wchar_t locale::GetDecimalSeparator()
{
	wchar_t Separator[4];
	GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_SDECIMAL,Separator, static_cast<int>(std::size(Separator)));
	if (Global && Global->Opt && Global->Opt->FormatNumberSeparators.size() > 1)
		*Separator = Global->Opt->FormatNumberSeparators[1];
	return *Separator;
}

wchar_t locale::GetThousandSeparator()
{
	wchar_t Separator[4];
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, Separator, static_cast<int>(std::size(Separator)));
	if (Global && Global->Opt && !Global->Opt->FormatNumberSeparators.empty())
		*Separator = Global->Opt->FormatNumberSeparators[0];
	return *Separator;
}

string locale::GetValue(LCID lcid, LCTYPE id)
{
	string Result;
	const auto Size = GetLocaleInfo(lcid, id, nullptr, 0);
	if (Size != 0)
	{
		wchar_t_ptr Buffer(Size);
		GetLocaleInfo(lcid, id, Buffer.get(), Size);
		Result.assign(Buffer.get(), Size - 1);
	}
	return Result;
}
