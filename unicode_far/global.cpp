/*
global.cpp

���������� ����������
*/
/*
Copyright � 1996 Eugene Roshal
Copyright � 2000 Far Group
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

#include "imports.hpp"
#include "scrbuf.hpp"
#include "config.hpp"
#include "language.hpp"
#include "elevation.hpp"
#include "configdb.hpp"
#include "ctrlobj.hpp"
#include "manager.hpp"

thread_local DWORD global::m_LastError = ERROR_SUCCESS;
thread_local NTSTATUS global::m_LastStatus = STATUS_SUCCESS;

global::global():
	OnlyEditorViewerUsed(),
	m_MainThreadId(GetCurrentThreadId()),
	m_SearchHex(),
	m_ConfigProvider(),
	ScrBuf(nullptr),
	Opt(nullptr),
	Lang(nullptr),
	Elevation(nullptr),
	CtrlObject(nullptr)
{
	Global = this;

	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_MainThreadHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);

	// BUGBUG

	// ���� ������� ���������� ������� � �������?
	IsProcessAssignMacroKey=FALSE;
	// ��� ������� ����������� ���� ����
	IsRedrawWindowInProcess=FALSE;
	PluginPanelsCount = 0;
	// ���� ������� �������� ������ � �������?
	WaitInFastFind=FALSE;
	// �� �������� � �������� �����?
	WaitInMainLoop=FALSE;
	StartIdleTime=0;
	GlobalSearchCase=false;
	GlobalSearchWholeWords=false; // �������� "Whole words" ��� ������
	GlobalSearchReverse=false;
	ScreenSaverActive=FALSE;
	CloseFAR=FALSE;
	CloseFARMenu=FALSE;
	AllowCancelExit=TRUE;
	DisablePluginsOutput=FALSE;
	ProcessException=FALSE;
	ProcessShowClock=FALSE;
	HelpFileMask=L"*.hlf";
#if defined(SYSLOG)
	StartSysLog=0;
#endif
#ifdef DIRECT_RT
	DirectRT = false;
#endif
	GlobalSaveScrPtr=nullptr;
	CriticalInternalError=FALSE;
	KeepUserScreen = 0;
	Macro_DskShowPosType=0; // ��� ����� ������ �������� ���� ������ ������ (0 - ������� �� ��������, 1 - ����� (AltF1), 2 - ������ (AltF2))
	ErrorMode = SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX;

	// BUGBUG end

	ScrBuf = new ScreenBuf;
	WindowManager = new Manager;
	Opt = new Options;
	Elevation = new elevation;
}

global::~global()
{
	delete CtrlObject;
	CtrlObject = nullptr;
	delete m_ConfigProvider;
	m_ConfigProvider = nullptr;
	delete Elevation;
	Elevation = nullptr;
	// TODO: it could be useful to delete Lang only at the very end
	delete Lang;
	Lang = nullptr;
	delete Opt;
	Opt = nullptr;
	delete WindowManager;
	WindowManager = nullptr;
	delete ScrBuf;
	ScrBuf = nullptr;

	CloseHandle(m_MainThreadHandle);

	Global = nullptr;
}

uint64_t global::FarUpTime() const
{
	return m_FarUpTime.query(os::hp_clock::microseconds);
}

void global::CatchError()
{
	m_LastError = GetLastError();
	m_LastStatus = Imports().RtlGetLastNtStatus();
}

void global::StoreSearchString(const string& Str, bool Hex)
{
	m_SearchHex = Hex;
	m_SearchString = Str;
	if (m_SearchHex)
	{
		m_SearchString.erase(std::remove(ALL_RANGE(m_SearchString), L' '), m_SearchString.end());
	}
}


#include "bootstrap/copyright.inc"
