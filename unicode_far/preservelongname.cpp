﻿/*
preservelongname.cpp

class PreserveLongName
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

#include "preservelongname.hpp"
#include "pathmix.hpp"

PreserveLongName::PreserveLongName(const string& ShortName, bool Preserve):
	m_Preserve(Preserve)
{
	if (Preserve)
	{
		os::FAR_FIND_DATA FindData;

		if (os::GetFindDataEx(ShortName, FindData))
			m_SaveLongName = FindData.strFileName;
		else
			m_SaveLongName.clear();

		m_SaveShortName = ShortName;
	}
}


PreserveLongName::~PreserveLongName()
{
	if (m_Preserve && os::fs::exists(m_SaveShortName))
	{
		os::FAR_FIND_DATA FindData;

		if (!os::GetFindDataEx(m_SaveShortName, FindData) || m_SaveLongName != FindData.strFileName)
		{
			string strNewName;
			strNewName = m_SaveShortName;

			if (CutToSlash(strNewName,true))
			{
				append(strNewName, L'\\', m_SaveLongName);
			}
			else
				strNewName = m_SaveLongName;

			os::MoveFile(m_SaveShortName, strNewName);
		}
	}
}
