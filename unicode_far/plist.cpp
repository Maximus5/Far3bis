﻿/*
plist.cpp

Список процессов (Ctrl-W)
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

#include "plist.hpp"
#include "keys.hpp"
#include "help.hpp"
#include "vmenu2.hpp"
#include "language.hpp"
#include "message.hpp"
#include "interf.hpp"
#include "imports.hpp"
#include "strmix.hpp"

struct menu_data
{
	string Title;
	DWORD Pid;
	HWND Hwnd;
};

static struct task_sort
{
	bool operator()(const MenuItemEx& a, const MenuItemEx& b, SortItemParam& p) const
	{
		return StrCmp(any_cast<menu_data>(a.UserData).Title, any_cast<menu_data>(b.UserData).Title) < 0;
	}
}
TaskSort;

static bool KillProcess(DWORD dwPID)
{
	bool Result = false;
	if (const auto Process = os::handle(OpenProcess(PROCESS_TERMINATE, FALSE, dwPID)))
	{
		Result = TerminateProcess(Process.native_handle(), 0xFFFFFFFF) != FALSE;
	}
	return Result;
}

struct ProcInfo
{
	VMenu2 *procList;
	bool ShowImage;
	std::exception_ptr ExceptionPtr;
};


static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM Param)
{
	const auto pi = reinterpret_cast<ProcInfo*>(Param);

	try
	{
		if (IsWindowVisible(hwnd) || (IsIconic(hwnd) && !(GetWindowLongPtr(hwnd, GWL_STYLE) & WS_DISABLED)))
		{
			DWORD ProcID;
			GetWindowThreadProcessId(hwnd, &ProcID);
			string WindowTitle, MenuItem;
			if (os::GetWindowText(hwnd, WindowTitle) && !WindowTitle.empty())
			{
				if (pi->ShowImage)
				{
					if (const auto Process = os::handle(OpenProcess(Imports().QueryFullProcessImageNameW? PROCESS_QUERY_LIMITED_INFORMATION : PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, ProcID)))
					{
						os::GetModuleFileNameEx(Process.native_handle(), nullptr, MenuItem);
					}
				}
				else
				{
					MenuItem = WindowTitle;
				}

				MenuItemEx NewItem(format(L"{0:9} {1} {2}", ProcID, BoxSymbols[BS_V1], MenuItem));
				NewItem.UserData = menu_data{ WindowTitle, ProcID, hwnd };
				pi->procList->AddItem(NewItem);
			}
		}

		return TRUE;
	}
	catch(...)
	{
		pi->ExceptionPtr = std::current_exception();
		return FALSE;
	}
}

void ShowProcessList()
{
	static bool Active = false;
	if (Active)
		return;
	Active = true;

	const auto ProcList = VMenu2::create(MSG(lng::MProcessListTitle), nullptr, 0, ScrY - 4);
	ProcList->SetMenuFlags(VMENU_WRAPMODE);
	ProcList->SetPosition(-1,-1,0,0);
	bool ShowImage = false;

	const auto& FillProcList = [&]
	{
		ProcList->clear();

		ProcInfo pi = { ProcList.get(), ShowImage };
		if (!EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&pi)))
		{
			RethrowIfNeeded(pi.ExceptionPtr);
			return false;
		}
		ProcList->SortItems(TaskSort);
		return true;
	};

	if (FillProcList())
	{
		ProcList->AssignHighlights(FALSE);
		ProcList->SetBottomTitle(MSG(lng::MProcessListBottom));

		ProcList->Run([&](const Manager::Key& RawKey)
		{
			const auto Key=RawKey();
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
					if (const auto MenuData = ProcList->GetUserDataPtr<menu_data>())
					{
						if (Message(MSG_WARNING, MSG(lng::MKillProcessTitle),
							{ MSG(lng::MAskKillProcess), MenuData->Title.data(), MSG(lng::MKillProcessWarning) },
							{ MSG(lng::MKillProcessKill), MSG(lng::MCancel) }) == Message::first_button)
						{
							if (!KillProcess(MenuData->Pid))
							{
								Global->CatchError();
								Message(MSG_WARNING|MSG_ERRORTYPE,1,MSG(lng::MKillProcessTitle),MSG(lng::MCannotKillProcess),MSG(lng::MOk));
							}
						}
					}
				}
				case KEY_CTRLR:
				case KEY_RCTRLR:
				{
					if (!FillProcList())
						ProcList->Close(-1);
					break;
				}
				case KEY_F2:
				{
					// TODO: change titles, don't enumerate again
					ShowImage = !ShowImage;
					int SelectPos=ProcList->GetSelectPos();
					if (!FillProcList())
					{
						ProcList->Close(-1);
					}
					else
					{
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
			if (const auto MenuData = ProcList->GetUserDataPtr<menu_data>())
			{
				DWORD dwMs;
				// Remember the current value.
				BOOL bSPI = SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &dwMs, 0);

				if (bSPI) // Reset foreground lock timeout
					bSPI = SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, nullptr, 0);

				SetForegroundWindow(MenuData->Hwnd);

				if (bSPI) // Restore old value
					SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, ToPtr(dwMs), 0);

				WINDOWPLACEMENT wp = { sizeof(wp) };
				if (!GetWindowPlacement(MenuData->Hwnd, &wp) || wp.showCmd != SW_SHOWMAXIMIZED)
					ShowWindowAsync(MenuData->Hwnd, SW_RESTORE);
			}
		}
	}
	Active = false;
}
