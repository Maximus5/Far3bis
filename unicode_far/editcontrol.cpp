/*
editcontrol.cpp

���������� ��� Edit.
��������� ������ ����� ��� �������� � ��������� (�� ��� ���������)

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

#include "editcontrol.hpp"
#include "config.hpp"
#include "keys.hpp"
#include "keyboard.hpp"
#include "language.hpp"
#include "pathmix.hpp"
#include "history.hpp"
#include "vmenu2.hpp"
#include "console.hpp"
#include "elevation.hpp"
#include "colormix.hpp"
#include "manager.hpp"
#include "interf.hpp"
#include "ctrlobj.hpp"
#include "strmix.hpp"

EditControl::EditControl(window_ptr Owner, SimpleScreenObject* Parent, parent_processkey_t&& ParentProcessKey, Callback* aCallback, History* iHistory, FarList* iList, DWORD iFlags):
	Edit(Owner),
	pHistory(iHistory),
	pList(iList),
	m_ParentProcessKey(ParentProcessKey? std::move(ParentProcessKey) : [Parent](const Manager::Key& Key) {return Parent->ProcessKey(Key); }),
	MaxLength(-1),
	CursorSize(-1),
	CursorPos(0),
	PrevCurPos(0),
	MacroSelectionStart(-1),
	SelectionStart(-1),
	MacroAreaAC(MACROAREA_DIALOGAUTOCOMPLETION),
	ECFlags(iFlags),
	Selection(false),
	MenuUp(false),
	ACState(ECFlags.Check(EC_ENABLEAUTOCOMPLETE)),
	CallbackSaveState(false)
{
	SetObjectColor();

	if (aCallback)
	{
		m_Callback=*aCallback;
	}
	else
	{
		m_Callback.Active=true;
		m_Callback.m_Callback=nullptr;
		m_Callback.m_Param=nullptr;
	}
}

void EditControl::Show()
{
	if (m_X2 - m_X1 + 1 > m_Str.size())
	{
		SetLeftPos(0);
	}
	if (GetOwner()->IsVisible())
	{
		Edit::Show();
	}
}

void EditControl::Changed(bool DelBlock)
{
	m_Flags.Set(FEDITLINE_CMP_CHANGED);
	if(m_Callback.Active)
	{
		if(m_Callback.m_Callback)
		{
			m_Callback.m_Callback(m_Callback.m_Param);
		}
		AutoComplete(false, DelBlock);
	}
}

void EditControl::SetMenuPos(VMenu2& menu)
{
	int MaxHeight = std::min(Global->Opt->Dialogs.CBoxMaxHeight.Get(),(long long)menu.GetItemCount()) + 1;

	int NewX2 = std::max(std::min(ScrX-2,(int)m_X2), m_X1 + 20);

	if((ScrY-m_Y1<MaxHeight && m_Y1>ScrY/2) || MenuUp)
	{
		MenuUp = true;
		menu.SetPosition(m_X1, std::max(0, m_Y1-1-MaxHeight), NewX2, m_Y1-1);
	}
	else
	{
		menu.SetPosition(m_X1, m_Y1+1, NewX2, std::min(static_cast<int>(ScrY), m_Y1+1+MaxHeight));
	}
}

static void AddSeparatorOrSetTitle(VMenu2& Menu, LNGID TitleId)
{
	bool Separator = false;
	for (intptr_t i = 0; i != Menu.GetItemCount(); ++i)
	{
		if (Menu.GetItemPtr(i)->Flags&LIF_SEPARATOR)
		{
			Separator = true;
			break;
		}
	}
	if (!Separator)
	{
		if (Menu.GetItemCount())
		{
			MenuItemEx Item(MSG(TitleId));
			Item.Flags = LIF_SEPARATOR;
			Menu.AddItem(Item);
		}
		else
		{
			Menu.SetTitle(MSG(TitleId));
		}
	}
}

static bool EnumFiles(VMenu2& Menu, const string& Str)
{
	bool Result = false;
	if(!Str.empty())
	{
		size_t Pos = 0;
		if(std::count(ALL_CONST_RANGE(Str), L'"') & 1) // odd quotes count
		{
			Pos = Str.rfind(L'"');
		}
		else
		{
			auto WordDiv = GetSpaces() + Global->Opt->strWordDiv.Get();
			static const string NoQuote = L"\":\\/%.-";
			for (size_t i = 0; i != WordDiv.size(); ++i)
			{
				if (NoQuote.find(WordDiv[i]) != string::npos)
				{
					WordDiv.erase(i--, 1);
				}
			}

			for(Pos = Str.size()-1; Pos!=static_cast<size_t>(-1); Pos--)
			{
				if(Str[Pos]==L'"')
				{
					Pos--;
					while(Str[Pos]!=L'"' && Pos!=static_cast<size_t>(-1))
					{
						Pos--;
					}
				}
				else if (WordDiv.find(Str[Pos]) != string::npos)
				{
					Pos++;
					break;
				}
			}
		}
		if(Pos==static_cast<size_t>(-1))
		{
			Pos=0;
		}
		bool StartQuote=false;
		if(Pos < Str.size() && Str[Pos]==L'"')
		{
			Pos++;
			StartQuote=true;
		}

		auto strStart = Str.substr(0, Pos);
		auto Token = Str.substr(Pos);
		Unquote(Token);
		if (!Token.empty())
		{
			std::set<string, string_i_less> ResultStrings;
			
			string strExp = os::env::expand_strings(Token);
			os::fs::enum_file Find(strExp+L"*");
			std::for_each(CONST_RANGE(Find, i)
			{
				const wchar_t* FileName = PointToName(Token);
				bool NameMatch=!StrCmpNI(FileName, i.strFileName.data(), StrLength(FileName)), AltNameMatch = NameMatch? false : !StrCmpNI(FileName, i.strAlternateFileName.data(), StrLength(FileName));
				if(NameMatch || AltNameMatch)
				{
					Token.resize(FileName - Token.data());
					string strAdd(Token + (NameMatch ? i.strFileName : i.strAlternateFileName));
					if (!StartQuote)
						QuoteSpace(strAdd);

					string strTmp(strStart+strAdd);
					if(StartQuote)
						strTmp += L'"';

					ResultStrings.emplace(strTmp);
				}
			});

			if (!ResultStrings.empty())
			{
				AddSeparatorOrSetTitle(Menu, MCompletionFilesTitle);

				std::for_each(CONST_RANGE(ResultStrings, i)
				{
					Menu.AddItem(i);
				});

				Result = true;
			}
		}
	}
	return Result;
}

static bool EnumModules(VMenu2& Menu, const string& Module)
{
	bool Result=false;

	SCOPED_ACTION(elevation::suppress);

	if(!Module.empty() && !FirstSlash(Module.data()))
	{
		std::set<string, string_i_less> ResultStrings;
		{
			std::vector<string> ExcludeCmds;
			split(ExcludeCmds, Global->Opt->Exec.strExcludeCmds);

			FOR(const auto& i, ExcludeCmds)
			{
				if (!StrCmpNI(Module.data(), i.data(), Module.size()))
				{
					ResultStrings.emplace(i);
				}
			}
		}

		{
			string strPathEnv;
			if (os::env::get_variable(L"PATH", strPathEnv))
			{
				std::vector<string> PathList;
				split(PathList, strPathEnv);

				std::vector<string> PathExtList;
				split(PathExtList, os::env::get_pathext());

				FOR(const auto& Path, PathList)
				{
					string str = Path;
					AddEndSlash(str);
					str.append(Module).append(L"*");
					FOR(const auto& FindData, os::fs::enum_file(str))
					{
						FOR(const auto& Ext, PathExtList)
						{
							if (!StrCmpI(Ext.data(), PointToExt(FindData.strFileName)))
							{
								ResultStrings.emplace(FindData.strFileName);
							}
						}
					}
				}
			}
		}

		static const wchar_t RegPath[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\";
		static const HKEY RootFindKey[]={HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE,HKEY_LOCAL_MACHINE};

		DWORD samDesired = KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE;

		for (size_t i=0; i<ARRAYSIZE(RootFindKey); i++)
		{
			if (i==ARRAYSIZE(RootFindKey)-1)
			{
				if (const auto RedirectionFlag = os::GetAppPathsRedirectionFlag())
				{
					samDesired|=RedirectionFlag;
				}
				else
				{
					break;
				}
			}
			HKEY hKey;
			if (RegOpenKeyEx(RootFindKey[i], RegPath, 0, samDesired, &hKey) == ERROR_SUCCESS)
			{
				FOR(const auto& Subkey, os::reg::enum_key(hKey))
				{
					HKEY hSubKey;
					if (RegOpenKeyEx(hKey, Subkey.data(), 0, samDesired, &hSubKey) == ERROR_SUCCESS)
					{
						DWORD cbSize = 0;
						if(RegQueryValueEx(hSubKey, L"", nullptr, nullptr, nullptr, &cbSize) == ERROR_SUCCESS)
						{
							if (!StrCmpNI(Module.data(), Subkey.data(), Module.size()))
							{
								ResultStrings.emplace(Subkey);
							}
						}
						RegCloseKey(hSubKey);
					}
				}
				RegCloseKey(hKey);
			}
		}

		if (!ResultStrings.empty())
		{
			AddSeparatorOrSetTitle(Menu, MCompletionFilesTitle);

			std::for_each(CONST_RANGE(ResultStrings, i)
			{
				Menu.AddItem(i);
			});

			Result = true;
		}
	}
	return Result;
}

static bool EnumEnvironment(VMenu2& Menu, const string& Str)
{
	bool Result=false;

	SCOPED_ACTION(elevation::suppress);

	auto Token = Str.data();
	auto TokenSize = Str.size();
	string Head;
	{
		auto WordDiv = GetSpaces() + Global->Opt->strWordDiv.Get();
		std::replace(ALL_RANGE(WordDiv), L'%', L' ');
		const auto WordStart = Str.find_last_of(WordDiv);
		if (WordStart != string::npos)
		{
			Token += WordStart + 1;
			TokenSize -= WordStart + 1;
			Head = Str.substr(0, Str.size() - TokenSize);
		}
	}

	if(*Token)
	{
		std::set<string, string_i_less> ResultStrings;
		FOR(const auto& i, os::env::enum_strings())
		{
			auto VarName = L"%" + string(i, wcschr(i + 1, L'=') - i) + L"%";
			if (!StrCmpNI(Token, VarName.data(), TokenSize))
			{
				ResultStrings.emplace(Head + VarName);
			}
		}

		if (!ResultStrings.empty())
		{
			AddSeparatorOrSetTitle(Menu, MCompletionEnvironmentTitle);

			std::for_each(CONST_RANGE(ResultStrings, i)
			{
				Menu.AddItem(i);
			});

			Result = true;
		}
	}
	return Result;
}

int EditControl::AutoCompleteProc(bool Manual,bool DelBlock,Manager::Key& BackKey, FARMACROAREA Area)
{
	int Result=0;
	static int Reenter=0;
	string CurrentLine;
	size_t EventsCount = 0;
	Console().GetNumberOfInputEvents(EventsCount);
	if(ECFlags.Check(EC_ENABLEAUTOCOMPLETE) && !m_Str.empty() && !Reenter && !EventsCount && (Global->CtrlObject->Macro.GetState() == MACROSTATE_NOMACRO || Manual))
	{
		Reenter++;

		if(Global->Opt->AutoComplete.AppendCompletion && !m_Flags.Check(FEDITLINE_CMP_CHANGED))
		{
			CurrentLine = m_Str;
			DeleteBlock();
		}
		m_Flags.Clear(FEDITLINE_CMP_CHANGED);

		auto ComplMenu = VMenu2::create(string(), nullptr, 0, 0);
		ComplMenu->SetDialogMode(DMODE_NODRAWSHADOW);
		ComplMenu->SetModeMoving(false);
		string strTemp = m_Str;

		ComplMenu->SetMacroMode(Area);

		const auto CompletionEnabled = [&Manual](int State)
		{
			return (Manual && State) || (!Manual && State == 1);
		};

		if(pHistory && ECFlags.Check(EC_COMPLETE_HISTORY) && CompletionEnabled(Global->Opt->AutoComplete.UseHistory))
		{
			if(pHistory->GetAllSimilar(*ComplMenu,strTemp))
			{
				ComplMenu->SetTitle(MSG(MCompletionHistoryTitle));
			}
		}
		else if(pList)
		{
			for(size_t i=0;i<pList->ItemsNumber;i++)
			{
				if (!StrCmpNI(pList->Items[i].Text, strTemp.data(), strTemp.size()) && pList->Items[i].Text != strTemp.data())
				{
					ComplMenu->AddItem(pList->Items[i].Text);
				}
			}
		}

		const auto Complete = [&](VMenu2& Menu, const string& Str)
		{
			if(ECFlags.Check(EC_COMPLETE_FILESYSTEM) && CompletionEnabled(Global->Opt->AutoComplete.UseFilesystem))
			{
				EnumFiles(Menu,Str);
			}
			if(ECFlags.Check(EC_COMPLETE_ENVIRONMENT) && CompletionEnabled(Global->Opt->AutoComplete.UseEnvironment))
			{
				EnumEnvironment(Menu, Str);
			}
			if(ECFlags.Check(EC_COMPLETE_PATH) && CompletionEnabled(Global->Opt->AutoComplete.UsePath))
			{
				EnumModules(Menu, Str);
			}
		};

		Complete(*ComplMenu, strTemp);

		if(ComplMenu->GetItemCount()>1 || (ComplMenu->GetItemCount()==1 && StrCmpI(strTemp, ComplMenu->GetItemPtr(0)->strName)))
		{
			ComplMenu->SetMenuFlags(VMENU_WRAPMODE | VMENU_SHOWAMPERSAND);
			if(!DelBlock && Global->Opt->AutoComplete.AppendCompletion && (!m_Flags.Check(FEDITLINE_PERSISTENTBLOCKS) || Global->Opt->AutoComplete.ShowList))
			{
				int SelStart=GetLength();

				// magic
				if(IsSlash(m_Str[SelStart-1]) && m_Str[SelStart-2] == L'"' && IsSlash(ComplMenu->GetItemPtr(0)->strName[SelStart-2]))
				{
					m_Str.erase(SelStart - 2, 1);
					SelStart--;
					m_CurPos--;
				}
				int Offset = 0;
				if(!CurrentLine.empty())
				{
					int Count = ComplMenu->GetItemCount();
					while(Offset < Count && (StrCmpI(ComplMenu->GetItemPtr(Offset)->strName, CurrentLine) || ComplMenu->GetItemPtr(Offset)->Flags&LIF_SEPARATOR))
						++Offset;
					if(Offset < Count)
						++Offset;
					if(Offset < Count && (ComplMenu->GetItemPtr(Offset)->Flags&LIF_SEPARATOR))
						++Offset;
					if(Offset >= Count)
						Offset = 0;
				}
				AppendString(ComplMenu->GetItemPtr(Offset)->strName.data()+SelStart);
				Select(SelStart, GetLength());
				m_Flags.Clear(FEDITLINE_CMP_CHANGED);
				m_CurPos = GetLength();
				Show();
			}
			if(Global->Opt->AutoComplete.ShowList)
			{
				ComplMenu->AddItem(MenuItemEx(), 0);
				SetMenuPos(*ComplMenu);
				ComplMenu->SetSelectPos(0,0);
				ComplMenu->SetBoxType(SHORT_SINGLE_BOX);
				Show();
				int PrevPos=0;

				bool Visible;
				DWORD Size;
				::GetCursorType(Visible, Size);
				ComplMenu->Key(KEY_NONE);

				int ExitCode=ComplMenu->Run([&](const Manager::Key& RawKey)->int
				{
					auto MenuKey=RawKey.FarKey();
					::SetCursorType(Visible, Size);

					if(!Global->Opt->AutoComplete.ModalList)
					{
						int CurPos=ComplMenu->GetSelectPos();
						if(CurPos>=0 && PrevPos!=CurPos)
						{
							PrevPos=CurPos;
							SetString(CurPos?ComplMenu->GetItemPtr(CurPos)->strName.data():strTemp.data());
							Show();
						}
					}
					if(MenuKey==KEY_CONSOLE_BUFFER_RESIZE)
						SetMenuPos(*ComplMenu);
					else if(MenuKey!=KEY_NONE)
					{
						// ����
						if((MenuKey>=L' ' && MenuKey<=static_cast<int>(WCHAR_MAX)) || MenuKey==KEY_BS || MenuKey==KEY_DEL || MenuKey==KEY_NUMDEL)
						{
							string strPrev;
							DeleteBlock();
							GetString(strPrev);
							ProcessKey(Manager::Key(MenuKey));
							GetString(strTemp);
							if(strPrev != strTemp)
							{
								ComplMenu->DeleteItems();
								PrevPos=0;
								if(!strTemp.empty())
								{
									if(pHistory && ECFlags.Check(EC_COMPLETE_HISTORY) && CompletionEnabled(Global->Opt->AutoComplete.UseHistory))
									{
										if(pHistory->GetAllSimilar(*ComplMenu,strTemp))
										{
											ComplMenu->SetTitle(MSG(MCompletionHistoryTitle));
										}
									}
									else if(pList)
									{
										for(size_t i=0;i<pList->ItemsNumber;i++)
										{
											if (!StrCmpNI(pList->Items[i].Text, strTemp.data(), strTemp.size()) && pList->Items[i].Text != strTemp.data())
											{
												ComplMenu->AddItem(pList->Items[i].Text);
											}
										}
									}
								}

								Complete(*ComplMenu, strTemp);

								if(ComplMenu->GetItemCount()>1 || (ComplMenu->GetItemCount()==1 && StrCmpI(strTemp, ComplMenu->GetItemPtr(0)->strName)))
								{
									if(MenuKey!=KEY_BS && MenuKey!=KEY_DEL && MenuKey!=KEY_NUMDEL && Global->Opt->AutoComplete.AppendCompletion)
									{
										int SelStart=GetLength();

										// magic
										if(IsSlash(m_Str[SelStart-1]) && m_Str[SelStart-2] == L'"' && IsSlash(ComplMenu->GetItemPtr(0)->strName[SelStart-2]))
										{
											m_Str.erase(SelStart - 2, 1);
											SelStart--;
											m_CurPos--;
										}

										DisableCallback();
										AppendString(ComplMenu->GetItemPtr(0)->strName.data()+SelStart);
										if(m_X2-m_X1>GetLength())
											SetLeftPos(0);
										this->Select(SelStart, GetLength());
										RevertCallback();
									}
									ComplMenu->AddItem(MenuItemEx(), 0);
									SetMenuPos(*ComplMenu);
									ComplMenu->SetSelectPos(0,0);
								}
								else
								{
									ComplMenu->Close(-1);
								}
								Show();
							}
							return 1;
						}
						else
						{
							switch(MenuKey)
							{
							// "������������" �������
							case KEY_CTRLEND:
							case KEY_RCTRLEND:

							case KEY_CTRLSPACE:
							case KEY_RCTRLSPACE:
								{
									ComplMenu->Key(KEY_DOWN);
									return 1;
								}

							case KEY_SHIFTDEL:
							case KEY_SHIFTNUMDEL:
								{
									if(ComplMenu->GetItemCount()>1)
									{
										unsigned __int64* CurrentRecord = static_cast<unsigned __int64*>(ComplMenu->GetUserData(nullptr, 0));
										if(CurrentRecord && pHistory->DeleteIfUnlocked(*CurrentRecord))
										{
											ComplMenu->DeleteItem(ComplMenu->GetSelectPos());
											if(ComplMenu->GetItemCount()>1)
											{
												SetMenuPos(*ComplMenu);
												Show();
											}
											else
											{
												ComplMenu->Close(-1);
											}
										}
									}
								}
								break;

							// ��������� �� ������ �����
							case KEY_LEFT:
							case KEY_NUMPAD4:
							case KEY_CTRLS:     case KEY_RCTRLS:
							case KEY_RIGHT:
							case KEY_NUMPAD6:
							case KEY_CTRLD:     case KEY_RCTRLD:
							case KEY_CTRLLEFT:  case KEY_RCTRLLEFT:
							case KEY_CTRLRIGHT: case KEY_RCTRLRIGHT:
							case KEY_CTRLHOME:  case KEY_RCTRLHOME:
								{
									if(MenuKey == KEY_LEFT || MenuKey == KEY_NUMPAD4)
									{
										MenuKey = KEY_CTRLS;
									}
									else if(MenuKey == KEY_RIGHT || MenuKey == KEY_NUMPAD6)
									{
										MenuKey = KEY_CTRLD;
									}
									m_ParentProcessKey(Manager::Key(MenuKey));
									Show();
									return 1;
								}

							// ��������� �� ������
							case KEY_SHIFT:
							case KEY_ALT:
							case KEY_RALT:
							case KEY_CTRL:
							case KEY_RCTRL:
							case KEY_HOME:
							case KEY_NUMPAD7:
							case KEY_END:
							case KEY_NUMPAD1:
							case KEY_IDLE:
							case KEY_NONE:
							case KEY_ESC:
							case KEY_F10:
							case KEY_ALTF9:
							case KEY_RALTF9:
							case KEY_UP:
							case KEY_NUMPAD8:
							case KEY_DOWN:
							case KEY_NUMPAD2:
							case KEY_PGUP:
							case KEY_NUMPAD9:
							case KEY_PGDN:
							case KEY_NUMPAD3:
							case KEY_ALTLEFT:
							case KEY_ALTRIGHT:
							case KEY_ALTHOME:
							case KEY_ALTEND:
							case KEY_RALTLEFT:
							case KEY_RALTRIGHT:
							case KEY_RALTHOME:
							case KEY_RALTEND:
							case KEY_MSWHEEL_UP:
							case KEY_MSWHEEL_DOWN:
							case KEY_MSWHEEL_LEFT:
							case KEY_MSWHEEL_RIGHT:
								{
									break;
								}

							case KEY_MSLCLICK:
								MenuKey = KEY_ENTER;
							case KEY_ENTER:
							case KEY_NUMENTER:
								{
									if (!Global->Opt->AutoComplete.ModalList)
									{
										ComplMenu->Close(-1);
										BackKey = Manager::Key(MenuKey);
										Result = 1;
									}
									break;
								}

							// �� ��������� ��������� ������ � ��� ���������
							default:
								{
									ComplMenu->Close(-1);
									BackKey=RawKey;
									Result=1;
								}
							}
						}
					}
					return 0;
				});
				// mouse click
				if(ExitCode>0)
				{
					if(Global->Opt->AutoComplete.ModalList)
					{
						SetString(ComplMenu->GetItemPtr(ExitCode)->strName.data());
						Show();
					}
					else
					{
						BackKey = Manager::Key(KEY_ENTER);
						Result=1;
					}
				}
			}
		}

		Reenter--;
	}
	return Result;
}

void EditControl::AutoComplete(bool Manual,bool DelBlock)
{
	Manager::Key Key;
	if(AutoCompleteProc(Manual,DelBlock,Key,MacroAreaAC))
	{
		// BUGBUG, hack
		int Wait=Global->WaitInMainLoop;
		Global->WaitInMainLoop=1;
		struct FAR_INPUT_RECORD irec={(DWORD)Key.FarKey(), Key.Event()};
		if(!Global->CtrlObject->Macro.ProcessEvent(&irec))
			m_ParentProcessKey(Manager::Key(Key));
		Global->WaitInMainLoop=Wait;
		int CurWindowType = Global->WindowManager->GetCurrentWindow()->GetType();
		if (CurWindowType == windowtype_dialog || CurWindowType == windowtype_panels)
		{
			Show();
		}
	}
}

int EditControl::ProcessMouse(const MOUSE_EVENT_RECORD *MouseEvent)
{
	if(Edit::ProcessMouse(MouseEvent))
	{
		while(IsMouseButtonPressed()==FROM_LEFT_1ST_BUTTON_PRESSED)
		{
			m_Flags.Clear(FEDITLINE_CLEARFLAG);
			SetTabCurPos(IntKeyState.MouseX - m_X1 + GetLeftPos());
			if(IntKeyState.MouseEventFlags&MOUSE_MOVED)
			{
				if(!Selection)
				{
					Selection=true;
					SelectionStart=-1;
					Select(SelectionStart,0);
				}
				else
				{
					if(SelectionStart==-1)
					{
						SelectionStart=m_CurPos;
					}
					Select(std::min(SelectionStart, m_CurPos), std::min(m_Str.size(), std::max(SelectionStart, m_CurPos)));
					Show();
				}
			}
		}
		Selection=false;
		return TRUE;
	}
	return FALSE;
}

void EditControl::SetObjectColor(PaletteColors Color,PaletteColors SelColor,PaletteColors ColorUnChanged)
{
	m_Color=colors::PaletteColorToFarColor(Color);
	m_SelectedColor=colors::PaletteColorToFarColor(SelColor);
	m_UnchangedColor=colors::PaletteColorToFarColor(ColorUnChanged);
}

void EditControl::SetObjectColor(const FarColor& Color,const FarColor& SelColor, const FarColor& ColorUnChanged)
{
	m_Color=Color;
	m_SelectedColor=SelColor;
	m_UnchangedColor=ColorUnChanged;
}

void EditControl::GetObjectColor(FarColor& Color, FarColor& SelColor, FarColor& ColorUnChanged) const
{
	Color = m_Color;
	SelColor = m_SelectedColor;
	ColorUnChanged = m_UnchangedColor;
}

const FarColor& EditControl::GetNormalColor() const
{
	return m_Color;
}

const FarColor& EditControl::GetSelectedColor() const
{
	return m_SelectedColor;
}

const FarColor& EditControl::GetUnchangedColor() const
{
	return m_UnchangedColor;
}

size_t EditControl::GetTabSize() const
{
	return Global->Opt->EdOpt.TabSize;
}

EXPAND_TABS EditControl::GetTabExpandMode() const
{
	return EXPAND_NOTABS;
}

void EditControl::SetInputMask(const string& InputMask)
{
	m_Mask = InputMask;
	if (!m_Mask.empty())
	{
		RefreshStrByMask(TRUE);
	}
}

// ������� ���������� ��������� ������ ����� �� ����������� Mask
void EditControl::RefreshStrByMask(int InitMode)
{
	auto Mask = GetInputMask();
	if (!Mask.empty())
	{
		const auto MaskLen = Mask.size();
		m_Str.resize(MaskLen, L' ');

		for (size_t i = 0; i != MaskLen; ++i)
		{
			if (InitMode)
				m_Str[i]=L' ';

			if (!CheckCharMask(Mask[i]))
				m_Str[i]=Mask[i];
		}
	}
}

const string& EditControl::WordDiv() const
{
	return Global->Opt->strWordDiv;
}
