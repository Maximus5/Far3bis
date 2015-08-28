/*
plist.cpp

������ ��������� (Ctrl-W)
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

#include "plist.hpp"
#include "keys.hpp"
#include "help.hpp"
#include "vmenu2.hpp"
#include "language.hpp"
#include "message.hpp"
#include "interf.hpp"
#include "imports.hpp"

static BOOL CALLBACK EnumWindowsProc(HWND hwnd,LPARAM lParam);
const size_t PID_LENGTH = 6;

struct ProcInfo
{
	VMenu2 *procList;
	bool bShowImage;
};

static struct task_sort
{
	bool operator()(const MenuItemEx& a, const MenuItemEx& b, SortItemParam& p) const
	{
		return StrCmp(a.strName.data() + PID_LENGTH + 1, b.strName.data() + PID_LENGTH + 1) < 0;
	}
}
TaskSort;

bool KillProcess(DWORD dwPID)
{
	bool Result = false;
	if (const auto Process = os::handle(OpenProcess(PROCESS_TERMINATE, FALSE, dwPID)))
	{
		Result = TerminateProcess(Process.native_handle(), 0xFFFFFFFF) != FALSE;
	}
	return Result;
}

void ShowProcessList()
{
	static bool Active = false;
	if (Active)
		return;
	Active = true;

	auto ProcList = VMenu2::create(MSG(MProcessListTitle), nullptr, 0, ScrY - 4);
	ProcList->SetMenuFlags(VMENU_WRAPMODE);
	ProcList->SetPosition(-1,-1,0,0);
	static bool bShowImage = false;

	ProcInfo pi = {ProcList.get(), bShowImage};

	if (EnumWindows(EnumWindowsProc,(LPARAM)&pi))
	{
		ProcList->AssignHighlights(FALSE);
		ProcList->SetBottomTitle(MSG(MProcessListBottom));
		ProcList->SortItems(TaskSort);

		ProcList->Run([&](const Manager::Key& RawKey)->int
		{
			const auto Key=RawKey.FarKey();
			int KeyProcessed = 1;
			switch (Key)
			{
				case KEY_F1:
				{
					Help::create(L"TaskList");
					break;
				}

				case KEY_NUMDEL:
				case KEY_DEL:
				{
					if (const auto ProcWnd = *ProcList->GetUserDataPtr<HWND>())
					{
						wchar_t_ptr Title;
						int LenTitle=GetWindowTextLength(ProcWnd);

						if (LenTitle)
						{
							Title.reset(LenTitle + 1);

							if (Title && (LenTitle=GetWindowText(ProcWnd, Title.get(), LenTitle+1)) != 0)
								Title[LenTitle]=0;
						}

						DWORD ProcID;
						GetWindowThreadProcessId(ProcWnd,&ProcID);

						if (!Message(MSG_WARNING,2,MSG(MKillProcessTitle),MSG(MAskKillProcess),
									NullToEmpty(Title.get()),MSG(MKillProcessWarning),MSG(MKillProcessKill),MSG(MCancel)))
						{
							if (!KillProcess(ProcID))
							{
								Global->CatchError();
								Message(MSG_WARNING|MSG_ERRORTYPE,1,MSG(MKillProcessTitle),MSG(MCannotKillProcess),MSG(MOk));
							}
						}
					}
				}
				case KEY_CTRLR:
				case KEY_RCTRLR:
				{
					ProcList->clear();

					if (!EnumWindows(EnumWindowsProc,(LPARAM)&pi))
						ProcList->Close(-1);
					else
						ProcList->SortItems(TaskSort);
					break;
				}
				case KEY_F2:
				{
					pi.bShowImage=(bShowImage=!bShowImage);
					int SelectPos=ProcList->GetSelectPos();
					ProcList->clear();

					if (!EnumWindows(EnumWindowsProc,(LPARAM)&pi))
						ProcList->Close(-1);
					else
					{
						ProcList->SortItems(TaskSort);
						ProcList->SetSelectPos(SelectPos);
					}
					break;
				}


				default:
					KeyProcessed = 0;
			}
			return KeyProcessed;
		});

		if (ProcList->GetExitCode()>=0)
		{
			if (const auto ProcWnd = *ProcList->GetUserDataPtr<HWND>())
			{
				//SetForegroundWindow(ProcWnd);
				// Allow SetForegroundWindow on Win98+.
				DWORD dwMs;
				// Remember the current value.
				BOOL bSPI = SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &dwMs, 0);

				if (bSPI) // Reset foreground lock timeout
					bSPI = SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, nullptr, 0);

				SetForegroundWindow(ProcWnd);

				if (bSPI) // Restore old value
					SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, ToPtr(dwMs), 0);

				WINDOWPLACEMENT wp = { sizeof(wp) };
				if (!GetWindowPlacement(ProcWnd,&wp) || wp.showCmd!=SW_SHOWMAXIMIZED)
					ShowWindowAsync(ProcWnd,SW_RESTORE);
			}
		}
	}
	Active = false;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd,LPARAM lParam)
{
	auto pi = reinterpret_cast<ProcInfo*>(lParam);
	auto ProcList=pi->procList;

	if (IsWindowVisible(hwnd) || (IsIconic(hwnd) && !(GetWindowLongPtr(hwnd,GWL_STYLE) & WS_DISABLED)))
	{
		DWORD ProcID;
		GetWindowThreadProcessId(hwnd,&ProcID);
		string strTitle;

		if (auto LenTitle = GetWindowTextLength(hwnd))
		{
			if (pi->bShowImage)
			{
				if (const auto Process = os::handle(OpenProcess(Imports().QueryFullProcessImageNameW? PROCESS_QUERY_LIMITED_INFORMATION : PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, false, ProcID)))
				{
					os::GetModuleFileNameEx(Process.native_handle(), nullptr, strTitle);
				}
			}
			else
			{
				wchar_t_ptr Title(LenTitle + 1);
				if ((LenTitle=GetWindowText(hwnd, Title.get(), LenTitle+1)) != 0)
					Title[LenTitle]=0;
				else
					Title[0]=0;
				strTitle=Title.get();
			}
		}
		if (!strTitle.empty())
		{
			MenuItemEx NewItem(FormatString() << fmt::MinWidth(PID_LENGTH) << ProcID << L' ' << BoxSymbols[BS_V1] << L' ' << strTitle);
			NewItem.UserData = hwnd;
			ProcList->AddItem(NewItem);
		}

	}

	return TRUE;
}
