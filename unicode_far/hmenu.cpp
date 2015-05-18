/*
hmenu.cpp

�������������� ����
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

#include "hmenu.hpp"
#include "colors.hpp"
#include "keys.hpp"
#include "dialog.hpp"
#include "vmenu2.hpp"
#include "ctrlobj.hpp"
#include "filepanels.hpp"
#include "macroopcode.hpp"
#include "savescr.hpp"
#include "lockscrn.hpp"
#include "interf.hpp"
#include "keyboard.hpp"
#include "colormix.hpp"
#include "manager.hpp"

HMenu::HMenu(HMenuData *Item,size_t ItemCount):
	Item(Item),
	SelectPos(),
	ItemCount(ItemCount),
	m_VExitCode(-1),
	ItemX(),
	m_SubmenuOpened()
{
}

hmenu_ptr HMenu::create(HMenuData *Item, size_t ItemCount)
{
	hmenu_ptr HmenuPtr(new HMenu(Item, ItemCount));
	HmenuPtr->init();
	return HmenuPtr;
}

void HMenu::init()
{
	SetMacroMode(MACROAREA_MAINMENU);
	SetRestoreScreenMode(true);
}

HMenu::~HMenu()
{
	Global->WindowManager->RefreshWindow();
}

void HMenu::DisplayObject()
{
	SetScreen(m_X1,m_Y1,m_X2,m_Y2,L' ',colors::PaletteColorToFarColor(COL_HMENUTEXT));
	SetCursorType(0,10);
	ShowMenu();
}


void HMenu::ShowMenu()
{
	string strTmpStr;
	GotoXY(m_X1+2,m_Y1);

	for (size_t i=0; i<ItemCount; i++)
	{
		ItemX[i]=WhereX();

		if (Item[i].Selected)
			SetColor(COL_HMENUSELECTEDTEXT);
		else
			SetColor(COL_HMENUTEXT);

		strTmpStr=L"  ";
		strTmpStr+=Item[i].Name;
		strTmpStr+=L"  ";
		HiText(strTmpStr,colors::PaletteColorToFarColor(Item[i].Selected ? COL_HMENUSELECTEDHIGHLIGHT:COL_HMENUHIGHLIGHT));
	}

	ItemX[ItemCount]=WhereX();
}


__int64 HMenu::VMProcess(int OpCode,void *vParam,__int64 iParam)
{
	SelectPos=0;
	for (size_t i = 0; i<ItemCount; i++)
	{
		if (Item[i].Selected)
		{
			SelectPos=i;
			break;
		}
	}

	switch (OpCode)
	{
		case MCODE_C_EMPTY:
			return ItemCount<=0;
		case MCODE_C_EOF:
			return SelectPos==ItemCount-1;
		case MCODE_C_BOF:
			return !SelectPos;
		case MCODE_C_SELECTED:
			return ItemCount != 0;
		case MCODE_V_ITEMCOUNT:
			return ItemCount;
		case MCODE_V_CURPOS:
			return SelectPos+1;
		case MCODE_F_MENU_CHECKHOTKEY:
		{
			return CheckHighlights(*(const wchar_t *)vParam, (int)iParam)+1;
		}
		case MCODE_F_MENU_GETHOTKEY:
		case MCODE_F_MENU_GETVALUE: // S=Menu.GetValue([N])
		{
			if (iParam == -1)
				iParam=SelectPos;

			if ((size_t)iParam < ItemCount)
			{
				if (OpCode == MCODE_F_MENU_GETVALUE)
				{
					*(string *)vParam=Item[(int)iParam].Name;
					return 1;
				}
				else
				{
					return GetHighlights((const HMenuData *)(Item+(int)iParam));
				}
			}

			return 0;
		}
		case MCODE_F_MENU_ITEMSTATUS: // N=Menu.ItemStatus([N])
		{
			__int64 RetValue=-1;

			if (iParam == -1)
				iParam=SelectPos;

			if ((size_t)iParam < ItemCount)
			{
				RetValue=0;
				if (Item[(int)iParam].Selected)
					RetValue |= 1;
			}

			return RetValue;
		}
		case MCODE_V_MENU_VALUE: // Menu.Value
		{
			*(string *)vParam=Item[SelectPos].Name;
			return 1;
		}
	}

	return 0;
}

int HMenu::ProcessKey(const Manager::Key& Key)
{
	auto LocalKey = Key.FarKey();
	SelectPos=0;
	for (size_t i = 0; i<ItemCount; i++)
	{
		if (Item[i].Selected)
		{
			SelectPos=i;
			break;
		}
	}

	switch (LocalKey)
	{
		case KEY_ALTF9:
		case KEY_RALTF9:
			Global->WindowManager->ProcessKey(Manager::Key(KEY_ALTF9));
			break;
		case KEY_OP_PLAINTEXT:
		{
			const wchar_t *str = Global->CtrlObject->Macro.GetStringToPrint();

			if (!*str)
				return FALSE;

			LocalKey=*str;
			break;
		}
		case KEY_NONE:
		case KEY_IDLE:
		{
			return FALSE;
		}
		case KEY_F1:
		{
			ShowHelp();
			return TRUE;
		}
		case KEY_NUMENTER:
		case KEY_ENTER:
		case KEY_UP:      case KEY_NUMPAD8:
		case KEY_DOWN:    case KEY_NUMPAD2:
		{
			if (Item[SelectPos].SubMenu)
			{
				ProcessSubMenu(Item[SelectPos].SubMenu,Item[SelectPos].SubMenuSize,
				               Item[SelectPos].SubMenuHelp,ItemX[SelectPos],
				               m_Y1+1,m_VExitCode);

				if (m_VExitCode!=-1)
				{
					Close(static_cast<int>(SelectPos));
				}

				return TRUE;
			}

			return FALSE;
		}
		case KEY_TAB:
		{
			Item[SelectPos].Selected=0;

			/* ����� ��� "���������" ���� - ������ � ���� ��������� ������ */
			if (SelectPos  && SelectPos != ItemCount-1)
			{
				if (Global->CtrlObject->Cp()->ActivePanel() == Global->CtrlObject->Cp()->RightPanel)
					SelectPos=0;
				else
					SelectPos=ItemCount-1;
			}
			else
			{
				if (!SelectPos)
					SelectPos=ItemCount-1;
				else
					SelectPos=0;
			}

			Item[SelectPos].Selected=1;
			ShowMenu();
			return TRUE;
		}
		case KEY_ESC:
		case KEY_F10:
		{
			Close(-1);
			return FALSE;
		}
		case KEY_HOME:      case KEY_NUMPAD7:
		case KEY_CTRLHOME:  case KEY_CTRLNUMPAD7:
		case KEY_RCTRLHOME: case KEY_RCTRLNUMPAD7:
		case KEY_CTRLPGUP:  case KEY_CTRLNUMPAD9:
		case KEY_RCTRLPGUP: case KEY_RCTRLNUMPAD9:
		{
			Item[SelectPos].Selected=0;
			Item[0].Selected=1;
			SelectPos=0;
			ShowMenu();
			return TRUE;
		}
		case KEY_END:       case KEY_NUMPAD1:
		case KEY_CTRLEND:   case KEY_CTRLNUMPAD1:
		case KEY_RCTRLEND:  case KEY_RCTRLNUMPAD1:
		case KEY_CTRLPGDN:  case KEY_CTRLNUMPAD3:
		case KEY_RCTRLPGDN: case KEY_RCTRLNUMPAD3:
		{
			Item[SelectPos].Selected=0;
			Item[ItemCount-1].Selected=1;
			SelectPos=ItemCount-1;
			ShowMenu();
			return TRUE;
		}
		case KEY_LEFT:      case KEY_NUMPAD4:      case KEY_MSWHEEL_LEFT:
		{
			Item[SelectPos].Selected=0;

			if (!SelectPos)
				SelectPos = ItemCount - 1;
			else
				--SelectPos;

			Item[SelectPos].Selected=1;
			ShowMenu();
			return TRUE;
		}
		case KEY_RIGHT:     case KEY_NUMPAD6:      case KEY_MSWHEEL_RIGHT:
		{
			Item[SelectPos].Selected=0;

			if (SelectPos == ItemCount - 1)
				SelectPos = 0;
			else
				++SelectPos;

			Item[SelectPos].Selected=1;
			ShowMenu();
			return TRUE;
		}
		default:
		{
			for (size_t i = 0; i<ItemCount; i++)
			{
				if (IsKeyHighlighted(Item[i].Name,LocalKey,FALSE))
				{
					Item[SelectPos].Selected=0;
					Item[i].Selected=1;
					SelectPos = i;
					ShowMenu();
					ProcessKey(Manager::Key(KEY_ENTER));
					return TRUE;
				}
			}

			for (size_t i = 0; i<ItemCount; i++)
			{
				if (IsKeyHighlighted(Item[i].Name,LocalKey,TRUE))
				{
					Item[SelectPos].Selected=0;
					Item[i].Selected=1;
					SelectPos = i;
					ShowMenu();
					ProcessKey(Manager::Key(KEY_ENTER));
					return TRUE;
				}
			}

			return FALSE;
		}
	}

	return FALSE;
}


bool HMenu::TestMouse(const MOUSE_EVENT_RECORD *MouseEvent) const
{
	int MsX=MouseEvent->dwMousePosition.X;
	int MsY=MouseEvent->dwMousePosition.Y;

	return MsY!=m_Y1 || (MsX>=ItemX[SelectPos] && MsX<ItemX[SelectPos+1]);
}

int HMenu::ProcessMouse(const MOUSE_EVENT_RECORD *MouseEvent)
{
	int MsX,MsY;

	SelectPos=0;
	for (size_t i = 0; i<ItemCount; i++)
	{
		if (Item[i].Selected)
		{
			SelectPos=i;
			break;
		}
	}

	MsX=MouseEvent->dwMousePosition.X;
	MsY=MouseEvent->dwMousePosition.Y;

	if (MsY==m_Y1 && MsX>=m_X1 && MsX<=m_X2)
	{
		for (size_t i = 0; i<ItemCount; i++)
			if (MsX>=ItemX[i] && MsX<ItemX[i+1])
			{
				if (m_SubmenuOpened && SelectPos==i)
					return FALSE;

				Item[SelectPos].Selected=0;
				Item[i].Selected=1;
				SelectPos=i;
				ShowMenu();
				ProcessKey(Manager::Key(KEY_ENTER));
			}
	}
	else if (!(MouseEvent->dwButtonState & 3) && !MouseEvent->dwEventFlags)
		ProcessKey(Manager::Key(KEY_ESC));

	return TRUE;
}


void HMenu::GetExitCode(int &ExitCode,int &VExitCode) const
{
	ExitCode = m_ExitCode;
	VExitCode = m_VExitCode;
}


void HMenu::ProcessSubMenu(const MenuDataEx *Data,int DataCount,
                           const wchar_t *SubMenuHelp,int X,int Y,int &Position)
{
	m_SubmenuOpened = true;
	SCOPE_EXIT { m_SubmenuOpened = false; };

	Position=-1;
	auto SubMenu = VMenu2::create(L"", Data, DataCount);
	SubMenu->SetBoxType(SHORT_DOUBLE_BOX);
	SubMenu->SetMenuFlags(VMENU_WRAPMODE);
	SubMenu->SetHelp(SubMenuHelp);
	SubMenu->SetPosition(X,Y,0,0);
	SubMenu->SetMacroMode(MACROAREA_MAINMENU);

	bool SendMouse=false;
	MOUSE_EVENT_RECORD MouseEvent;
	int SendKey=0;

	Position=SubMenu->RunEx([&](int Msg, void *param)->int
	{
		if(Msg!=DN_INPUT)
			return 0;

		auto& rec = *static_cast<INPUT_RECORD*>(param);
		int Key=InputRecordToKey(&rec);

		if (Key==KEY_CONSOLE_BUFFER_RESIZE)
		{
			SCOPED_ACTION(LockScreen);
			ResizeConsole();
			Show();
			return 1;
		}
		else if (rec.EventType==MOUSE_EVENT)
		{
			if (!TestMouse(&rec.Event.MouseEvent))
			{
				MouseEvent=rec.Event.MouseEvent;
				SendMouse=true;
				SubMenu->Close(-1);
				return 1;
			}
			if (rec.Event.MouseEvent.dwMousePosition.Y==m_Y1)
				return 1;
		}
		else
		{
			if (Key == KEY_LEFT || Key == KEY_RIGHT ||Key == KEY_TAB ||
			        Key == KEY_NUMPAD4 || Key == KEY_NUMPAD6 ||
			        Key == KEY_MSWHEEL_LEFT || Key == KEY_MSWHEEL_RIGHT)
			{
				SendKey=Key;
				SubMenu->Close(-1);
				return 1;
			}
		}
		return 0;
	});

	if(SendMouse)
		ProcessMouse(&MouseEvent);
	if(SendKey)
	{
		ProcessKey(Manager::Key(SendKey));
		ProcessKey(Manager::Key(KEY_ENTER));
	}
}

void HMenu::ResizeConsole()
{
	if (SaveScr)
	{
		SaveScr->Discard();
		SaveScr.reset();
	}

	Hide();
	Modal::ResizeConsole();
	SetPosition(0,0,::ScrX,0);
}

wchar_t HMenu::GetHighlights(const HMenuData *_item)
{
	SCOPED_ACTION(CriticalSectionLock)(CS);
	wchar_t Ch=0;

	if (_item)
	{
		const wchar_t *Name=_item->Name;

		if (Name && *Name)
		{
			const wchar_t *ChPtr=wcschr(Name,L'&');

			if (ChPtr)
				Ch=ChPtr[1];
		}
	}

	return Ch;
}

size_t HMenu::CheckHighlights(WORD CheckSymbol, int StartPos)
{
	SCOPED_ACTION(CriticalSectionLock)(CS);

	if (StartPos < 0)
		StartPos=0;

	for (size_t I = StartPos; I < ItemCount; I++)
	{
		wchar_t Ch=GetHighlights((const HMenuData *)(Item+I));

		if (Ch)
		{
			if (ToUpper(CheckSymbol) == ToUpper(Ch))
				return I;
		}
		else if (!CheckSymbol)
			return I;
	}

	return -1;
}
