/*
main.cpp

������� main.
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
#include "chgprior.hpp"
#include "colors.hpp"
#include "filepanels.hpp"
#include "panel.hpp"
#include "fileedit.hpp"
#include "fileview.hpp"
#include "manager.hpp"
#include "ctrlobj.hpp"
#include "scrbuf.hpp"
#include "language.hpp"
#include "farexcpt.hpp"
#include "imports.hpp"
#include "syslog.hpp"
#include "interf.hpp"
#include "keyboard.hpp"
#include "clipboard.hpp"
#include "pathmix.hpp"
#include "strmix.hpp"
#include "dirmix.hpp"
#include "elevation.hpp"
#include "cmdline.hpp"
#include "console.hpp"
#include "configdb.hpp"
#include "colormix.hpp"
#include "treelist.hpp"
#include "plugins.hpp"
#include "notification.hpp"
#include "datetime.hpp"

global *Global = nullptr;

static void show_help()
{
	WCHAR HelpMsg[]=
		L"Usage: far [switches] [apath [ppath]]" EOL_STR EOL_STR
		L"where" EOL_STR
		L"  apath - path to a folder (or a file or an archive or command with prefix)" EOL_STR
		L"          for the active panel" EOL_STR
		L"  ppath - path to a folder (or a file or an archive or command with prefix)" EOL_STR
		L"          for the passive panel" EOL_STR
		L"The following switches may be used in the command line:" EOL_STR
		L" -?   This help." EOL_STR
		L" -a   Disable display of characters with codes 0 - 31 and 255." EOL_STR
		L" -ag  Disable display of pseudographics characters." EOL_STR
		L" -clearcache [profilepath [localprofilepath]]" EOL_STR
		L"      Clear plugins cache." EOL_STR
		L" -co  Forces FAR to load plugins from the cache only." EOL_STR
#ifdef DIRECT_RT
		L" -do  Direct output." EOL_STR
#endif
		L" -e[<line>[:<pos>]] <filename>" EOL_STR
		L"      Edit the specified file." EOL_STR
		L" -export <out.farconfig> [profilepath [localprofilepath]]" EOL_STR
		L"      Export settings." EOL_STR
		L" -import <in.farconfig> [profilepath [localprofilepath]]" EOL_STR
		L"      Import settings." EOL_STR
		L" -m   Do not load macros." EOL_STR
		L" -ma  Do not execute auto run macros." EOL_STR
		L" -p[<path>]" EOL_STR
		L"      Search for \"common\" plugins in the directory, specified by <path>." EOL_STR
		L" -ro[-] Read-Only or Normal config mode." EOL_STR
		L" -s <profilepath> [<localprofilepath>]" EOL_STR
		L"      Custom location for Far configuration files - overrides Far.exe.ini." EOL_STR
		L" -set:<parameter>=<value>" EOL_STR
		L"      Override the configuration parameter, see far:config for details." EOL_STR
		L" -t <path>" EOL_STR
		L"      Location of Far template configuration file - overrides Far.exe.ini." EOL_STR
#ifndef NO_WRAPPER
		L" -u <username>" EOL_STR
		L"      Allows to have separate registry settings for different users." EOL_STR
		L"      Affects only 1.x Far Manager plugins." EOL_STR
#endif // NO_WRAPPER
		L" -v <filename>" EOL_STR
		L"      View the specified file. If <filename> is -, data is read from the stdin." EOL_STR
		L" -w[-] Stretch to console window instead of console buffer or vise versa." EOL_STR
		;
	Console().Write(HelpMsg, ARRAYSIZE(HelpMsg)-1);
	Console().Commit();
}

static int MainProcess(
    const string& lpwszEditName,
    const string& lpwszViewName,
    const string& lpwszDestName1,
    const string& lpwszDestName2,
    int StartLine,
    int StartChar
)
{
		SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
		FarColor InitAttributes={};
		Console().GetTextAttributes(InitAttributes);
		SetRealColor(colors::PaletteColorToFarColor(COL_COMMANDLINEUSERSCREEN));

		string ename(lpwszEditName),vname(lpwszViewName), apanel(lpwszDestName1),ppanel(lpwszDestName2);
		if (ConfigProvider().ShowProblems() > 0)
		{
			ename.clear();
			vname.clear();
			StartLine = StartChar = -1;
			apanel = Global->Opt->ProfilePath;
			ppanel = Global->Opt->LocalProfilePath;
		}

		if (!ename.empty() || !vname.empty())
		{
			Global->OnlyEditorViewerUsed = true;

			_tran(SysLog(L"create dummy panels"));
			Global->CtrlObject->CreateDummyFilePanels();
			const auto DummyPanel = new dummy_panel(Global->CtrlObject->Panels());
			Global->CtrlObject->Cp()->LeftPanel = Global->CtrlObject->Cp()->RightPanel = DummyPanel;
			Global->CtrlObject->Cp()->SetActivePanel(DummyPanel);
			Global->WindowManager->PluginCommit();

			Global->CtrlObject->Plugins->LoadPlugins();
			Global->CtrlObject->Macro.LoadMacros(true, true);

			if (!ename.empty())
			{
				const auto ShellEditor = FileEditor::create(ename, CP_DEFAULT, FFILEEDIT_CANNEWFILE | FFILEEDIT_ENABLEF6, StartLine, StartChar);
				_tran(SysLog(L"make shelleditor %p",ShellEditor));

				if (!ShellEditor->GetExitCode())  // ????????????
				{
					Global->WindowManager->ExitMainLoop(0);
				}
			}
			// TODO: ���� else ������ ������ ����� �������� � ������������ �������� ��������� /e � /v � ���.������
			else if (!vname.empty())
			{
				const auto ShellViewer = FileViewer::create(vname, TRUE);

				if (!ShellViewer->GetExitCode())
				{
					Global->WindowManager->ExitMainLoop(0);
				}

				_tran(SysLog(L"make shellviewer, %p",ShellViewer));
			}

			Global->WindowManager->EnterMainLoop();
			Global->CtrlObject->Cp()->LeftPanel = Global->CtrlObject->Cp()->RightPanel = nullptr;
			Global->CtrlObject->Cp()->SetActivePanel(nullptr);
			DummyPanel->Destroy();
			_tran(SysLog(L"editor/viewer closed, delete dummy panels"));
		}
		else
		{
			int DirCount=0;
			string strPath;

			// ������������� ���, ��� ControlObject::Init() ������� ������
			// ���� Global->Opt->*

			const auto SetupPanel = [&](bool active)
			{
				++DirCount;
				strPath = active? apanel : ppanel;
				CutToNameUNC(strPath);
				DeleteEndSlash(strPath); //BUGBUG!! ���� �������� ���� �� ������ - �������� �������� ������ - ����������� ".."

				bool Root = false;
				const auto Type = ParsePath(strPath, nullptr, &Root);
				if(Root && (Type == PATH_DRIVELETTER || Type == PATH_DRIVELETTERUNC || Type == PATH_VOLUMEGUID))
				{
					AddEndSlash(strPath);
				}

				auto& CurrentPanelOptions = (Global->Opt->LeftFocus == active)? Global->Opt->LeftPanel : Global->Opt->RightPanel;
				CurrentPanelOptions.m_Type = FILE_PANEL;  // ������ ���� ������
				CurrentPanelOptions.Visible = true;     // � ������� ��
				CurrentPanelOptions.Folder = strPath;
			};

			if (!apanel.empty())
			{
				SetupPanel(true);

				if (!ppanel.empty())
				{
					SetupPanel(false);
				}
			}

			// ������ ��� ������ - ������� ������!
			Global->CtrlObject->Init(DirCount);

			// � ������ "����������" � ������� ��� ����-���� (���� ��������� ;-)
			if (!apanel.empty())  // �������� ������
			{
				Panel *ActivePanel = Global->CtrlObject->Cp()->ActivePanel();
				Panel *AnotherPanel = Global->CtrlObject->Cp()->PassivePanel();

				if (!ppanel.empty())  // ��������� ������
				{
					FarChDir(AnotherPanel->GetCurDir());

					if (IsPluginPrefixPath(ppanel))
					{
						AnotherPanel->SetFocus();
						Global->CtrlObject->CmdLine()->ExecString(ppanel,0);
						ActivePanel->SetFocus();
					}
					else
					{
						strPath = PointToName(ppanel);

						if (!strPath.empty())
						{
							if (AnotherPanel->GoToFile(strPath))
								AnotherPanel->ProcessKey(Manager::Key(KEY_CTRLPGDN));
						}
					}
				}

				FarChDir(ActivePanel->GetCurDir());

				if (IsPluginPrefixPath(apanel))
				{
					Global->CtrlObject->CmdLine()->ExecString(apanel,0);
				}
				else
				{
					strPath = PointToName(apanel);

					if (!strPath.empty())
					{
						if (ActivePanel->GoToFile(strPath))
							ActivePanel->ProcessKey(Manager::Key(KEY_CTRLPGDN));
					}
				}

				// !!! �������� !!!
				// ������� �������� ��������� ������, � ����� ��������!
				AnotherPanel->Redraw();
				ActivePanel->Redraw();
			}

			Global->WindowManager->EnterMainLoop();
		}

		TreeList::FlushCache();

		// ������� �� �����!
		SetScreen(0,0,ScrX,ScrY,L' ',colors::PaletteColorToFarColor(COL_COMMANDLINEUSERSCREEN));
		Console().SetTextAttributes(InitAttributes);
		Global->ScrBuf->ResetShadow();
		Global->ScrBuf->ResetLockCount();
		Global->ScrBuf->Flush();
		MoveRealCursor(0,0);

		return 0;
}

static LONG WINAPI FarUnhandledExceptionFilter(EXCEPTION_POINTERS *ExceptionInfo)
{
	return ProcessSEHException(L"FarUnhandledExceptionFilter", ExceptionInfo)? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

static void InitTemplateProfile(string &strTemplatePath)
{
	if (strTemplatePath.empty())
	{
		strTemplatePath = GetFarIniString(L"General", L"TemplateProfile", L"%FARHOME%\\Default.farconfig");
	}

	if (!strTemplatePath.empty())
	{
		ConvertNameToFull(Unquote(os::env::expand_strings(strTemplatePath)), strTemplatePath);
		DeleteEndSlash(strTemplatePath);

		if (os::fs::is_directory(strTemplatePath))
			strTemplatePath += L"\\Default.farconfig";

		Global->Opt->TemplateProfilePath = strTemplatePath;
	}
}

static void InitProfile(string &strProfilePath, string &strLocalProfilePath)
{
	if (!strProfilePath.empty())
	{
		ConvertNameToFull(Unquote(os::env::expand_strings(strProfilePath)), strProfilePath);
	}
	if (!strLocalProfilePath.empty())
	{
		ConvertNameToFull(Unquote(os::env::expand_strings(strLocalProfilePath)), strLocalProfilePath);
	}

	if (strProfilePath.empty())
	{
		int UseSystemProfiles = GetFarIniInt(L"General", L"UseSystemProfiles", 1);
		if (UseSystemProfiles)
		{
			// roaming data default path: %APPDATA%\Far Manager\Profile
			wchar_t Buffer[MAX_PATH];
			SHGetFolderPath(nullptr, CSIDL_APPDATA|CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, Buffer);
			Global->Opt->ProfilePath = Buffer;
			AddEndSlash(Global->Opt->ProfilePath);
			Global->Opt->ProfilePath += L"Far Manager";

			if (UseSystemProfiles == 2)
			{
				Global->Opt->LocalProfilePath = Global->Opt->ProfilePath;
			}
			else
			{
				// local data default path: %LOCALAPPDATA%\Far Manager\Profile
				SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, Buffer);
				Global->Opt->LocalProfilePath = Buffer;
				AddEndSlash(Global->Opt->LocalProfilePath);
				Global->Opt->LocalProfilePath += L"Far Manager";
			}

			string* Paths[]={&Global->Opt->ProfilePath, &Global->Opt->LocalProfilePath};
			std::for_each(RANGE(Paths, i)
			{
				AddEndSlash(*i);
				*i += L"Profile";
				CreatePath(*i, true);
			});
		}
		else
		{
			string strUserProfileDir = GetFarIniString(L"General", L"UserProfileDir", L"%FARHOME%\\Profile");
			string strUserLocalProfileDir = GetFarIniString(L"General", L"UserLocalProfileDir", strUserProfileDir);
			ConvertNameToFull(Unquote(os::env::expand_strings(strUserProfileDir)), Global->Opt->ProfilePath);
			ConvertNameToFull(Unquote(os::env::expand_strings(strUserLocalProfileDir)), Global->Opt->LocalProfilePath);
		}
	}
	else
	{
		Global->Opt->ProfilePath = strProfilePath;
		Global->Opt->LocalProfilePath = strLocalProfilePath.empty() ? strProfilePath : strLocalProfilePath;
	}

	Global->Opt->LoadPlug.strPersonalPluginsPath = Global->Opt->ProfilePath + L"\\Plugins";

	os::env::set_variable(L"FARPROFILE", Global->Opt->ProfilePath);
	os::env::set_variable(L"FARLOCALPROFILE", Global->Opt->LocalProfilePath);

	if (Global->Opt->ReadOnlyConfig < 0) // do not override 'far /ro', 'far /ro-'
		Global->Opt->ReadOnlyConfig = GetFarIniInt(L"General", L"ReadOnlyConfig", 0);

	if (!Global->Opt->ReadOnlyConfig)
	{
		CreatePath(Global->Opt->ProfilePath + L"\\PluginsData", true);
		if (Global->Opt->ProfilePath != Global->Opt->LocalProfilePath)
		{
			CreatePath(Global->Opt->LocalProfilePath, true);
		}
	}
}

static bool ProcessServiceModes(const range<wchar_t**>& Args, int& ServiceResult)
{
	const auto isArg = [](const wchar_t* Arg, const wchar_t* Name)
	{
		return (*Arg == L'/' || *Arg == L'-') && !StrCmpI(Arg + 1, Name);
	};

	if (Args.size() == 4 && IsElevationArgument(Args[0])) // /service:elevation {GUID} PID UsePrivileges
	{
		ServiceResult = ElevationMain(Args[1], std::wcstoul(Args[2], nullptr, 10), *Args[3] == L'1');
		return true;
	}
	else if (InRange(size_t(2), Args.size(), size_t(5)) && (isArg(Args[0], L"export") || isArg(Args[0], L"import")))
	{
		bool Export = isArg(Args[0], L"export");
		string strProfilePath(Args.size() > 2 ? Args[2] : L""), strLocalProfilePath(Args.size() > 3 ? Args[3] : L""), strTemplatePath(Args.size() > 4 ? Args[4] : L"");
		InitTemplateProfile(strTemplatePath);
		InitProfile(strProfilePath, strLocalProfilePath);
		Global->m_ConfigProvider = new config_provider(Export ? config_provider::export_mode : config_provider::import_mode);
		ServiceResult = !(Export ? ConfigProvider().Export(Args[1]) : ConfigProvider().Import(Args[1]));
		return true;
	}
	else if (InRange(size_t(1), Args.size(), size_t(3)) && isArg(Args[0], L"clearcache"))
	{
		string strProfilePath(Args.size() > 1 ? Args[1] : L"");
		string strLocalProfilePath(Args.size() > 2 ? Args[2] : L"");
		InitProfile(strProfilePath, strLocalProfilePath);
		config_provider::ClearPluginsCache();
		ServiceResult = 0;
		return true;
	}
	return false;
}

static void UpdateErrorMode()
{
	Global->ErrorMode |= SEM_NOGPFAULTERRORBOX;
	long long IgnoreDataAlignmentFaults = 0;
	ConfigProvider().GeneralCfg()->GetValue(L"System.Exception", L"IgnoreDataAlignmentFaults", IgnoreDataAlignmentFaults, IgnoreDataAlignmentFaults);
	if (IgnoreDataAlignmentFaults)
	{
		Global->ErrorMode |= SEM_NOALIGNMENTFAULTEXCEPT;
	}
	SetErrorMode(Global->ErrorMode);
}

static void SetDriveMenuHotkeys()
{
	long long InitDriveMenuHotkeys = 1;
	ConfigProvider().GeneralCfg()->GetValue(L"Interface", L"InitDriveMenuHotkeys", InitDriveMenuHotkeys, InitDriveMenuHotkeys);

	if (InitDriveMenuHotkeys)
	{
		static const struct
		{
			const wchar_t* PluginId; // BUGBUG, GUID?
			GUID MenuId;
			const wchar_t* Hotkey;
		}
		DriveMenuHotkeys[] =
		{
			{ L"1E26A927-5135-48C6-88B2-845FB8945484", { 0x61026851, 0x2643, 0x4C67, { 0xBF, 0x80, 0xD3, 0xC7, 0x7A, 0x3A, 0xE8, 0x30 } }, L"0" }, // ProcList
			{ L"B77C964B-E31E-4D4C-8FE5-D6B0C6853E7C", { 0xF98C70B3, 0xA1AE, 0x4896, { 0x93, 0x88, 0xC5, 0xC8, 0xE0, 0x50, 0x13, 0xB7 } }, L"1" }, // TmpPanel
			{ L"42E4AEB1-A230-44F4-B33C-F195BB654931", { 0xC9FB4F53, 0x54B5, 0x48FF, { 0x9B, 0xA2, 0xE8, 0xEB, 0x27, 0xF0, 0x12, 0xA2 } }, L"2" }, // NetBox
			{ L"773B5051-7C5F-4920-A201-68051C4176A4", { 0x24B6DD41, 0xDF12, 0x470A, { 0xA4, 0x7C, 0x86, 0x75, 0xED, 0x8D, 0x2E, 0xD4 } }, L"3" }, // Network
		};

		std::for_each(CONST_RANGE(DriveMenuHotkeys, i)
		{
			ConfigProvider().PlHotkeyCfg()->SetHotkey(i.PluginId, i.MenuId, PluginsHotkeysConfig::DRIVE_MENU, i.Hotkey);
		});

		ConfigProvider().GeneralCfg()->SetValue(L"Interface", L"InitDriveMenuHotkeys", 0ull);
	}
}

static int mainImpl(const range<wchar_t**>& Args)
{
	SCOPED_ACTION(global);

	auto NoElevetionDuringBoot = std::make_unique<elevation::suppress>();

	SetErrorMode(Global->ErrorMode);

	TestPathParser();

	RegisterTestExceptionsHook();

	// Starting with Windows Vista, the system uses the low-fragmentation heap (LFH) as needed to service memory allocation requests.
	// Applications do not need to enable the LFH for their heaps.
	if (!IsWindowsVistaOrGreater())
	{
		os::EnableLowFragmentationHeap();
	}

	if(!Console().IsFullscreenSupported())
	{
		const BYTE ReserveAltEnter = 0x8;
		Imports().SetConsoleKeyShortcuts(TRUE, ReserveAltEnter, nullptr, 0);
	}

	os::InitCurrentDirectory();

	if (os::GetModuleFileName(nullptr, Global->g_strFarModuleName))
	{
		ConvertNameToLong(Global->g_strFarModuleName, Global->g_strFarModuleName);
		PrepareDiskPath(Global->g_strFarModuleName);
	}

	Global->g_strFarINI = Global->g_strFarModuleName+L".ini";
	Global->g_strFarPath = Global->g_strFarModuleName;
	CutToSlash(Global->g_strFarPath,true);
	os::env::set_variable(L"FARHOME", Global->g_strFarPath);
	AddEndSlash(Global->g_strFarPath);

	if (os::security::is_admin())
		os::env::set_variable(L"FARADMINMODE", L"1");
	else
		os::env::delete_variable(L"FARADMINMODE");

	{
		int ServiceResult;
		if (ProcessServiceModes(Args, ServiceResult))
			return ServiceResult;
	}

	SCOPED_ACTION(listener)(update_environment, &ReloadEnvironment);
	SCOPED_ACTION(listener)(update_intl, &OnIntlSettingsChange);

	_OT(SysLog(L"[[[[[[[[New Session of FAR]]]]]]]]]"));

	string strEditName;
	string strViewName;
	string DestNames[2];
	int StartLine=-1,StartChar=-1;
	int CntDestName=0; // ���������� ����������-���� ���������

	string strProfilePath, strLocalProfilePath, strTemplatePath;

	std::vector<std::pair<string, string>> Overridden;
	FOR_RANGE(Args, Iter)
	{
		const auto& Arg = *Iter;
		if ((Arg[0]==L'/' || Arg[0]==L'-') && Arg[1])
		{
			switch (ToUpper(Arg[1]))
			{
				case L'A':
					switch (ToUpper(Arg[2]))
					{
					case 0:
						Global->Opt->CleanAscii = true;
						break;

					case L'G':
						if (!Arg[3])
							Global->Opt->NoGraphics = true;
						break;
					}
					break;

				case L'E':
					if (std::iswdigit(Arg[2]))
					{
						StartLine = static_cast<int>(std::wcstol(Arg + 2, nullptr, 10));
						const wchar_t *ChPtr = wcschr(Arg + 2, L':');

						if (ChPtr)
							StartChar = static_cast<int>(std::wcstol(ChPtr + 1, nullptr, 10));;
					}

					if (Iter + 1 != Args.end())
					{
						strEditName = *++Iter;
					}
					break;

				case L'V':
					if (Iter + 1 != Args.end())
					{
						strViewName = *++Iter;
					}
					break;

				case L'M':
					switch (ToUpper(Arg[2]))
					{
					case L'\0':
						Global->Opt->Macro.DisableMacro|=MDOL_ALL;
						break;

					case L'A':
						if (!Arg[3])
							Global->Opt->Macro.DisableMacro|=MDOL_AUTOSTART;
						break;
					}
					break;

#ifndef NO_WRAPPER
				case L'U':
					if (Iter + 1 != Args.end())
					{
						//Affects OEM plugins only!
						Global->strRegUser = *++Iter;
					}
					break;
#endif // NO_WRAPPER

				case L'S':
					if (!StrCmpNI(Arg + 1, L"set:", 4))
					{
						if (const auto EqualPtr = wcschr(Arg + 1, L'='))
						{
							Overridden.push_back(VALUE_TYPE(Overridden)(string(Arg + 1 + 4, EqualPtr), EqualPtr + 1));
						}
					}
					else if (Iter + 1 != Args.end())
					{
						strProfilePath = *++Iter;
						const auto Next = Iter + 1;
						if (Next != Args.end() && *Next[0] != L'-'  && *Next[0] != L'/')
						{
							strLocalProfilePath = *Next;
							Iter = Next;
						}
					}
					break;

				case L'T':
					if (Iter + 1 != Args.end())
					{
						strTemplatePath = *++Iter;
					}
					break;

				case L'P':
					{
						Global->Opt->LoadPlug.PluginsPersonal = false;
						Global->Opt->LoadPlug.MainPluginDir = false;

						if (Arg[2])
						{
							// we can't expand it here - some environment variables might not be available yet
							Global->Opt->LoadPlug.strCustomPluginsPath = &Arg[2];
						}
						else
						{
							// ���� ������ -P ��� <����>, ��, �������, ��� ��������
							//  ������� �� ��������� ��������!!!
							Global->Opt->LoadPlug.strCustomPluginsPath.clear();
						}
					}
					break;

				case L'C':
					if (ToUpper(Arg[2])==L'O' && !Arg[3])
					{
						Global->Opt->LoadPlug.PluginsCacheOnly = true;
						Global->Opt->LoadPlug.PluginsPersonal = false;
					}
					break;

				case L'?':
				case L'H':
					ControlObject::ShowCopyright(1);
					show_help();
					return 0;

#ifdef DIRECT_RT
				case L'D':
					if (ToUpper(Arg[2])==L'O' && !Arg[3])
						Global->DirectRT=true;
					break;
#endif
				case L'W':
					{
						if(Arg[2] == L'-')
						{
							Global->Opt->WindowMode= false;
						}
						else if(!Arg[2])
						{
							Global->Opt->WindowMode= true;
						}
					}
					break;

				case L'R':
					if (ToUpper(Arg[2]) == L'O') // -ro
						Global->Opt->ReadOnlyConfig = TRUE;
					if (ToUpper(Arg[2]) == L'W') // -rw
						Global->Opt->ReadOnlyConfig = FALSE;
					break;
			}
		}
		else // ������� ���������. �� ����� ���� max ��� �����.
		{
			if (CntDestName < 2)
			{
				if (IsPluginPrefixPath(Arg))
				{
					DestNames[CntDestName++] = Arg;
				}
				else
				{
					string ArgvI = Unquote(os::env::expand_strings(Arg));
					ConvertNameToFull(ArgvI, ArgvI);

					if (os::fs::exists(ArgvI))
					{
						DestNames[CntDestName++] = ArgvI;
					}
				}
			}
		}
	}

	InitTemplateProfile(strTemplatePath);
	InitProfile(strProfilePath, strLocalProfilePath);
	Global->m_ConfigProvider = new config_provider;

	Global->Opt->Load(Overridden);

	//������������� ������� ������.
	InitKeysArray();

	if (!Global->Opt->LoadPlug.MainPluginDir) //���� ���� ���� /p �� �� �������� /co
		Global->Opt->LoadPlug.PluginsCacheOnly=false;

	if (Global->Opt->LoadPlug.PluginsCacheOnly)
	{
		Global->Opt->LoadPlug.strCustomPluginsPath.clear();
		Global->Opt->LoadPlug.MainPluginDir=false;
		Global->Opt->LoadPlug.PluginsPersonal=false;
	}

	InitConsole();

	SCOPE_EXIT
	{
		// BUGBUG, reorder to delete only in ~global()
		delete Global->CtrlObject;
		Global->CtrlObject = nullptr;

		ClearInternalClipboard();
		CloseConsole();
	};

	Global->Lang = new Language(Global->g_strFarPath, MNewFileName + 1);

	os::env::set_variable(L"FARLANG", Global->Opt->strLanguage);

	if (!Global->Opt->LoadPlug.strCustomPluginsPath.empty())
		ConvertNameToFull(Unquote(os::env::expand_strings(Global->Opt->LoadPlug.strCustomPluginsPath)), Global->Opt->LoadPlug.strCustomPluginsPath);

	UpdateErrorMode();

	SetDriveMenuHotkeys();

	Global->CtrlObject = new ControlObject;

	NoElevetionDuringBoot.reset();

	int Result = 1;

	try
	{
		Result = MainProcess(strEditName, strViewName, DestNames[0], DestNames[1], StartLine, StartChar);
	}
	catch (const SException& e)
	{
		if (ProcessSEHException(L"mainImpl", e.GetInfo()))
		{
			std::terminate();
		}
		else
		{
			SetErrorMode(Global->ErrorMode&~SEM_NOGPFAULTERRORBOX);
			throw;
		}
	}
	catch (const std::exception& e)
	{
		if (ProcessStdException(e, nullptr, L"mainImpl"))
		{
			std::terminate();
		}
		else
		{
			SetErrorMode(Global->ErrorMode&~SEM_NOGPFAULTERRORBOX);
			throw;
		}
	}

	_OT(SysLog(L"[[[[[Exit of FAR]]]]]]]]]"));

	return Result;
}

int wmain(int Argc, wchar_t *Argv[])
{
	try
	{
		atexit(PrintMemory);
#if defined(SYSLOG)
		atexit(PrintSysLogStat);
#endif
		setlocale(LC_ALL, "");
		EnableSeTranslation();
		EnableVectoredExceptionHandling();
		SetUnhandledExceptionFilter(FarUnhandledExceptionFilter);

		// Must be static - dependent static objects exist
		static SCOPED_ACTION(os::co_initialize);
		return mainImpl(make_range(Argv + 1, Argv + Argc));
	}
	catch (const SException& e)
	{
		if (ProcessSEHException(L"wmain", e.GetInfo()))
		{
			std::terminate();
		}
		else
		{
			throw;
		}
	}
	catch (const std::exception& e)
	{
		std::wcerr << L"\nException: " << e.what() << std::endl;
	}
	catch (...)
	{
		std::wcerr << L"\nUnknown exception" << std::endl;
	}
	return 1;
}

#if COMPILER == C_GCC
int main()
{
	int nArgs;
	const auto wstrCmdLineArgs = os::memory::local::ptr(CommandLineToArgvW(GetCommandLineW(), &nArgs));
	int Result=wmain(nArgs, wstrCmdLineArgs.get());
	return Result;
}
#endif
