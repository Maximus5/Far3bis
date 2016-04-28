﻿#ifndef ELEVATION_HPP_19857862_0EE5_4709_B3E9_C7E50239C2E0
#define ELEVATION_HPP_19857862_0EE5_4709_B3E9_C7E50239C2E0
#pragma once

/*
elevation.hpp

Elevation
*/
/*
Copyright © 2010 Far Group
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

#include "synchro.hpp"

enum ELEVATION_MODE
{
	ELEVATION_MODIFY_REQUEST = 0x00000001,
	ELEVATION_READ_REQUEST   = 0x00000002,
	ELEVATION_USE_PRIVILEGES = 0xf0000000,
};

enum ELEVATION_COMMAND: int;

class elevation: noncopyable
{
public:
	elevation();
	~elevation();
	void ResetApprove();
	bool Elevated() const {return Elevation;}

	bool fCreateDirectoryEx(const string& TemplateObject, const string& Object, LPSECURITY_ATTRIBUTES Attributes);
	bool fRemoveDirectory(const string& Object);
	bool fDeleteFile(const string& Object);
	void fCallbackRoutine(LPPROGRESS_ROUTINE ProgressRoutine) const;
	bool fCopyFileEx(const string& From, const string& To, LPPROGRESS_ROUTINE ProgressRoutine, LPVOID Data, LPBOOL Cancel, DWORD Flags);
	bool fMoveFileEx(const string& From, const string& To, DWORD Flags);
	DWORD fGetFileAttributes(const string& Object);
	bool fSetFileAttributes(const string& Object, DWORD FileAttributes);
	bool fCreateHardLink(const string& Object,const string& Target,LPSECURITY_ATTRIBUTES SecurityAttributes);
	bool fCreateSymbolicLink(const string& Object, const string& Target, DWORD Flags);
	int fMoveToRecycleBin(SHFILEOPSTRUCT& FileOpStruct);
	bool fSetOwner(const string& Object, const string& Owner);
	HANDLE fCreateFile(const string& Object, DWORD DesiredAccess, DWORD ShareMode, LPSECURITY_ATTRIBUTES SecurityAttributes, DWORD CreationDistribution, DWORD FlagsAndAttributes, HANDLE TemplateFile);
	bool fSetFileEncryption(const string& Object, bool Encrypt);
	bool fDetachVirtualDisk(const string& Object, VIRTUAL_STORAGE_TYPE& VirtualStorageType);
	bool fGetDiskFreeSpaceEx(const string& Object, ULARGE_INTEGER* FreeBytesAvailableToCaller, ULARGE_INTEGER* TotalNumberOfBytes, ULARGE_INTEGER* TotalNumberOfFreeBytes);

	class suppress: noncopyable
	{
	public:
		suppress(): m_owner(Global? Global->Elevation : nullptr) { if (m_owner) ++m_owner->m_suppressions; }
		~suppress() { if (m_owner) --m_owner->m_suppressions; }

	private:
		elevation* m_owner;
	};

private:
	bool Write(const void* Data, size_t DataSize) const;
	template<typename T>
	bool Read(T& Data) const;
	template<typename T>
	bool Write(const T& Data) const;
	bool SendCommand(ELEVATION_COMMAND Command);
	bool ReceiveLastError() const;
	bool Initialize();
	bool ElevationApproveDlg(LNGID Why, const string& Object);

	std::atomic_ulong m_suppressions;
	os::handle m_pipe;
	os::handle m_process;
	os::handle m_job;
	int m_pid;

	bool IsApproved;
	bool AskApprove;
	bool Elevation;
	bool DontAskAgain;
	bool Recurse;
	CriticalSection CS;
	string strPipeID;
};

bool ElevationRequired(ELEVATION_MODE Mode, bool UseNtStatus = true);
int ElevationMain(const wchar_t* guid, DWORD PID, bool UsePrivileges);
bool IsElevationArgument(const wchar_t* Argument);

#endif // ELEVATION_HPP_19857862_0EE5_4709_B3E9_C7E50239C2E0
