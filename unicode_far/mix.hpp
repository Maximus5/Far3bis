﻿#ifndef MIX_HPP_67869A41_F20D_4C95_86E1_4D598A356EE1
#define MIX_HPP_67869A41_F20D_4C95_86E1_4D598A356EE1
#pragma once

/*
mix.hpp

Mix
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

template<class T>
unsigned int ToPercent(T N1, T N2)
{
	if (N1 > 10000)
	{
		N1 /= 100;
		N2 /= 100;
	}

	return N2? static_cast<unsigned int>(N1 * 100 / N2) : 0;
}

bool FarMkTempEx(string &strDest, const wchar_t *Prefix=nullptr, bool WithTempPath = true, const wchar_t *UserTempPath=nullptr);

void PluginPanelItemToFindDataEx(const PluginPanelItem& Src, os::FAR_FIND_DATA& Dest);

class PluginPanelItemHolder
{
public:
	NONCOPYABLE(PluginPanelItemHolder);

	PluginPanelItemHolder() = default;
	~PluginPanelItemHolder();

	PluginPanelItem Item{};
};

class PluginPanelItemHolderNonOwning: public PluginPanelItemHolder
{
public:
	~PluginPanelItemHolderNonOwning()
	{
		Item = {};
	}
};

void FindDataExToPluginPanelItemHolder(const os::FAR_FIND_DATA& Src, PluginPanelItemHolder& Dest);

void FreePluginPanelItem(const PluginPanelItem& Data);
void FreePluginPanelItems(std::vector<PluginPanelItem>& Items);

void FreePluginPanelItemsUserData(HANDLE hPlugin,PluginPanelItem *PanelItem,size_t ItemsNumber);

template<class T>
void DeleteRawArray(const T* const* Data, size_t Size)
{
	std::for_each(Data, Data + Size, std::default_delete<const T[]>());
	delete[] Data;
}

WINDOWINFO_TYPE WindowTypeToPluginWindowType(const int fType);

class SetAutocomplete: noncopyable
{
public:
	SetAutocomplete(class EditControl* edit, bool NewState = false);
	SetAutocomplete(class DlgEdit* dedit, bool NewState = false);
	SetAutocomplete(class CommandLine* cedit, bool NewState = false);
	~SetAutocomplete();

private:
	class EditControl* edit;
	bool OldState;
};

struct uuid_hash
{
	size_t operator ()(const GUID& Key) const
	{
		RPC_STATUS Status;
		return UuidHash(const_cast<UUID*>(&Key), &Status);
	}
};

struct uuid_equal
{
	bool operator ()(const GUID& a, const GUID& b) const
	{
		// In WinSDK's older than 8.0 operator== for GUIDs declared as int (sic!), This will suppress the warning:
		return (a == b) != 0;
	}
};

void ReloadEnvironment();

unsigned int CRC32(unsigned int crc, const void* buffer, size_t size);

#endif // MIX_HPP_67869A41_F20D_4C95_86E1_4D598A356EE1
