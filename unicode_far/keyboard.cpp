﻿/*
keyboard.cpp

Функции, имеющие отношение к клавиатуре
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

#include "keyboard.hpp"
#include "keys.hpp"
#include "ctrlobj.hpp"
#include "filepanels.hpp"
#include "panel.hpp"
#include "manager.hpp"
#include "scrbuf.hpp"
#include "savescr.hpp"
#include "lockscrn.hpp"
#include "TPreRedrawFunc.hpp"
#include "syslog.hpp"
#include "interf.hpp"
#include "message.hpp"
#include "config.hpp"
#include "scrsaver.hpp"
#include "strmix.hpp"
#include "console.hpp"
#include "plugins.hpp"
#include "notification.hpp"
#include "language.hpp"
#include "datetime.hpp"

/* start Глобальные переменные */

FarKeyboardState IntKeyState={};

/* end Глобальные переменные */

static std::array<short, WCHAR_MAX + 1> KeyToVKey;
static std::array<wchar_t, 512> VKeyToASCII;

static unsigned int AltValue=0;
static unsigned int KeyCodeForALT_LastPressed=0;

static MOUSE_EVENT_RECORD lastMOUSE_EVENT_RECORD;
enum MODIF_PRESSED_LAST
{
	MODIF_SHIFT = bit(0),
	MODIF_ALT   = bit(1),
	MODIF_RALT  = bit(2),
	MODIF_CTRL  = bit(3),
	MODIF_RCTRL = bit(4),
};
static TBitFlags<size_t> PressedLast;

static clock_t KeyPressedLastTime;

/* ----------------------------------------------------------------- */
struct TFKey
{
	DWORD Key;
	LNGID LocalizedNameId;
	size_t Len;
	const wchar_t *Name;
	const wchar_t *UName;

	bool operator ==(DWORD rhsKey) const {return Key == rhsKey;}
};

static const TFKey FKeys1[]=
{
	{KEY_LAUNCH_MEDIA_SELECT,  MKeyLaunchMediaSelect, 17, L"LaunchMediaSelect",        L"LAUNCHMEDIASELECT"},
	{KEY_BROWSER_FAVORITES,    MKeyBrowserFavorites,  16, L"BrowserFavorites",         L"BROWSERFAVORITES"},
	{KEY_MEDIA_PREV_TRACK,     MKeyMediaPrevTrack,    14, L"MediaPrevTrack",           L"MEDIAPREVTRACK"},
	{KEY_MEDIA_PLAY_PAUSE,     MKeyMediaPlayPause,    14, L"MediaPlayPause",           L"MEDIAPLAYPAUSE"},
	{KEY_MEDIA_NEXT_TRACK,     MKeyMediaNextTrack,    14, L"MediaNextTrack",           L"MEDIANEXTTRACK"},
	{KEY_BROWSER_REFRESH,      MKeyBrowserRefresh,    14, L"BrowserRefresh",           L"BROWSERREFRESH"},
	{KEY_BROWSER_FORWARD,      MKeyBrowserForward,    14, L"BrowserForward",           L"BROWSERFORWARD"},
	{KEY_BROWSER_SEARCH,       MKeyBrowserSearch,     13, L"BrowserSearch",            L"BROWSERSEARCH"},
	{KEY_MSWHEEL_RIGHT,        MKeyMswheelRight,      12, L"MsWheelRight",             L"MSWHEELRIGHT"},
	{KEY_MSWHEEL_DOWN,         MKeyMswheelDown,       11, L"MsWheelDown",              L"MSWHEELDOWN"},
	{KEY_MSWHEEL_LEFT,         MKeyMswheelLeft,       11, L"MsWheelLeft",              L"MSWHEELLEFT"},
	{KEY_BROWSER_STOP,         MKeyBrowserStop,       11, L"BrowserStop",              L"BROWSERSTOP"},
	{KEY_BROWSER_HOME,         MKeyBrowserHome,       11, L"BrowserHome",              L"BROWSERHOME"},
	{KEY_BROWSER_BACK,         MKeyBrowserBack,       11, L"BrowserBack",              L"BROWSERBACK"},
	{KEY_VOLUME_MUTE,          MKeyVolumeMute,        10, L"VolumeMute",               L"VOLUMEMUTE"},
	{KEY_VOLUME_DOWN,          MKeyVolumeDown,        10, L"VolumeDown",               L"VOLUMEDOWN"},
	{KEY_SCROLLLOCK,           MKeyScrolllock,        10, L"ScrollLock",               L"SCROLLLOCK"},
	{KEY_LAUNCH_MAIL,          MKeyLaunchMail,        10, L"LaunchMail",               L"LAUNCHMAIL"},
	{KEY_LAUNCH_APP2,          MKeyLaunchApp2,        10, L"LaunchApp2",               L"LAUNCHAPP2"},
	{KEY_LAUNCH_APP1,          MKeyLaunchApp1,        10, L"LaunchApp1",               L"LAUNCHAPP1"},
	{KEY_MSWHEEL_UP,           MKeyMswheelUp,         9,  L"MsWheelUp",                L"MSWHEELUP"},
	{KEY_MEDIA_STOP,           MKeyMediaStop,         9,  L"MediaStop",                L"MEDIASTOP"},
	{KEY_BACKSLASH,            MKeyBackslash,         9,  L"BackSlash",                L"BACKSLASH"},
	{KEY_MSM1CLICK,            MKeyMsm1click,         9,  L"MsM1Click",                L"MSM1CLICK"},
	{KEY_MSM2CLICK,            MKeyMsm2click,         9,  L"MsM2Click",                L"MSM2CLICK"},
	{KEY_MSM3CLICK,            MKeyMsm3click,         9,  L"MsM3Click",                L"MSM3CLICK"},
	{KEY_MSLCLICK,             MKeyMslclick,          8,  L"MsLClick",                 L"MSLCLICK"},
	{KEY_MSRCLICK,             MKeyMsrclick,          8,  L"MsRClick",                 L"MSRCLICK"},
	{KEY_VOLUME_UP,            MKeyVolumeUp,          8,  L"VolumeUp",                 L"VOLUMEUP"},
	{KEY_SUBTRACT,             MKeySubtract,          8,  L"Subtract",                 L"SUBTRACT"},
	{KEY_NUMENTER,             MKeyNumenter,          8,  L"NumEnter",                 L"NUMENTER"},
	{KEY_MULTIPLY,             MKeyMultiply,          8,  L"Multiply",                 L"MULTIPLY"},
	{KEY_CAPSLOCK,             MKeyCapslock,          8,  L"CapsLock",                 L"CAPSLOCK"},
	{KEY_PRNTSCRN,             MKeyPrntscrn,          8,  L"PrntScrn",                 L"PRNTSCRN"},
	{KEY_NUMLOCK,              MKeyNumlock,           7,  L"NumLock",                  L"NUMLOCK"},
	{KEY_DECIMAL,              MKeyDecimal,           7,  L"Decimal",                  L"DECIMAL"},
	{KEY_STANDBY,              MKeyStandby,           7,  L"Standby",                  L"STANDBY"},
	{KEY_DIVIDE,               MKeyDivide,            6,  L"Divide",                   L"DIVIDE"},
	{KEY_NUMDEL,               MKeyNumdel,            6,  L"NumDel",                   L"NUMDEL"},
	{KEY_SPACE,                MKeySpace,             5,  L"Space",                    L"SPACE"},
	{KEY_RIGHT,                MKeyRight,             5,  L"Right",                    L"RIGHT"},
	{KEY_PAUSE,                MKeyPause,             5,  L"Pause",                    L"PAUSE"},
	{KEY_ENTER,                MKeyEnter,             5,  L"Enter",                    L"ENTER"},
	{KEY_CLEAR,                MKeyClear,             5,  L"Clear",                    L"CLEAR"},
	{KEY_BREAK,                MKeyBreak,             5,  L"Break",                    L"BREAK"},
	{KEY_PGUP,                 MKeyPgup,              4,  L"PgUp",                     L"PGUP"},
	{KEY_PGDN,                 MKeyPgdn,              4,  L"PgDn",                     L"PGDN"},
	{KEY_LEFT,                 MKeyLeft,              4,  L"Left",                     L"LEFT"},
	{KEY_HOME,                 MKeyHome,              4,  L"Home",                     L"HOME"},
	{KEY_DOWN,                 MKeyDown,              4,  L"Down",                     L"DOWN"},
	{KEY_APPS,                 MKeyApps,              4,  L"Apps",                     L"APPS"},
	{KEY_RWIN,                 MKeyRwin,              4,  L"RWin",                     L"RWIN"},
	{KEY_NUMPAD9,              MKeyNumpad9,           4,  L"Num9",                     L"NUM9"},
	{KEY_NUMPAD8,              MKeyNumpad8,           4,  L"Num8",                     L"NUM8"},
	{KEY_NUMPAD7,              MKeyNumpad7,           4,  L"Num7",                     L"NUM7"},
	{KEY_NUMPAD6,              MKeyNumpad6,           4,  L"Num6",                     L"NUM6"},
	{KEY_NUMPAD5,              MKeyNumpad5,           4,  L"Num5",                     L"NUM5"},
	{KEY_NUMPAD4,              MKeyNumpad4,           4,  L"Num4",                     L"NUM4"},
	{KEY_NUMPAD3,              MKeyNumpad3,           4,  L"Num3",                     L"NUM3"},
	{KEY_NUMPAD2,              MKeyNumpad2,           4,  L"Num2",                     L"NUM2"},
	{KEY_NUMPAD1,              MKeyNumpad1,           4,  L"Num1",                     L"NUM1"},
	{KEY_NUMPAD0,              MKeyNumpad0,           4,  L"Num0",                     L"NUM0"},
	{KEY_LWIN,                 MKeyLwin,              4,  L"LWin",                     L"LWIN"},
	{KEY_TAB,                  MKeyTab,               3,  L"Tab",                      L"TAB"},
	{KEY_INS,                  MKeyIns,               3,  L"Ins",                      L"INS"},
	{KEY_F10,                  MKeyF10,               3,  L"F10",                      L"F10"},
	{KEY_F11,                  MKeyF11,               3,  L"F11",                      L"F11"},
	{KEY_F12,                  MKeyF12,               3,  L"F12",                      L"F12"},
	{KEY_F13,                  MKeyF13,               3,  L"F13",                      L"F13"},
	{KEY_F14,                  MKeyF14,               3,  L"F14",                      L"F14"},
	{KEY_F15,                  MKeyF15,               3,  L"F15",                      L"F15"},
	{KEY_F16,                  MKeyF16,               3,  L"F16",                      L"F16"},
	{KEY_F17,                  MKeyF17,               3,  L"F17",                      L"F17"},
	{KEY_F18,                  MKeyF18,               3,  L"F18",                      L"F18"},
	{KEY_F19,                  MKeyF19,               3,  L"F19",                      L"F19"},
	{KEY_F20,                  MKeyF20,               3,  L"F20",                      L"F20"},
	{KEY_F21,                  MKeyF21,               3,  L"F21",                      L"F21"},
	{KEY_F22,                  MKeyF22,               3,  L"F22",                      L"F22"},
	{KEY_F23,                  MKeyF23,               3,  L"F23",                      L"F23"},
	{KEY_F24,                  MKeyF24,               3,  L"F24",                      L"F24"},
	{KEY_ESC,                  MKeyEsc,               3,  L"Esc",                      L"ESC"},
	{KEY_END,                  MKeyEnd,               3,  L"End",                      L"END"},
	{KEY_DEL,                  MKeyDel,               3,  L"Del",                      L"DEL"},
	{KEY_ADD,                  MKeyAdd,               3,  L"Add",                      L"ADD"},
	{KEY_UP,                   MKeyUp,                2,  L"Up",                       L"UP"},
	{KEY_F9,                   MKeyF9,                2,  L"F9",                       L"F9"},
	{KEY_F8,                   MKeyF8,                2,  L"F8",                       L"F8"},
	{KEY_F7,                   MKeyF7,                2,  L"F7",                       L"F7"},
	{KEY_F6,                   MKeyF6,                2,  L"F6",                       L"F6"},
	{KEY_F5,                   MKeyF5,                2,  L"F5",                       L"F5"},
	{KEY_F4,                   MKeyF4,                2,  L"F4",                       L"F4"},
	{KEY_F3,                   MKeyF3,                2,  L"F3",                       L"F3"},
	{KEY_F2,                   MKeyF2,                2,  L"F2",                       L"F2"},
	{KEY_F1,                   MKeyF1,                2,  L"F1",                       L"F1"},
	{KEY_BS,                   MKeyBs,                2,  L"BS",                       L"BS"},
	{KEY_BACKBRACKET,          MKeyBackbracket,       1,  L"]",                        L"]"},
	{KEY_QUOTE,                MKeyQuote,             1,  L"\"",                       L"\""},
	{KEY_BRACKET,              MKeyBracket,           1,  L"[",                        L"["},
	{KEY_COLON,                MKeyColon,             1,  L":",                        L":"},
	{KEY_SEMICOLON,            MKeySemicolon,         1,  L";",                        L";"},
	{KEY_SLASH,                MKeySlash,             1,  L"/",                        L"/"},
	{KEY_DOT,                  MKeyDot,               1,  L".",                        L"."},
	{KEY_COMMA,                MKeyComma,             1,  L",",                        L","},
};

enum modifs
{
	m_rctrl,
	m_ctrl,
	m_shift,
	m_ralt,
	m_alt,
	m_spec,
	m_oem,

	m_count
};
static const TFKey ModifKeyName[]=
{
	{KEY_RCTRL,    MKeyRCtrl,  5, L"RCtrl",  L"RCTRL"},
	{KEY_CTRL,     MKeyCtrl,   4, L"Ctrl",   L"CTRL"},
	{KEY_SHIFT,    MKeyShift,  5, L"Shift",  L"SHIFT"},
	{KEY_RALT,     MKeyRAlt,   4, L"RAlt",   L"RALT"},
	{KEY_ALT,      MKeyAlt,    3, L"Alt",    L"ALT"},
	{KEY_M_SPEC,   LNGID(-1),  4, L"Spec",   L"SPEC"},
	{KEY_M_OEM,    LNGID(-1),  3, L"Oem",    L"OEM"},
};

static_assert(std::size(ModifKeyName) == m_count, "Incomplete ModifKeyName array");

#if defined(SYSLOG)
static const TFKey SpecKeyName[]=
{
	{ KEY_CONSOLE_BUFFER_RESIZE, LNGID(-1), 19, L"ConsoleBufferResize", L"CONSOLEBUFFERRESIZE"},
	{ KEY_OP_SELWORD,            LNGID(-1), 10, L"OP_SelWord",          L"OP_SELWORD"},
	{ KEY_KILLFOCUS,             LNGID(-1), 9,  L"KillFocus",           L"KILLFOCUS"},
	{ KEY_GOTFOCUS,              LNGID(-1), 8,  L"GotFocus",            L"GOTFOCUS"},
	{ KEY_DRAGCOPY,              LNGID(-1), 8,  L"DragCopy",            L"DRAGCOPY"},
	{ KEY_DRAGMOVE,              LNGID(-1), 8,  L"DragMove",            L"DRAGMOVE"},
	{ KEY_OP_PLAINTEXT,          LNGID(-1), 7,  L"OP_Text",             L"OP_TEXT"},
	{ KEY_OP_XLAT,               LNGID(-1), 7,  L"OP_Xlat",             L"OP_XLAT"},
	{ KEY_NONE,                  LNGID(-1), 4,  L"None",                L"NONE"},
	{ KEY_IDLE,                  LNGID(-1), 4,  L"Idle",                L"IDLE"},
};
#endif

/* ----------------------------------------------------------------- */

static auto& Layout()
{
	static std::vector<HKL> s_Layout;
	return s_Layout;
}

/*
   Инициализация массива клавиш.
   Вызывать только после CopyGlobalSettings, потому что только тогда GetRegKey
   считает правильные данные.
*/
void InitKeysArray()
{
	if (const auto LayoutNumber = GetKeyboardLayoutList(0, nullptr))
	{
		Layout().resize(LayoutNumber);
		Layout().resize(GetKeyboardLayoutList(LayoutNumber, Layout().data())); // if less than expected
	}
	else // GetKeyboardLayoutList can return 0 in telnet mode
	{
		Layout().reserve(10);
		for (const auto& i: os::reg::enum_value(HKEY_CURRENT_USER, L"Keyboard Layout\\Preload"))
		{
			if (i.Type() == REG_SZ && std::iswdigit(i.Name().front()))
			{
				string Value = i.GetString();
				if (!Value.empty() && std::iswxdigit(Value.front()))
				{
					try
					{
						if (uintptr_t KbLayout = std::stoul(Value, nullptr, 16))
						{
							if (KbLayout <= 0xffff)
								KbLayout |= KbLayout << 16;
							Layout().emplace_back(reinterpret_cast<HKL>(KbLayout));
						}
					}
					catch (const std::exception&)
					{
						// TODO: log
					}
				}
			}
		}
	}

	KeyToVKey.fill(0);
	VKeyToASCII.fill(0);

	if (!Layout().empty())
	{
		BYTE KeyState[0x100]={};
		//KeyToVKey - используется чтоб проверить если два символа это одна и та же кнопка на клаве
		//*********
		//Так как сделать полноценное мапирование между всеми раскладками не реально,
		//по причине того что во время проигрывания макросов нет такого понятия раскладка
		//то сделаем наилучшую попытку - смысл такой, делаем полное мапирование всех возможных
		//VKs и ShiftVKs в юникодные символы проходясь по всем раскладкам с одним но:
		//если разные VK мапятся в тот же юникод символ то мапирование будет только для первой
		//раскладки которая вернула этот символ
		//
		for (BYTE j=0; j<2; j++)
		{
			KeyState[VK_SHIFT]=j*0x80;

			std::for_each(CONST_RANGE(Layout(), i)
			{
				for (int VK=0; VK<256; VK++)
				{
					wchar_t idx;
					if (ToUnicodeEx(VK, 0, KeyState, &idx, 1, 0, i) > 0)
					{
						if (!KeyToVKey[idx])
							KeyToVKey[idx] = VK + j * 0x100;
					}
				}
			});
		}

		//VKeyToASCII - используется вместе с KeyToVKey чтоб подменить нац. символ на US-ASCII
		//***********
		//Имея мапирование юникод -> VK строим обратное мапирование
		//VK -> символы с кодом меньше 0x80, т.е. только US-ASCII символы
		for (WCHAR i=1; i < 0x80; i++)
		{
			auto x = KeyToVKey[i];

			if (x && !VKeyToASCII[x])
				VKeyToASCII[x]=Upper(i);
		}
	}
}

//Сравнивает если Key и CompareKey это одна и та же клавиша в разных раскладках
bool KeyToKeyLayoutCompare(int Key, int CompareKey)
{
	_KEYMACRO(CleverSysLog Clev(L"KeyToKeyLayoutCompare()"));
	_KEYMACRO(SysLog(L"Param: Key=%08X",Key));
	Key = KeyToVKey[Key&0xFFFF]&0xFF;
	CompareKey = KeyToVKey[CompareKey&0xFFFF]&0xFF;

	if (Key  && Key == CompareKey)
		return true;

	return false;
}

//Должно вернуть клавишный Eng эквивалент Key
int KeyToKeyLayout(int Key)
{
	_KEYMACRO(CleverSysLog Clev(L"KeyToKeyLayout()"));
	_KEYMACRO(SysLog(L"Param: Key=%08X",Key));
	int VK = KeyToVKey[Key&0xFFFF];

	if (VK && VKeyToASCII[VK])
		return VKeyToASCII[VK];

	return Key;
}

/*
  State:
    -1 get state, 0 off, 1 on, 2 flip
*/
int SetFLockState(UINT vkKey, int State)
{
	UINT ExKey=(vkKey==VK_CAPITAL?0:KEYEVENTF_EXTENDEDKEY);

	switch (vkKey)
	{
		case VK_NUMLOCK:
		case VK_CAPITAL:
		case VK_SCROLL:
			break;
		default:
			return -1;
	}

	short oldState=GetKeyState(vkKey);

	if (State >= 0)
	{
		if (State == 2 || (State==1 && !oldState) || (!State && oldState))
		{
			keybd_event(vkKey, 0, ExKey, 0);
			keybd_event(vkKey, 0, ExKey | KEYEVENTF_KEYUP, 0);
		}
	}

	return (int)(WORD)oldState;
}

unsigned int InputRecordToKey(const INPUT_RECORD *r)
{
	if (r)
	{
		INPUT_RECORD Rec=*r; // НАДО!, т.к. внутри CalcKeyCode
		//   структура INPUT_RECORD модифицируется!

		return ShieldCalcKeyCode(&Rec, false);
	}

	return KEY_NONE;
}


bool KeyToInputRecord(int Key, INPUT_RECORD *Rec)
{
	int VirtKey, ControlState;
	return TranslateKeyToVK(Key, VirtKey, ControlState, Rec) != 0;
}

//BUGBUG - временная затычка
void ProcessKeyToInputRecord(int Key, unsigned int dwControlState, INPUT_RECORD *Rec)
{
	if (Rec)
	{
		Rec->EventType=KEY_EVENT;
		Rec->Event.KeyEvent.bKeyDown=1;
		Rec->Event.KeyEvent.wRepeatCount=1;
		Rec->Event.KeyEvent.wVirtualKeyCode=Key;
		Rec->Event.KeyEvent.wVirtualScanCode = MapVirtualKey(Rec->Event.KeyEvent.wVirtualKeyCode,MAPVK_VK_TO_VSC);

		//BUGBUG
		Rec->Event.KeyEvent.uChar.UnicodeChar=MapVirtualKey(Rec->Event.KeyEvent.wVirtualKeyCode,MAPVK_VK_TO_CHAR);

		//BUGBUG
		Rec->Event.KeyEvent.dwControlKeyState=
			(dwControlState&PKF_SHIFT?SHIFT_PRESSED:0)|
			(dwControlState&PKF_ALT?LEFT_ALT_PRESSED:0)|
			(dwControlState&PKF_RALT?RIGHT_ALT_PRESSED:0)|
			(dwControlState&PKF_RCONTROL?RIGHT_CTRL_PRESSED:0)|
			(dwControlState&PKF_CONTROL?LEFT_CTRL_PRESSED:0);
	}
}

void FarKeyToInputRecord(const FarKey& Key,INPUT_RECORD* Rec)
{
	if (Rec)
	{
		Rec->EventType=KEY_EVENT;
		Rec->Event.KeyEvent.bKeyDown=1;
		Rec->Event.KeyEvent.wRepeatCount=1;
		Rec->Event.KeyEvent.wVirtualKeyCode=Key.VirtualKeyCode;
		Rec->Event.KeyEvent.wVirtualScanCode = MapVirtualKey(Rec->Event.KeyEvent.wVirtualKeyCode,MAPVK_VK_TO_VSC);

		//BUGBUG
		Rec->Event.KeyEvent.uChar.UnicodeChar=MapVirtualKey(Rec->Event.KeyEvent.wVirtualKeyCode,MAPVK_VK_TO_CHAR);

		Rec->Event.KeyEvent.dwControlKeyState=Key.ControlKeyState;
	}
}

DWORD IsMouseButtonPressed()
{
	INPUT_RECORD rec;

	if (PeekInputRecord(&rec))
	{
		GetInputRecord(&rec);
	}

	Sleep(1);
	return IntKeyState.MouseButtonState;
}

static auto ButtonStateToKeyMsClick(DWORD ButtonState)
{
	switch (ButtonState)
	{
	case FROM_LEFT_1ST_BUTTON_PRESSED:
		return KEY_MSLCLICK;
	case RIGHTMOST_BUTTON_PRESSED:
		return KEY_MSRCLICK;
	case FROM_LEFT_2ND_BUTTON_PRESSED:
		return KEY_MSM1CLICK;
	case FROM_LEFT_3RD_BUTTON_PRESSED:
		return KEY_MSM2CLICK;
	case FROM_LEFT_4TH_BUTTON_PRESSED:
		return KEY_MSM3CLICK;
	default:
		return KEY_NONE;
	}
}

static auto KeyMsClickToButtonState(DWORD Key)
{
	switch (Key)
	{
	case KEY_MSLCLICK:
		return FROM_LEFT_1ST_BUTTON_PRESSED;
	case KEY_MSM1CLICK:
		return FROM_LEFT_2ND_BUTTON_PRESSED;
	case KEY_MSM2CLICK:
		return FROM_LEFT_3RD_BUTTON_PRESSED;
	case KEY_MSM3CLICK:
		return FROM_LEFT_4TH_BUTTON_PRESSED;
	case KEY_MSRCLICK:
		return RIGHTMOST_BUTTON_PRESSED;
	default:
		return 0;
	}
}

static bool was_repeat = false;
static WORD last_pressed_keycode = (WORD)-1;

bool IsRepeatedKey()
{
	return was_repeat;
}

// "дополнительная" очередь кодов клавиш
static auto& KeyQueue()
{
	static std::deque<DWORD> s_KeyQueue;
	return s_KeyQueue;
}

void ClearKeyQueue()
{
	KeyQueue().clear();
}

DWORD GetInputRecordNoMacroArea(INPUT_RECORD *rec,bool AllowSynchro)
{
	const auto SavedArea = Global->CtrlObject->Macro.GetArea();
	SCOPE_EXIT{ Global->CtrlObject->Macro.SetArea(SavedArea); };

	Global->CtrlObject->Macro.SetArea(MACROAREA_LAST); // чтобы не срабатывали макросы :-)
	return GetInputRecord(rec, false, false, AllowSynchro);
}

static bool ProcessMacros(INPUT_RECORD* rec, DWORD& Result)
{
	if (!Global->CtrlObject && Global->CtrlObject->Cp())
		return false;

	Global->CtrlObject->Macro.RunStartMacro();

	if (const auto MacroKey = Global->CtrlObject->Macro.GetKey())
	{
		static int LastMsClickMacroKey = 0;
		if (const auto MsClickKey = KeyMsClickToButtonState(MacroKey))
		{
			// Ахтунг! Для мышиной клавиши вернем значение MOUSE_EVENT, соответствующее _последнему_ событию мыши.
			rec->EventType = MOUSE_EVENT;
			rec->Event.MouseEvent = lastMOUSE_EVENT_RECORD;
			rec->Event.MouseEvent.dwButtonState = MsClickKey;
			rec->Event.MouseEvent.dwEventFlags = 0;
			LastMsClickMacroKey = MacroKey;
			Result = MacroKey;
			return true;
		}

		// если предыдущая клавиша мышиная - сбросим состояние панели Drag
		if (KeyMsClickToButtonState(LastMsClickMacroKey))
		{
			LastMsClickMacroKey = 0;
			Panel::EndDrag();
		}

		Global->ScrBuf->Flush();
		int VirtKey, ControlState;
		TranslateKeyToVK(MacroKey, VirtKey, ControlState, rec);
		rec->EventType =
			InRange(KEY_MACRO_BASE, static_cast<far_key_code>(MacroKey), KEY_MACRO_ENDBASE) ||
			InRange(KEY_OP_BASE, static_cast<far_key_code>(MacroKey), KEY_OP_ENDBASE) ||
			(MacroKey&~0xFF000000) >= KEY_END_FKEY?
			0 : KEY_EVENT;

		if (!(MacroKey&KEY_SHIFT))
			IntKeyState.LeftShiftPressed = IntKeyState.RightShiftPressed = 0;

		Result = MacroKey;
		return true;
	}


	// BUGBUG should it be here?
	if (Global->WindowManager->HaveAnyMessage())
	{
		Global->WindowManager->PluginCommit();
	}
	return false;
}

static void DropConsoleInputEvent()
{
	INPUT_RECORD rec;
	size_t ReadCount;
	Console().ReadInput(&rec, 1, ReadCount);
}

static void UpdateIntKeyState(DWORD CtrlState)
{
	IntKeyState.LeftCtrlPressed = (CtrlState & LEFT_CTRL_PRESSED) != 0;
	IntKeyState.LeftAltPressed = (CtrlState & LEFT_ALT_PRESSED) != 0;
	IntKeyState.LeftShiftPressed = (CtrlState & SHIFT_PRESSED) != 0;
	IntKeyState.RightCtrlPressed = (CtrlState & RIGHT_CTRL_PRESSED) != 0;
	IntKeyState.RightAltPressed = (CtrlState & RIGHT_ALT_PRESSED) != 0;
	IntKeyState.RightShiftPressed = (CtrlState & SHIFT_PRESSED) != 0; // ???
}

static DWORD ProcessFocusEvent(bool Got)
{
	/* $ 28.04.2001 VVM
	+ Не только обработаем сами смену фокуса, но и передадим дальше */
	PressedLast.ClearAll();

	UpdateIntKeyState(0);

	IntKeyState.MouseButtonState = 0;

	const auto CalcKey = Got? KEY_GOTFOCUS : KEY_KILLFOCUS;

	if (!IsWindows10OrGreater())
	{
		//чтоб решить баг винды приводящий к появлению скролов и т.п. после потери фокуса
		CalcKey == KEY_GOTFOCUS? RestoreConsoleWindowRect() : SaveConsoleWindowRect();
	}
	return CalcKey;
}

static DWORD ProcessBufferSizeEvent(COORD Size)
{
	// BUGBUG If initial mode was fullscreen - first transition will not be detected
	static auto StoredConsoleFullscreen = false;

	DWORD DisplayMode = 0;
	const auto CurrentConsoleFullscreen = IsWindows10OrGreater() && Console().GetDisplayMode(DisplayMode) && DisplayMode & CONSOLE_FULLSCREEN;
	const auto TransitionFromFullScreen = StoredConsoleFullscreen && !CurrentConsoleFullscreen;
	StoredConsoleFullscreen = CurrentConsoleFullscreen;

	int PScrX = ScrX;
	int PScrY = ScrY;

	GetVideoMode(CurSize);
	const auto Ignore = !Global->Opt->WindowMode || (Size.X == CurSize.X && Size.Y == CurSize.Y);
	if (PScrX + 1 == CurSize.X && PScrY + 1 == CurSize.Y && Ignore)
	{
		return KEY_NONE;
	}

	PrevScrX = PScrX;
	PrevScrY = PScrY;

	AdjustConsoleScreenBufferSize(TransitionFromFullScreen);

	if (Global->WindowManager)
	{
		// апдейтим панели (именно они сейчас!)
		SCOPED_ACTION(LockScreen);

		if (Global->GlobalSaveScrPtr)
			Global->GlobalSaveScrPtr->Discard();

		Global->WindowManager->ResizeAllWindows();
		Global->WindowManager->GetCurrentWindow()->Show();
		// _SVS(SysLog(L"PreRedrawFunc = %p",PreRedrawFunc));
		if (!PreRedrawStack().empty())
		{
			PreRedrawStack().top()->m_PreRedrawFunc();
		}
	}

	return KEY_CONSOLE_BUFFER_RESIZE;
}

static const far_key_code WheelKeys[][2] =
{
	{ KEY_MSWHEEL_DOWN, KEY_MSWHEEL_UP },
	{ KEY_MSWHEEL_LEFT, KEY_MSWHEEL_RIGHT }
};

static bool ProcessMouseEvent(const MOUSE_EVENT_RECORD& MouseEvent, bool ExcludeMacro, bool ProcessMouse, DWORD& CalcKey)
{
	lastMOUSE_EVENT_RECORD = MouseEvent;
	IntKeyState.PreMouseEventFlags = std::exchange(IntKeyState.MouseEventFlags, MouseEvent.dwEventFlags);
	const auto CtrlState = MouseEvent.dwControlKeyState;
	KeyMacro::SetMacroConst(constMsCtrlState, CtrlState);
	KeyMacro::SetMacroConst(constMsEventFlags, IntKeyState.MouseEventFlags);
	KeyMacro::SetMacroConst(constMsLastCtrlState, CtrlState);

	UpdateIntKeyState(CtrlState);

	const auto BtnState = MouseEvent.dwButtonState;
	KeyMacro::SetMacroConst(constMsButton, MouseEvent.dwButtonState);

	if (IntKeyState.MouseEventFlags != MOUSE_MOVED)
	{
		IntKeyState.PrevMouseButtonState = IntKeyState.MouseButtonState;
	}

	IntKeyState.MouseButtonState = BtnState;
	IntKeyState.PrevMouseX = IntKeyState.MouseX;
	IntKeyState.PrevMouseY = IntKeyState.MouseY;
	IntKeyState.MouseX = MouseEvent.dwMousePosition.X;
	IntKeyState.MouseY = MouseEvent.dwMousePosition.Y;
	KeyMacro::SetMacroConst(constMsX, IntKeyState.MouseX);
	KeyMacro::SetMacroConst(constMsY, IntKeyState.MouseY);

	/* $ 26.04.2001 VVM
	+ Обработка колесика мышки. */

	const auto GetModifiers = [CtrlState]
	{
		return
			(CtrlState & SHIFT_PRESSED? KEY_SHIFT : NO_KEY) |
			(CtrlState & LEFT_CTRL_PRESSED? KEY_CTRL : NO_KEY) |
			(CtrlState & RIGHT_CTRL_PRESSED? KEY_RCTRL : NO_KEY) |
			(CtrlState & LEFT_ALT_PRESSED? KEY_ALT : NO_KEY) |
			(CtrlState & RIGHT_ALT_PRESSED? KEY_RALT : NO_KEY);
	};

	if (IntKeyState.MouseEventFlags == MOUSE_WHEELED || IntKeyState.MouseEventFlags == MOUSE_HWHEELED)
	{
		const auto& WheelKeysPair = WheelKeys[IntKeyState.MouseEventFlags == MOUSE_HWHEELED? 1 : 0];
		const auto Key = WheelKeysPair[static_cast<short>(HIWORD(MouseEvent.dwButtonState)) > 0? 1 : 0];
		CalcKey = Key | GetModifiers();
	}

	if ((!ExcludeMacro || ProcessMouse) && Global->CtrlObject && (ProcessMouse || !(Global->CtrlObject->Macro.IsRecording() || Global->CtrlObject->Macro.IsExecuting())))
	{
		if (IntKeyState.MouseEventFlags != MOUSE_MOVED)
		{
			const auto MsCalcKey = ButtonStateToKeyMsClick(MouseEvent.dwButtonState);
			if (MsCalcKey != KEY_NONE)
			{
				CalcKey = MsCalcKey | GetModifiers();

				// для WaitKey()
				if (ProcessMouse)
				{
					return true;
				}
			}
		}
	}
	return false;
}

static DWORD GetInputRecordImpl(INPUT_RECORD *rec,bool ExcludeMacro,bool ProcessMouse,bool AllowSynchro)
{
	_KEYMACRO(CleverSysLog Clev(L"GetInputRecord()"));

	if (AllowSynchro)
		MessageManager().dispatch();

	DWORD CalcKey;

	if (!ExcludeMacro)
	{
		if (ProcessMacros(rec, CalcKey))
		{
			return CalcKey;
		}
	}

	auto NotMacros = false;

	const auto ProcessMacroEvent = [&]
	{
		if (NotMacros || ExcludeMacro)
			return CalcKey;

		_KEYMACRO(SysLog(L"[%d] CALL Global->CtrlObject->Macro.ProcessEvent(%s)", __LINE__, _FARKEY_ToName(CalcKey)));

		FAR_INPUT_RECORD irec = { CalcKey, *rec };
		if (!Global->CtrlObject || !Global->CtrlObject->Macro.ProcessEvent(&irec))
			return CalcKey;

		rec->EventType = 0;
		return static_cast<DWORD>(KEY_NONE);
	};

	if (KeyQueue().size())
	{
		CalcKey=KeyQueue().front();
		KeyQueue().pop_front();
		NotMacros = (CalcKey & 0x80000000) != 0;
		CalcKey &= ~0x80000000;
		return ProcessMacroEvent();
	}

	const auto EnableShowTime = Global->Opt->Clock && (Global->WaitInMainLoop || (Global->CtrlObject && Global->CtrlObject->Macro.GetArea() == MACROAREA_SEARCH));

	if (EnableShowTime)
		ShowTime();

	Global->ScrBuf->Flush();

	static auto LastEventIdle = false;

	if (!LastEventIdle)
		Global->StartIdleTime=clock();

	LastEventIdle = false;

	BOOL ZoomedState=IsZoomed(Console().GetWindow());
	BOOL IconicState=IsIconic(Console().GetWindow());

	bool FullscreenState=IsConsoleFullscreen();

	DWORD LoopCount = 0;
	for (;;)
	{
		// "Реакция" на максимизацию/восстановление окна консоли
		if (ZoomedState!=IsZoomed(Console().GetWindow()) && IconicState==IsIconic(Console().GetWindow()))
		{
			ZoomedState=!ZoomedState;
			ChangeVideoMode(ZoomedState != FALSE);
		}

		if (!(LoopCount & 15))
		{
			if(Global->CtrlObject && Global->CtrlObject->Plugins->size())
			{
				SetFarConsoleMode();
			}

			const auto CurrentFullscreenState = IsConsoleFullscreen();
			if(CurrentFullscreenState && !FullscreenState)
			{
				ChangeVideoMode(25,80);
			}
			FullscreenState=CurrentFullscreenState;
		}

		size_t ReadCount;
		Console().PeekInput(rec, 1, ReadCount);
		if (ReadCount)
		{
			//check for flock
			if (rec->EventType==KEY_EVENT && !rec->Event.KeyEvent.wVirtualScanCode && (rec->Event.KeyEvent.wVirtualKeyCode==VK_NUMLOCK||rec->Event.KeyEvent.wVirtualKeyCode==VK_CAPITAL||rec->Event.KeyEvent.wVirtualKeyCode==VK_SCROLL))
			{
				DropConsoleInputEvent();
				was_repeat = false;
				last_pressed_keycode = (WORD)-1;
				continue;
			}
			break;
		}

		Global->ScrBuf->Flush();
		Sleep(10);

		static bool ExitInProcess = false;
		if (Global->CloseFAR && !ExitInProcess)
		{
			ExitInProcess = true;
			Global->WindowManager->ExitMainLoop(FALSE);
			return KEY_NONE;
		}

		if (!(LoopCount & 15))
		{
			clock_t CurTime=clock();

			if (EnableShowTime)
				ShowTime();

			if (Global->WaitInMainLoop)
			{
				if (!(LoopCount & 63))
				{
					static bool UpdateReenter = false;

					if (!UpdateReenter && CurTime-KeyPressedLastTime>300)
					{
						if (Global->WindowManager->IsPanelsActive())
						{
							UpdateReenter = true;
							Global->CtrlObject->Cp()->LeftPanel()->UpdateIfChanged(true);
							Global->CtrlObject->Cp()->RightPanel()->UpdateIfChanged(true);
							UpdateReenter = false;
						}
					}
				}
			}

			if (Global->Opt->ScreenSaver && Global->Opt->ScreenSaverTime>0 &&
			        CurTime-Global->StartIdleTime>Global->Opt->ScreenSaverTime*60000)
				if (!ScreenSaver())
					return KEY_NONE;

			if (!Global->WaitInMainLoop && LoopCount==64)
			{
				LastEventIdle = true;
				*rec = {};
				rec->EventType=KEY_EVENT;
				return KEY_IDLE;
			}
		}

		if (!(LoopCount & 3))
		{
			if (MessageManager().dispatch())
			{
				*rec = {};
				return KEY_NONE;
			}
		}

		LoopCount++;
	}


	clock_t CurClock=clock();

	if (rec->EventType==KEY_EVENT)
	{
		static bool bForceAltGr = false;

		if (!rec->Event.KeyEvent.bKeyDown)
		{
			was_repeat = false;
			last_pressed_keycode = (WORD)-1;
		}
		else
		{
			was_repeat = (last_pressed_keycode == rec->Event.KeyEvent.wVirtualKeyCode);
			last_pressed_keycode = rec->Event.KeyEvent.wVirtualKeyCode;

			if (rec->Event.KeyEvent.wVirtualKeyCode == VK_MENU)
			{
				// Шаманство с AltGr (виртуальная клавиатура)
				bForceAltGr = (rec->Event.KeyEvent.wVirtualScanCode == 0)
					&& ((rec->Event.KeyEvent.dwControlKeyState & 0x1F) == 0x0A);
			}
		}

		if (bForceAltGr && (rec->Event.KeyEvent.dwControlKeyState & 0x1F) == 0x0A)
		{
			rec->Event.KeyEvent.dwControlKeyState &= ~LEFT_ALT_PRESSED;
			rec->Event.KeyEvent.dwControlKeyState |= RIGHT_ALT_PRESSED;
		}

		DWORD CtrlState=rec->Event.KeyEvent.dwControlKeyState;

		if (Global->CtrlObject && Global->CtrlObject->Macro.IsRecording())
		{
			static WORD PrevVKKeyCode=0; // NumLock+Cursor
			WORD PrevVKKeyCode2=PrevVKKeyCode;
			PrevVKKeyCode=rec->Event.KeyEvent.wVirtualKeyCode;

			// Для Shift-Enter в диалоге назначения вылазил Shift после отпускания клавиш.
			//
			if (PrevVKKeyCode2==VK_RETURN && PrevVKKeyCode==VK_SHIFT  && !rec->Event.KeyEvent.bKeyDown)
			{
				DropConsoleInputEvent();
				return KEY_NONE;
			}
		}

		UpdateIntKeyState(CtrlState);

		KeyPressedLastTime=CurClock;
	}
	else
	{
		was_repeat = false;
		last_pressed_keycode = (WORD)-1;
	}

	IntKeyState.ReturnAltValue=FALSE;
	CalcKey=CalcKeyCode(rec, true, &NotMacros);

	if (IntKeyState.ReturnAltValue)
	{
		return ProcessMacroEvent();
	}

	{
		size_t ReadCount;
		Console().ReadInput(rec, 1, ReadCount);
	}

	if (EnableShowTime)
		ShowTime();

	if (rec->EventType == FOCUS_EVENT)
	{
		return ProcessFocusEvent(rec->Event.FocusEvent.bSetFocus != FALSE);
	}

	if (rec->EventType==WINDOW_BUFFER_SIZE_EVENT || IsConsoleSizeChanged())
	{
		return ProcessBufferSizeEvent(rec->Event.WindowBufferSizeEvent.dwSize);
	}

	if (rec->EventType==KEY_EVENT)
	{
		DWORD CtrlState=rec->Event.KeyEvent.dwControlKeyState;
		DWORD KeyCode=rec->Event.KeyEvent.wVirtualKeyCode;

		UpdateIntKeyState(CtrlState);

		KeyMacro::SetMacroConst(constMsLastCtrlState,CtrlState);

		// Для NumPad!
		if ((CalcKey&(KEY_CTRL|KEY_SHIFT|KEY_ALT|KEY_RCTRL|KEY_RALT)) == KEY_SHIFT &&
		        (CalcKey&KEY_MASKF) >= KEY_NUMPAD0 && (CalcKey&KEY_MASKF) <= KEY_NUMPAD9)
			IntKeyState.LeftShiftPressed = IntKeyState.RightShiftPressed = true;
		else
			IntKeyState.LeftShiftPressed = IntKeyState.RightShiftPressed = (CtrlState & SHIFT_PRESSED) != 0;

		if (!KeyCode)
			return KEY_NONE;

		struct KeysData
		{
			size_t FarKey;
			size_t VkKey;
			size_t Modif;
			bool Enhanced;
		} Keys[]=
		{
			{KEY_SHIFT,VK_SHIFT,MODIF_SHIFT,false},
			{KEY_ALT,VK_MENU,MODIF_ALT,false},
			{KEY_RALT,VK_MENU,MODIF_RALT,true},
			{KEY_CTRL,VK_CONTROL,MODIF_CTRL,false},
			{KEY_RCTRL,VK_CONTROL,MODIF_RCTRL,true}
		};
		std::for_each(ALL_CONST_RANGE(Keys), [&CalcKey](const KeysData& A){if (CalcKey == A.FarKey && !PressedLast.Check(A.Modif)) CalcKey=KEY_NONE;});
		const size_t AllModif = KEY_CTRL | KEY_ALT | KEY_SHIFT | KEY_RCTRL | KEY_RALT;
		if ((CalcKey&AllModif) && !(CalcKey&~AllModif) && !PressedLast.Check(MODIF_SHIFT | MODIF_ALT | MODIF_RALT | MODIF_CTRL | MODIF_RCTRL)) CalcKey=KEY_NONE;
		PressedLast.ClearAll();
		if (rec->Event.KeyEvent.bKeyDown)
		{
			std::for_each(ALL_CONST_RANGE(Keys), [KeyCode, CtrlState](const KeysData& A){if (KeyCode == A.VkKey && (!A.Enhanced || CtrlState&ENHANCED_KEY)) PressedLast.Set(A.Modif);});
		}

		Panel::EndDrag();
	}

	if (rec->EventType==MOUSE_EVENT)
	{
		if (ProcessMouseEvent(rec->Event.MouseEvent, ExcludeMacro, ProcessMouse, CalcKey))
			return CalcKey;
	}

	return ProcessMacroEvent();
}

DWORD GetInputRecord(INPUT_RECORD *rec, bool ExcludeMacro, bool ProcessMouse, bool AllowSynchro)
{
	DWORD Key = GetInputRecordImpl(rec, ExcludeMacro, ProcessMouse, AllowSynchro);

	if (Key)
	{
		if (Global->CtrlObject)
		{
			ProcessConsoleInputInfo Info = { sizeof(Info), PCIF_NONE, *rec };

			if (Global->WaitInMainLoop)
				Info.Flags |= PCIF_FROMMAIN;
			switch (Global->CtrlObject->Plugins->ProcessConsoleInput(&Info))
			{
			case 1:
				Key = KEY_NONE;
				KeyToInputRecord(Key, rec);
				break;
			case 2:
				*rec = Info.Rec;
				Key = CalcKeyCode(rec, false);
				break;
			}
		}
	}
	return Key;
}

DWORD PeekInputRecord(INPUT_RECORD *rec,bool ExcludeMacro)
{
	size_t ReadCount;
	DWORD Key;
	Global->ScrBuf->Flush();

	if (!KeyQueue().empty() && (Key = KeyQueue().front()) != 0)
	{
		int VirtKey,ControlState;
		ReadCount=TranslateKeyToVK(Key,VirtKey,ControlState,rec)?1:0;
	}
	else if (!ExcludeMacro && (Key=Global->CtrlObject->Macro.PeekKey()) != 0)
	{
		int VirtKey,ControlState;
		ReadCount=TranslateKeyToVK(Key,VirtKey,ControlState,rec)?1:0;
	}
	else
	{
		Console().PeekInput(rec, 1, ReadCount);
	}

	if (!ReadCount)
		return 0;

	return CalcKeyCode(rec, true); // ShieldCalcKeyCode?
}

/* $ 24.08.2000 SVS
 + Параметр у функции WaitKey - возможность ожидать конкретную клавишу
     Если KeyWait = -1 - как и раньше
*/
DWORD WaitKey(DWORD KeyWait,DWORD delayMS,bool ExcludeMacro)
{
	clock_t CheckTime=clock()+delayMS;
	DWORD Key;

	for (;;)
	{
		INPUT_RECORD rec;
		Key=KEY_NONE;

		if (PeekInputRecord(&rec,ExcludeMacro))
		{
			Key=GetInputRecord(&rec,ExcludeMacro,true);
		}

		if (KeyWait == (DWORD)-1)
		{
			if ((Key&~KEY_CTRLMASK) < KEY_END_FKEY || IsInternalKeyReal(Key&~KEY_CTRLMASK))
				break;
		}
		else if (Key == KeyWait)
			break;

		if (delayMS && clock() >= CheckTime)
		{
			Key=KEY_NONE;
			break;
		}

		Sleep(1);
	}

	return Key;
}

bool WriteInput(int Key)
{
	if (KeyQueue().size() > 1024)
		return false;

	KeyQueue().emplace_back(Key);
	return true;
}

bool CheckForEscSilent()
{
	if(Global->CloseFAR)
	{
		return true;
	}

	INPUT_RECORD rec;
	bool Processed = true;
	/* TODO: Здесь, в общем то - ХЗ, т.к.
	         по хорошему нужно проверять Global->CtrlObject->Macro.PeekKey() на ESC или BREAK
	         Но к чему это приведет - пока не могу дать ответ !!!
	*/

	// если в "макросе"...
	if (Global->CtrlObject->Macro.IsExecuting() != MACROSTATE_NOMACRO && Global->WindowManager->GetCurrentWindow())
	{
		if (Global->CtrlObject->Macro.IsDisableOutput())
			Processed = false;
	}

	if (Processed && PeekInputRecord(&rec))
	{
		int Key=GetInputRecordNoMacroArea(&rec,false);

		if (Key==KEY_ESC)
			return true;
		if (Key==KEY_BREAK)
			return true;
		if (Key==KEY_ALTF9 || Key==KEY_RALTF9)
			Global->WindowManager->ProcessKey(Manager::Key(KEY_ALTF9));
	}

	if (!Processed && Global->CtrlObject->Macro.IsExecuting() != MACROSTATE_NOMACRO)
		Global->ScrBuf->Flush();

	return false;
}

bool ConfirmAbortOp()
{
	return (Global->Opt->Confirm.Esc && !Global->CloseFAR)? AbortMessage() : true;
}

/* $ 09.02.2001 IS
     Подтверждение нажатия Esc
*/
bool CheckForEsc()
{
	return CheckForEscSilent()? ConfirmAbortOp() : false;
}

using tfkey_to_text = const wchar_t*(const TFKey*);
using add_separator = void(string&);

static void GetShiftKeyName(string& strName, DWORD Key, tfkey_to_text ToText, add_separator AddSeparator)
{
	static const std::pair<far_key_code, modifs> Mapping[] =
	{
		{ KEY_CTRL, m_ctrl },
		{ KEY_RCTRL, m_rctrl },
		{ KEY_ALT, m_alt },
		{ KEY_RALT, m_ralt },
		{ KEY_SHIFT, m_shift },
		{ KEY_M_SPEC, m_spec },
		{ KEY_M_OEM, m_oem },
	};

	for (const auto& i:  Mapping)
	{
		if (Key & i.first)
		{
			AddSeparator(strName);
			strName += ToText(ModifKeyName + i.second);
		}
	}
}


/* $ 24.09.2000 SVS
 + Функция KeyNameToKey - получение кода клавиши по имени
   Если имя не верно или нет такого - возвращается -1
   Может и криво, но правильно и коротко!

   Функция KeyNameToKey ждет строку по вот такой спецификации:

   1. Сочетания, определенные в структуре FKeys1[]
   2. Опциональные модификаторы (Alt/RAlt/Ctrl/RCtrl/Shift) и 1 символ, например, AltD или CtrlC
   3. "Alt" (или RAlt) и 5 десятичных цифр (с ведущими нулями)
   4. "Spec" и 5 десятичных цифр (с ведущими нулями)
   5. "Oem" и 5 десятичных цифр (с ведущими нулями)
   6. только модификаторы (Alt/RAlt/Ctrl/RCtrl/Shift)
*/
int KeyNameToKey(const string& Name)
{
	if (Name.empty())
		return -1;

	DWORD Key=0;

	if (Name.size() > 1) // если не один символ
	{
		if (Name[0] == L'%')
		return -1;

		if (Name.find_first_of(L"()") != string::npos) // встречаются '(' или ')', то это явно не клавиша!
		return -1;
	}

	size_t Pos = 0;
	static string strTmpName;
	strTmpName = Upper(Name);
	const auto Len = strTmpName.size();

	// пройдемся по всем модификаторам
	for (const auto& i: ModifKeyName)
	{
		if (!(Key & i.Key) && strTmpName.find(i.UName) != string::npos)
		{
			Key |= i.Key;
			Pos += i.Len * ReplaceStrings(strTmpName, i.UName, L"", true);
		}
	}

	// если что-то осталось - преобразуем.
	if (Pos < Len)
	{
		// сначала - FKeys1 - Вариант (1)
		const wchar_t* Ptr=Name.data()+Pos;
		const auto PtrLen = Len-Pos;

		const auto ItemIterator = std::find_if(CONST_REVERSE_RANGE(FKeys1, i)
		{
			return PtrLen == i.Len && !StrCmpI(Ptr, i.Name);
		});

		if (ItemIterator != std::crend(FKeys1))
		{
			Key |= ItemIterator->Key;
			Pos += ItemIterator->Len;
		}
		else // F-клавиш нет?
		{
			/*
				здесь только 5 оставшихся вариантов:
				2) Опциональные модификаторы (Alt/RAlt/Ctrl/RCtrl/Shift) и 1 символ, например, AltD или CtrlC
				3) "Alt" (или RAlt) и 5 десятичных цифр (с ведущими нулями)
				4) "Spec" и 5 десятичных цифр (с ведущими нулями)
				5) "Oem" и 5 десятичных цифр (с ведущими нулями)
				6) только модификаторы (Alt/RAlt/Ctrl/RCtrl/Shift)
			*/

			if (Len == 1 || Pos == Len-1) // Вариант (2)
			{
				int Chr=Name[Pos];

				// если были модификаторы Alt/Ctrl, то преобразуем в "физическую клавишу" (независимо от языка)
				if (Key&(KEY_ALT|KEY_RCTRL|KEY_CTRL|KEY_RALT))
				{
					if (Chr > 0x7F)
						Chr=KeyToKeyLayout(Chr);

					Chr=Upper(Chr);
				}

				Key|=Chr;

				if (Chr)
					Pos++;
			}
			else if (Key == KEY_ALT || Key == KEY_RALT || Key == KEY_M_SPEC || Key == KEY_M_OEM) // Варианты (3), (4) и (5)
			{
				wchar_t *endptr=nullptr;
				int K = static_cast<int>(std::wcstol(Ptr, &endptr, 10));

				if (Ptr+5 == endptr)
				{
					if (Key == KEY_M_SPEC) // Вариант (4)
						Key=(Key|(K+KEY_VK_0xFF_BEGIN))&(~(KEY_M_SPEC|KEY_M_OEM));
					else if (Key == KEY_M_OEM) // Вариант (5)
						Key=(Key|(K+KEY_FKEY_BEGIN))&(~(KEY_M_SPEC|KEY_M_OEM));

					Pos=Len;
				}
			}
			// Вариант (6). Уже "собран".
		}
	}

	return (!Key || Pos < Len)? -1: (int)Key;
}

bool InputRecordToText(const INPUT_RECORD *Rec, string &strKeyText)
{
	return KeyToText(InputRecordToKey(Rec),strKeyText) != 0;
}

bool KeyToTextImpl(int Key0, string& strKeyText, tfkey_to_text ToText, add_separator AddSeparator)
{
	strKeyText.clear();

	if (Key0 == -1)
		return false;

	DWORD Key=(DWORD)Key0, FKey=(DWORD)Key0&0xFFFFFF;

	GetShiftKeyName(strKeyText, Key, ToText, AddSeparator);

	const auto FKeys1Iterator = std::find(ALL_CONST_RANGE(FKeys1), FKey);
	if (FKeys1Iterator != std::cend(FKeys1))
	{
		AddSeparator(strKeyText);
		strKeyText += ToText(FKeys1Iterator);
	}
	else
	{
		FormatString strKeyTemp;
		if (FKey >= KEY_VK_0xFF_BEGIN && FKey <= KEY_VK_0xFF_END)
		{
			strKeyTemp << L"Spec" <<fmt::MinWidth(5) << FKey-KEY_VK_0xFF_BEGIN;
			AddSeparator(strKeyText);
			strKeyText += strKeyTemp;
		}
		else if (FKey > KEY_LAUNCH_APP2 && FKey < KEY_MSWHEEL_UP)
		{
			strKeyTemp << L"Oem" <<fmt::MinWidth(5) << FKey-KEY_FKEY_BEGIN;
			AddSeparator(strKeyText);
			strKeyText += strKeyTemp;
		}
		else
		{
#if defined(SYSLOG)
			// Этот кусок кода нужен только для того, что "спецклавиши" логировались нормально
			const auto SpecKeyIterator = std::find(ALL_CONST_RANGE(SpecKeyName), FKey);
			if (SpecKeyIterator != std::cend(SpecKeyName))
			{
				AddSeparator(strKeyText);
				strKeyText += ToText(SpecKeyIterator);
			}
			else
#endif
			{
				FKey=Upper((wchar_t)(Key&0xFFFF));

				wchar_t KeyText[2]={};

				if (FKey >= L'A' && FKey <= L'Z')
				{
					if (Key&(KEY_RCTRL|KEY_CTRL|KEY_RALT|KEY_ALT)) // ??? а если есть другие модификаторы ???
						KeyText[0]=(wchar_t)FKey; // для клавиш с модификаторами подставляем "латиницу" в верхнем регистре
					else
						KeyText[0]=(wchar_t)(Key&0xFFFF);
				}
				else
					KeyText[0]=(wchar_t)(Key&0xFFFF);

				AddSeparator(strKeyText);
				strKeyText += KeyText;
			}
		}
	}

	return !strKeyText.empty();
}

bool KeyToText(int Key, string &strKeyText)
{
	return KeyToTextImpl(Key, strKeyText,
		[](const TFKey* i) { return i->Name; },
		[](string&) {}
	);
}

bool KeyToLocalizedText(int Key, string &strKeyText)
{
	return KeyToTextImpl(Key, strKeyText,
		[](const TFKey* i)
		{
			if (i->LocalizedNameId != LNGID(-1))
			{
				const auto Msg = MSG(i->LocalizedNameId);
				if (*Msg)
					return Msg;
			}
			return i->Name;
		},
		[](string& str)
		{
			if (!str.empty())
				str += L"+";
		}
	);
}

int TranslateKeyToVK(int Key,int &VirtKey,int &ControlState,INPUT_RECORD *Rec)
{
	_KEYMACRO(CleverSysLog Clev(L"TranslateKeyToVK()"));
	_KEYMACRO(SysLog(L"Param: Key=%08X",Key));

	WORD EventType=KEY_EVENT;

	DWORD FKey  =Key&KEY_END_SKEY;
	DWORD FShift=Key&KEY_CTRLMASK;

	VirtKey=0;

	ControlState=(FShift&KEY_SHIFT?PKF_SHIFT:0)|
	             (FShift&KEY_ALT?PKF_ALT:0)|
	             (FShift&KEY_RALT?PKF_RALT:0)|
	             (FShift&KEY_RCTRL?PKF_RCONTROL:0)|
	             (FShift&KEY_CTRL?PKF_CONTROL:0);

	bool KeyInTable = false;
	{
		static const std::pair<int, int> Table_KeyToVK[] =
		{
			{ KEY_BREAK, VK_CANCEL },
			{ KEY_BS, VK_BACK },
			{ KEY_TAB, VK_TAB },
			{ KEY_ENTER, VK_RETURN },
			{ KEY_NUMENTER, VK_RETURN }, //????
			{ KEY_ESC, VK_ESCAPE },
			{ KEY_SPACE, VK_SPACE },
			{ KEY_NUMPAD5, VK_CLEAR },
		};

		const auto ItemIterator = std::find_if(CONST_RANGE(Table_KeyToVK, i) { return static_cast<DWORD>(i.first) == FKey; });
		if (ItemIterator != std::cend(Table_KeyToVK))
		{
			VirtKey = ItemIterator->second;
			KeyInTable = true;
		}
	}

	if (!KeyInTable)
	{
		if ((FKey>=L'0' && FKey<=L'9') || (FKey>=L'A' && FKey<=L'Z'))
		{
			VirtKey=FKey;
			if ((FKey>=L'A' && FKey<=L'Z') && !(FShift&0xFF000000))
				FShift |= KEY_SHIFT;
		}
		else if (FKey > KEY_FKEY_BEGIN && FKey < KEY_END_FKEY)
			VirtKey=FKey-KEY_FKEY_BEGIN;
		else if (FKey && FKey < WCHAR_MAX)
		{
			short Vk = VkKeyScan(static_cast<WCHAR>(FKey));
			if (Vk == -1)
			{
				std::any_of(CONST_RANGE(Layout(), i)
				{
					return (Vk = VkKeyScanEx(static_cast<WCHAR>(FKey), i)) != -1;
				});
			}

			if (Vk == -1)
			{
				// Заполнить хотя бы .UnicodeChar = FKey
				VirtKey = -1;
			}
			else
			{
				if (IsCharUpper(FKey) && !(FShift&0xFF000000))
					FShift |= KEY_SHIFT;

				VirtKey = Vk&0xFF;
				if (HIBYTE(Vk)&&(HIBYTE(Vk)&6)!=6) //RAlt-E в немецкой раскладке это евро, а не CtrlRAltЕвро
				{
					FShift|=(HIBYTE(Vk)&1?KEY_SHIFT:NO_KEY)|
					        (HIBYTE(Vk)&2?KEY_CTRL:NO_KEY)|
					        (HIBYTE(Vk)&4?KEY_ALT:NO_KEY);

					ControlState=(FShift&KEY_SHIFT?PKF_SHIFT:0)|
					        (FShift&KEY_ALT?PKF_ALT:0)|
					        (FShift&KEY_RALT?PKF_RALT:0)|
					        (FShift&KEY_RCTRL?PKF_RCONTROL:0)|
					        (FShift&KEY_CTRL?PKF_CONTROL:0);
				}
			}

		}
		else if (!FKey)
		{
			static const std::pair<far_key_code, DWORD> ExtKeyMap[]=
			{
				{KEY_SHIFT, VK_SHIFT},
				{KEY_CTRL, VK_CONTROL},
				{KEY_ALT, VK_MENU},
				{KEY_RSHIFT, VK_RSHIFT},
				{KEY_RCTRL, VK_RCONTROL},
				{KEY_RALT, VK_RMENU},
			};

			// In case of CtrlShift, CtrlAlt, AltShift, CtrlAltShift there is no unambiguous mapping.
			const auto ItemIterator = std::find_if(CONST_RANGE(ExtKeyMap, i) { return (i.first & FShift) != 0; });
			if (ItemIterator != std::cend(ExtKeyMap))
				VirtKey = ItemIterator->second;
		}
		else
		{
			VirtKey=FKey;
			switch (FKey)
			{
				case KEY_NUMDEL:
					VirtKey=VK_DELETE;
					break;
				case KEY_NUMENTER:
					VirtKey=VK_RETURN;
					break;

				case KEY_NONE:
				case KEY_IDLE:
					EventType=MENU_EVENT;
					break;

				case KEY_DRAGCOPY:
				case KEY_DRAGMOVE:
					EventType=MENU_EVENT;
					break;

				case KEY_MSWHEEL_UP:
				case KEY_MSWHEEL_DOWN:
				case KEY_MSWHEEL_LEFT:
				case KEY_MSWHEEL_RIGHT:
				case KEY_MSLCLICK:
				case KEY_MSRCLICK:
				case KEY_MSM1CLICK:
				case KEY_MSM2CLICK:
				case KEY_MSM3CLICK:
					EventType=MOUSE_EVENT;
					break;
				case KEY_KILLFOCUS:
				case KEY_GOTFOCUS:
					EventType=FOCUS_EVENT;
					break;
				case KEY_CONSOLE_BUFFER_RESIZE:
					EventType=WINDOW_BUFFER_SIZE_EVENT;
					break;
				default:
					EventType=MENU_EVENT;
					break;
			}
		}
		if (FShift&KEY_SHIFT)
		{
			const struct KeysData
			{
				DWORD FarKey;
				wchar_t Char;
			} Keys[]=
			{
				{'0',')'},{'1','!'},{'2','@'},{'3','#'},{'4','$'},
				{'5','%'},{'6','^'},{'7','&'},{'8','*'},{'9','('},
				{'`','~'},{'-','_'},{'=','+'},{'\\','|'},{'[','{'},
				{']','}'},{';',':'},{'\'','"'},{',','<'},{'.','>'},
				{'/','?'}
			};
			std::for_each(ALL_CONST_RANGE(Keys), [&FKey](const KeysData& A){if (FKey == A.FarKey) FKey=A.Char;});
			const auto ItemIterator = std::find_if(CONST_RANGE(Keys, Item)
			{
				return Item.FarKey == FKey;
			});
			if (ItemIterator != std::cend(Keys))
			{
				FKey = ItemIterator->Char;
			}
		}
	}

	if (Rec)
	{
		Rec->EventType=EventType;

		switch (EventType)
		{
			case KEY_EVENT:
			{
				if (VirtKey)
				{
					Rec->Event.KeyEvent.bKeyDown=1;
					Rec->Event.KeyEvent.wRepeatCount=1;
					if (VirtKey != -1)
					{
						// При нажатии RCtrl и RAlt в консоль приходит VK_CONTROL и VK_MENU а не их правые аналоги
						Rec->Event.KeyEvent.wVirtualKeyCode = (VirtKey==VK_RCONTROL)?VK_CONTROL:(VirtKey==VK_RMENU)?VK_MENU:VirtKey;
						Rec->Event.KeyEvent.wVirtualScanCode = MapVirtualKey(Rec->Event.KeyEvent.wVirtualKeyCode,MAPVK_VK_TO_VSC);
					}
					else
					{
						Rec->Event.KeyEvent.wVirtualKeyCode = 0;
						Rec->Event.KeyEvent.wVirtualScanCode = 0;
					}
					Rec->Event.KeyEvent.uChar.UnicodeChar=FKey > WCHAR_MAX?0:FKey;

					// здесь подход к Shift-клавишам другой, нежели для ControlState
					Rec->Event.KeyEvent.dwControlKeyState=
					    (FShift&KEY_SHIFT?SHIFT_PRESSED:0)|
					    (FShift&KEY_ALT?LEFT_ALT_PRESSED:0)|
					    (FShift&KEY_CTRL?LEFT_CTRL_PRESSED:0)|
					    (FShift&KEY_RALT?RIGHT_ALT_PRESSED:0)|
					    (FShift&KEY_RCTRL?RIGHT_CTRL_PRESSED:0)|
					    (FKey==KEY_DECIMAL?NUMLOCK_ON:0);

					static const DWORD ExtKey[] = {KEY_PGUP, KEY_PGDN, KEY_END, KEY_HOME, KEY_LEFT, KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_INS, KEY_DEL, KEY_NUMENTER};
					const auto ItemIterator = std::find(ALL_CONST_RANGE(ExtKey), FKey);
					if (ItemIterator != std::cend(ExtKey))
						Rec->Event.KeyEvent.dwControlKeyState|=ENHANCED_KEY;
				}
				break;
			}

			case MOUSE_EVENT:
			{
				DWORD ButtonState=0;
				DWORD EventFlags=0;

				switch (FKey)
				{
					case KEY_MSWHEEL_UP:
						ButtonState=MAKELONG(0,120);
						EventFlags|=MOUSE_WHEELED;
						break;
					case KEY_MSWHEEL_DOWN:
						ButtonState=MAKELONG(0,(WORD)(short)-120);
						EventFlags|=MOUSE_WHEELED;
						break;
					case KEY_MSWHEEL_RIGHT:
						ButtonState=MAKELONG(0,120);
						EventFlags|=MOUSE_HWHEELED;
						break;
					case KEY_MSWHEEL_LEFT:
						ButtonState=MAKELONG(0,(WORD)(short)-120);
						EventFlags|=MOUSE_HWHEELED;
						break;

					case KEY_MSLCLICK:
					case KEY_MSRCLICK:
					case KEY_MSM1CLICK:
					case KEY_MSM2CLICK:
					case KEY_MSM3CLICK:
						ButtonState = KeyMsClickToButtonState(FKey);
						break;
				}

				Rec->Event.MouseEvent.dwButtonState=ButtonState;
				Rec->Event.MouseEvent.dwEventFlags=EventFlags;
				Rec->Event.MouseEvent.dwControlKeyState=
					    (FShift&KEY_SHIFT?SHIFT_PRESSED:0)|
					    (FShift&KEY_ALT?LEFT_ALT_PRESSED:0)|
					    (FShift&KEY_CTRL?LEFT_CTRL_PRESSED:0)|
					    (FShift&KEY_RALT?RIGHT_ALT_PRESSED:0)|
					    (FShift&KEY_RCTRL?RIGHT_CTRL_PRESSED:0);
				Rec->Event.MouseEvent.dwMousePosition.X=IntKeyState.MouseX;
				Rec->Event.MouseEvent.dwMousePosition.Y=IntKeyState.MouseY;
				break;
			}
			case WINDOW_BUFFER_SIZE_EVENT:
				GetVideoMode(Rec->Event.WindowBufferSizeEvent.dwSize);
				break;
			case MENU_EVENT:
				Rec->Event.MenuEvent.dwCommandId=0;
				break;
			case FOCUS_EVENT:
				Rec->Event.FocusEvent.bSetFocus = FKey != KEY_KILLFOCUS;
				break;
		}
	}

	_SVS(string strKeyText0;KeyToText(Key,strKeyText0));
	_SVS(SysLog(L"%s or %s ==> %s",_FARKEY_ToName(Key),_MCODE_ToName(Key),_INPUT_RECORD_Dump(Rec)));
	_SVS(SysLog(L"return VirtKey=%x",VirtKey));
	return VirtKey;
}


int IsNavKey(DWORD Key)
{
	static const std::pair<DWORD, DWORD> NavKeysMap[] =
	{
		{0,KEY_CTRLC},
		{0,KEY_RCTRLC},
		{0,KEY_INS},      {0,KEY_NUMPAD0},
		{0,KEY_CTRLINS},  {0,KEY_CTRLNUMPAD0},
		{0,KEY_RCTRLINS}, {0,KEY_RCTRLNUMPAD0},

		{1,KEY_LEFT},     {1,KEY_NUMPAD4},
		{1,KEY_RIGHT},    {1,KEY_NUMPAD6},
		{1,KEY_HOME},     {1,KEY_NUMPAD7},
		{1,KEY_END},      {1,KEY_NUMPAD1},
		{1,KEY_UP},       {1,KEY_NUMPAD8},
		{1,KEY_DOWN},     {1,KEY_NUMPAD2},
		{1,KEY_PGUP},     {1,KEY_NUMPAD9},
		{1,KEY_PGDN},     {1,KEY_NUMPAD3},
		//!!!!!!!!!!!
	};

	return std::any_of(CONST_RANGE(NavKeysMap, i)
	{
		return (!i.first && Key==i.second) || (i.first && (Key&0x00FFFFFF) == (i.second&0x00FFFFFF));
	});
}

int IsShiftKey(DWORD Key)
{
	static const DWORD ShiftKeys[] =
	{
		KEY_SHIFTLEFT,          KEY_SHIFTNUMPAD4,
		KEY_SHIFTRIGHT,         KEY_SHIFTNUMPAD6,
		KEY_SHIFTHOME,          KEY_SHIFTNUMPAD7,
		KEY_SHIFTEND,           KEY_SHIFTNUMPAD1,
		KEY_SHIFTUP,            KEY_SHIFTNUMPAD8,
		KEY_SHIFTDOWN,          KEY_SHIFTNUMPAD2,
		KEY_SHIFTPGUP,          KEY_SHIFTNUMPAD9,
		KEY_SHIFTPGDN,          KEY_SHIFTNUMPAD3,
		KEY_CTRLSHIFTHOME,      KEY_CTRLSHIFTNUMPAD7,
		KEY_RCTRLSHIFTHOME,     KEY_RCTRLSHIFTNUMPAD7,
		KEY_CTRLSHIFTPGUP,      KEY_CTRLSHIFTNUMPAD9,
		KEY_RCTRLSHIFTPGUP,     KEY_RCTRLSHIFTNUMPAD9,
		KEY_CTRLSHIFTEND,       KEY_CTRLSHIFTNUMPAD1,
		KEY_RCTRLSHIFTEND,      KEY_RCTRLSHIFTNUMPAD1,
		KEY_CTRLSHIFTPGDN,      KEY_CTRLSHIFTNUMPAD3,
		KEY_RCTRLSHIFTPGDN,     KEY_RCTRLSHIFTNUMPAD3,
		KEY_CTRLSHIFTLEFT,      KEY_CTRLSHIFTNUMPAD4,
		KEY_RCTRLSHIFTLEFT,     KEY_RCTRLSHIFTNUMPAD4,
		KEY_CTRLSHIFTRIGHT,     KEY_CTRLSHIFTNUMPAD6,
		KEY_RCTRLSHIFTRIGHT,    KEY_RCTRLSHIFTNUMPAD6,
		KEY_ALTSHIFTDOWN,       KEY_ALTSHIFTNUMPAD2,
		KEY_RALTSHIFTDOWN,      KEY_RALTSHIFTNUMPAD2,
		KEY_ALTSHIFTLEFT,       KEY_ALTSHIFTNUMPAD4,
		KEY_RALTSHIFTLEFT,      KEY_RALTSHIFTNUMPAD4,
		KEY_ALTSHIFTRIGHT,      KEY_ALTSHIFTNUMPAD6,
		KEY_RALTSHIFTRIGHT,     KEY_RALTSHIFTNUMPAD6,
		KEY_ALTSHIFTUP,         KEY_ALTSHIFTNUMPAD8,
		KEY_RALTSHIFTUP,        KEY_RALTSHIFTNUMPAD8,
		KEY_ALTSHIFTEND,        KEY_ALTSHIFTNUMPAD1,
		KEY_RALTSHIFTEND,       KEY_RALTSHIFTNUMPAD1,
		KEY_ALTSHIFTHOME,       KEY_ALTSHIFTNUMPAD7,
		KEY_RALTSHIFTHOME,      KEY_RALTSHIFTNUMPAD7,
		KEY_ALTSHIFTPGDN,       KEY_ALTSHIFTNUMPAD3,
		KEY_RALTSHIFTPGDN,      KEY_RALTSHIFTNUMPAD3,
		KEY_ALTSHIFTPGUP,       KEY_ALTSHIFTNUMPAD9,
		KEY_RALTSHIFTPGUP,      KEY_RALTSHIFTNUMPAD9,
		KEY_CTRLALTPGUP,        KEY_CTRLALTNUMPAD9,
		KEY_RCTRLRALTPGUP,      KEY_RCTRLRALTNUMPAD9,
		KEY_CTRLRALTPGUP,       KEY_CTRLRALTNUMPAD9,
		KEY_RCTRLALTPGUP,       KEY_RCTRLALTNUMPAD9,
		KEY_CTRLALTHOME,        KEY_CTRLALTNUMPAD7,
		KEY_RCTRLRALTHOME,      KEY_RCTRLRALTNUMPAD7,
		KEY_CTRLRALTHOME,       KEY_CTRLRALTNUMPAD7,
		KEY_RCTRLALTHOME,       KEY_RCTRLALTNUMPAD7,
		KEY_CTRLALTPGDN,        KEY_CTRLALTNUMPAD2,
		KEY_RCTRLRALTPGDN,      KEY_RCTRLRALTNUMPAD2,
		KEY_CTRLRALTPGDN,       KEY_CTRLRALTNUMPAD2,
		KEY_RCTRLALTPGDN,       KEY_RCTRLALTNUMPAD2,
		KEY_CTRLALTEND,         KEY_CTRLALTNUMPAD1,
		KEY_RCTRLRALTEND,       KEY_RCTRLRALTNUMPAD1,
		KEY_CTRLRALTEND,        KEY_CTRLRALTNUMPAD1,
		KEY_RCTRLALTEND,        KEY_RCTRLALTNUMPAD1,
		KEY_CTRLALTLEFT,        KEY_CTRLALTNUMPAD4,
		KEY_RCTRLRALTLEFT,      KEY_RCTRLRALTNUMPAD4,
		KEY_CTRLRALTLEFT,       KEY_CTRLRALTNUMPAD4,
		KEY_RCTRLALTLEFT,       KEY_RCTRLALTNUMPAD4,
		KEY_CTRLALTRIGHT,       KEY_CTRLALTNUMPAD6,
		KEY_RCTRLRALTRIGHT,     KEY_RCTRLRALTNUMPAD6,
		KEY_CTRLRALTRIGHT,      KEY_CTRLRALTNUMPAD6,
		KEY_RCTRLALTRIGHT,      KEY_RCTRLALTNUMPAD6,
		KEY_ALTUP,
		KEY_RALTUP,
		KEY_ALTLEFT,
		KEY_RALTLEFT,
		KEY_ALTDOWN,
		KEY_RALTDOWN,
		KEY_ALTRIGHT,
		KEY_RALTRIGHT,
		KEY_ALTHOME,
		KEY_RALTHOME,
		KEY_ALTEND,
		KEY_RALTEND,
		KEY_ALTPGUP,
		KEY_RALTPGUP,
		KEY_ALTPGDN,
		KEY_RALTPGDN,
		KEY_ALT,
		KEY_RALT,
		KEY_CTRL,
		KEY_RCTRL,
		KEY_SHIFT,
	};

	return std::find(ALL_CONST_RANGE(ShiftKeys), Key) != std::cend(ShiftKeys);
}

unsigned int ShieldCalcKeyCode(const INPUT_RECORD* rec, bool RealKey, bool* NotMacros)
{
	const auto SavedIntKeyState = IntKeyState; // нада! ибо CalcKeyCode "портит"... (Mantis#0001760)
	IntKeyState = {};
	DWORD Ret=CalcKeyCode(rec, RealKey, NotMacros);
	IntKeyState = SavedIntKeyState;
	return Ret;
}

int GetDirectlyMappedKey(int VKey)
{
	switch (VKey)
	{
	case VK_BROWSER_BACK: return KEY_BROWSER_BACK;
	case VK_BROWSER_FORWARD: return KEY_BROWSER_FORWARD;
	case VK_BROWSER_REFRESH: return KEY_BROWSER_REFRESH;
	case VK_BROWSER_STOP: return KEY_BROWSER_STOP;
	case VK_BROWSER_SEARCH: return KEY_BROWSER_SEARCH;
	case VK_BROWSER_FAVORITES: return KEY_BROWSER_FAVORITES;
	case VK_BROWSER_HOME: return KEY_BROWSER_HOME;
	case VK_VOLUME_MUTE: return KEY_VOLUME_MUTE;
	case VK_VOLUME_DOWN: return KEY_VOLUME_DOWN;
	case VK_VOLUME_UP: return KEY_VOLUME_UP;
	case VK_MEDIA_NEXT_TRACK: return KEY_MEDIA_NEXT_TRACK;
	case VK_MEDIA_PREV_TRACK: return KEY_MEDIA_PREV_TRACK;
	case VK_MEDIA_STOP: return KEY_MEDIA_STOP;
	case VK_MEDIA_PLAY_PAUSE: return KEY_MEDIA_PLAY_PAUSE;
	case VK_LAUNCH_MAIL: return KEY_LAUNCH_MAIL;
	case VK_LAUNCH_MEDIA_SELECT: return KEY_LAUNCH_MEDIA_SELECT;
	case VK_LAUNCH_APP1: return KEY_LAUNCH_APP1;
	case VK_LAUNCH_APP2: return KEY_LAUNCH_APP2;
	case VK_APPS: return KEY_APPS;
	case VK_LWIN: return KEY_LWIN;
	case VK_RWIN: return KEY_RWIN;
	case VK_BACK: return KEY_BS;
	case VK_TAB: return KEY_TAB;
	case VK_ADD: return KEY_ADD;
	case VK_SUBTRACT: return KEY_SUBTRACT;
	case VK_ESCAPE: return KEY_ESC;
	case VK_CAPITAL: return KEY_CAPSLOCK;
	case VK_NUMLOCK: return KEY_NUMLOCK;
	case VK_SCROLL: return KEY_SCROLLLOCK;
	case VK_DIVIDE: return KEY_DIVIDE;
	case VK_CANCEL: return KEY_BREAK;
	case VK_MULTIPLY: return KEY_MULTIPLY;
	case VK_SLEEP: return KEY_STANDBY;
	case VK_SNAPSHOT: return KEY_PRNTSCRN;
	default: return 0;
	}
}

// These VK_* map to different characters if Shift (and only Shift) is pressed
int GetMappedCharacter(int VKey)
{
	switch (VKey)
	{
	case VK_OEM_PERIOD: return KEY_DOT;
	case VK_OEM_COMMA: return KEY_COMMA;
	case VK_OEM_MINUS: return '-';
	case VK_OEM_PLUS: return '=';
	// BUGBUG hard-coded for US keyboard
	case VK_OEM_1: return KEY_SEMICOLON;
	case VK_OEM_2: return KEY_SLASH;
	case VK_OEM_3: return '`';
	case VK_OEM_4: return KEY_BRACKET;
	case VK_OEM_5: return KEY_BACKSLASH;
	case VK_OEM_6: return KEY_BACKBRACKET;
	case VK_OEM_7: return '\'';
	case VK_OEM_102: return KEY_BACKSLASH; // <> \|
	default: return 0;
	}
};

static int GetNumpadKey(const int KeyCode, const int CtrlState, const int Modif)
{
	static const struct numpad_mapping
	{
		int VCode;
		int FarCodeNumpad;
		int FarCodeEnhached;
		int FarCodeForNumLock;
	}
	NumpadMapping[] =
	{
		{ VK_NUMPAD0, KEY_NUMPAD0, KEY_INS, '0' },
		{ VK_NUMPAD1, KEY_NUMPAD1, KEY_END, '1' },
		{ VK_NUMPAD2, KEY_NUMPAD2, KEY_DOWN, '2' },
		{ VK_NUMPAD3, KEY_NUMPAD3, KEY_PGDN, '3' },
		{ VK_NUMPAD4, KEY_NUMPAD4, KEY_LEFT, '4' },
		{ VK_NUMPAD5, KEY_NUMPAD5, KEY_CLEAR, '5' },
		{ VK_NUMPAD6, KEY_NUMPAD6, KEY_RIGHT, '6' },
		{ VK_NUMPAD7, KEY_NUMPAD7, KEY_HOME, '7' },
		{ VK_NUMPAD8, KEY_NUMPAD8, KEY_UP, '8' },
		{ VK_NUMPAD9, KEY_NUMPAD9, KEY_PGUP, '9' },
		{ VK_DECIMAL, KEY_NUMDEL, KEY_DEL, KEY_DECIMAL },
	};

	const auto GetMappingIndex = [KeyCode]
	{
		switch (KeyCode)
		{
		case VK_INSERT: case VK_NUMPAD0: return 0;
		case VK_END:    case VK_NUMPAD1: return 1;
		case VK_DOWN:   case VK_NUMPAD2: return 2;
		case VK_NEXT:   case VK_NUMPAD3: return 3;
		case VK_LEFT:   case VK_NUMPAD4: return 4;
		case VK_CLEAR:  case VK_NUMPAD5: return 5;
		case VK_RIGHT:  case VK_NUMPAD6: return 6;
		case VK_HOME:   case VK_NUMPAD7: return 7;
		case VK_UP:     case VK_NUMPAD8: return 8;
		case VK_PRIOR:  case VK_NUMPAD9: return 9;
		case VK_DELETE: case VK_DECIMAL: return 10;
		default:
			return -1;
		}
	};

	const auto MappingIndex = GetMappingIndex();
	if (MappingIndex == -1)
		return 0;

	const auto& Mapping = NumpadMapping[MappingIndex];

	if (CtrlState & ENHANCED_KEY)
	{
		return Modif | Mapping.FarCodeEnhached;
	}

	if ((CtrlState & NUMLOCK_ON) && !Modif && KeyCode == Mapping.VCode)
	{
		return Mapping.FarCodeForNumLock;
	}

	return Modif | Mapping.FarCodeNumpad;
};

static int GetMouseKey(const MOUSE_EVENT_RECORD& MouseEvent)
{
	switch (MouseEvent.dwEventFlags)
	{
	case 0:
	{
		const auto MsKey = ButtonStateToKeyMsClick(MouseEvent.dwButtonState);
		if (MsKey != KEY_NONE)
		{
			return MsKey;
		}
	}
	break;

	case MOUSE_WHEELED:
	case MOUSE_HWHEELED:
	{
		const auto& WheelKeysPair = WheelKeys[MouseEvent.dwEventFlags == MOUSE_HWHEELED? 1 : 0];
		const auto Key = WheelKeysPair[static_cast<short>(HIWORD(MouseEvent.dwButtonState)) > 0? 1 : 0];
		return Key;
	}

	default:
		break;
	}

	return 0;
}

unsigned int CalcKeyCode(const INPUT_RECORD* rec, bool RealKey, bool* NotMacros)
{
	_SVS(CleverSysLog Clev(L"CalcKeyCode"));
	_SVS(SysLog(L"CalcKeyCode -> %s| RealKey=%d  *NotMacros=%d",_INPUT_RECORD_Dump(rec),RealKey,(NotMacros?*NotMacros:0)));
	const UINT CtrlState=(rec->EventType==MOUSE_EVENT)?rec->Event.MouseEvent.dwControlKeyState:rec->Event.KeyEvent.dwControlKeyState;
	const UINT ScanCode=rec->Event.KeyEvent.wVirtualScanCode;
	const UINT KeyCode=rec->Event.KeyEvent.wVirtualKeyCode;
	const WCHAR Char=rec->Event.KeyEvent.uChar.UnicodeChar;

	if (NotMacros)
		*NotMacros = (CtrlState&0x80000000) != 0;

	if (!(rec->EventType==KEY_EVENT || rec->EventType == MOUSE_EVENT))
		return KEY_NONE;

	if (!RealKey)
	{
		UpdateIntKeyState(CtrlState);
	}

	const auto ModifCtrl = IntKeyState.RightCtrlPressed? KEY_RCTRL : IntKeyState.CtrlPressed()? KEY_CTRL : NO_KEY;
	const auto ModifAlt = IntKeyState.RightAltPressed? KEY_RALT: IntKeyState.AltPressed()? KEY_ALT : NO_KEY;
	const auto ModifShift = IntKeyState.RightShiftPressed? KEY_SHIFT : IntKeyState.ShiftPressed() ? KEY_SHIFT : NO_KEY;
	const auto Modif = ModifCtrl | ModifAlt | ModifShift;

	if (rec->EventType==MOUSE_EVENT)
	{
		if (const auto MouseKey = GetMouseKey(rec->Event.MouseEvent))
		{
			return Modif | MouseKey;
		}
	}

	if (rec->Event.KeyEvent.wVirtualKeyCode >= 0xFF && RealKey)
	{
		//VK_?=0x00FF, Scan=0x0013 uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000120 (casac - EcNs)
		//VK_?=0x00FF, Scan=0x0014 uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000120 (casac - EcNs)
		//VK_?=0x00FF, Scan=0x0015 uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000120 (casac - EcNs)
		//VK_?=0x00FF, Scan=0x001A uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000120 (casac - EcNs)
		//VK_?=0x00FF, Scan=0x001B uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000120 (casac - EcNs)
		//VK_?=0x00FF, Scan=0x001E uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000120 (casac - EcNs)
		//VK_?=0x00FF, Scan=0x001F uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000120 (casac - EcNs)
		//VK_?=0x00FF, Scan=0x0023 uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000120 (casac - EcNs)
		if (!rec->Event.KeyEvent.bKeyDown && (CtrlState&(ENHANCED_KEY|NUMLOCK_ON)))
			return Modif|(KEY_VK_0xFF_BEGIN+ScanCode);

		return KEY_IDLE;
	}

	static time_check TimeCheck(time_check::mode::delayed, 50);

	if (!AltValue)
	{
		TimeCheck.reset();
	}

	if (!rec->Event.KeyEvent.bKeyDown)
	{
		KeyCodeForALT_LastPressed=0;

		switch (KeyCode)
		{
			case VK_MENU:
				if (AltValue)
				{
					if (RealKey)
					{
						DropConsoleInputEvent();
					}
					IntKeyState.ReturnAltValue=TRUE;
					AltValue&=0xFFFF;
					/*
					О перетаскивании из проводника / вставке текста в консоль, на примере буквы 'ы':

					1. Нажимается Alt:
					bKeyDown=TRUE,  wRepeatCount=1, wVirtualKeyCode=VK_MENU,    UnicodeChar=0,    dwControlKeyState=LEFT_ALT_PRESSED

					2. Через numpad-клавиши вводится код символа в OEM, если он туда мапится, или 63 ('?'), если не мапится:
					bKeyDown=TRUE,  wRepeatCount=1, wVirtualKeyCode=VK_NUMPAD2, UnicodeChar=0,    dwControlKeyState=LEFT_ALT_PRESSED
					bKeyDown=FALSE, wRepeatCount=1, wVirtualKeyCode=VK_NUMPAD2, UnicodeChar=0,    dwControlKeyState=LEFT_ALT_PRESSED
					bKeyDown=TRUE,  wRepeatCount=1, wVirtualKeyCode=VK_NUMPAD3, UnicodeChar=0,    dwControlKeyState=LEFT_ALT_PRESSED
					bKeyDown=FALSE, wRepeatCount=1, wVirtualKeyCode=VK_NUMPAD3, UnicodeChar=0,    dwControlKeyState=LEFT_ALT_PRESSED
					bKeyDown=TRUE,  wRepeatCount=1, wVirtualKeyCode=VK_NUMPAD5, UnicodeChar=0,    dwControlKeyState=LEFT_ALT_PRESSED
					bKeyDown=FALSE, wRepeatCount=1, wVirtualKeyCode=VK_NUMPAD5, UnicodeChar=0,    dwControlKeyState=LEFT_ALT_PRESSED

					3. Отжимается Alt, при этом в uChar.UnicodeChar лежит исходный символ:
					bKeyDown=FALSE, wRepeatCount=1, wVirtualKeyCode=VK_MENU,    UnicodeChar=1099, dwControlKeyState=0

					Мораль сей басни такова: если rec->Event.KeyEvent.uChar.UnicodeChar не пуст - берём его, а не то, что во время удерживания Alt пришло.
					*/

					if (rec->Event.KeyEvent.uChar.UnicodeChar)
					{
						// BUGBUG: в Windows 7 Event.KeyEvent.uChar.UnicodeChar _всегда_ заполнен, но далеко не всегда тем, чем надо.
						// условно считаем, что если интервал между нажатиями не превышает 50 мс, то это сгенерированная при D&D или вставке комбинация,
						// иначе - ручной ввод.
						if (!TimeCheck)
						{
							AltValue=rec->Event.KeyEvent.uChar.UnicodeChar;
						}
					}

					return AltValue;
				}
				return Modif|((CtrlState&ENHANCED_KEY)?KEY_RALT:KEY_ALT);

			case VK_CONTROL:
				return Modif|((CtrlState&ENHANCED_KEY)?KEY_RCTRL:KEY_CTRL);

			case VK_SHIFT:
				return Modif|KEY_SHIFT;
		}
		return KEY_NONE;
	}

	//прежде, чем убирать это шаманство, поставьте себе раскладку, в которой по ralt+символ можно вводить символы.
	//например немецкую:
	//ralt+m - мю
	//ralt+q - @
	//ralt+e - евро
	//ralt+] - ~
	//ralt+2 - квадрат
	//ralt+3 - куб
	//ralt+7 - {
	//ralt+8 - [
	//ralt+9 - ]
	//ralt+0 - }
	//ralt+- - "\"
	//или латышскую:
	//ralt+4 - евро
	//ralt+a/ralt+shift+a
	//ralt+c/ralt+shift+c
	//и т.д.
	if ((CtrlState & (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)) == (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED))
	{
		if (Char >= ' ')
		{
			return Char;
		}

		if (RealKey && ScanCode && !Char && (KeyCode && KeyCode != VK_MENU))
		{
			//Это шаманство для ввода всяческих букв с тильдами, акцентами и прочим.
			//Например на Шведской раскладке, "AltGr+VK_OEM_1" вообще не должно обрабатываться фаром, т.к. это DeadKey
			//Dn, 1, Vk="VK_CONTROL" [17/0x0011], Scan=0x001D uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000008 (Casac - ecns)
			//Dn, 1, Vk="VK_MENU" [18/0x0012], Scan=0x0038 uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000109 (CasAc - Ecns)
			//Dn, 1, Vk="VK_OEM_1" [186/0x00BA], Scan=0x001B uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000009 (CasAc - ecns)
			//Up, 1, Vk="VK_CONTROL" [17/0x0011], Scan=0x001D uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000001 (casAc - ecns)
			//Up, 1, Vk="VK_MENU" [18/0x0012], Scan=0x0038 uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00000100 (casac - Ecns)
			//Up, 1, Vk="VK_OEM_1" [186/0x00BA], Scan=0x0000 uChar=[U='~' (0x007E): A='~' (0x7E)] Ctrl=0x00000000 (casac - ecns)
			//Up, 1, Vk="VK_OEM_1" [186/0x00BA], Scan=0x001B uChar=[U='и' (0x00A8): A='' (0xA8)] Ctrl=0x00000000 (casac - ecns)
			//Dn, 1, Vk="VK_A" [65/0x0041], Scan=0x001E uChar=[U='у' (0x00E3): A='' (0xE3)] Ctrl=0x00000000 (casac - ecns)
			//Up, 1, Vk="VK_A" [65/0x0041], Scan=0x001E uChar=[U='a' (0x0061): A='a' (0x61)] Ctrl=0x00000000 (casac - ecns)
			return KEY_NONE;
		}

		IntKeyState.LeftCtrlPressed = IntKeyState.RightCtrlPressed = 0;
	}

	if (KeyCode==VK_MENU)
		AltValue=0;

	if (InRange<unsigned>(VK_F1, KeyCode, VK_F24))
		return Modif + KEY_F1 + (KeyCode - VK_F1);

	if (IntKeyState.OnlyAltPressed())
	{
		if (!(CtrlState & ENHANCED_KEY))
		{
			static unsigned int ScanCodes[]={82,79,80,81,75,76,77,71,72,73};

			for (size_t i = 0; i != std::size(ScanCodes); ++i)
			{
				if (ScanCodes[i]==ScanCode)
				{
					if (RealKey && KeyCodeForALT_LastPressed != KeyCode)
					{
						AltValue = AltValue * 10 + static_cast<int>(i);
						KeyCodeForALT_LastPressed=KeyCode;
					}

					if (AltValue)
						return KEY_NONE;
				}
			}
		}
	}

	switch (KeyCode)
	{
	case VK_RETURN:
		return Modif | ((CtrlState & ENHANCED_KEY)? KEY_NUMENTER : KEY_ENTER);

	case VK_PAUSE:
		return Modif | ((CtrlState & ENHANCED_KEY)? KEY_NUMLOCK : KEY_PAUSE);

	case VK_SPACE:
		if (Char == L' ' || !Char)
			return Modif | KEY_SPACE;
		return Char;
	}

	if (const auto NumpadKey = GetNumpadKey(KeyCode, CtrlState, Modif))
	{
		// Modif is added from within GetNumpadKey conditionally
		return NumpadKey;
	}

	if(const auto MappedKey = GetDirectlyMappedKey(KeyCode))
	{
		return Modif | MappedKey;
	}

	if (!IntKeyState.CtrlPressed() && !IntKeyState.AltPressed() && (KeyCode >= VK_OEM_1 && KeyCode <= VK_OEM_8) && !Char)
	{
		//Это шаманство для того, чтобы фар не реагировал на DeadKeys (могут быть нажаты с Shift-ом)
		//которые используются для ввода символов с диакритикой (тильды, шапки, и пр.)
		//Dn, Vk="VK_SHIFT"    [ 16/0x0010], Scan=0x002A uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x10
		//Dn, Vk="VK_OEM_PLUS" [187/0x00BB], Scan=0x000D uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x10
		//Up, Vk="VK_OEM_PLUS" [187/0x00BB], Scan=0x0000 uChar=[U=''  (0x02C7): A='?' (0xC7)] Ctrl=0x10
		//Up, Vk="VK_OEM_PLUS" [187/0x00BB], Scan=0x000D uChar=[U=''  (0x02C7): A='?' (0xC7)] Ctrl=0x10
		//Up, Vk="VK_SHIFT"    [ 16/0x0010], Scan=0x002A uChar=[U=' ' (0x0000): A=' ' (0x00)] Ctrl=0x00
		//Dn, Vk="VK_C"        [ 67/0x0043], Scan=0x002E uChar=[U=''  (0x010D): A=' ' (0x0D)] Ctrl=0x00
		//Up, Vk="VK_C"        [ 67/0x0043], Scan=0x002E uChar=[U='c' (0x0063): A='c' (0x63)] Ctrl=0x00
		return KEY_NONE;
	}

	if (!IntKeyState.CtrlPressed() && !IntKeyState.AltPressed())
	{
		// Shift or none - characters only
		if (!Char || KeyCode == VK_SHIFT)
			return KEY_NONE;
		return Char;
	}

	if (InRange(L'0',  KeyCode, L'9') || InRange(L'A', KeyCode, L'Z'))
		return Modif | KeyCode;

	if (const auto OemKey = GetMappedCharacter(KeyCode))
	{
		return Modif + OemKey;
	}

	return Char? Modif | Char : KEY_NONE;
}
