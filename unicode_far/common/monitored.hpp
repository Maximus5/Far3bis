#pragma once

/*
Copyright � 2015 Far Group
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

template<class T>
class monitored
{
public:
	monitored(): m_Value(), m_Touched() {}
	monitored(const T& Value): m_Value(Value), m_Touched() {}
	monitored(const monitored& rhs): m_Value(rhs.m_Value), m_Touched() {}

	monitored(T&& Value) noexcept: m_Value(std::move(Value)), m_Touched() {}
	monitored(monitored&& rhs) noexcept: m_Value(std::move(rhs.m_Value)), m_Touched() {}

	monitored& operator=(const T& Value) { m_Value = Value; m_Touched = true; return *this; }
	monitored& operator=(const monitored& rhs) { m_Value = rhs.Value; m_Touched = true; return *this; }

	monitored& operator=(T&& Value) noexcept { m_Value = std::move(Value); m_Touched = true; return *this; }
	monitored& operator=(monitored&& rhs) noexcept { m_Value = std::move(rhs.m_Value); m_Touched = true; return *this; }

	void swap(monitored& rhs) noexcept
	{
		using std::swap;
		swap(m_Value, rhs.m_Value);
		swap(m_Touched, rhs.m_Touched);
	}

	FREE_SWAP(monitored);

	T& value() { return m_Value; }
	const T& value() const { return m_Value; }
	operator T&() { return m_Value; }
	operator const T&() const { return m_Value; }

	bool touched() const noexcept { return m_Touched; }

private:
	T m_Value;
	bool m_Touched;
};
