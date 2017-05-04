﻿#ifndef DATETIME_HPP_58256A07_E483_4DB7_9DAC_DFA9D90D8A32
#define DATETIME_HPP_58256A07_E483_4DB7_9DAC_DFA9D90D8A32
#pragma once

/*
datetime.hpp

Функции для работы с датой и временем
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

inline auto get_local_time() { SYSTEMTIME Time; GetLocalTime(&Time); return Time; }
inline auto get_utc_time() { SYSTEMTIME Time; GetSystemTime(&Time); return Time; }
inline auto get_utc_time_as_filetime() { FILETIME Time; GetSystemTimeAsFileTime(&Time); return Time; }

DWORD ConvertYearToFull(DWORD ShortYear);

void OnIntlSettingsChange();

void GetFileDateAndTime(const string& Src, LPWORD Dst, size_t Count, wchar_t Separator);
void StrToDateTime(const string& CDate, const string& CTime, FILETIME &ft, int DateFormat, wchar_t DateSeparator, wchar_t TimeSeparator, bool bRelative=false);
void ConvertDate(const FILETIME &ft,string &strDateText, string &strTimeText,int TimeLength, int Brief=FALSE,int TextMonth=FALSE,int FullYear=0);
void ConvertRelativeDate(const FILETIME &ft,string &strDaysText,string &strTimeText);

string StrFTime(const wchar_t* Format, const tm* t);
string MkStrFTime(const wchar_t* Format = nullptr);

inline auto FileTimeToUI64(const FILETIME& ft)
{
	return ULARGE_INTEGER {ft.dwLowDateTime, ft.dwHighDateTime}.QuadPart;
}

inline auto UI64ToFileTime(LARGE_INTEGER time)
{
	return FILETIME{ time.LowPart, static_cast<DWORD>(time.HighPart) };
}

inline auto UI64ToFileTime(unsigned long long time)
{
	LARGE_INTEGER i;
	i.QuadPart = time;
	return UI64ToFileTime(i);
}

inline int CompareFileTime(const FILETIME& a, const FILETIME& b)
{
	const long long Result = FileTimeToUI64(a) - FileTimeToUI64(b);
	return Result ? (Result > 0 ? 1 : -1) : 0;
}

inline bool operator==(const FILETIME& a, const FILETIME& b)
{
	return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

inline bool operator!=(const FILETIME& a, const FILETIME& b)
{
	return !(a == b);
}

inline bool operator<(const FILETIME& a, const FILETIME& b)
{
	return CompareFileTime(a, b) < 0;
}

inline unsigned long long GetCurrentUTCTimeInUI64()
{
	return FileTimeToUI64(get_utc_time_as_filetime());
}

bool Utc2Local(const FILETIME &ft, SYSTEMTIME &lst);
bool Local2Utc(const FILETIME &lft, SYSTEMTIME &st);
bool Utc2Local(const SYSTEMTIME &st, FILETIME &lft);
bool Local2Utc(const SYSTEMTIME &lst, FILETIME &ft);

class time_check: noncopyable, public conditional<time_check>
{
	using clock_type = std::chrono::steady_clock;

public:
	enum class mode { delayed, immediate };
	time_check(mode Mode, clock_type::duration Interval):
		m_Begin(Mode == mode::delayed? clock_type::now() : clock_type::now() - Interval),
		m_Interval(Interval) {}

	void reset(clock_type::time_point Value = clock_type::now()) const { m_Begin = Value; }

	bool operator!() const noexcept
	{
		const auto Current = clock_type::now();
		if (m_Interval != 0s && Current - m_Begin > m_Interval)
		{
			reset(Current);
			return false;
		}
		return true;
	}

private:
	mutable clock_type::time_point m_Begin;
	const clock_type::duration m_Interval;
};


#endif // DATETIME_HPP_58256A07_E483_4DB7_9DAC_DFA9D90D8A32
