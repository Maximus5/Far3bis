﻿/*
manager.cpp

Переключение между несколькими file panels, viewers, editors, dialogs
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

#include "manager.hpp"
#include "keys.hpp"
#include "window.hpp"
#include "vmenu2.hpp"
#include "filepanels.hpp"
#include "panel.hpp"
#include "cmdline.hpp"
#include "ctrlobj.hpp"
#include "syslog.hpp"
#include "interf.hpp"
#include "keyboard.hpp"
#include "grabber.hpp"
#include "message.hpp"
#include "config.hpp"
#include "plist.hpp"
#include "pathmix.hpp"
#include "strmix.hpp"
#include "exitcode.hpp"
#include "scrbuf.hpp"
#include "DlgGuid.hpp"
#include "plugins.hpp"
#include "language.hpp"
#include "desktop.hpp"
#include "keybar.hpp"
#include "fileedit.hpp"
#include "scrsaver.hpp"

std::atomic_long Manager::CurrentWindowType(-1);

bool Manager::window_comparer::operator()(const window_ptr& lhs, const window_ptr& rhs) const
{
	return lhs->ID() < rhs->ID();
}

void Manager::Key::Fill(unsigned int Key)
{
	m_FarKey = Key;
	m_EventFilled = KeyToInputRecord(m_FarKey, &m_Event);
	assert(m_EventFilled);
}

Manager::Key::Key(int Key): m_Event(), m_FarKey(), m_EventFilled(false)
{
	Fill(Key);
}

bool Manager::Key::IsReal(void)const
{
	return m_FarKey!=KEY_IDLE && m_Event.EventType!=0;
}

Manager::Key& Manager::Key::operator=(unsigned int Key)
{
	Fill(Key);
	return *this;
}

Manager::Key& Manager::Key::operator&=(unsigned int Key)
{
	assert(m_EventFilled);
	Fill(m_FarKey&Key);
	return *this;
}

static int CASHook(const Manager::Key& key)
{
	if (key.IsEvent())
	{
		const KEY_EVENT_RECORD& rec=key.Event().Event.KeyEvent;
		if (rec.bKeyDown)
		{
			switch (rec.wVirtualKeyCode)
			{
				case VK_SHIFT:
				case VK_MENU:
				case VK_CONTROL:
				{
					const auto
						maskLeft=LEFT_CTRL_PRESSED|LEFT_ALT_PRESSED|SHIFT_PRESSED,
						maskRight=RIGHT_CTRL_PRESSED|RIGHT_ALT_PRESSED|SHIFT_PRESSED;
					const auto state = rec.dwControlKeyState;
					const auto wait = [](DWORD mask)
					{
						for (;;)
						{
							INPUT_RECORD rec;

							if (PeekInputRecord(&rec,true))
							{
								GetInputRecord(&rec,true,true);
								if ((rec.Event.KeyEvent.dwControlKeyState&mask) != mask)
									break;
							}

							Sleep(1);
						}
					};
					const auto
						case1 = Global->Opt->CASRule&1 && (state&maskLeft) == maskLeft,
						case2 = Global->Opt->CASRule&2 && (state&maskRight) == maskRight;
					if (case1 || case2)
					{
						const auto maskCurrent = case1?maskLeft:maskRight;
						const auto currentWindow = Global->WindowManager->GetCurrentWindow();
						if (currentWindow->CanFastHide())
						{
							int isPanelFocus=currentWindow->GetType() == windowtype_panels;

							if (isPanelFocus)
							{
								const auto LeftVisible = Global->CtrlObject->Cp()->LeftPanel()->IsVisible();
								const auto RightVisible = Global->CtrlObject->Cp()->RightPanel()->IsVisible();
								const auto CmdLineVisible = Global->CtrlObject->CmdLine()->IsVisible();
								const auto KeyBarVisible = Global->CtrlObject->Cp()->GetKeybar().IsVisible();
								Manager::ShowBackground();
								Global->CtrlObject->Cp()->LeftPanel()->HideButKeepSaveScreen();
								Global->CtrlObject->Cp()->RightPanel()->HideButKeepSaveScreen();

								switch (Global->Opt->PanelCtrlAltShiftRule)
								{
									case 0:
										if (CmdLineVisible)
											Global->CtrlObject->CmdLine()->Show();
										if (KeyBarVisible)
											Global->CtrlObject->Cp()->GetKeybar().Show();
										break;
									case 1:
										if (KeyBarVisible)
											Global->CtrlObject->Cp()->GetKeybar().Show();
										break;
								}

								wait(maskCurrent);

								if (LeftVisible)      Global->CtrlObject->Cp()->LeftPanel()->Show();
								if (RightVisible)     Global->CtrlObject->Cp()->RightPanel()->Show();
								if (CmdLineVisible)   Global->CtrlObject->CmdLine()->Show();
								if (KeyBarVisible)    Global->CtrlObject->Cp()->GetKeybar().Show();
							}
							else
							{
								Global->WindowManager->ImmediateHide();
								wait(maskCurrent);
							}

							Global->WindowManager->RefreshWindow();
						}

						return TRUE;
					}
					break;
				}
			}
		}
	}
	return FALSE;
}

static void RegisterCASHook(Manager& WindowManager)
{
	static bool registered = false;
	if (!registered)
	{
		WindowManager.AddGlobalKeyHandler(CASHook);
		registered = true;
	}
}

Manager::Manager():
	m_NonModalSize(0),
	EndLoop(false),
	ModalExitCode(-1),
	StartManager(false),
	m_DesktopModalled(0)
{
	m_windows.reserve(1024);
	RegisterCASHook(*this);
}

Manager::~Manager()
{
}

/* $ 29.12.2000 IS
  Аналог CloseAll, но разрешает продолжение полноценной работы в фаре,
  если пользователь продолжил редактировать файл.
  Возвращает TRUE, если все закрыли и можно выходить из фара.
*/
BOOL Manager::ExitAll()
{
	_MANAGER(CleverSysLog clv(L"Manager::ExitAll()"));

	// BUGBUG don't use iterators here, may be invalidated by DeleteCommit()
	for(size_t i = m_windows.size(); i; --i)
	{
		if (i - 1 >= m_windows.size())
			continue;
		const auto CurrentWindow = m_windows[i - 1];
		if (!CurrentWindow->GetCanLoseFocus(TRUE))
		{
			ActivateWindow(CurrentWindow);
			Commit();
			const auto PrevWindoowCount = m_windows.size();
			CurrentWindow->ProcessKey(Manager::Key(KEY_ESC));
			Commit();

			if (PrevWindoowCount == m_windows.size())
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

void Manager::RefreshAll()
{
	if (!m_windows.empty())
	{
		const auto PtrCopy = m_windows.front();
		RefreshWindow(PtrCopy);
	}
}

void Manager::CloseAll()
{
	_MANAGER(CleverSysLog clv(L"Manager::CloseAll()"));
	while(!m_windows.empty())
	{
		DeleteWindow(m_windows.back());
		Commit();
	}
	m_windows.clear();
}

void Manager::PushWindow(const window_ptr& Param, window_callback Callback)
{
	m_Queue.emplace([=]{ (this->*Callback)(Param); });
}

void Manager::CheckAndPushWindow(const window_ptr& Param, window_callback Callback)
{
	//assert(Param);
	if (Param&&!Param->IsDeleting()) PushWindow(Param,Callback);
}

void Manager::CallbackWindow(const std::function<void(void)>& Callback)
{
	m_Queue.emplace(Callback);
}

void Manager::InsertWindow(const window_ptr& Inserted)
{
	_MANAGER(CleverSysLog clv(L"Manager::InsertWindow(window *Inserted, int Index)"));
	_MANAGER(SysLog(L"Inserted=%p, Index=%i",Inserted, Index));

	CheckAndPushWindow(Inserted,&Manager::InsertCommit);
}

void Manager::DeleteWindow(const window_ptr& Deleted)
{
	_MANAGER(CleverSysLog clv(L"Manager::DeleteWindow(window *Deleted)"));
	_MANAGER(SysLog(L"Deleted=%p",Deleted));

	const auto& Window=Deleted?Deleted:GetCurrentWindow();
	assert(Window);
	CheckAndPushWindow(Window,&Manager::DeleteCommit);
	Window->SetDeleting();
}

void Manager::RedeleteWindow(const window_ptr& Deleted)
{
	m_Queue.emplace(nullptr);
	PushWindow(Deleted,&Manager::DeleteCommit);
}

void Manager::ExecuteNonModal(const window_ptr& NonModal)
{
	_MANAGER(CleverSysLog clv(L"Manager::ExecuteNonModal ()"));
	if (!NonModal) return;
	for (;;)
	{
		Commit();

		if (GetCurrentWindow()!=NonModal || EndLoop)
		{
			break;
		}

		ProcessMainLoop();
	}
}

void Manager::ExecuteModal(const window_ptr& Executed)
{
	_MANAGER(CleverSysLog clv(L"Manager::ExecuteModal (window *Executed)"));
	_MANAGER(SysLog(L"Executed=%p",Executed));

	bool stop=false;
	auto& stop_ref=m_Executed[Executed];
	if (stop_ref) return;
	stop_ref=&stop;

	const auto OriginalStartManager = StartManager;
	StartManager = true;

	for (;;)
	{
		Commit();

		if (stop || EndLoop)
		{
			break;
		}

		ProcessMainLoop();
	}

	StartManager = OriginalStartManager;
	return;// GetModalExitCode();
}

int Manager::GetModalExitCode() const
{
	return ModalExitCode;
}

/* $ 11.10.2001 IS
   Подсчитать количество окон с указанным именем.
*/
int Manager::CountWindowsWithName(const string& Name, BOOL IgnoreCase)
{
	using CompareFunction = int (*)(const string&, const string&);
	CompareFunction CaseSenitive = StrCmp, CaseInsensitive = StrCmpI;
	CompareFunction CmpFunction = IgnoreCase? CaseInsensitive : CaseSenitive;

	string strType, strCurName;

	return std::count_if(CONST_RANGE(m_windows, i)
	{
		i->GetTypeAndName(strType, strCurName);
		return !CmpFunction(Name, strCurName);
	});
}

/*!
  \return Возвращает nullptr если нажат "отказ" или если нажато текущее окно.
  Другими словами, если немодальное окно не поменялось.
  Если же поменялось, то тогда функция должна возвратить
  указатель на предыдущее окно.
*/
window_ptr Manager::WindowMenu()
{
	/* $ 28.04.2002 KM
	    Флаг для определения того, что меню переключения
	    экранов уже активировано.
	*/
	static int AlreadyShown=FALSE;

	if (AlreadyShown)
		return nullptr;

	int ExitCode, CheckCanLoseFocus=GetCurrentWindow()->GetCanLoseFocus();
	{
		const auto ModalMenu = VMenu2::create(MSG(MScreensTitle), nullptr, 0, ScrY - 4);
		ModalMenu->SetHelp(L"ScrSwitch");
		ModalMenu->SetMenuFlags(VMENU_WRAPMODE);
		ModalMenu->SetPosition(-1,-1,0,0);
		ModalMenu->SetId(ScreensSwitchId);

		size_t n = 0;
		const auto windows = GetSortedWindows();
		std::for_each(CONST_RANGE(windows, i)
		{
			string strType, strName, strNumText;
			i->GetTypeAndName(strType, strName);
			MenuItemEx ModalMenuItem;

			if (n < 10)
				strNumText = string_format(L"&{0}. ", n);
			else if (n < 36)
				strNumText = string_format(L"&{0}. ", wchar_t(L'A' + n - 10));
			else
				strNumText = L"&   ";

			//TruncPathStr(strName,ScrX-24);
			ReplaceStrings(strName, L"&", L"&&");
			/*  добавляется "*" если файл изменен */
			ModalMenuItem.strName = str_printf(L"%s%-10.10s %c %s", strNumText.data(), strType.data(),(i->IsFileModified()?L'*':L' '), strName.data());
			const auto tmp = i.get();
			ModalMenuItem.UserData = tmp;
			ModalMenuItem.SetSelect(i==GetBottomWindow());
			ModalMenu->AddItem(ModalMenuItem);
			++n;
		});

		AlreadyShown=TRUE;
		ExitCode=ModalMenu->Run();
		AlreadyShown=FALSE;

		if (CheckCanLoseFocus)
		{
			if (ExitCode>=0)
			{
				const auto ActivatedWindow = *ModalMenu->GetUserDataPtr<window*>(ExitCode);
				ActivateWindow(ActivatedWindow->shared_from_this());
				return (ActivatedWindow == GetCurrentWindow().get() || !GetCurrentWindow()->GetCanLoseFocus())? nullptr : GetCurrentWindow();
			}
		}
	}

	return nullptr;
}


int Manager::GetWindowCountByType(int Type)
{
	return std::count_if(CONST_RANGE(m_windows, i)
	{
		return !i->IsDeleting() && i->GetExitCode() != XC_QUIT && i->GetType() == Type;
	});
}

/*$ 11.05.2001 OT Теперь можно искать файл не только по полному имени, но и отдельно - путь, отдельно имя */
window_ptr Manager::FindWindowByFile(int ModalType,const string& FileName, const wchar_t *Dir)
{
	string strBufFileName;
	string strFullFileName = FileName;

	if (Dir)
	{
		strBufFileName = Dir;
		AddEndSlash(strBufFileName);
		strBufFileName += FileName;
		strFullFileName = strBufFileName;
	}

	const auto ItemIterator = std::find_if(CONST_RANGE(m_windows, i)
	{
		string strType, strName;

		// Mantis#0000469 - получать Name будем только при совпадении ModalType
		if (!i->IsDeleting() && i->GetType() == ModalType)
		{
			i->GetTypeAndName(strType, strName);

			if (!StrCmpI(strName, strFullFileName))
				return true;
		}
		return false;
	});

	return ItemIterator == m_windows.cend()? nullptr : *ItemIterator;
}

bool Manager::ShowBackground()
{
	Global->CtrlObject->Desktop->Show();
	return true;
}

void Manager::ActivateWindow(const window_ptr& Activated)
{
	_MANAGER(CleverSysLog clv(L"Manager::ActivateWindow(window *Activated)"));
	_MANAGER(SysLog(L"Activated=%i",Activated));

	if (Activated) CheckAndPushWindow(Activated,&Manager::ActivateCommit);
}

void Manager::SwitchWindow(DirectionType Direction)
{
	const auto windows = GetSortedWindows();
	auto pos = windows.find(GetBottomWindow());
	const auto process = [&, this]()
	{
		if (Direction==Manager::NextWindow)
		{
			++pos;
			if (pos==windows.end()) pos = windows.begin();
		}
		else if (Direction==Manager::PreviousWindow)
		{
			if (pos==windows.begin()) pos=windows.end();
			--pos;
		}
	};
	process();
	// For now we don't want to switch to the desktop window with Ctrl-[Shift-]Tab
	if (std::dynamic_pointer_cast<desktop>(*pos))
		process();
	ActivateWindow(*pos);
}

void Manager::RefreshWindow(const window_ptr& Refreshed)
{
	_MANAGER(CleverSysLog clv(L"Manager::RefreshWindow(window *Refreshed)"));
	_MANAGER(SysLog(L"Refreshed=%p",Refreshed));

	CheckAndPushWindow(Refreshed?Refreshed:GetCurrentWindow(),&Manager::RefreshCommit);
}

void Manager::ExecuteWindow(const window_ptr& Executed)
{
	_MANAGER(CleverSysLog clv(L"Manager::ExecuteWindow(window *Executed)"));
	_MANAGER(SysLog(L"Executed=%p",Executed));
	CheckAndPushWindow(Executed,&Manager::ExecuteCommit);
}

void Manager::ReplaceWindow(const window_ptr& Old, const window_ptr& New)
{
	m_Queue.emplace([=]{ ReplaceCommit(Old, New); });
}

void Manager::ModalDesktopWindow()
{
	CheckAndPushWindow(Global->CtrlObject->Desktop, &Manager::ModalDesktopCommit);
}

void Manager::UnModalDesktopWindow()
{
	CheckAndPushWindow(Global->CtrlObject->Desktop, &Manager::UnModalDesktopCommit);
}

void Manager::SwitchToPanels()
{
	_MANAGER(CleverSysLog clv(L"Manager::SwitchToPanels()"));
	if (!Global->OnlyEditorViewerUsed)
	{
		const auto PanelsWindow = std::find_if(ALL_CONST_RANGE(m_windows), [](const auto& item) { return std::dynamic_pointer_cast<FilePanels>(item) != nullptr; });
		if (PanelsWindow != m_windows.cend())
		{
			ActivateWindow(*PanelsWindow);
		}
	}
}


bool Manager::HaveAnyWindow() const
{
	return !m_windows.empty() || !m_Queue.empty() || GetCurrentWindow();
}

bool Manager::OnlyDesktop() const
{
	return m_windows.size() == 1 && m_Queue.empty();
}

bool Manager::HaveAnyMessage() const
{
	return !m_Queue.empty();
}

void Manager::EnterMainLoop()
{
	StartManager = true;

	for (;;)
	{
		Commit();

		if (EndLoop || (!HaveAnyWindow() || OnlyDesktop()))
		{
			break;
		}

		ProcessMainLoop();
	}
}

void Manager::ProcessMainLoop()
{
	if ( GetCurrentWindow() && !GetCurrentWindow()->ProcessEvents() )
	{
		ProcessKey(Manager::Key(KEY_IDLE));
	}
	else
	{
		// Mantis#0000073: Не работает автоскролинг в QView
		Global->WaitInMainLoop=IsPanelsActive(true);
		INPUT_RECORD rec;
		int Key=GetInputRecord(&rec);
		Global->WaitInMainLoop=FALSE;

		if (EndLoop)
			return;

		if (rec.EventType==MOUSE_EVENT && !(Key==KEY_MSWHEEL_UP || Key==KEY_MSWHEEL_DOWN || Key==KEY_MSWHEEL_RIGHT || Key==KEY_MSWHEEL_LEFT))
		{
				// используем копию структуры, т.к. LastInputRecord может внезапно измениться во время выполнения ProcessMouse
				MOUSE_EVENT_RECORD mer=rec.Event.MouseEvent;
				ProcessMouse(&mer);
		}
		else
			ProcessKey(Manager::Key(Key, rec));
	}

	if(IsPanelsActive())
	{
		if(!Global->PluginPanelsCount)
		{
			Global->CtrlObject->Plugins->RefreshPluginsList();
		}
	}
}

void Manager::ExitMainLoop(int Ask)
{
	if (Global->CloseFAR)
	{
		Global->CloseFARMenu=TRUE;
	};

	if (!Ask || !Global->Opt->Confirm.Exit || Message(0, MSG(MQuit),
		{ MSG(MAskQuit) },
		{ MSG(MYes), MSG(MNo) },
		nullptr, nullptr, &FarAskQuitId) == Message::first_button)
	{
		/* $ 29.12.2000 IS
		   + Проверяем, сохранены ли все измененные файлы. Если нет, то не выходим
		     из фара.
		*/
		if (ExitAll() || Global->CloseFAR)
		{
			const auto cp = Global->CtrlObject->Cp();
			if (!cp || (!cp->LeftPanel()->ProcessPluginEvent(FE_CLOSE, nullptr) && !cp->RightPanel()->ProcessPluginEvent(FE_CLOSE, nullptr)))
				EndLoop=true;
		}
		else
		{
			Global->CloseFARMenu=FALSE;
		}
	}
}

void Manager::AddGlobalKeyHandler(const std::function<int(const Key&)>& Handler)
{
	m_GlobalKeyHandlers.emplace_back(Handler);
}

int Manager::ProcessKey(Key key)
{
	if (GetCurrentWindow())
	{
		/*** БЛОК ПРИВИЛЕГИРОВАННЫХ КЛАВИШ ! ***/
		/***   КОТОРЫЕ НЕЛЬЗЯ НАМАКРОСИТЬ    ***/

		switch (key())
		{
			case KEY_ALT|KEY_NUMPAD0:
			case KEY_RALT|KEY_NUMPAD0:
			case KEY_ALTINS:
			case KEY_RALTINS:
			{
				RunGraber();
				return TRUE;
			}
			case KEY_CONSOLE_BUFFER_RESIZE:
				Sleep(1);
				ResizeAllWindows();
				return TRUE;
		}

		/*** А вот здесь - все остальное! ***/
		if (!Global->IsProcessAssignMacroKey)
		{
			if (std::any_of(CONST_RANGE(m_GlobalKeyHandlers, i) { return i(key); }))
			{
				return TRUE;
			}

			switch (key())
			{
				case KEY_CTRLW:
				case KEY_RCTRLW:
					ShowProcessList();
					return TRUE;

				case KEY_F11:
				{
					int WindowType = Global->WindowManager->GetCurrentWindow()->GetType();
					static int reentry=0;
					if(!reentry && (WindowType == windowtype_dialog || WindowType == windowtype_menu))
					{
						++reentry;
						int r=GetCurrentWindow()->ProcessKey(key);
						--reentry;
						return r;
					}

					PluginsMenu();
					Global->WindowManager->RefreshWindow();
					//_MANAGER(SysLog(-1));
					return TRUE;
				}
				case KEY_ALTF9:
				case KEY_RALTF9:
				{
					//_MANAGER(SysLog(1,"Manager::ProcessKey, KEY_ALTF9 pressed..."));
					Sleep(1);
					SetVideoMode();
					Sleep(1);

					/* В процессе исполнения Alt-F9 (в нормальном режиме) в очередь
					   консоли попадает WINDOW_BUFFER_SIZE_EVENT, формируется в
					   ChangeVideoMode().
					   В режиме исполнения макросов ЭТО не происходит по вполне понятным
					   причинам.
					*/
					if (Global->CtrlObject->Macro.IsExecuting())
					{
						int PScrX=ScrX;
						int PScrY=ScrY;
						Sleep(1);
						GetVideoMode(CurSize);

						if (PScrX+1 == CurSize.X && PScrY+1 == CurSize.Y)
						{
							//_MANAGER(SysLog(-1,"GetInputRecord(WINDOW_BUFFER_SIZE_EVENT); return KEY_NONE"));
							return TRUE;
						}
						else
						{
							PrevScrX=PScrX;
							PrevScrY=PScrY;
							//_MANAGER(SysLog(-1,"GetInputRecord(WINDOW_BUFFER_SIZE_EVENT); return KEY_CONSOLE_BUFFER_RESIZE"));
							Global->WindowManager->ResizeAllWindows();
							return TRUE;
						}
					}

					//_MANAGER(SysLog(-1));
					return TRUE;
				}
				case KEY_F12:
				{
					if (!std::dynamic_pointer_cast<Modal>(Global->WindowManager->GetCurrentWindow()))
					{
						WindowMenu();
						//_MANAGER(SysLog(-1));
						return TRUE;
					}

					break; // отдадим F12 дальше по цепочке
				}
				case KEY_CTRLTAB:
				case KEY_RCTRLTAB:
				case KEY_CTRLSHIFTTAB:
				case KEY_RCTRLSHIFTTAB:

					if (GetCurrentWindow()->GetCanLoseFocus())
					{
						SwitchWindow((key()==KEY_CTRLTAB||key()==KEY_RCTRLTAB)?NextWindow:PreviousWindow);
					}
					else
						break;

					_MANAGER(SysLog(-1));
					return TRUE;
			}
		}

		GetCurrentWindow()->UpdateKeyBar();
		GetCurrentWindow()->ProcessKey(key);
	}

	_MANAGER(SysLog(-1));
	return FALSE;
}

static bool FilterMouseMoveNoise(const MOUSE_EVENT_RECORD* MouseEvent)
{
	static COORD LastPosition = {};
	if (MouseEvent->dwEventFlags == MOUSE_MOVED && LastPosition.X == MouseEvent->dwMousePosition.X && LastPosition.Y == MouseEvent->dwMousePosition.Y)
	{
		return false;
	}
	LastPosition = MouseEvent->dwMousePosition;
	return true;
}

int Manager::ProcessMouse(const MOUSE_EVENT_RECORD *MouseEvent) const
{
	int ret=FALSE;

	if (!MouseEvent->dwMousePosition.Y && MouseEvent->dwMousePosition.X == ScrX)
	{
		if (Global->Opt->ScreenSaver && !(MouseEvent->dwButtonState & 3))
		{
			ScreenSaver();
			return TRUE;
		}
	}

	if (GetCurrentWindow() && FilterMouseMoveNoise(MouseEvent))
		ret=GetCurrentWindow()->ProcessMouse(MouseEvent);

	_MANAGER(SysLog(-1));
	return ret;
}

void Manager::PluginsMenu() const
{
	_MANAGER(SysLog(1));
	int curType = GetCurrentWindow()->GetType();

	if (curType == windowtype_panels || curType == windowtype_editor || curType == windowtype_viewer || curType == windowtype_dialog || curType == windowtype_menu)
	{
		/* 02.01.2002 IS
		   ! Вывод правильной помощи по Shift-F1 в меню плагинов в редакторе/viewer-е/диалоге
		   ! Если на панели QVIEW или INFO открыт файл, то считаем, что это
		     полноценный viewer и запускаем с соответствующим параметром плагины
		*/
		if (curType==windowtype_panels)
		{
			const auto pType = Global->CtrlObject->Cp()->ActivePanel()->GetType();

			if (pType == panel_type::QVIEW_PANEL || pType == panel_type::INFO_PANEL)
			{
				string strType, strCurFileName;
				Global->CtrlObject->Cp()->GetTypeAndName(strType, strCurFileName);

				if (!strCurFileName.empty())
				{
					// интересуют только обычные файлы
					if (os::fs::is_file(strCurFileName))
						curType=windowtype_viewer;
				}
			}
		}

		// в редакторе, viewer-е или диалоге покажем свою помощь по Shift-F1
		const wchar_t *Topic=curType==windowtype_editor?L"Editor":
		                     curType==windowtype_viewer?L"Viewer":
		                     curType==windowtype_dialog?L"Dialog":nullptr;
		Global->CtrlObject->Plugins->CommandsMenu(curType,0,Topic);
	}

	_MANAGER(SysLog(-1));
}

bool Manager::IsPanelsActive(bool and_not_qview) const
{
	if (!m_windows.empty() && GetCurrentWindow())
	{
		const auto fp = std::dynamic_pointer_cast<FilePanels>(GetCurrentWindow());
		return fp && (!and_not_qview || fp->ActivePanel()->GetType() != panel_type::QVIEW_PANEL);
	}
	else
	{
		return false;
	}
}

window_ptr Manager::GetWindow(size_t Index) const
{
	if (Index >= m_windows.size() || m_windows.empty())
	{
		return nullptr;
	}

	return m_windows[Index];
}

int Manager::IndexOf(const window_ptr& Window) const
{
	const auto ItemIterator = std::find(ALL_CONST_RANGE(m_windows), Window);
	return ItemIterator != m_windows.cend() ? ItemIterator - m_windows.cbegin() : -1;
}

void Manager::Commit(void)
{
	_MANAGER(CleverSysLog clv(L"Manager::Commit()"));
	_MANAGER(ManagerClass_Dump(L"ManagerClass"));
	while (!m_Queue.empty())
	{
		const auto Handler = std::move(m_Queue.front());
		m_Queue.pop();
		if (!Handler)
			break;
		Handler();
	}
}

void Manager::InsertCommit(const window_ptr& Param)
{
	_MANAGER(CleverSysLog clv(L"Manager::InsertCommit()"));
	_MANAGER(SysLog(L"InsertedWindow=%p",Param));
	if (Param && AddWindow(Param))
	{
		auto CurrentWindow = GetCurrentWindow();
		m_windows.insert(m_windows.begin()+m_NonModalSize,Param);
		++m_NonModalSize;
		if (InModal())
		{
			RefreshWindow(Param);
		}
		else
		{
			DoActivation(CurrentWindow, Param);
		}
	}
}

void Manager::DeleteCommit(const window_ptr& Param)
{
	_MANAGER(CleverSysLog clv(L"Manager::DeleteCommit()"));
	_MANAGER(SysLog(L"DeletedWindow=%p",Param));

	if (!Param)
		return;

	if (Param->IsPinned())
	{
		RedeleteWindow(Param);
		return;
	}

	auto CurrentWindow = GetCurrentWindow();
	if (CurrentWindow == Param) DeactivateCommit(Param);
	Param->OnDestroy();

	int WindowIndex=IndexOf(Param);
	assert(-1!=WindowIndex);

	if (-1!=WindowIndex)
	{
		m_windows.erase(m_windows.begin() + WindowIndex);
		if (static_cast<size_t>(WindowIndex) < m_NonModalSize) --m_NonModalSize;

		if (m_windows.empty())
		{
			CurrentWindowType = -1;
		}
		else
		{
			if (CurrentWindow == Param)
			{
				// ActivateCommit accepts a reference,
				// so when it alter m_windows reference to m_windows.back() will be invalidated.
				// PtrCopy will keep the window alive as much as needed.
				const auto PtrCopy = m_windows.back();
				DoActivation(CurrentWindow, PtrCopy);
			}
			else
			{
				const auto PtrCopy = m_windows.back();
				RefreshWindow(PtrCopy);
			}
		}
	}

	assert(GetCurrentWindow()!=Param);

	const auto stop = m_Executed.find(Param);
	if (stop != m_Executed.end())
	{
		*(stop->second)=true;
		m_Executed.erase(stop);
	}
	const auto size = m_Added.erase(Param);
	(void)size;
	assert(size==1);
}

void Manager::ActivateCommit(const window_ptr& Param)
{
	_MANAGER(CleverSysLog clv(L"Manager::ActivateCommit()"));
	_MANAGER(SysLog(L"ActivatedWindow=%p",Param));

	if (GetCurrentWindow()==Param)
	{
		RefreshCommit(Param);
		return;
	}
	DoActivation(GetCurrentWindow(), Param);
}

void Manager::DoActivation(const window_ptr& Old, const window_ptr& New)
{
	int WindowIndex=IndexOf(New);

	assert(WindowIndex >= 0);
	if (static_cast<size_t>(WindowIndex) < m_NonModalSize)
	{
		std::rotate(m_windows.begin() + WindowIndex, m_windows.begin() + WindowIndex + 1, m_windows.begin() + m_NonModalSize);
	}
	else
	{
		m_windows.erase(m_windows.begin() + WindowIndex);
		m_windows.emplace_back(New);
	}

	DeactivateCommit(Old);
	CurrentWindowType = GetCurrentWindow()->GetType();
	UpdateMacroArea();
	RefreshCommit(New);
	New->OnChangeFocus(true);
}

void Manager::RefreshCommit(const window_ptr& Param)
{
	_MANAGER(CleverSysLog clv(L"Manager::RefreshCommit()"));
	_MANAGER(SysLog(L"RefreshedWindow=%p",Param));

	if (!Param)
		return;

	const auto WindowIndex = IndexOf(Param);

	if (-1==WindowIndex)
		return;

	const auto first = std::next(m_windows.begin(), WindowIndex);
	std::for_each(first, m_windows.end(), [](const auto& i)
	{
		i->Refresh();
		if
		(
			(Global->Opt->ViewerEditorClock && (i->GetType() == windowtype_editor || i->GetType() == windowtype_viewer))
			||
			(Global->WaitInMainLoop && Global->Opt->Clock)
		)
			ShowTime();
	});
}

void Manager::DeactivateCommit(const window_ptr& Param)
{
	_MANAGER(CleverSysLog clv(L"Manager::DeactivateCommit()"));
	_MANAGER(SysLog(L"DeactivatedWindow=%p",Param));
	if (Param)
	{
		Param->OnChangeFocus(false);
	}
}

void Manager::ExecuteCommit(const window_ptr& Param)
{
	_MANAGER(CleverSysLog clv(L"Manager::ExecuteCommit()"));
	_MANAGER(SysLog(L"ExecutedWindow=%p",Param));

	if (Param && AddWindow(Param))
	{
	    auto CurrentWindow = GetCurrentWindow();
		m_windows.emplace_back(Param);
		DoActivation(CurrentWindow, Param);
	}
}

void Manager::ReplaceCommit(const window_ptr& Old, const window_ptr& New)
{
	int WindowIndex = IndexOf(Old);

	if (-1 != WindowIndex)
	{
		New->SetID(Old->ID());
		DeleteCommit(Old);
		InsertCommit(New);
	}
	else
	{
		_MANAGER(SysLog(L"ERROR! DeletedWindow not found"));
	}
}

void Manager::ModalDesktopCommit(const window_ptr& Param)
{
	if (m_DesktopModalled++ == 0)
	{
		auto Old = GetCurrentWindow();
		assert(Old != Param);
		assert(IndexOf(Param) >= 0);
		const auto Position = m_windows.begin() + IndexOf(Param);
		std::rotate(Position, Position + 1, m_windows.end());
		--m_NonModalSize;
		DoActivation(Old, Param);
	}
}

void Manager::UnModalDesktopCommit(const window_ptr& Param)
{
	if (--m_DesktopModalled == 0)
	{
		auto Old = GetCurrentWindow();
		assert(IndexOf(Param) >= 0);
		assert(static_cast<size_t>(IndexOf(Param)) >= m_NonModalSize);
		const auto Position = m_windows.begin() + IndexOf(Param);
		std::rotate(m_windows.begin(), Position, Position + 1);
		++m_NonModalSize;
		auto New = GetCurrentWindow();
		if (Old != New)
		{
			DoActivation(Old, New);
		}
	}
	RefreshAll();
}

bool Manager::AddWindow(const window_ptr& Param)
{
	return m_Added.emplace(Param).second;
}

/*$ 26.06.2001 SKV
  Для вызова из плагинов посредством ACTL_COMMIT
*/
void Manager::PluginCommit()
{
	Commit();
}

/* $ Введена для нужд CtrlAltShift OT */
void Manager::ImmediateHide()
{
	if (m_windows.empty())
		return;

	// Сначала проверяем, есть ли у скрываемого окна SaveScreen
	if (GetCurrentWindow()->HasSaveScreen())
	{
		GetCurrentWindow()->Hide();
		return;
	}

	// Окна перерисовываются, значит для нижних
	// не выставляем заголовок консоли, чтобы не мелькал.
	if (InModal())
	{
		/* $ 28.04.2002 KM
		    Проверим, а не модальный ли редактор или вьювер на вершине
		    модального стека? И если да, покажем User screen.
		*/
		if (m_windows.back()->GetType()==windowtype_editor || m_windows.back()->GetType()==windowtype_viewer)
		{
			ShowBackground();
		}
		else
		{
			RefreshAll();
			Commit();
		}
	}
	else
	{
		ShowBackground();
	}
}

void Manager::ResizeAllWindows()
{
	Global->ScrBuf->Lock();
	std::for_each(ALL_CONST_RANGE(m_windows), std::mem_fn(&window::ResizeConsole));

	RefreshAll();
	Global->ScrBuf->Unlock();
}

void Manager::InitKeyBar() const
{
	std::for_each(ALL_CONST_RANGE(m_windows), std::mem_fn(&window::InitKeyBar));
}

void Manager::UpdateMacroArea() const
{
	if (GetCurrentWindow()) Global->CtrlObject->Macro.SetArea(GetCurrentWindow()->GetMacroArea());
}

Manager::sorted_windows Manager::GetSortedWindows(void) const
{
	return sorted_windows(m_windows.cbegin(),m_windows.cbegin() + m_NonModalSize);
}

void* Manager::GetCurrent(std::function<void*(windows::const_reverse_iterator)> Check) const
{
	const auto iterator = std::find_if(CONST_REVERSE_RANGE(m_windows, i) { return !std::dynamic_pointer_cast<Modal>(i); });
	if (iterator!=m_windows.crend())
	{
		return Check(iterator);
	}
	return nullptr;
}

Viewer* Manager::GetCurrentViewer(void) const
{
	return reinterpret_cast<Viewer*>(GetCurrent([](windows::const_reverse_iterator Iterator)
	{
		const auto result = std::dynamic_pointer_cast<ViewerContainer>(*Iterator);
		return result?result->GetViewer():nullptr;
	}
	));
}

FileEditor* Manager::GetCurrentEditor(void) const
{
	return reinterpret_cast<FileEditor*>(GetCurrent([](windows::const_reverse_iterator Iterator)
	{
		return std::dynamic_pointer_cast<FileEditor>(*Iterator).get();
	}
	));
}
