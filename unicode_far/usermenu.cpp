/*
usermenu.cpp

User menu � ����
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

#include "keys.hpp"
#include "filepanels.hpp"
#include "panel.hpp"
#include "cmdline.hpp"
#include "vmenu2.hpp"
#include "dialog.hpp"
#include "fileedit.hpp"
#include "preservelongname.hpp"
#include "ctrlobj.hpp"
#include "manager.hpp"
#include "constitle.hpp"
#include "message.hpp"
#include "usermenu.hpp"
#include "filetype.hpp"
#include "fnparce.hpp"
#include "pathmix.hpp"
#include "strmix.hpp"
#include "panelmix.hpp"
#include "filestr.hpp"
#include "interf.hpp"
#include "language.hpp"

enum
{
	default_menu_file_codepage = CP_UTF8
};


#if defined(PROJECT_DI_MEMOEDIT)
/*
  ���� � ���������.
  1. ������ � ������� �������� ��� � ������, �.�. CommandXXX
  2. ��� DI_MEMOEDIT �� �� ������ ��������������� � ���� ������
*/
#endif


// ������ ������ ���� (Menu mode)
ENUM(MENUMODE)
{
	MM_LOCAL,  // ��������� ����
	MM_USER,   // ���������������� ����
	MM_GLOBAL, // ���������� ����
};

// ���� ������ �� ���� (Exit codes)
enum
{
	EC_CLOSE_LEVEL      = -1, // ����� �� ���� �� ���� ������� �����
	EC_CLOSE_MENU       = -2, // ����� �� ���� �� SHIFT+F10
	EC_PARENT_MENU      = -3, // �������� ���� ������������� ��������
	EC_MAIN_MENU        = -4, // �������� ������� ����
	EC_COMMAND_SELECTED = -5, // ������� ������� - ������� ���� � �������� �����
};

static int PrepareHotKey(string &strHotKey)
{
	int FuncNum=0;

	if (strHotKey.size() > 1)
	{
		// ���� ������ ������ 1 �������, ������� ��� ������� "F?", ������ ��� �������� ������ ����� "F1"
		FuncNum = static_cast<int>(std::wcstoul(strHotKey.data() + 1, nullptr, 10));

		if (FuncNum < 1 || FuncNum > 24)
		{
			FuncNum=1;
			strHotKey=L"F1";
		}
	}
	else
	{
		// ��� ������� "&" ������������
		if (strHotKey == L"&")
			strHotKey += L"&";
	}

	return FuncNum;
}

static const wchar_t LocalMenuFileName[] = L"FarMenu.ini";

struct UserMenu::UserMenuItem
{
	UserMenuItem():Submenu(false) {}
	string strHotKey;
	string strLabel;
	std::list<string> Commands;
	bool Submenu;
	menu_container Menu;
};

static string SerializeMenu(const UserMenu::menu_container& Menu)
{
	string Result;
	FOR(const auto& i, Menu)
	{
		Result += i.strHotKey;
		Result += L":  ";
		Result += i.strLabel;
		Result += L"\r\n";

		if (i.Submenu)
		{
			Result += L"{\r\n";
			Result += SerializeMenu(i.Menu);
			Result += L"}\r\n";
		}
		else
		{
			FOR(const auto& str, i.Commands)
			{
				Result += L"    ";
				Result += str;
				Result += L"\r\n";
			}
		}
	}
	return Result;
}

static void ParseMenu(UserMenu::menu_container& Menu, GetFileString& GetStr, bool OldFormat)
{
	UserMenu::menu_container::value_type *MenuItem = nullptr;

	string MenuStr;
	while (GetStr.GetString(MenuStr))
	{
		RemoveTrailingSpaces(MenuStr);

		if (MenuStr.empty())
			continue;

		if (MenuStr.front() == L'{' && MenuItem)
		{
			ParseMenu(MenuItem->Menu, GetStr, OldFormat);
			MenuItem = nullptr;
			continue;
		}

		// '}' can be a hotkey as well
		if (MenuStr.front() == L'}' && MenuStr[1] != L':')
			break;

		if (!IsSpace(MenuStr.front()))
		{
			size_t ChPos = MenuStr.find(L':');

			if (ChPos == string::npos)
				continue;

			// special case: hotkey is ':'
			if (ChPos + 1 != MenuStr.size() && MenuStr[ChPos + 1] == ':')
			{
				++ChPos;
			}

			Menu.resize(Menu.size() + 1);
			MenuItem = &Menu.back();

			MenuItem->strHotKey = MenuStr.substr(0, ChPos);
			MenuItem->strLabel = MenuStr.substr(ChPos + 1);
			RemoveLeadingSpaces(MenuItem->strLabel);

			wchar_t* Tmp;
			size_t TmpLength;
			MenuItem->Submenu = (GetStr.PeekString(&Tmp, TmpLength) && *Tmp == L'{');

			// Support for old 1.x separator format
			if (OldFormat && MenuItem->strHotKey == L"-" && MenuItem->strLabel.empty())
			{
				MenuItem->strHotKey += L"-";
			}
		}
		else if (MenuItem)
		{
			RemoveLeadingSpaces(MenuStr);
			MenuItem->Commands.emplace_back(MenuStr);
		}
	}
}

static void DeserializeMenu(UserMenu::menu_container& Menu, os::fs::file& File, uintptr_t& Codepage)
{
	bool OldFormat = false;

	if (!GetFileFormat(File, Codepage))
	{
		Codepage = GetOEMCP();
		OldFormat = true;
	}

	GetFileString GetStr(File, Codepage);
	ParseMenu(Menu, GetStr, OldFormat);

	if (!IsUnicodeOrUtfCodePage(Codepage))
	{
		Codepage = default_menu_file_codepage;
	}
}

UserMenu::UserMenu(bool ChooseMenuType):
	m_MenuMode(MM_LOCAL),
	m_MenuModified(false),
	m_ItemChanged(false),
	m_MenuCP(default_menu_file_codepage)
{
	ProcessUserMenu(ChooseMenuType, string());
}

UserMenu::UserMenu(const string& MenuFileName):
	m_MenuMode(MM_LOCAL),
	m_MenuModified(false),
	m_ItemChanged(false),
	m_MenuCP(default_menu_file_codepage)
{
	ProcessUserMenu(false, MenuFileName);
}

UserMenu::~UserMenu()
{
}

void UserMenu::SaveMenu(const string& MenuFileName)
{
	if (m_MenuModified)
	{
		DWORD FileAttr=os::GetFileAttributes(MenuFileName);

		if (FileAttr != INVALID_FILE_ATTRIBUTES)
		{
			if (FileAttr & FILE_ATTRIBUTE_READONLY)
			{
				int AskOverwrite;
				AskOverwrite=Message(MSG_WARNING,2,MSG(MUserMenuTitle),LocalMenuFileName,MSG(MEditRO),MSG(MEditOvr),MSG(MYes),MSG(MNo));

				if (!AskOverwrite)
					os::SetFileAttributes(MenuFileName,FileAttr & ~FILE_ATTRIBUTE_READONLY);
			}

			if (FileAttr & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))
				os::SetFileAttributes(MenuFileName,FILE_ATTRIBUTE_NORMAL);
		}

		auto SerializedMenu = SerializeMenu(m_Menu);
		if (!SerializedMenu.empty())
		{
			os::fs::file MenuFile;
			// Don't use CreationDisposition=CREATE_ALWAYS here - it kills alternate streams
			if (MenuFile.Open(MenuFileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, FileAttr == INVALID_FILE_ATTRIBUTES ? CREATE_NEW : TRUNCATE_EXISTING))
			{
				if (IsUnicodeOrUtfCodePage(m_MenuCP))
				{
					SerializedMenu.insert(0, 1, SIGN_UNICODE);
				}

				std::string Buffer;
				blob MenuBlob;
				if (m_MenuCP == CP_UNICODE)
				{
					// no translation
					MenuBlob = blob(SerializedMenu.data(), SerializedMenu.size() * sizeof(wchar_t));
				}
				else
				{
					Buffer = unicode::to(m_MenuCP, SerializedMenu.data(), SerializedMenu.size());
					MenuBlob = blob(Buffer.data(), Buffer.size());
				}

				size_t Written;
				MenuFile.Write(MenuBlob.data(), MenuBlob.size(), Written);

				//BUGBUG, check if successful

				MenuFile.Close();
				if (FileAttr != INVALID_FILE_ATTRIBUTES)
				{
					os::SetFileAttributes(MenuFileName, FileAttr);
				}
			}
		}
		else
		{
			// ���� ���� FarMenu.ini ����, �� ������ ���
			os::DeleteFile(MenuFileName);
		}
	}
}

static string GetMenuTitle(MENUMODE MenuMode)
{
	string strMenuTitle;

	switch (MenuMode)
	{
	case MM_LOCAL:
		strMenuTitle = MSG(MLocalMenuTitle);
		break;

	case MM_GLOBAL:
		strMenuTitle = MSG(MMainMenuTitle);
		strMenuTitle += L" (";
		strMenuTitle += MSG(MMainMenuGlobal);
		strMenuTitle += L")";
		break;

	default:
		strMenuTitle = MSG(MMainMenuTitle);
		strMenuTitle += L" (";
		strMenuTitle += MSG(MMainMenuUser);
		strMenuTitle += L")";
	}
	return strMenuTitle;
}

void UserMenu::ProcessUserMenu(bool ChooseMenuType, const string& MenuFileName)
{
	// ���� � �������� �������� � ������ LocalMenuFileName
	auto strMenuFilePath = Global->CtrlObject->CmdLine()->GetCurDir();
	// �� ��������� ���� - ��� FarMenu.ini
	m_MenuMode = MM_LOCAL;
	m_MenuModified = false;

	if (ChooseMenuType)
	{
		int EditChoice=Message(0,3,MSG(MUserMenuTitle),MSG(MChooseMenuType),MSG(MChooseMenuMain),MSG(MChooseMenuLocal),MSG(MCancel));

		if (EditChoice<0 || EditChoice==2)
			return;

		if (!EditChoice)
		{
			m_MenuMode = MM_GLOBAL;
			strMenuFilePath = Global->Opt->GlobalUserMenuDir;
		}
	}

	// �������� ���� ���������
	bool FirstRun=true;
	int ExitCode = 0;

	while ((ExitCode != EC_CLOSE_LEVEL) && (ExitCode != EC_CLOSE_MENU) && (ExitCode != EC_COMMAND_SELECTED))
	{
		string strMenuFileFullPath;
		if (MenuFileName.empty())
		{
			strMenuFileFullPath = strMenuFilePath;
			AddEndSlash(strMenuFileFullPath);
			strMenuFileFullPath += LocalMenuFileName;
		}
		else
		{
			strMenuFileFullPath = MenuFileName;
		}

		m_Menu.clear();

		// �������� ������� ���� �� ��������� �����
		os::fs::file MenuFile;
		bool FileOpened = PathCanHoldRegularFile(strMenuFilePath) ? MenuFile.Open(strMenuFileFullPath,GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING) : false;
		if (FileOpened)
		{
			DeserializeMenu(m_Menu, MenuFile, m_MenuCP);
			MenuFile.Close();
		}
		else if (m_MenuMode != MM_USER)
		{
			// ���� �� ��������. ������� ������.
			if (m_MenuMode == MM_GLOBAL) // ��� � %FARHOME%?
			{
				m_MenuMode = MM_USER;
				strMenuFilePath = Global->Opt->ProfilePath;
				continue;
			}
			else if (!ChooseMenuType)
			{
				if (!FirstRun)
				{
					// ���������� ����...
					if(!IsRootPath(strMenuFilePath))
					{
						size_t pos;
						if (FindLastSlash(pos,strMenuFilePath))
						{
							strMenuFilePath.resize(pos--);
							continue;
						}
					}
				}

				FirstRun = false;
				m_MenuMode = MM_GLOBAL;
				strMenuFilePath = Global->Opt->GlobalUserMenuDir;
				continue;
			}
		}

		// �������� ����
		ExitCode=ProcessSingleMenu(m_Menu, 0, m_Menu, strMenuFileFullPath, GetMenuTitle(m_MenuMode));

		// ...������� ��������� ������� � ����
		SaveMenu(strMenuFileFullPath);

		// ��� ���� ����� ������ ����?
		switch (ExitCode)
		{
				// �������� ���� ������������� ��������
			case EC_PARENT_MENU:
			{
				if (m_MenuMode == MM_LOCAL)
				{
					if(!IsRootPath(strMenuFilePath))
					{
						size_t pos;
						if (FindLastSlash(pos,strMenuFilePath))
						{
							strMenuFilePath.resize(pos--);
							continue;
						}
					}

					m_MenuMode = MM_GLOBAL;
					strMenuFilePath = Global->Opt->GlobalUserMenuDir;
				}
				else
				{
					m_MenuMode = MM_USER;
					strMenuFilePath = Global->Opt->ProfilePath;
				}

				break;
			}
			// �������� ������� ����
			case EC_MAIN_MENU:
			{
				// $ 14.07.2000 VVM: Shift+F2 ����������� ������� ����/��������� � �����
				switch (m_MenuMode)
				{
					case MM_LOCAL:
						m_MenuMode = MM_GLOBAL;
						strMenuFilePath = Global->Opt->GlobalUserMenuDir;
						break;

					case MM_GLOBAL:
						m_MenuMode = MM_USER;
						strMenuFilePath = Global->Opt->ProfilePath;
						break;

					default: // MM_USER
						strMenuFilePath = Global->CtrlObject->CmdLine()->GetCurDir();
						m_MenuMode=MM_LOCAL;
				}

				break;
			}
		}
	}

	if (Global->WindowManager->IsPanelsActive() && (ExitCode == EC_COMMAND_SELECTED || m_MenuModified))
		ShellUpdatePanels(Global->CtrlObject->Cp()->ActivePanel(), FALSE);
}

// ���������� ����
static void FillUserMenu(VMenu2& FarUserMenu, UserMenu::menu_container& Menu, int MenuPos, int *FuncPos, const string& Name, const string& ShortName)
{
	FarUserMenu.clear();
	int NumLines = -1;

	FOR_RANGE(Menu, MenuItem)
	{
		++NumLines;
		MenuItemEx FarUserMenuItem;
		int FuncNum=0;

		// ����������� �������� ������, ����� ������ == "--"
		if (MenuItem->strHotKey == L"--")
		{
			FarUserMenuItem.Flags|=LIF_SEPARATOR;
			FarUserMenuItem.strName=MenuItem->strLabel;

			if (NumLines==MenuPos)
			{
				MenuPos++;
			}
		}
		else
		{
			string strLabel = MenuItem->strLabel;
			SubstFileName(nullptr,strLabel,Name,ShortName,nullptr,nullptr,nullptr,nullptr,TRUE);
			strLabel = os::env::expand_strings(strLabel);
			string strHotKey = MenuItem->strHotKey;
			FuncNum = PrepareHotKey(strHotKey);
			bool have_hotkey = !strHotKey.empty();
			int Offset = have_hotkey && strHotKey.front() == L'&'? 5 : 4;
			strHotKey.resize(Offset, L' ');
			FarUserMenuItem.strName = ((have_hotkey && !FuncNum)?L"&":L"") + strHotKey + strLabel;

			if (MenuItem->Submenu)
			{
				FarUserMenuItem.Flags|=MIF_SUBMENU;
			}

			FarUserMenuItem.SetSelect(NumLines==MenuPos);
		}

		FarUserMenuItem.UserData = MenuItem;
		int ItemPos=FarUserMenu.AddItem(FarUserMenuItem);

		if (FuncNum>0)
		{
			FuncPos[FuncNum-1]=ItemPos;
		}
	}
}

// ��������� ���������� ����
int UserMenu::ProcessSingleMenu(std::list<UserMenuItem>& Menu, int MenuPos, std::list<UserMenuItem>& MenuRoot, const string& MenuFileName, const string& Title)
{
	for (;;)
	{
		int ExitCode, FuncPos[24];

		// ������� F-�������
		std::fill(ALL_RANGE(FuncPos), -1);

		string strName,strShortName;
		Global->CtrlObject->Cp()->ActivePanel()->GetCurName(strName,strShortName);
		/* $ 24.07.2000 VVM + ��� ������ �������� ���� � ��������� ��������� ��� - FAR/Registry */

		auto UserMenu = VMenu2::create(Title, nullptr, 0, ScrY - 4);
		UserMenu->SetMenuFlags(VMENU_WRAPMODE);
		UserMenu->SetHelp(L"UserMenu");
		UserMenu->SetPosition(-1,-1,0,0);
		UserMenu->SetBottomTitle(MSG(MMainMenuBottomTitle));
		UserMenu->SetMacroMode(MACROAREA_USERMENU);

		int ReturnCode=1;

		FillUserMenu(*UserMenu,Menu,MenuPos,FuncPos,strName,strShortName);
		ITERATOR(Menu)* CurrentMenuItem;
		ExitCode=UserMenu->Run([&](const Manager::Key& RawKey)->int
		{
			const auto Key=RawKey.FarKey();
			MenuPos=UserMenu->GetSelectPos();
			{
				CurrentMenuItem = UserMenu->GetUserDataPtr<ITERATOR(Menu)>(MenuPos);
			}
			if (Key==KEY_SHIFTF1)
			{
				UserMenu->Key(KEY_F1);
				return 1;
			}
			if (Key==KEY_SHIFTF11)
			{
				UserMenu->Key(KEY_F11);
				return 1;
			}
			if (Key>=KEY_F1 && Key<=KEY_F24)
			{
				int FuncItemPos;

				if ((FuncItemPos=FuncPos[Key-KEY_F1])!=-1)
				{
					UserMenu->Close(FuncItemPos);
					return 1;
				}
			}
			else if (Key == L' ') // ��������� ������ �� "�������"!
				return 1;

			int KeyProcessed = 1;
			switch (Key)
			{
					/* $ 24.08.2001 VVM + ������� ������/����� ���������/��������� ������� �������������� */
				case KEY_RIGHT:
				case KEY_NUMPAD6:
				case KEY_MSWHEEL_RIGHT:
					if (CurrentMenuItem && (*CurrentMenuItem)->Submenu)
						UserMenu->Close(MenuPos);
					break;

				case KEY_LEFT:
				case KEY_NUMPAD4:
				case KEY_MSWHEEL_LEFT:
					if (&Menu != &MenuRoot)
						UserMenu->Close(-1);
					break;

				case KEY_NUMDEL:
				case KEY_DEL:
					if (CurrentMenuItem)
					{
						DeleteMenuRecord(Menu, *CurrentMenuItem);
						FillUserMenu(*UserMenu,Menu,MenuPos,FuncPos,strName,strShortName);
					}
					break;

				case KEY_INS:
				case KEY_F4:
				case KEY_SHIFTF4:
				case KEY_NUMPAD0:
				{
					bool bNew = Key == KEY_INS || Key == KEY_NUMPAD0;
					if (!bNew && !CurrentMenuItem)
						break;

					EditMenu(Menu, CurrentMenuItem, bNew);
					FillUserMenu(*UserMenu,Menu,MenuPos,FuncPos,strName,strShortName);
					break;
				}

				case KEY_CTRLUP:
				case KEY_RCTRLUP:
				case KEY_CTRLDOWN:
				case KEY_RCTRLDOWN:
				{

					if (CurrentMenuItem)
					{
						int Pos=UserMenu->GetSelectPos();
						if (!((Key == KEY_CTRLUP || Key == KEY_RCTRLUP) && !Pos) && !((Key == KEY_CTRLDOWN || Key == KEY_RCTRLDOWN) && Pos == static_cast<int>(UserMenu->size() - 1)))
						{
							m_MenuModified = true;
							auto Other = *CurrentMenuItem;
							if (Key==KEY_CTRLUP || Key==KEY_RCTRLUP)
							{
								--Other;
								--MenuPos;
								Menu.splice(Other, Menu, *CurrentMenuItem);
							}
							else
							{
								++Other;
								++MenuPos;
								Menu.splice(*CurrentMenuItem, Menu, Other);
							}
							FillUserMenu(*UserMenu,Menu,MenuPos,FuncPos,strName,strShortName);
						}
					}
				}
				break;

				case KEY_ALTF4:       // ������������� ��� ����
				case KEY_RALTF4:
				{
					os::fs::file MenuFile;
					Global->CtrlObject->Cp()->Unlock();
					{
						auto OldTitle = std::make_unique<ConsoleTitle>();
						SaveMenu(MenuFileName);
						auto ShellEditor = FileEditor::create(MenuFileName, m_MenuCP, FFILEEDIT_DISABLEHISTORY, -1, -1, nullptr);
						OldTitle.reset();
						Global->WindowManager->ExecuteModal(ShellEditor);
						if (!ShellEditor->IsFileChanged() || (!MenuFile.Open(MenuFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING)))
						{
							ReturnCode=0;
							UserMenu->Close(-1);
							return 1;
						}
					}
					MenuRoot.clear();
					DeserializeMenu(MenuRoot, MenuFile, m_MenuCP);
					MenuFile.Close();
					m_MenuModified=true;
					ReturnCode=0;
					UserMenu->Close(-1);

					return 1; // ������� ����
				}

				/* $ 28.06.2000 tran
				����� �� ����������������� ���� �� ShiftF10 �� ������ ������
				����������� ������ ������ ExitCode -1, � ���������� FALSE -
				�� FALSE ��� � ������ ������ ������ */
				case KEY_SHIFTF10:
					//UserMenu->SetExitCode(-1);
					ReturnCode=EC_CLOSE_MENU;
					UserMenu->Close(-1);
					return 1;

				case KEY_SHIFTF2: // �������� ������� ����
					ReturnCode=EC_MAIN_MENU;
					UserMenu->Close(-1);
					return 1;

				case KEY_BS: // �������� ���� �� ������������� �������� ������ � MM_LOCAL ������

					if (m_MenuMode == MM_LOCAL)
					{
						ReturnCode=EC_PARENT_MENU;
						UserMenu->Close(-1);
						return 1;
					}

				default:

					KeyProcessed = 0;

					if (MenuPos!=UserMenu->GetSelectPos())
					{
						MenuPos=UserMenu->GetSelectPos();
						CurrentMenuItem = UserMenu->GetUserDataPtr<ITERATOR(Menu)>(MenuPos);
					}
			} // switch(Key)
			return KeyProcessed;
		});

		if (ReturnCode!=1)
			return ReturnCode;

		if (ExitCode < 0 || !CurrentMenuItem)
			return EC_CLOSE_LEVEL; //  ����� �� ���� �������

		CurrentMenuItem = UserMenu->GetUserDataPtr<ITERATOR(Menu)>(MenuPos);

		if (!CurrentMenuItem)
			return EC_CLOSE_LEVEL; //  ����� �� ���� �������

		if ((*CurrentMenuItem)->Submenu)
		{
			/* $ 20.08.2001 VVM + ��� ��������� ���� ���������� ��������� ���������� */
			string strSubMenuLabel = (*CurrentMenuItem)->strLabel;
			SubstFileName(nullptr,strSubMenuLabel,strName,strShortName,nullptr,nullptr,nullptr,nullptr,TRUE);
			strSubMenuLabel = os::env::expand_strings(strSubMenuLabel);

			size_t pos = strSubMenuLabel.find(L'&');
			if (pos != string::npos)
				strSubMenuLabel.erase(pos, 1);

			/* $ 14.07.2000 VVM ! ���� ������� �������, �� ��������. ����� �������� ���������� ���� */
			MenuPos = ProcessSingleMenu((*CurrentMenuItem)->Menu, 0, MenuRoot, MenuFileName, Title + L" \xbb " + strSubMenuLabel);

			if (MenuPos!=EC_CLOSE_LEVEL)
				return MenuPos;

			MenuPos = ExitCode;
			continue;
		}

		/* $ 01.05.2001 IS �������� �� ������ ������ */
		//int LeftVisible,RightVisible,PanelsHidden=0;
		const auto strCmdLineDir = Global->CtrlObject->CmdLine()->GetCurDir();
		Global->CtrlObject->CmdLine()->LockUpdatePanel(true);

		// ���� ���������� ������ ���� (CommandX)
		std::for_each(CONST_RANGE((*CurrentMenuItem)->Commands, str)
		{
			string strCommand = str;

			string strListName, strAnotherListName;
			string strShortListName, strAnotherShortListName;

			if (!((!StrCmpNI(strCommand.data(),L"REM",3) && (strCommand.size() == 3 || IsSpace(strCommand[3]))) || !StrCmpNI(strCommand.data(),L"::",2)))
			{
				/*
				  �������� ��������� ���������� ��������, ��������:
				  if exist !#!\!^!.! far:edit < diff -c -p !#!\!^!.! !\!.!
				  �.�. ������� "���������" ����� "if exist !#!\!^!.!", �� � ����
				  ����������, �� ������ ������.
				  ��� ��� ������,
				  if exist ..\a.bat D:\FAR\170\DIFF.MY\mkdiff.bat !?&����� �����?!
				  ��� ����������� ������, �.�. ������� ���� ������ ����, � ����
				  ��������� ���� "if exist ..\a.bat", � �� ����� ������ ������...
				*/
				//if(ExtractIfExistCommand(Command))
				{
					/* $ 01.05.2001 IS �������� �� ������ ������ */
					/*
					if (!PanelsHidden)
					{
						LeftVisible=Global->CtrlObject->Cp()->LeftPanel->IsVisible();
						RightVisible=Global->CtrlObject->Cp()->RightPanel->IsVisible();
						Global->CtrlObject->Cp()->LeftPanel->Hide();
						Global->CtrlObject->Cp()->RightPanel->Hide();
						Global->CtrlObject->Cp()->LeftPanel->SetUpdateMode(FALSE);
						Global->CtrlObject->Cp()->RightPanel->SetUpdateMode(FALSE);
						PanelsHidden=TRUE;
					}
					*/
					//;
					string strTempStr = (*CurrentMenuItem)->strLabel;
					ReplaceStrings(strTempStr, L"&", L"");

					int PreserveLFN=SubstFileName(strTempStr.data(),strCommand, strName, strShortName, &strListName, &strAnotherListName, &strShortListName, &strAnotherShortListName, FALSE, strCmdLineDir.data());
					bool ListFileUsed=!strListName.empty()||!strAnotherListName.empty()||!strShortListName.empty()||!strAnotherShortListName.empty();

					if (ExtractIfExistCommand(strCommand))
					{
						SCOPED_ACTION(PreserveLongName)(strShortName, PreserveLFN);
						RemoveExternalSpaces(strCommand);

						if (!strCommand.empty())
						{
							Global->CtrlObject->CmdLine()->ExecString(strCommand, false, 0, 0, ListFileUsed, false, true);
						}
					}
				}
			} // strCommand != "REM"

			const string* Names[] = { &strListName, &strAnotherListName, &strShortListName, &strAnotherShortListName };

			FOR(auto& i, Names)
			{
				if (!i->empty())
					os::DeleteFile(*i);
			}
		});

		Global->CtrlObject->CmdLine()->LockUpdatePanel(false);

		/* $ 01.05.2001 IS �������� �� ������ ������ */
		/*
		if (PanelsHidden)
		{
			Global->CtrlObject->Cp()->LeftPanel->SetUpdateMode(TRUE);
			Global->CtrlObject->Cp()->RightPanel->SetUpdateMode(TRUE);
			Global->CtrlObject->Cp()->LeftPanel->Update(UPDATE_KEEP_SELECTION);
			Global->CtrlObject->Cp()->RightPanel->Update(UPDATE_KEEP_SELECTION);
			if (RightVisible)
				Global->CtrlObject->Cp()->RightPanel->Show();
			if (LeftVisible)
				Global->CtrlObject->Cp()->LeftPanel->Show();
		}
		*/
		/* $ 14.07.2000 VVM ! ������� ���� */
		/* $ 25.04.2001 DJ - ��������, ��� ���� ��������� ������� (����� ������������ ������) */
		return EC_COMMAND_SELECTED;
	}
}

enum EditMenuItems
{
	EM_DOUBLEBOX,
	EM_HOTKEY_TEXT,
	EM_HOTKEY_EDIT,
	EM_LABEL_TEXT,
	EM_LABEL_EDIT,
	EM_SEPARATOR1,
	EM_COMMANDS_TEXT,
#ifdef PROJECT_DI_MEMOEDIT
	EM_MEMOEDIT,
#else
	EM_EDITLINE_0,
	EM_EDITLINE_1,
	EM_EDITLINE_2,
	EM_EDITLINE_3,
	EM_EDITLINE_4,
	EM_EDITLINE_5,
	EM_EDITLINE_6,
	EM_EDITLINE_7,
	EM_EDITLINE_8,
	EM_EDITLINE_9,
#endif
	EM_SEPARATOR2,
	EM_BUTTON_OK,
	EM_BUTTON_CANCEL,
};

intptr_t UserMenu::EditMenuDlgProc(Dialog* Dlg, intptr_t Msg, intptr_t Param1, void* Param2)
{
	switch (Msg)
	{
		case DN_EDITCHANGE:
#ifdef PROJECT_DI_MEMOEDIT
			if (Param1 == EM_MEMOEDIT)
#else
			if (Param1 >= EM_EDITLINE_0 && Param1 <= EM_EDITLINE_9)
#endif
				m_ItemChanged = true;
			break;

		case DN_CLOSE:

			if (Param1==EM_BUTTON_OK)
			{
				BOOL Result=TRUE;
				auto HotKey = reinterpret_cast<const wchar_t*>(Dlg->SendMessage(DM_GETCONSTTEXTPTR, EM_HOTKEY_EDIT, nullptr));
				auto Label = reinterpret_cast<const wchar_t*>(Dlg->SendMessage(DM_GETCONSTTEXTPTR, EM_LABEL_EDIT, nullptr));
				int FocusPos=-1;

				if(StrCmp(HotKey,L"--"))
				{
					if (!*Label)
					{
						FocusPos=EM_LABEL_EDIT;
					}
					else if (wcslen(HotKey)>1)
					{
						FocusPos=EM_HOTKEY_EDIT;

						if (ToUpper(*HotKey)==L'F')
						{
							int FuncNum = static_cast<int>(std::wcstol(HotKey + 1, nullptr, 10));

							if (FuncNum > 0 && FuncNum < 25)
								FocusPos=-1;
						}
					}
				}

				if (FocusPos!=-1)
				{
					Message(MSG_WARNING,1,MSG(MUserMenuTitle),MSG((*Label?MUserMenuInvalidInputHotKey:MUserMenuInvalidInputLabel)),MSG(MOk));
					Dlg->SendMessage(DM_SETFOCUS, FocusPos, nullptr);
					Result=FALSE;
				}

				return Result;
			}
			else if (m_ItemChanged)
			{
				switch(Message(MSG_WARNING, 3, MSG(MUserMenuTitle), MSG(MEditMenuConfirmation), MSG(MHYes), MSG(MHNo), MSG(MHCancel)))
				{
				case 0:
					Dlg->SendMessage( DM_CLOSE, EM_BUTTON_OK, nullptr);
					break;
				case 1:
					return TRUE;
				case 2:
				default:
					return FALSE;
				}
			}

			break;
		default:
			break;
	}

	return Dlg->DefProc(Msg,Param1,Param2);
}


bool UserMenu::EditMenu(std::list<UserMenuItem>& Menu, std::list<UserMenuItem>::iterator* MenuItem, bool Create)
{
	bool Result = false;
	bool SubMenu = false;
	bool Continue = true;
	m_ItemChanged = false;

	if (Create)
	{
		switch (Message(0,2,MSG(MUserMenuTitle),MSG(MAskInsertMenuOrCommand),MSG(MMenuInsertCommand),MSG(MMenuInsertMenu)))
		{
			case -1:
			case -2:
				Continue = false;
			case 1:
				SubMenu = true;
		}
	}
	else
	{
		SubMenu = (*MenuItem)->Submenu;
	}

	if (Continue)
	{
		const int DLG_X=76, DLG_Y=SubMenu?10:22;
		FARDIALOGITEMFLAGS State=SubMenu?DIF_HIDDEN|DIF_DISABLE:DIF_NONE;
		FarDialogItem EditDlgData[]=
		{
			{DI_DOUBLEBOX,3,1,DLG_X-4,DLG_Y-2,0,nullptr,nullptr,0,MSG(SubMenu?MEditSubmenuTitle:MEditMenuTitle)},
			{DI_TEXT,5,2,0,2,0,nullptr,nullptr,0,MSG(MEditMenuHotKey)},
			{DI_FIXEDIT,5,3,7,3,0,nullptr,nullptr,DIF_FOCUS,L""},
			{DI_TEXT,5,4,0,4,0,nullptr,nullptr,0,MSG(MEditMenuLabel)},
			{DI_EDIT,5,5,DLG_X-6,5,0,nullptr,nullptr,0,L""},

			{DI_TEXT,-1,6,0,6,0,nullptr,nullptr,DIF_SEPARATOR|State,L""},
			{DI_TEXT,5,7,0,7,0,nullptr,nullptr,State,MSG(MEditMenuCommands)},
#ifdef PROJECT_DI_MEMOEDIT
			{DI_MEMOEDIT,5, 8,DLG_X-6,17,0,nullptr,nullptr,DIF_EDITPATH,L""},
#else
			{DI_EDIT,5, 8,DLG_X-6,8,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
			{DI_EDIT,5, 9,DLG_X-6,9,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
			{DI_EDIT,5,10,DLG_X-6,10,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
			{DI_EDIT,5,11,DLG_X-6,11,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
			{DI_EDIT,5,12,DLG_X-6,12,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
			{DI_EDIT,5,13,DLG_X-6,13,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
			{DI_EDIT,5,14,DLG_X-6,14,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
			{DI_EDIT,5,15,DLG_X-6,15,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
			{DI_EDIT,5,16,DLG_X-6,16,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
			{DI_EDIT,5,17,DLG_X-6,17,0,nullptr,nullptr,DIF_EDITPATH|DIF_EDITPATHEXEC|DIF_EDITOR|State,L""},
#endif

			{DI_TEXT,-1,DLG_Y-4,0,DLG_Y-4,0,nullptr,nullptr,DIF_SEPARATOR,L""},
			{DI_BUTTON,0,DLG_Y-3,0,DLG_Y-3,0,nullptr,nullptr,DIF_DEFAULTBUTTON|DIF_CENTERGROUP,MSG(MOk)},
			{DI_BUTTON,0,DLG_Y-3,0,DLG_Y-3,0,nullptr,nullptr,DIF_CENTERGROUP,MSG(MCancel)},
		};
		auto EditDlg = MakeDialogItemsEx(EditDlgData);
#ifndef PROJECT_DI_MEMOEDIT
		enum {DI_EDIT_COUNT=EM_SEPARATOR2-EM_COMMANDS_TEXT-1};
#endif

		if (!Create)
		{
			EditDlg[EM_HOTKEY_EDIT].strData = (*MenuItem)->strHotKey;
			EditDlg[EM_LABEL_EDIT].strData = (*MenuItem)->strLabel;
#if defined(PROJECT_DI_MEMOEDIT)
			/*
				...
				����� ������� ����� �� "Command%d" � EMR_MEMOEDIT
				...
			*/
			string strBuffer;
			for (string *str=MenuItem->Commands.First(); str && CommandNumber < DI_EDIT_COUNT; str=MenuItem->Commands.Next(str))
			{
				strBuffer+=*str;
				strBuffer+=L"\n";    //??? "\n\r"
			}

			EditDlg[EM_MEMOEDIT].strData = strBuffer; //???
#else
			int CommandNumber=0;
			FOR(const auto& i, (*MenuItem)->Commands)
			{
				EditDlg[EM_EDITLINE_0+CommandNumber].strData = i;
				if (++CommandNumber == DI_EDIT_COUNT)
					break;
			}
#endif
		}

		auto Dlg = Dialog::create(EditDlg, &UserMenu::EditMenuDlgProc, this);
		Dlg->SetHelp(L"UserMenu");
		Dlg->SetPosition(-1,-1,DLG_X,DLG_Y);
		Dlg->Process();

		if (Dlg->GetExitCode()==EM_BUTTON_OK)
		{
			m_MenuModified=true;
			auto NewItemIterator = Menu.end();

			if (Create)
			{
				NewItemIterator = Menu.emplace(MenuItem? *MenuItem : Menu.begin(), UserMenuItem());
				MenuItem = &NewItemIterator;
			}

			(*MenuItem)->strHotKey = EditDlg[EM_HOTKEY_EDIT].strData;
			(*MenuItem)->strLabel = EditDlg[EM_LABEL_EDIT].strData;
			(*MenuItem)->Submenu = SubMenu;

			if (!SubMenu)
			{
#if defined(PROJECT_DI_MEMOEDIT)
				/*
				...
				����� �������������� ����������� �������� EMR_MEMOEDIT � "Command%d"
				...
				*/
#else
				int CommandNumber=0;

				for (int i=0 ; i < DI_EDIT_COUNT ; i++)
					if (!EditDlg[i+EM_EDITLINE_0].strData.empty())
						CommandNumber=i+1;

				(*MenuItem)->Commands.clear();
				for (int i=0 ; i < DI_EDIT_COUNT ; i++)
				{
					if (i>=CommandNumber)
						break;
					else
						(*MenuItem)->Commands.emplace_back(EditDlg[i+EM_EDITLINE_0].strData);
				}
#endif
			}

			Result=true;
		}
	}

	return Result;
}

bool UserMenu::DeleteMenuRecord(std::list<UserMenuItem>& Menu, const std::list<UserMenuItem>::iterator& MenuItem)
{
	string strItemName=MenuItem->strLabel;
	InsertQuote(strItemName);

	if (Message(MSG_WARNING,2,MSG(MUserMenuTitle),MSG(!MenuItem->Submenu?MAskDeleteMenuItem:MAskDeleteSubMenuItem),strItemName.data(),MSG(MDelete),MSG(MCancel)))
		return false;

	m_MenuModified=true;
	Menu.erase(MenuItem);
	return true;
}
