﻿#ifndef FORMAT_HPP_27C3F464_170B_432E_9D44_3884DDBB95AC
#define FORMAT_HPP_27C3F464_170B_432E_9D44_3884DDBB95AC
#pragma once

/*
format.hpp

Форматирование строк
*/
/*
Copyright © 2009 Far Group
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
namespace fmt
{
	template<typename T, T Default> class ManipulatorTemplate
	{
	public:
		ManipulatorTemplate(T Value=DefaultValue):Value(Value) {}
		T GetValue() const { return Value; }
		static T GetDefault() { return DefaultValue; }
	private:
		const T Value;
		static const T DefaultValue = Default;
	};

	typedef ManipulatorTemplate<size_t, 0> MinWidth;
	typedef ManipulatorTemplate<size_t, static_cast<size_t>(-1)> MaxWidth;
	typedef ManipulatorTemplate<size_t, 1 /*any*/> ExactWidth;
	typedef ManipulatorTemplate<wchar_t, L' '> FillChar;
	typedef ManipulatorTemplate<int, 10> Radix;

	enum AlignType
	{
		A_LEFT,
		A_RIGHT,
	};
	typedef ManipulatorTemplate<AlignType, A_RIGHT> Align;

	template<AlignType T>class SimpleAlign {};
	typedef SimpleAlign<A_LEFT> LeftAlign;
	typedef SimpleAlign<A_RIGHT> RightAlign;

	class Flush {};
};

class BaseFormat
{
protected:
	BaseFormat();
	virtual ~BaseFormat() = default;

	virtual BaseFormat& Flush() { return *this; }

	// attributes
	BaseFormat& SetMaxWidth(size_t Precision);
	BaseFormat& SetMinWidth(size_t Width);
	BaseFormat& SetExactWidth(size_t ExactWidth);
	BaseFormat& SetAlign(fmt::AlignType Align);
	BaseFormat& SetFillChar(wchar_t Char);
	BaseFormat& SetRadix(int Radix);

	BaseFormat& Put(LPCWSTR Data, size_t Length);

	// data
	BaseFormat& operator<<(INT64 Value);
	BaseFormat& operator<<(UINT64 Value);
	BaseFormat& operator<<(short Value);
	BaseFormat& operator<<(USHORT Value);
	BaseFormat& operator<<(int Value);
	BaseFormat& operator<<(UINT Value);
	BaseFormat& operator<<(long Value);
	BaseFormat& operator<<(ULONG Value);
	BaseFormat& operator<<(wchar_t Value);
	BaseFormat& operator<<(LPCWSTR Data);
	BaseFormat& operator<<(const string& String);

	// manipulators
	BaseFormat& operator<<(const fmt::MinWidth& Manipulator);
	BaseFormat& operator<<(const fmt::MaxWidth& Manipulator);
	BaseFormat& operator<<(const fmt::ExactWidth& Manipulator);
	BaseFormat& operator<<(const fmt::FillChar& Manipulator);
	BaseFormat& operator<<(const fmt::Radix& Manipulator);
	BaseFormat& operator<<(const fmt::Align& Manipulator);
	BaseFormat& operator<<(const fmt::LeftAlign& Manipulator);
	BaseFormat& operator<<(const fmt::RightAlign& Manipulator);
	BaseFormat& operator<<(const fmt::Flush& Manipulator);

	virtual void Commit(const string& Data)=0;

private:
	template<class T>
	BaseFormat& ToString(T Value);
	void Reset();

	size_t m_MinWidth;
	size_t m_MaxWidth;
	wchar_t m_FillChar;
	fmt::AlignType m_Align;
	int m_Radix;
};

class FormatString:public BaseFormat, public string
{
public:
	template<class T>
	FormatString& operator<<(const T& param) {return static_cast<FormatString&>(BaseFormat::operator<<(param));}

private:
	virtual void Commit(const string& Data) override;
};

class FormatScreen: noncopyable, public BaseFormat
{
public:
	template<class T>
	FormatScreen& operator<<(const T& param) {return static_cast<FormatScreen&>(BaseFormat::operator<<(param));}

private:
	virtual void Commit(const string& Data) override;
};

enum LNGID: int;

namespace detail
{
	class formatter: public BaseFormat
	{
	public:
		formatter(LNGID MessageId);
		formatter(string Str): m_Data(std::move(Str)) {}
		string&& str() { return std::move(m_Data); }
		template<class T>
		formatter& operator<<(const T& param) {return static_cast<formatter&>(BaseFormat::operator<<(param));}

	protected:
		string m_Data;
		size_t Iteration{};

	private:
		virtual void Commit(const string& Data) override;
	};

	class old_style_formatter: public formatter
	{
	public:
		old_style_formatter(string Str): formatter(std::move(Str)) {}

	private:
		virtual void Commit(const string& Data) override;
	};
}

namespace detail
{
	template<typename formatter>
	void string_format_impl(formatter&) {}

	template<typename formatter, typename arg, typename... args>
	void string_format_impl(formatter& Formatter, arg&& Arg, args&&... Args)
	{
		Formatter << std::forward<arg>(Arg);
		string_format_impl(Formatter, std::forward<args>(Args)...);
	}
}

template<typename formatter = detail::formatter, typename T, typename... args>
string string_format(T&& Format, args&&... Args)
{
	formatter Formatter(std::forward<T>(Format));
	detail::string_format_impl(Formatter, std::forward<args>(Args)...);
	return Formatter.str();
}

#endif // FORMAT_HPP_27C3F464_170B_432E_9D44_3884DDBB95AC
