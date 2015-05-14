/*
plugins.cpp

������ � ��������� (������ �������, ���-��� ������ � filelist.cpp)
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

#include "plugins.hpp"
#include "keys.hpp"
#include "scantree.hpp"
#include "chgprior.hpp"
#include "constitle.hpp"
#include "cmdline.hpp"
#include "filepanels.hpp"
#include "panel.hpp"
#include "vmenu2.hpp"
#include "dialog.hpp"
#include "savescr.hpp"
#include "ctrlobj.hpp"
#include "scrbuf.hpp"
#include "fileedit.hpp"
#include "refreshwindowmanager.hpp"
#include "plugapi.hpp"
#include "TaskBar.hpp"
#include "pathmix.hpp"
#include "strmix.hpp"
#include "processname.hpp"
#include "interf.hpp"
#include "message.hpp"
#include "FarGuid.hpp"
#include "configdb.hpp"
#include "FarDlgBuilder.hpp"
#include "DlgGuid.hpp"
#include "mix.hpp"
#include "manager.hpp"
#include "language.hpp"
#include "desktop.hpp"
#include "PluginA.hpp"

static const wchar_t PluginsFolderName[] = L"Plugins";

static void ReadUserBackgound(SaveScreen *SaveScr)
{
	FilePanels *FPanel = Global->CtrlObject->Cp();
	FPanel->LeftPanel->ProcessingPluginCommand++;
	FPanel->RightPanel->ProcessingPluginCommand++;

	if (Global->KeepUserScreen)
	{
		if (SaveScr)
			SaveScr->Discard();

		Global->CtrlObject->Desktop->FillFromConsole();
	}

	FPanel->LeftPanel->ProcessingPluginCommand--;
	FPanel->RightPanel->ProcessingPluginCommand--;
}

static void GetHotKeyPluginKey(Plugin *pPlugin, string &strPluginKey)
{
	/*
	FarPath
	C:\Program Files\Far\

	ModuleName                                             PluginName
	---------------------------------------------------------------------------------------
	C:\Program Files\Far\Plugins\MultiArc\MULTIARC.DLL  -> Plugins\MultiArc\MULTIARC.DLL
	C:\MultiArc\MULTIARC.DLL                            -> C:\MultiArc\MULTIARC.DLL
	---------------------------------------------------------------------------------------
	*/
	strPluginKey = pPlugin->GetHotkeyName();
#ifndef NO_WRAPPER
	size_t FarPathLength = Global->g_strFarPath.size();
	if (pPlugin->IsOemPlugin() && FarPathLength < pPlugin->GetModuleName().size() && !StrCmpNI(pPlugin->GetModuleName().data(), Global->g_strFarPath.data(), FarPathLength))
		strPluginKey.erase(0, FarPathLength);
#endif // NO_WRAPPER
}

static void GetPluginHotKey(Plugin *pPlugin, const GUID& Guid, PluginsHotkeysConfig::HotKeyTypeEnum HotKeyType, string &strHotKey)
{
	string strPluginKey;
	strHotKey.clear();
	GetHotKeyPluginKey(pPlugin, strPluginKey);
	strHotKey = ConfigProvider().PlHotkeyCfg()->GetHotkey(strPluginKey, GuidToStr(Guid), HotKeyType);
}

bool PluginManager::plugin_less::operator ()(const Plugin* a, const Plugin *b) const
{
	return StrCmpI(PointToName(a->GetModuleName()),PointToName(b->GetModuleName())) < 0;
}

static void CallPluginSynchroEvent(const any& Payload)
{
	#if 0
	//Maximus: �������� �������� ������� �� ��� ���, ���� ��� �� ������ ������ �������������
	if (!Global->CtrlObject /*|| !CtrlObject->Plugins->IsPluginsLoaded()*/)
	{
		//��� ���� ��� ���������� �� Mantis?
		_ASSERTE(Global->CtrlObject /*&& CtrlObject->Plugins->IsPluginsLoaded()*/);
		return false;
	}
	#endif

	const auto& Data = any_cast<std::pair<GUID, void*>>(Payload);
	if (const auto pPlugin = Global->CtrlObject->Plugins->FindPlugin(Data.first))
	{
		ProcessSynchroEventInfo Info = { sizeof(Info) };
		Info.Event = SE_COMMONSYNCHRO;
		Info.Param = Data.second;
		pPlugin->ProcessSynchroEvent(&Info);
	}
}

PluginManager::PluginManager():
	m_PluginSynchro(plugin_synchro, &CallPluginSynchroEvent),
#ifndef NO_WRAPPER
	OemPluginsCount(),
#endif // NO_WRAPPER
	m_PluginsLoaded()
{
}

PluginManager::~PluginManager()
{
	Plugin *Luamacro=nullptr; // ���������� �������� ������� ������� ���������.

	std::for_each(CONST_RANGE(SortedPlugins, i)
	{
		if (i->GetGUID() == Global->Opt->KnownIDs.Luamacro.Id)
		{
			Luamacro=i;
		}
		else
		{
			i->Unload(true);
		}
	});

	if (Luamacro)
	{
		Luamacro->Unload(true);
	}

	// some plugins might still have dialogs (if DialogFree wasn't called)
	// better to delete them explicitly while manager is still alive
	m_Plugins.clear();
}

bool PluginManager::AddPlugin(Plugin *pPlugin)
{
	auto Result = m_Plugins.emplace(VALUE_TYPE(m_Plugins)(pPlugin->GetGUID(), VALUE_TYPE(m_Plugins)::second_type()));
	if (!Result.second)
	{
		return false;
	}
	Result.first->second.reset(pPlugin);

	SortedPlugins.insert(pPlugin);
#ifndef NO_WRAPPER
	if(pPlugin->IsOemPlugin())
	{
		OemPluginsCount++;
	}
#endif // NO_WRAPPER
	return true;
}

bool PluginManager::UpdateId(Plugin *pPlugin, const GUID& Id)
{
	auto Iterator = m_Plugins.find(pPlugin->GetGUID());
	// important, do not delete Plugin instance
	Iterator->second.release();
	m_Plugins.erase(Iterator);
	pPlugin->SetGuid(Id);
	auto Result = m_Plugins.emplace(VALUE_TYPE(m_Plugins)(pPlugin->GetGUID(), VALUE_TYPE(m_Plugins)::second_type()));
	if (!Result.second)
	{
		return false;
	}
	Result.first->second.reset(pPlugin);
	return true;
}

bool PluginManager::RemovePlugin(Plugin *pPlugin)
{
#ifndef NO_WRAPPER
	if(pPlugin->IsOemPlugin())
	{
		OemPluginsCount--;
	}
#endif // NO_WRAPPER
	SortedPlugins.erase(std::find(SortedPlugins.begin(), SortedPlugins.end(), pPlugin));
	m_Plugins.erase(pPlugin->GetGUID());
	return true;
}


Plugin* PluginManager::LoadPlugin(const string& FileName, const os::FAR_FIND_DATA &FindData, bool LoadToMem)
{
	Plugin *pPlugin = nullptr;

	std::any_of(CONST_RANGE(PluginModels, i) { return pPlugin = i->CreatePlugin(FileName); });

	if (pPlugin)
	{
		bool Result = false, bDataLoaded = false;

		if (!LoadToMem)
		{
			Result = pPlugin->LoadFromCache(FindData);
		}

		if (!Result && (pPlugin->CheckWorkFlags(PIWF_PRELOADED) || !Global->Opt->LoadPlug.PluginsCacheOnly))
		{
			Result = bDataLoaded = pPlugin->LoadData();
		}

		if (!Result || !AddPlugin(pPlugin))
		{
			pPlugin->Unload(true);
			delete pPlugin;
			return nullptr;
		}

		if (bDataLoaded && !pPlugin->Load())
		{
			pPlugin->Unload(true);
			RemovePlugin(pPlugin);
			pPlugin = nullptr;
		}
	}
	return pPlugin;
}

Plugin* PluginManager::LoadPluginExternal(const string& lpwszModuleName, bool LoadToMem)
{
	Plugin *pPlugin = GetPlugin(lpwszModuleName);

	if (pPlugin)
	{
		if ((LoadToMem || pPlugin->bPendingRemove) && !pPlugin->Load())
		{
			if (!pPlugin->bPendingRemove)
			{
				UnloadedPlugins.emplace_back(pPlugin);
			}
			return nullptr;
		}
	}
	else
	{
		os::FAR_FIND_DATA FindData;

		if (os::GetFindDataEx(lpwszModuleName, FindData))
		{
			pPlugin = LoadPlugin(lpwszModuleName, FindData, LoadToMem);
			if (!pPlugin)
				return nullptr;
		}
	}
	return pPlugin;
}

int PluginManager::UnloadPlugin(Plugin *pPlugin, int From)
{
	int nResult = FALSE;

	if (pPlugin && (From != iExitFAR))   //�������, ���� ����� � EXITFAR, �� ������� � ��������, �� � ��� � Unload
	{
		for(int i = static_cast<int>(Global->WindowManager->GetModalWindowCount()-1); i >= 0; --i)
		{
			auto Window = Global->WindowManager->GetModalWindow(i);
			if((Window->GetType()==windowtype_dialog && std::static_pointer_cast<Dialog>(Window)->GetPluginOwner() == pPlugin) || Window->GetType()==windowtype_help)
			{
				Window->Lock();
				if(i)
				{
					Global->WindowManager->GetModalWindow(i-1)->Lock();
				}
				Global->WindowManager->DeleteWindow(Window);
				Global->WindowManager->PluginCommit();
			}
		}

		bool bPanelPlugin = pPlugin->IsPanelPlugin();

		nResult = pPlugin->Unload(true);

		pPlugin->WorkFlags.Set(PIWF_DONTLOADAGAIN);

		if (bPanelPlugin /*&& bUpdatePanels*/)
		{
			Global->CtrlObject->Cp()->ActivePanel()->SetCurDir(L".",true);
			Panel *ActivePanel = Global->CtrlObject->Cp()->ActivePanel();
			ActivePanel->Update(UPDATE_KEEP_SELECTION);
			ActivePanel->Redraw();
			Panel *AnotherPanel=Global->CtrlObject->Cp()->PassivePanel();
			AnotherPanel->Update(UPDATE_KEEP_SELECTION|UPDATE_SECONDARY);
			AnotherPanel->Redraw();
		}

		UnloadedPlugins.emplace_back(pPlugin);
	}

	return nResult;
}

bool PluginManager::IsPluginUnloaded(Plugin* pPlugin)
{
	return std::find(UnloadedPlugins.cbegin(), UnloadedPlugins.cend(), pPlugin) != UnloadedPlugins.cend();
}

int PluginManager::UnloadPluginExternal(Plugin* pPlugin)
{
	//BUGBUG ����� �������� �� ����������� ��������
	int nResult = FALSE;
	if(pPlugin->Active())
	{
		nResult = TRUE;
		if(!IsPluginUnloaded(pPlugin))
		{
			UnloadedPlugins.emplace_back(pPlugin);
		}
	}
	else
	{
		UnloadedPlugins.remove(pPlugin);
		nResult = pPlugin->Unload(true);
		RemovePlugin(pPlugin);
	}
	return nResult;
}

Plugin *PluginManager::GetPlugin(const string& ModuleName)
{
	auto ItemIterator = std::find_if(CONST_RANGE(SortedPlugins, i)
	{
		return !StrCmpI(i->GetModuleName(), ModuleName);
	});
	return ItemIterator == SortedPlugins.cend()? nullptr : *ItemIterator;
}

void PluginManager::LoadModels()
{
	PluginModels.emplace_back(std::make_unique<NativePluginModel>(this));
#ifndef NO_WRAPPER
	if (Global->Opt->LoadPlug.OEMPluginsSupport)
		PluginModels.emplace_back(std::make_unique<wrapper::OEMPluginModel>(this));
#endif // NO_WRAPPER

	ScanTree ScTree(false, true, Global->Opt->LoadPlug.ScanSymlinks);
	os::FAR_FIND_DATA FindData;
	ScTree.SetFindPath(Global->g_strFarPath + L"\\Adapters", L"*");

	string filename;
	while (ScTree.GetNextName(&FindData, filename))
	{
		if (CmpName(L"*.dll", filename.data(), false) && !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			auto CustomModel = std::make_unique<CustomPluginModel>(this, filename);
			if (CustomModel->Success())
			{
				PluginModels.emplace_back(std::move(CustomModel));
			}
		}
	}
}

void PluginManager::LoadPlugins()
{
	SCOPED_ACTION(IndeterminateTaskBar)(false);
	m_PluginsLoaded = false;

	LoadModels();

	if (Global->Opt->LoadPlug.PluginsCacheOnly)  // $ 01.09.2000 tran  '/co' switch
	{
		LoadPluginsFromCache();
	}
	else if (Global->Opt->LoadPlug.MainPluginDir || !Global->Opt->LoadPlug.strCustomPluginsPath.empty() || (Global->Opt->LoadPlug.PluginsPersonal && !Global->Opt->LoadPlug.strPersonalPluginsPath.empty()))
	{
		ScanTree ScTree(false, true, Global->Opt->LoadPlug.ScanSymlinks);
		string strPluginsDir;
		string strFullName;
		os::FAR_FIND_DATA FindData;

		// ������� ���������� ������
		if (Global->Opt->LoadPlug.MainPluginDir) // ������ �������� � ������������?
		{
			strPluginsDir=Global->g_strFarPath+PluginsFolderName;
			// ...� ������������ ����?
			if (Global->Opt->LoadPlug.PluginsPersonal && !Global->Opt->LoadPlug.strPersonalPluginsPath.empty())
				strPluginsDir += L";" + Global->Opt->LoadPlug.strPersonalPluginsPath;
		}
		else if (!Global->Opt->LoadPlug.strCustomPluginsPath.empty())  // ������ "��������" ����?
		{
			strPluginsDir = Global->Opt->LoadPlug.strCustomPluginsPath;
		}

		// ������ ��������� �� ����� ����� ���������� ������
		std::vector<string> Strings;
		split(Strings, strPluginsDir, STLF_UNIQUE);
		FOR(const auto& i, Strings)
		{
			// ��������� �������� ����
			strFullName = Unquote(os::env::expand_strings(i)); //??? ����� ��

			if (!IsAbsolutePath(strFullName))
			{
				strPluginsDir = Global->g_strFarPath;
				strPluginsDir += strFullName;
				strFullName = strPluginsDir;
			}

			// ������� �������� �������� ������� �������� ����
			ConvertNameToFull(strFullName,strFullName);
			ConvertNameToLong(strFullName,strFullName);
			strPluginsDir = strFullName;

			// ������ �� ����� ��������� ���� �� ������...
			ScTree.SetFindPath(strPluginsDir,L"*");

			// ...� ��������� �� ����
			while (ScTree.GetNextName(&FindData,strFullName))
			{
				if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					LoadPlugin(strFullName, FindData, false);
				}
			} // end while
		}
	}

	m_PluginsLoaded = true;
}

/* $ 01.09.2000 tran
   Load cache only plugins  - '/co' switch */
void PluginManager::LoadPluginsFromCache()
{
	string strModuleName;

	for (DWORD i=0; ConfigProvider().PlCacheCfg()->EnumPlugins(i, strModuleName); i++)
	{
		ReplaceSlashToBackslash(strModuleName);

		os::FAR_FIND_DATA FindData;

		if (os::GetFindDataEx(strModuleName, FindData))
			LoadPlugin(strModuleName, FindData, false);
	}
}

PluginHandle* PluginManager::OpenFilePlugin(
	const string* Name,
	int OpMode,	//!!! potential future error: OPERATION_MODES is __int64
	OPENFILEPLUGINTYPE Type
)
{
	struct PluginInfo
	{
		PluginHandle Handle;
		HANDLE Analyse;
		bool operator ==(const PluginInfo& rhs) const {return Handle.hPlugin == rhs.Handle.hPlugin && Handle.pPlugin == rhs.Handle.pPlugin && Analyse == rhs.Analyse;}
		bool operator !=(const PluginInfo& rhs) const {return !(*this == rhs);}
	};
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	ConsoleTitle ct(Global->Opt->ShowCheckingFile?MSG(MCheckingFileInPlugin):L"");
	PluginHandle* hResult = nullptr;
	std::list<PluginInfo> items;
	string strFullName;

	if (Name)
	{
		ConvertNameToFull(*Name,strFullName);
		Name = &strFullName;
	}

	bool ShowMenu = Global->Opt->PluginConfirm.OpenFilePlugin==BSTATE_3STATE? !(Type == OFP_NORMAL || Type == OFP_SEARCH) : Global->Opt->PluginConfirm.OpenFilePlugin != 0;
	bool ShowWarning = !OpMode;
	 //� ���� �������� OpMode ���.
	if(Type==OFP_ALTERNATIVE) OpMode|=OPM_PGDN;
	if(Type==OFP_COMMANDS) OpMode|=OPM_COMMANDS;

	os::fs::file file;
	AnalyseInfo Info={sizeof(Info), Name? Name->data() : nullptr, nullptr, 0, (OPERATION_MODES)OpMode};
	std::vector<BYTE> Buffer(Global->Opt->PluginMaxReadData);

	bool DataRead = false;
	FOR(const auto& i, SortedPlugins)
	{
		if (!i->has<iOpenFilePlugin>() && !(i->has<iAnalyse>() && i->has<iOpen>()))
			continue;

		if(Name && !DataRead)
		{
			if (file.Open(*Name, FILE_READ_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN))
			{
				size_t DataSize = 0;
				if (file.Read(Buffer.data(), Buffer.size(), DataSize))
				{
					Info.Buffer = Buffer.data();
					Info.BufferSize = DataSize;
					DataRead = true;
				}
				file.Close();
			}
			if(!DataRead)
			{
				if(ShowWarning)
				{
					Global->CatchError();
					Message(MSG_WARNING|MSG_ERRORTYPE, 1, L"", MSG(MOpenPluginCannotOpenFile), Name->data(), MSG(MOk));
				}
				break;
			}
		}

		if (i->has<iOpenFilePlugin>())
		{
			if (Global->Opt->ShowCheckingFile)
				ct << MSG(MCheckingFileInPlugin) << L" - [" << PointToName(i->GetModuleName()) << L"]..." << fmt::Flush();

			auto hPlugin = i->OpenFilePlugin(Name? Name->data() : nullptr, (BYTE*)Info.Buffer, Info.BufferSize, OpMode);

			if (hPlugin == PANEL_STOP)   //����� �� �����, ������ ����� ����� ���������� ��� ��� (Autorun/PictureView)!!!
			{
				hResult = reinterpret_cast<PluginHandle*>(PANEL_STOP);
				break;
			}

			if (hPlugin)
			{
				PluginInfo handle;
				handle.Handle.hPlugin = hPlugin;
				handle.Handle.pPlugin = i;
				handle.Analyse = nullptr;
				items.emplace_back(handle);
			}
		}
		else
		{
			HANDLE analyse = i->Analyse(&Info);
			if (analyse)
			{
				PluginInfo handle;
				handle.Handle.pPlugin = i;
				handle.Handle.hPlugin = nullptr;
				handle.Analyse = analyse;
				items.emplace_back(handle);
			}
		}

		if (!items.empty() && !ShowMenu)
			break;
	}

	auto pResult = items.end(), pAnalyse = pResult;
	if (!items.empty() && (hResult != PANEL_STOP))
	{
		bool OnlyOne = (items.size() == 1) && !(Name && Global->Opt->PluginConfirm.OpenFilePlugin && Global->Opt->PluginConfirm.StandardAssociation && Global->Opt->PluginConfirm.EvenIfOnlyOnePlugin);

		if(!OnlyOne && ShowMenu)
		{
			auto menu = VMenu2::create(MSG(MPluginConfirmationTitle), nullptr, 0, ScrY - 4);
			menu->SetPosition(-1, -1, 0, 0);
			menu->SetHelp(L"ChoosePluginMenu");
			menu->SetMenuFlags(VMENU_SHOWAMPERSAND | VMENU_WRAPMODE);

			std::for_each(CONST_RANGE(items, i)
			{
				menu->AddItem(i.Handle.pPlugin->GetTitle());
			});

			if (Global->Opt->PluginConfirm.StandardAssociation && Type == OFP_NORMAL)
			{
				MenuItemEx mitem;
				mitem.Flags |= MIF_SEPARATOR;
				menu->AddItem(mitem);
				menu->AddItem(MSG(MMenuPluginStdAssociation));
			}

			int ExitCode = menu->Run();
			if (ExitCode == -1)
				hResult = static_cast<PluginHandle*>(PANEL_STOP);
			else
			{
				if(ExitCode < static_cast<int>(items.size()))
				{
					pResult = std::next(items.begin(), ExitCode);
				}
			}
		}
		else
		{
			pResult = items.begin();
		}

		if (pResult != items.end() && pResult->Handle.hPlugin == nullptr)
		{
			pAnalyse = pResult;
			OpenAnalyseInfo oainfo={sizeof(OpenAnalyseInfo),&Info,pResult->Analyse};

			OpenInfo oInfo = {sizeof(oInfo)};
			oInfo.OpenFrom = OPEN_ANALYSE;
			oInfo.Guid = &FarGuid;
			oInfo.Data = (intptr_t)&oainfo;

			HANDLE h = pResult->Handle.pPlugin->Open(&oInfo);

			if (h == PANEL_STOP)
			{
				hResult = static_cast<PluginHandle*>(PANEL_STOP);
				pResult = items.end();
			}
			else if (h)
			{
				pResult->Handle.hPlugin = h;
			}
			else
			{
				pResult = items.end();
			}
		}
	}

	std::for_each(CONST_RANGE(items, i)
	{
		if (pResult == items.end() || i != *pResult)
		{
			if (i.Handle.hPlugin)
			{
				ClosePanelInfo cpInfo = {sizeof(cpInfo)};
				cpInfo.hPanel = i.Handle.hPlugin;
				i.Handle.pPlugin->ClosePanel(&cpInfo);
			}
		}
		if (pAnalyse == items.end() || i != *pAnalyse)
		{
			if(i.Analyse)
			{
				CloseAnalyseInfo cpInfo = {sizeof(cpInfo)};
				cpInfo.Handle = i.Analyse;
				i.Handle.pPlugin->CloseAnalyse(&cpInfo);
			}
		}
	});

	if (pResult != items.end())
	{
		PluginHandle* pDup=new PluginHandle;
		pDup->hPlugin=pResult->Handle.hPlugin;
		pDup->pPlugin=pResult->Handle.pPlugin;
		hResult = pDup;
	}

	return hResult;
}

PluginHandle* PluginManager::OpenFindListPlugin(const PluginPanelItem *PanelItem, size_t ItemsNumber)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	std::list<PluginHandle> items;
	auto pResult = items.end();

	FOR(const auto& i, SortedPlugins)
	{
		if (!i->has<iSetFindList>())
			continue;

		OpenInfo Info = {sizeof(Info)};
		Info.OpenFrom = OPEN_FINDLIST;
		Info.Guid = &FarGuid;
		Info.Data = 0;

		HANDLE hPlugin = i->Open(&Info);

		if (hPlugin)
		{
			PluginHandle handle;
			handle.hPlugin = hPlugin;
			handle.pPlugin = i;
			items.emplace_back(handle);
		}

		if (!items.empty() && !Global->Opt->PluginConfirm.SetFindList)
			break;
	}

	if (!items.empty())
	{
		if (items.size()>1)
		{
			auto menu = VMenu2::create(MSG(MPluginConfirmationTitle), nullptr, 0, ScrY - 4);
			menu->SetPosition(-1, -1, 0, 0);
			menu->SetHelp(L"ChoosePluginMenu");
			menu->SetMenuFlags(VMENU_SHOWAMPERSAND | VMENU_WRAPMODE);

			std::for_each(CONST_RANGE(items, i)
			{
				menu->AddItem(i.pPlugin->GetTitle());
			});

			int ExitCode=menu->Run();

			if (ExitCode>=0)
			{
				pResult = std::next(items.begin(), ExitCode);
			}
		}
		else
		{
			pResult = items.begin();
		}
	}

	if (pResult != items.end())
	{
		SetFindListInfo Info = {sizeof(Info)};
		Info.hPanel = pResult->hPlugin;
		Info.PanelItem = PanelItem;
		Info.ItemsNumber = ItemsNumber;

		if (!pResult->pPlugin->SetFindList(&Info))
		{
			pResult = items.end();
		}
	}

	FOR_CONST_RANGE(items, i)
	{
		if (i!=pResult)
		{
			if (i->hPlugin)
			{
				ClosePanelInfo Info = {sizeof(Info)};
				Info.hPanel = i->hPlugin;
				i->pPlugin->ClosePanel(&Info);
			}
		}
	}

	if (pResult != items.end())
	{
		PluginHandle* pDup=new PluginHandle;
		pDup->hPlugin=pResult->hPlugin;
		pDup->pPlugin=pResult->pPlugin;
		return pDup;
	}

	return nullptr;
}


void PluginManager::ClosePanel(PluginHandle* hPlugin)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	ClosePanelInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	hPlugin->pPlugin->ClosePanel(&Info);
	delete hPlugin;
}


int PluginManager::ProcessEditorInput(const INPUT_RECORD *Rec) const
{
	ProcessEditorInputInfo Info={sizeof(Info)};
	Info.Rec=*Rec;

	return std::any_of(CONST_RANGE(SortedPlugins, i) {return i->has<iProcessEditorInput>() && i->ProcessEditorInput(&Info);});
}


int PluginManager::ProcessEditorEvent(int Event,void *Param,int EditorID) const
{
	int nResult = 0;
	if (auto editor = Global->WindowManager->GetEditorContainerById(EditorID))
	{
		if (Event == EE_REDRAW)
		{
			auto FED = std::dynamic_pointer_cast<FileEditor>(editor).get();
			FED->AutoDeleteColors();
		}

		ProcessEditorEventInfo Info = {sizeof(Info)};
		Info.Event = Event;
		Info.Param = Param;
		Info.EditorID = EditorID;

		SCOPED_ACTION(auto)(editor->GetPinner());
		std::for_each(CONST_RANGE(SortedPlugins, i)
		{
			if (i->has<iProcessEditorEvent>())
				nResult = i->ProcessEditorEvent(&Info);
		});
	}

	return nResult;
}


int PluginManager::ProcessSubscribedEditorEvent(int Event, void *Param, int EditorID, const std::unordered_set<GUID, uuid_hash, uuid_equal>& PluginIds) const
{
	int nResult = 0;
	if (auto editor = Global->WindowManager->GetEditorContainerById(EditorID))
	{
		ProcessEditorEventInfo Info = {sizeof(Info)};
		Info.Event = Event;
		Info.Param = Param;
		Info.EditorID = EditorID;

		SCOPED_ACTION(auto)(editor->GetPinner());
		std::for_each(CONST_RANGE(SortedPlugins, i)
		{
			if (i->has<iProcessEditorEvent>() && PluginIds.count(i->GetGUID()))
			{
				nResult = i->ProcessEditorEvent(&Info);
			}
		});
	}

	return nResult;
}


int PluginManager::ProcessViewerEvent(int Event, void *Param,int ViewerID) const
{
	int nResult = 0;
	if (auto viewer = Global->WindowManager->GetViewerContainerById(ViewerID))
	{
		ProcessViewerEventInfo Info = {sizeof(Info)};
		Info.Event = Event;
		Info.Param = Param;
		Info.ViewerID = ViewerID;

		SCOPED_ACTION(auto)(viewer->GetPinner());
		std::for_each(CONST_RANGE(SortedPlugins, i)
		{
			if (i->has<iProcessViewerEvent>())
				nResult = i->ProcessViewerEvent(&Info);
		});
	}
	return nResult;
}

int PluginManager::ProcessDialogEvent(int Event, FarDialogEvent *Param) const
{
	ProcessDialogEventInfo Info = {sizeof(Info)};
	Info.Event = Event;
	Info.Param = Param;

	return std::any_of(CONST_RANGE(SortedPlugins, i) {return i->has<iProcessDialogEvent>() && i->ProcessDialogEvent(&Info);});
}

int PluginManager::ProcessConsoleInput(ProcessConsoleInputInfo *Info) const
{
	int nResult = 0;

	FOR(const auto& i, SortedPlugins)
	{
		if (i->has<iProcessConsoleInput>())
		{
			int n = i->ProcessConsoleInput(Info);
			if (n == 1)
			{
				nResult = 1;
				break;
			}
			else if (n == 2)
			{
				nResult = 2;
			}
		}
	}

	return nResult;
}


int PluginManager::GetFindData(PluginHandle* hPlugin, PluginPanelItem **pPanelData, size_t *pItemsNumber, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	GetFindDataInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.OpMode = OpMode;
	int Result = hPlugin->pPlugin->GetFindData(&Info);
	*pPanelData = Info.PanelItem;
	*pItemsNumber = Info.ItemsNumber;
	return Result;
}


void PluginManager::FreeFindData(PluginHandle* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, bool FreeUserData)
{
	if (FreeUserData)
		FreePluginPanelItemsUserData(hPlugin,PanelItem,ItemsNumber);

	FreeFindDataInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	hPlugin->pPlugin->FreeFindData(&Info);
}


int PluginManager::GetVirtualFindData(PluginHandle* hPlugin, PluginPanelItem **pPanelData, size_t *pItemsNumber, const string& Path)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	*pItemsNumber=0;

	GetVirtualFindDataInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.Path = Path.data();
	int Result = hPlugin->pPlugin->GetVirtualFindData(&Info);
	*pPanelData = Info.PanelItem;
	*pItemsNumber = Info.ItemsNumber;
	return Result;
}


void PluginManager::FreeVirtualFindData(PluginHandle* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber)
{
	FreeFindDataInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	return hPlugin->pPlugin->FreeVirtualFindData(&Info);
}


int PluginManager::SetDirectory(PluginHandle* hPlugin, const string& Dir, int OpMode, UserDataItem *UserData)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SetDirectoryInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.Dir = Dir.data();
	Info.OpMode = OpMode;
	if (UserData)
	{
		Info.UserData.Data = UserData->Data;
		Info.UserData.FreeData = UserData->FreeData;
	}
	return hPlugin->pPlugin->SetDirectory(&Info);
}


int PluginManager::GetFile(PluginHandle* hPlugin, PluginPanelItem *PanelItem, const string& DestPath, string &strResultName, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	std::unique_ptr<SaveScreen> SaveScr;
	int Found=FALSE;
	Global->KeepUserScreen=FALSE;

	if (!(OpMode & OPM_FIND))
		SaveScr = std::make_unique<SaveScreen>(); //???

	SCOPED_ACTION(UndoGlobalSaveScrPtr)(SaveScr.get());

	GetFilesInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = 1;
	Info.Move = 0;
	Info.DestPath = DestPath.data();
	Info.OpMode = OpMode;

	int GetCode = hPlugin->pPlugin->GetFiles(&Info);

	string strFindPath;
	strFindPath = Info.DestPath;
	AddEndSlash(strFindPath);
	strFindPath += L"*";
	os::fs::enum_file Find(strFindPath);
	auto ItemIterator = std::find_if(CONST_RANGE(Find, i) { return !(i.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); });
	if (ItemIterator != Find.cend())
	{
		strResultName = Info.DestPath;
		AddEndSlash(strResultName);
		strResultName += ItemIterator->strFileName;

		if (GetCode!=1)
		{
			os::SetFileAttributes(strResultName,FILE_ATTRIBUTE_NORMAL);
			os::DeleteFile(strResultName); //BUGBUG
		}
		else
			Found=TRUE;
	}

	ReadUserBackgound(SaveScr.get());
	return Found;
}


int PluginManager::DeleteFiles(PluginHandle* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SaveScreen SaveScr;
	Global->KeepUserScreen=FALSE;

	DeleteFilesInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	Info.OpMode = OpMode;

	int Code = hPlugin->pPlugin->DeleteFiles(&Info);

	ReadUserBackgound(&SaveScr);

	return Code;
}


int PluginManager::MakeDirectory(PluginHandle* hPlugin, const wchar_t **Name, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SaveScreen SaveScr;
	Global->KeepUserScreen=FALSE;

	MakeDirectoryInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.Name = *Name;
	Info.OpMode = OpMode;

	int Code = hPlugin->pPlugin->MakeDirectory(&Info);

	*Name = Info.Name;

	ReadUserBackgound(&SaveScr);

	return Code;
}


int PluginManager::ProcessHostFile(PluginHandle* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SaveScreen SaveScr;
	Global->KeepUserScreen=FALSE;

	ProcessHostFileInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	Info.OpMode = OpMode;

	int Code = hPlugin->pPlugin->ProcessHostFile(&Info);

	ReadUserBackgound(&SaveScr);

	return Code;
}


int PluginManager::GetFiles(PluginHandle* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, bool Move, const wchar_t **DestPath, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);

	GetFilesInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	Info.Move = Move;
	Info.DestPath = *DestPath;
	Info.OpMode = OpMode;

	int Result = hPlugin->pPlugin->GetFiles(&Info);
	*DestPath = Info.DestPath;
	return Result;
}


int PluginManager::PutFiles(PluginHandle* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, bool Move, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SaveScreen SaveScr;
	Global->KeepUserScreen=FALSE;

	static string strCurrentDirectory;
	os::GetCurrentDirectory(strCurrentDirectory);
	PutFilesInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	Info.Move = Move;
	Info.SrcPath = strCurrentDirectory.data();
	Info.OpMode = OpMode;

	int Code = hPlugin->pPlugin->PutFiles(&Info);

	ReadUserBackgound(&SaveScr);

	return Code;
}

void PluginManager::GetOpenPanelInfo(PluginHandle* hPlugin, OpenPanelInfo *Info)
{
	if (!Info)
		return;

	ClearStruct(*Info);

	Info->StructSize = sizeof(OpenPanelInfo);
	Info->hPanel = hPlugin->hPlugin;
	hPlugin->pPlugin->GetOpenPanelInfo(Info);

	if (Info->CurDir && *Info->CurDir && (Info->Flags & OPIF_REALNAMES) && (Global->CtrlObject->Cp()->ActivePanel()->GetPluginHandle() == hPlugin) && ParsePath(Info->CurDir)!=PATH_UNKNOWN)
		os::SetCurrentDirectory(Info->CurDir, false);
}


int PluginManager::ProcessKey(PluginHandle* hPlugin,const INPUT_RECORD *Rec, bool Pred)
{

	ProcessPanelInputInfo Info={sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.Rec=*Rec;

#ifndef NO_WRAPPER
	if (Pred && hPlugin->pPlugin->IsOemPlugin())
		Info.Rec.EventType |= 0x4000;
#endif
	return hPlugin->pPlugin->ProcessPanelInput(&Info);
}


int PluginManager::ProcessEvent(PluginHandle* hPlugin, int Event, void *Param)
{
	ProcessPanelEventInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.Event = Event;
	Info.Param = Param;

	return hPlugin->pPlugin->ProcessPanelEvent(&Info);
}


int PluginManager::Compare(PluginHandle* hPlugin, const PluginPanelItem *Item1, const PluginPanelItem *Item2, unsigned int Mode)
{
	CompareInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->hPlugin;
	Info.Item1 = Item1;
	Info.Item2 = Item2;
	Info.Mode = static_cast<OPENPANELINFO_SORTMODES>(Mode);

	return hPlugin->pPlugin->Compare(&Info);
}

void PluginManager::ConfigureCurrent(Plugin *pPlugin, const GUID& Guid)
{
	ConfigureInfo Info = {sizeof(Info)};
	Info.Guid = &Guid;

	if (pPlugin->Configure(&Info))
	{
		Panel* Panels[] =
		{
			Global->CtrlObject->Cp()->LeftPanel,
			Global->CtrlObject->Cp()->RightPanel,
		};

		std::for_each(CONST_RANGE(Panels, i)
		{
			if (i->GetMode() == PLUGIN_PANEL)
			{
				i->Update(UPDATE_KEEP_SELECTION);
				i->SetViewMode(i->GetViewMode());
				i->Redraw();
			}
		});
		pPlugin->SaveToCache();
	}
}

struct PluginMenuItemData
{
	Plugin *pPlugin;
	GUID Guid;
};

/* $ 29.05.2001 IS
   ! ��� ��������� "���������� ������� �������" ��������� ���� � ��
     ������� ������ ��� ������� �� ESC
*/
void PluginManager::Configure(int StartPos)
{
		auto PluginList = VMenu2::create(MSG(MPluginConfigTitle), nullptr, 0, ScrY - 4);
		PluginList->SetMenuFlags(VMENU_WRAPMODE);
		PluginList->SetHelp(L"PluginsConfig");
		PluginList->SetId(PluginsConfigMenuId);

		while (!Global->CloseFAR)
		{
			bool NeedUpdateItems = true;
			bool HotKeysPresent = ConfigProvider().PlHotkeyCfg()->HotkeysPresent(PluginsHotkeysConfig::CONFIG_MENU);

			if (NeedUpdateItems)
			{
				PluginList->DeleteItems();
				LoadIfCacheAbsent();
				string strHotKey, strName;
				GUID guid;

				FOR(const auto& i, SortedPlugins)
				{
					bool bCached = i->CheckWorkFlags(PIWF_CACHED);
					unsigned __int64 id = 0;

					PluginInfo Info = {sizeof(Info)};
					if (bCached)
					{
						id = ConfigProvider().PlCacheCfg()->GetCacheID(i->GetCacheName());
					}
					else
					{
						if (!i->GetPluginInfo(&Info))
							continue;
					}

					for (size_t J=0; ; J++)
					{
						if (bCached)
						{
							string strGuid;

							if (!ConfigProvider().PlCacheCfg()->GetPluginsConfigMenuItem(id, J, strName, strGuid))
								break;
							if (!StrToGuid(strGuid,guid))
								break;
						}
						else
						{
							if (J >= Info.PluginConfig.Count)
								break;

							strName = NullToEmpty(Info.PluginConfig.Strings[J]);
							guid = Info.PluginConfig.Guids[J];
						}

						GetPluginHotKey(i, guid, PluginsHotkeysConfig::CONFIG_MENU, strHotKey);
						MenuItemEx ListItem;
#ifndef NO_WRAPPER
						if (i->IsOemPlugin())
							ListItem.Flags=LIF_CHECKED|L'A';
#endif // NO_WRAPPER
						if (!HotKeysPresent)
							ListItem.strName = strName;
						else if (!strHotKey.empty())
							ListItem.strName = str_printf(L"&%c%s  %s", static_cast<wchar_t>(strHotKey.front()),(strHotKey.front()==L'&'?L"&":L""), strName.data());
						else
							ListItem.strName = str_printf(L"   %s", strName.data());

						PluginMenuItemData item;
						item.pPlugin = i;
						item.Guid = guid;
						PluginList->SetUserData(&item, sizeof(PluginMenuItemData),PluginList->AddItem(ListItem));
					}
				}

				PluginList->AssignHighlights(FALSE);
				PluginList->SetBottomTitle(MSG(MPluginHotKeyBottom));
				PluginList->SortItems(false, HotKeysPresent? 3 : 0);
				PluginList->SetSelectPos(StartPos,1);
				NeedUpdateItems = false;
			}

			string strPluginModuleName;

			PluginList->Run([&](const Manager::Key& RawKey)->int
			{
				const auto Key=RawKey.FarKey();
				int SelPos=PluginList->GetSelectPos();
				PluginMenuItemData *item = (PluginMenuItemData*)PluginList->GetUserData(nullptr,0,SelPos);
				int KeyProcessed = 1;

				switch (Key)
				{
					case KEY_SHIFTF1:
						if (item)
						{
							strPluginModuleName = item->pPlugin->GetModuleName();
							if (!pluginapi::apiShowHelp(strPluginModuleName.data(),L"Config",FHELP_SELFHELP|FHELP_NOSHOWERROR) &&
							        !pluginapi::apiShowHelp(strPluginModuleName.data(),L"Configure",FHELP_SELFHELP|FHELP_NOSHOWERROR))
							{
								pluginapi::apiShowHelp(strPluginModuleName.data(),nullptr,FHELP_SELFHELP|FHELP_NOSHOWERROR);
							}
						}
						break;

					case KEY_F3:
						if (item)
						{
							ShowPluginInfo(item->pPlugin, item->Guid);
						}
						break;

					case KEY_F4:
						if (item)
						{
							string strTitle;
							int nOffset = HotKeysPresent?3:0;
							strTitle = PluginList->GetItemPtr()->strName.substr(nOffset);
							RemoveExternalSpaces(strTitle);

							if (SetHotKeyDialog(item->pPlugin, item->Guid, PluginsHotkeysConfig::CONFIG_MENU, strTitle))
							{
								NeedUpdateItems = true;
								StartPos = SelPos;
								PluginList->Close(SelPos);
								break;
							}
						}
						break;

					default:
						KeyProcessed = 0;
				}
				return KeyProcessed;
			});

			if (!NeedUpdateItems)
			{
				StartPos=PluginList->GetExitCode();

				if (StartPos<0)
					break;

				PluginMenuItemData *item = (PluginMenuItemData*)PluginList->GetUserData(nullptr,0,StartPos);
				ConfigureCurrent(item->pPlugin, item->Guid);
			}
		}
}

int PluginManager::CommandsMenu(int ModalType,int StartPos,const wchar_t *HistoryName)
{
	if (ModalType == windowtype_dialog || ModalType == windowtype_menu)
	{
		auto dlg = std::static_pointer_cast<Dialog>(Global->WindowManager->GetCurrentWindow());
		if (dlg->CheckDialogMode(DMODE_NOPLUGINS) || dlg->GetId()==PluginsMenuId)
		{
			return 0;
		}
	}

	bool Editor = ModalType==windowtype_editor;
	bool Viewer = ModalType==windowtype_viewer;
	bool Dialog = ModalType==windowtype_dialog || ModalType==windowtype_menu;

	PluginMenuItemData item;

	{
		auto PluginList = VMenu2::create(MSG(MPluginCommandsMenuTitle), nullptr, 0, ScrY - 4);
		PluginList->SetMenuFlags(VMENU_WRAPMODE);
		PluginList->SetHelp(L"PluginCommands");
		PluginList->SetId(PluginsMenuId);
		bool NeedUpdateItems = true;

		while (NeedUpdateItems)
		{
			bool HotKeysPresent = ConfigProvider().PlHotkeyCfg()->HotkeysPresent(PluginsHotkeysConfig::PLUGINS_MENU);

			if (NeedUpdateItems)
			{
				PluginList->DeleteItems();
				LoadIfCacheAbsent();
				string strHotKey, strName;
				GUID guid;

				FOR(const auto& i, SortedPlugins)
				{
					bool bCached = i->CheckWorkFlags(PIWF_CACHED);
					UINT64 IFlags;
					unsigned __int64 id = 0;

					PluginInfo Info = {sizeof(Info)};
					if (bCached)
					{
						id = ConfigProvider().PlCacheCfg()->GetCacheID(i->GetCacheName());
						IFlags = ConfigProvider().PlCacheCfg()->GetFlags(id);
					}
					else
					{
						if (!i->GetPluginInfo(&Info))
							continue;

						IFlags = Info.Flags;
					}

					if ((Editor && !(IFlags & PF_EDITOR)) ||
					        (Viewer && !(IFlags & PF_VIEWER)) ||
					        (Dialog && !(IFlags & PF_DIALOG)) ||
					        (!Editor && !Viewer && !Dialog && (IFlags & PF_DISABLEPANELS)))
						continue;

					for (size_t J=0; ; J++)
					{
						if (bCached)
						{
							string strGuid;

							if (!ConfigProvider().PlCacheCfg()->GetPluginsMenuItem(id, J, strName, strGuid))
								break;
							if (!StrToGuid(strGuid,guid))
								break;
						}
						else
						{
							if (J >= Info.PluginMenu.Count)
								break;

							strName = NullToEmpty(Info.PluginMenu.Strings[J]);
							guid = Info.PluginMenu.Guids[J];
						}

						GetPluginHotKey(i, guid, PluginsHotkeysConfig::PLUGINS_MENU, strHotKey);
						MenuItemEx ListItem;
#ifndef NO_WRAPPER
						if (i->IsOemPlugin())
							ListItem.Flags=LIF_CHECKED|L'A';
#endif // NO_WRAPPER
						if (!HotKeysPresent)
							ListItem.strName = strName;
						else if (!strHotKey.empty())
							ListItem.strName = str_printf(L"&%c%s  %s", static_cast<wchar_t>(strHotKey.front()),(strHotKey.front()==L'&'?L"&":L""), strName.data());
						else
							ListItem.strName = str_printf(L"   %s", strName.data());

						PluginMenuItemData itemdata;
						itemdata.pPlugin = i;
						itemdata.Guid = guid;
						PluginList->SetUserData(&itemdata, sizeof(PluginMenuItemData),PluginList->AddItem(ListItem));
					}
				}

				PluginList->AssignHighlights(FALSE);
				PluginList->SetBottomTitle(MSG(MPluginHotKeyBottom));
				PluginList->SortItems(false, HotKeysPresent? 3 : 0);
				PluginList->SetSelectPos(StartPos,1);
				NeedUpdateItems = false;
			}

			PluginList->Run([&](const Manager::Key& RawKey)->int
			{
				const auto Key=RawKey.FarKey();
				int SelPos=PluginList->GetSelectPos();
				PluginMenuItemData *ItemPtr = (PluginMenuItemData*)PluginList->GetUserData(nullptr,0,SelPos);
				int KeyProcessed = 1;

				switch (Key)
				{
					case KEY_SHIFTF1:
						// �������� ������ �����, ������� �������� � CommandsMenu()
						if (ItemPtr)
							pluginapi::apiShowHelp(ItemPtr->pPlugin->GetModuleName().data(), HistoryName, FHELP_SELFHELP | FHELP_NOSHOWERROR | FHELP_USECONTENTS);
						break;

					case KEY_F3:
						if (ItemPtr)
						{
							ShowPluginInfo(ItemPtr->pPlugin, ItemPtr->Guid);
						}
						break;

					case KEY_F4:
						if (ItemPtr)
						{
							string strTitle;
							int nOffset = HotKeysPresent?3:0;
							strTitle = PluginList->GetItemPtr()->strName.substr(nOffset);
							RemoveExternalSpaces(strTitle);

							if (SetHotKeyDialog(ItemPtr->pPlugin, ItemPtr->Guid, PluginsHotkeysConfig::PLUGINS_MENU, strTitle))
							{
								NeedUpdateItems = true;
								StartPos = SelPos;
								PluginList->Close(SelPos);
							}
						}
						break;

					case KEY_ALTSHIFTF9:
					case KEY_RALTSHIFTF9:
					{
						if (ItemPtr)
						{
							NeedUpdateItems = true;
							StartPos = SelPos;
							Configure();
							PluginList->Close(SelPos);
						}
						break;
					}

					case KEY_SHIFTF9:
					{
						if (ItemPtr)
						{
							NeedUpdateItems = true;
							StartPos=SelPos;

							if (ItemPtr->pPlugin->has<iConfigure>())
								ConfigureCurrent(ItemPtr->pPlugin, ItemPtr->Guid);

							PluginList->Close(SelPos);
						}

						break;
					}

					default:
						KeyProcessed = 0;
				}
				return KeyProcessed;
			});
		}

		int ExitCode=PluginList->GetExitCode();

		if (ExitCode<0)
		{
			return FALSE;
		}

		Global->ScrBuf->Flush();
		item = *(PluginMenuItemData*)PluginList->GetUserData(nullptr,0,ExitCode);
	}

	Panel *ActivePanel = Global->CtrlObject->Cp()->ActivePanel();
	int OpenCode=OPEN_PLUGINSMENU;
	intptr_t Item=0;
	OpenDlgPluginData pd={sizeof(OpenDlgPluginData)};

	if (Editor)
	{
		OpenCode=OPEN_EDITOR;
	}
	else if (Viewer)
	{
		OpenCode=OPEN_VIEWER;
	}
	else if (Dialog)
	{
		OpenCode=OPEN_DIALOG;
		pd.hDlg=(HANDLE)Global->WindowManager->GetCurrentWindow().get();
		Item=(intptr_t)&pd;
	}

	auto hPlugin=Open(item.pPlugin,OpenCode,item.Guid,Item);

	if (hPlugin && !Editor && !Viewer && !Dialog)
	{
		if (ActivePanel->ProcessPluginEvent(FE_CLOSE,nullptr))
		{
			ClosePanel(hPlugin);
			return FALSE;
		}

		Panel *NewPanel=Global->CtrlObject->Cp()->ChangePanel(ActivePanel,FILE_PANEL,TRUE,TRUE);
		NewPanel->SetPluginMode(hPlugin,L"",true);
		NewPanel->Update(0);
		NewPanel->Show();
	}

	// restore title for old plugins only.
#ifndef NO_WRAPPER
	if (item.pPlugin->IsOemPlugin() && Editor && Global->WindowManager->GetCurrentEditor())
	{
		Global->WindowManager->GetCurrentEditor()->SetPluginTitle(nullptr);
	}
#endif // NO_WRAPPER
	return TRUE;
}

bool PluginManager::SetHotKeyDialog(Plugin *pPlugin, const GUID& Guid, PluginsHotkeysConfig::HotKeyTypeEnum HotKeyType, const string& DlgPluginTitle)
{
	string strPluginKey;
	GetHotKeyPluginKey(pPlugin, strPluginKey);
	string strGuid = GuidToStr(Guid);
	string strHotKey = ConfigProvider().PlHotkeyCfg()->GetHotkey(strPluginKey, strGuid, HotKeyType);

	DialogBuilder Builder(MPluginHotKeyTitle, L"SetHotKeyDialog");
	Builder.AddText(MPluginHotKey);
	Builder.AddTextAfter(Builder.AddFixEditField(strHotKey, 1), DlgPluginTitle.data());
	Builder.AddOKCancel();
	if(Builder.ShowDialog())
	{
		if (!strHotKey.empty() && strHotKey.front() != L' ')
			ConfigProvider().PlHotkeyCfg()->SetHotkey(strPluginKey, strGuid, HotKeyType, strHotKey);
		else
			ConfigProvider().PlHotkeyCfg()->DelHotkey(strPluginKey, strGuid, HotKeyType);
		return true;
	}
	return false;
}

void PluginManager::ShowPluginInfo(Plugin *pPlugin, const GUID& Guid)
{
	string strPluginGuid = GuidToStr(pPlugin->GetGUID());
	string strItemGuid = GuidToStr(Guid);
	string strPluginPrefix;
	if (pPlugin->CheckWorkFlags(PIWF_CACHED))
	{
		unsigned __int64 id = ConfigProvider().PlCacheCfg()->GetCacheID(pPlugin->GetCacheName());
		strPluginPrefix = ConfigProvider().PlCacheCfg()->GetCommandPrefix(id);
	}
	else
	{
		PluginInfo Info = {sizeof(Info)};
		if (pPlugin->GetPluginInfo(&Info))
		{
			strPluginPrefix = NullToEmpty(Info.CommandPrefix);
		}
	}
	const int Width = 36;
	DialogBuilder Builder(MPluginInformation, L"ShowPluginInfo");
	Builder.AddText(MPluginModuleTitle);
	Builder.AddConstEditField(pPlugin->GetTitle(), Width);
	Builder.AddText(MPluginDescription);
	Builder.AddConstEditField(pPlugin->GetDescription(), Width);
	Builder.AddText(MPluginAuthor);
	Builder.AddConstEditField(pPlugin->GetAuthor(), Width);
	Builder.AddText(MPluginVersion);
	Builder.AddConstEditField(pPlugin->GetVersionString(), Width);
	Builder.AddText(MPluginModulePath);
	Builder.AddConstEditField(pPlugin->GetModuleName(), Width);
	Builder.AddText(MPluginGUID);
	Builder.AddConstEditField(strPluginGuid, Width);
	Builder.AddText(MPluginItemGUID);
	Builder.AddConstEditField(strItemGuid, Width);
	Builder.AddText(MPluginPrefix);
	Builder.AddConstEditField(strPluginPrefix, Width);
	Builder.AddOK();
	Builder.ShowDialog();
}

char* BufReserve(char*& Buf, size_t Count, size_t& Rest, size_t& Size)
{
	char* Res = nullptr;

	if (Buf)
	{
		if (Rest >= Count)
		{
			Res = Buf;
			Buf += Count;
			Rest -= Count;
		}
		else
		{
			Buf += Rest;
			Rest = 0;
		}
	}

	Size += Count;
	return Res;
}


wchar_t* StrToBuf(const string& Str, char*& Buf, size_t& Rest, size_t& Size)
{
	size_t Count = (Str.size() + 1) * sizeof(wchar_t);
	auto Res = reinterpret_cast<wchar_t*>(BufReserve(Buf, Count, Rest, Size));
	if (Res)
	{
		wcscpy(Res, Str.data());
	}
	return Res;
}


void ItemsToBuf(PluginMenuItem& Menu, const std::vector<string>& NamesArray, const std::vector<string>& GuidsArray, char*& Buf, size_t& Rest, size_t& Size)
{
	Menu.Count = NamesArray.size();
	Menu.Strings = nullptr;
	Menu.Guids = nullptr;

	if (Menu.Count)
	{
		auto Items = reinterpret_cast<wchar_t**>(BufReserve(Buf, Menu.Count * sizeof(wchar_t*), Rest, Size));
		auto Guids = reinterpret_cast<GUID*>(BufReserve(Buf, Menu.Count * sizeof(GUID), Rest, Size));
		Menu.Strings = Items;
		Menu.Guids = Guids;

		for (size_t i = 0; i < Menu.Count; ++i)
		{
			wchar_t* pStr = StrToBuf(NamesArray[i], Buf, Rest, Size);
			if (Items)
			{
				Items[i] = pStr;
			}

			if (Guids)
			{
				GUID Guid;
				if (StrToGuid(GuidsArray[i], Guid))
				{
					Guids[i] = Guid;
				}
			}
		}
	}
}

size_t PluginManager::GetPluginInformation(Plugin *pPlugin, FarGetPluginInformation *pInfo, size_t BufferSize)
{
	if(IsPluginUnloaded(pPlugin)) return 0;
	string Prefix;
	PLUGIN_FLAGS Flags = 0;
	std::vector<string> MenuNames, MenuGuids, DiskNames, DiskGuids, ConfNames, ConfGuids;

	if (pPlugin->CheckWorkFlags(PIWF_CACHED))
	{
		unsigned __int64 id = ConfigProvider().PlCacheCfg()->GetCacheID(pPlugin->GetCacheName());
		Flags = ConfigProvider().PlCacheCfg()->GetFlags(id);
		Prefix = ConfigProvider().PlCacheCfg()->GetCommandPrefix(id);

		string Name, Guid;

		for(int i = 0; ConfigProvider().PlCacheCfg()->GetPluginsMenuItem(id, i, Name, Guid); ++i)
		{
			MenuNames.emplace_back(Name);
			MenuGuids.emplace_back(Guid);
		}

		for(int i = 0; ConfigProvider().PlCacheCfg()->GetDiskMenuItem(id, i, Name, Guid); ++i)
		{
			DiskNames.emplace_back(Name);
			DiskGuids.emplace_back(Guid);
		}

		for(int i = 0; ConfigProvider().PlCacheCfg()->GetPluginsConfigMenuItem(id, i, Name, Guid); ++i)
		{
			ConfNames.emplace_back(Name);
			ConfGuids.emplace_back(Guid);
		}
	}
	else
	{
		PluginInfo Info = {sizeof(Info)};
		if (pPlugin->GetPluginInfo(&Info))
		{
			Flags = Info.Flags;
			Prefix = NullToEmpty(Info.CommandPrefix);

			for (size_t i = 0; i < Info.PluginMenu.Count; i++)
			{
					MenuNames.emplace_back(Info.PluginMenu.Strings[i]);
					MenuGuids.emplace_back(GuidToStr(Info.PluginMenu.Guids[i]));
			}

			for (size_t i = 0; i < Info.DiskMenu.Count; i++)
			{
				DiskNames.emplace_back(Info.DiskMenu.Strings[i]);
				DiskGuids.emplace_back(GuidToStr(Info.DiskMenu.Guids[i]));
			}

			for (size_t i = 0; i < Info.PluginConfig.Count; i++)
			{
				ConfNames.emplace_back(Info.PluginConfig.Strings[i]);
				ConfGuids.emplace_back(GuidToStr(Info.PluginConfig.Guids[i]));
			}
		}
	}

	struct
	{
		FarGetPluginInformation fgpi;
		PluginInfo PInfo;
		GlobalInfo GInfo;
	} Temp;
	char* Buffer = nullptr;
	size_t Rest = 0;
	size_t Size = sizeof(Temp);

	if (pInfo)
	{
		Rest = BufferSize - Size;
		Buffer = reinterpret_cast<char*>(pInfo) + Size;
	}
	else
	{
		pInfo = &Temp.fgpi;
	}

	pInfo->PInfo = reinterpret_cast<PluginInfo*>(pInfo+1);
	pInfo->GInfo = reinterpret_cast<GlobalInfo*>(pInfo->PInfo+1);
	pInfo->ModuleName = StrToBuf(pPlugin->GetModuleName(), Buffer, Rest, Size);

	pInfo->Flags = 0;

	if (pPlugin->m_Instance)
	{
		pInfo->Flags |= FPF_LOADED;
	}
#ifndef NO_WRAPPER
	if (pPlugin->IsOemPlugin())
	{
		pInfo->Flags |= FPF_ANSI;
	}
#endif // NO_WRAPPER

	pInfo->GInfo->StructSize = sizeof(GlobalInfo);
	pInfo->GInfo->Guid = pPlugin->GetGUID();
	pInfo->GInfo->Version = pPlugin->GetVersion();
	pInfo->GInfo->MinFarVersion = pPlugin->GetMinFarVersion();
	pInfo->GInfo->Title = StrToBuf(pPlugin->strTitle, Buffer, Rest, Size);
	pInfo->GInfo->Description = StrToBuf(pPlugin->strDescription, Buffer, Rest, Size);
	pInfo->GInfo->Author = StrToBuf(pPlugin->strAuthor, Buffer, Rest, Size);

	pInfo->PInfo->StructSize = sizeof(PluginInfo);
	pInfo->PInfo->Flags = Flags;
	pInfo->PInfo->CommandPrefix = StrToBuf(Prefix, Buffer, Rest, Size);

	ItemsToBuf(pInfo->PInfo->DiskMenu, DiskNames, DiskGuids, Buffer, Rest, Size);
	ItemsToBuf(pInfo->PInfo->PluginMenu, MenuNames, MenuGuids, Buffer, Rest, Size);
	ItemsToBuf(pInfo->PInfo->PluginConfig, ConfNames, ConfGuids, Buffer, Rest, Size);

	return Size;
}

bool PluginManager::GetDiskMenuItem(
     Plugin *pPlugin,
     size_t PluginItem,
     bool &ItemPresent,
     wchar_t& PluginHotkey,
     string &strPluginText,
     GUID &Guid
)
{
	LoadIfCacheAbsent();

	ItemPresent = false;

	if (pPlugin->CheckWorkFlags(PIWF_CACHED))
	{
		string strGuid;
		if (ConfigProvider().PlCacheCfg()->GetDiskMenuItem(ConfigProvider().PlCacheCfg()->GetCacheID(pPlugin->GetCacheName()), PluginItem, strPluginText, strGuid))
			if (StrToGuid(strGuid,Guid))
				ItemPresent = true;
		ItemPresent = ItemPresent && !strPluginText.empty();
	}
	else
	{
		PluginInfo Info = {sizeof(Info)};

		if (!pPlugin->GetPluginInfo(&Info) || Info.DiskMenu.Count <= PluginItem)
		{
			ItemPresent = false;
		}
		else
		{
			strPluginText = NullToEmpty(Info.DiskMenu.Strings[PluginItem]);
			Guid = Info.DiskMenu.Guids[PluginItem];
			ItemPresent = true;
		}
	}
	if (ItemPresent)
	{
		string strHotKey;
		GetPluginHotKey(pPlugin,Guid,PluginsHotkeysConfig::DRIVE_MENU,strHotKey);
		PluginHotkey = strHotKey.empty() ? 0 : strHotKey[0];
	}

	return true;
}

int PluginManager::UseFarCommand(PluginHandle* hPlugin,int CommandType)
{
	OpenPanelInfo Info;
	GetOpenPanelInfo(hPlugin,&Info);

	if (!(Info.Flags & OPIF_REALNAMES))
		return FALSE;

	switch (CommandType)
	{
		case PLUGIN_FARGETFILE:
		case PLUGIN_FARGETFILES:
			return !hPlugin->pPlugin->has<iGetFiles>() || (Info.Flags & OPIF_EXTERNALGET);
		case PLUGIN_FARPUTFILES:
			return !hPlugin->pPlugin->has<iPutFiles>() || (Info.Flags & OPIF_EXTERNALPUT);
		case PLUGIN_FARDELETEFILES:
			return !hPlugin->pPlugin->has<iDeleteFiles>() || (Info.Flags & OPIF_EXTERNALDELETE);
		case PLUGIN_FARMAKEDIRECTORY:
			return !hPlugin->pPlugin->has<iMakeDirectory>() || (Info.Flags & OPIF_EXTERNALMKDIR);
	}

	return TRUE;
}


void PluginManager::ReloadLanguage()
{
	std::for_each(ALL_CONST_RANGE(SortedPlugins), std::mem_fn(&Plugin::CloseLang));
	ConfigProvider().PlCacheCfg()->DiscardCache();
}

void PluginManager::LoadIfCacheAbsent()
{
	if (ConfigProvider().PlCacheCfg()->IsCacheEmpty())
	{
		std::for_each(ALL_CONST_RANGE(SortedPlugins), std::mem_fn(&Plugin::Load));
	}
}

//template parameters must have external linkage
struct PluginData
{
	Plugin *pPlugin;
	UINT64 PluginFlags;
};

int PluginManager::ProcessCommandLine(const string& CommandParam,Panel *Target)
{
	size_t PrefixLength=0;
	string strCommand=CommandParam;
	UnquoteExternal(strCommand);
	RemoveLeadingSpaces(strCommand);

	if (!IsPluginPrefixPath(strCommand))
		return FALSE;

	LoadIfCacheAbsent();
	string strPrefix = strCommand.substr(0, strCommand.find(L':'));
	string strPluginPrefix;
	std::list<PluginData> items;

	FOR(const auto& i, SortedPlugins)
	{
		UINT64 PluginFlags=0;

		if (i->CheckWorkFlags(PIWF_CACHED))
		{
			unsigned __int64 id = ConfigProvider().PlCacheCfg()->GetCacheID(i->GetCacheName());
			strPluginPrefix = ConfigProvider().PlCacheCfg()->GetCommandPrefix(id);
			PluginFlags = ConfigProvider().PlCacheCfg()->GetFlags(id);
		}
		else
		{
			PluginInfo Info = {sizeof(Info)};

			if (i->GetPluginInfo(&Info))
			{
				strPluginPrefix = NullToEmpty(Info.CommandPrefix);
				PluginFlags = Info.Flags;
			}
			else
				continue;
		}

		if (strPluginPrefix.empty())
			continue;

		const wchar_t *PrStart = strPluginPrefix.data();
		PrefixLength=strPrefix.size();

		for (;;)
		{
			const wchar_t *PrEnd = wcschr(PrStart, L':');
			size_t Len = PrEnd? (PrEnd - PrStart) : wcslen(PrStart);

			if (Len<PrefixLength)Len=PrefixLength;

			if (!StrCmpNI(strPrefix.data(), PrStart, Len))
			{
				if (i->Load() && i->has<iOpen>())
				{
					PluginData pD;
					pD.pPlugin = i;
					pD.PluginFlags=PluginFlags;
					items.emplace_back(pD);
					break;
				}
			}

			if (!PrEnd)
				break;

			PrStart = ++PrEnd;
		}

		if (!items.empty() && !Global->Opt->PluginConfirm.Prefix)
			break;
	}

	if (items.empty())
		return FALSE;

	Panel *ActivePanel=Global->CtrlObject->Cp()->ActivePanel();
	Panel *CurPanel=(Target)?Target:ActivePanel;

	if (CurPanel->ProcessPluginEvent(FE_CLOSE,nullptr))
		return FALSE;

	auto PData = items.begin();

	if (items.size()>1)
	{
		auto menu = VMenu2::create(MSG(MPluginConfirmationTitle), nullptr, 0, ScrY - 4);
		menu->SetPosition(-1, -1, 0, 0);
		menu->SetHelp(L"ChoosePluginMenu");
		menu->SetMenuFlags(VMENU_SHOWAMPERSAND | VMENU_WRAPMODE);

		std::for_each(CONST_RANGE(items, i)
		{
			MenuItemEx mitem;
			mitem.strName=PointToName(i.pPlugin->GetModuleName());
			menu->AddItem(mitem);
		});

		int ExitCode=menu->Run();

		if (ExitCode>=0)
		{
			std::advance(PData, ExitCode);
		}
	}

	string strPluginCommand=strCommand.substr(PData->PluginFlags & PF_FULLCMDLINE ? 0:PrefixLength+1);
	RemoveTrailingSpaces(strPluginCommand);
	OpenCommandLineInfo info={sizeof(OpenCommandLineInfo),strPluginCommand.data()}; //BUGBUG
	auto hPlugin=Open(PData->pPlugin,OPEN_COMMANDLINE,FarGuid,(intptr_t)&info);
	Global->CtrlObject->CmdLine()->SetString(L"", false);

	if (hPlugin)
	{
		Panel *NewPanel=Global->CtrlObject->Cp()->ChangePanel(CurPanel,FILE_PANEL,TRUE,TRUE);
		NewPanel->SetPluginMode(hPlugin,L"",!Target || Target == ActivePanel);
		NewPanel->Update(0);
		NewPanel->Show();
	}

	return TRUE;
}


/* $ 27.09.2000 SVS
  ������� CallPlugin - ����� ������ �� ID � ���������
  � ���������� ���������!
*/
int PluginManager::CallPlugin(const GUID& SysID,int OpenFrom, void *Data,void **Ret)
{
	if (auto Dlg = std::dynamic_pointer_cast<Dialog>(Global->WindowManager->GetCurrentWindow()))
	{
		if (Dlg->CheckDialogMode(DMODE_NOPLUGINS))
		{
			return FALSE;
		}
	}

	Plugin *pPlugin = FindPlugin(SysID);

	if (pPlugin)
	{
		if (pPlugin->has<iOpen>() && !Global->ProcessException)
		{
			auto hNewPlugin=Open(pPlugin,OpenFrom,FarGuid,(intptr_t)Data);
			bool process=false;

			if (OpenFrom == OPEN_FROMMACRO)
			{
				if (hNewPlugin)
				{
					if (os::memory::is_pointer(hNewPlugin->hPlugin) && hNewPlugin->hPlugin != INVALID_HANDLE_VALUE)
					{
						auto fmc = reinterpret_cast<FarMacroCall*>(hNewPlugin->hPlugin);
						if (fmc->Count > 0 && fmc->Values[0].Type == FMVT_PANEL)
						{
							process = true;
							hNewPlugin->hPlugin = fmc->Values[0].Pointer;
							if (fmc->Callback)
								fmc->Callback(fmc->CallbackData, fmc->Values, fmc->Count);
						}
					}
				}
			}
			else
			{
				process=OpenFrom == OPEN_PLUGINSMENU || OpenFrom == OPEN_FILEPANEL;
			}

			if (hNewPlugin && process)
			{
				int CurFocus=Global->CtrlObject->Cp()->ActivePanel()->GetFocus();
				Panel *NewPanel = Global->CtrlObject->Cp()->ChangePanel(Global->CtrlObject->Cp()->ActivePanel(), FILE_PANEL, TRUE, TRUE);
				NewPanel->SetPluginMode(hNewPlugin,L"",CurFocus || !Global->CtrlObject->Cp()->GetAnotherPanel(NewPanel)->IsVisible());
				if (OpenFrom != OPEN_FROMMACRO)
				{
					if (Data && *(const wchar_t *)Data)
					{
						UserDataItem UserData = {};  // !!! NEED CHECK !!!
						SetDirectory(hNewPlugin,(const wchar_t *)Data,0,&UserData);
					}
				}
				else
				{
					NewPanel->Update(0);
					NewPanel->Show();
				}
			}

			if (Ret)
			{
				auto handle = reinterpret_cast<PluginHandle*>(hNewPlugin);
				if (OpenFrom == OPEN_FROMMACRO && process)
					*Ret = ToPtr(1);
				else
				{
					*Ret = hNewPlugin? handle->hPlugin : nullptr;
					delete handle;
				}
			}

			return TRUE;
		}
	}
	return FALSE;
}

// ��������� ������������ plugin.call, plugin.cmd, plugin.config � �.�
int PluginManager::CallPluginItem(const GUID& Guid, CallPluginInfo *Data)
{
	BOOL Result=FALSE;

	if (!Global->ProcessException)
	{
		int curType = Global->WindowManager->GetCurrentWindow()->GetType();

		if (curType==windowtype_dialog)
		{
			if (std::static_pointer_cast<Dialog>(Global->WindowManager->GetCurrentWindow())->CheckDialogMode(DMODE_NOPLUGINS))
			{
				return FALSE;
			}
		}

		bool Editor = curType==windowtype_editor;
		bool Viewer = curType==windowtype_viewer;
		bool Dialog = curType==windowtype_dialog;

		if (Data->CallFlags & CPT_CHECKONLY)
		{
			Data->pPlugin = FindPlugin(Guid);
			if (Data->pPlugin && Data->pPlugin->Load())
			{
				// �������� �� ����� ������� ���� � ������� ������� (��������������� ��������)
				switch ((Data->CallFlags & CPT_MASK))
				{
					case CPT_MENU:
						if (!Data->pPlugin->has<iOpen>())
							return FALSE;
						break;
					case CPT_CONFIGURE:
						if (curType!=windowtype_panels)
						{
							//TODO: ����������� �� ������?
							return FALSE;
						}
						if (!Data->pPlugin->has<iConfigure>())
							return FALSE;
						break;
					case CPT_CMDLINE:
						if (curType!=windowtype_panels)
						{
							//TODO: ����������� �� ������?
							return FALSE;
						}
						//TODO: OpenPanel ��� OpenFilePlugin?
						if (!Data->pPlugin->has<iOpen>())
							return FALSE;
						break;
					case CPT_INTERNAL:
						//TODO: �������� �������
						if (!Data->pPlugin->has<iOpen>())
							return FALSE;
						break;
				}

				UINT64 IFlags;
				PluginInfo Info = {sizeof(Info)};
				if (!Data->pPlugin->GetPluginInfo(&Info))
					return FALSE;
				else
					IFlags = Info.Flags;

				PluginMenuItem *MenuItems=nullptr;

				// �������� �� ����� ������� ���� � ������� �������
				switch ((Data->CallFlags & CPT_MASK))
				{
					case CPT_MENU:
						if ((Editor && !(IFlags & PF_EDITOR)) ||
								(Viewer && !(IFlags & PF_VIEWER)) ||
								(Dialog && !(IFlags & PF_DIALOG)) ||
								(!Editor && !Viewer && !Dialog && (IFlags & PF_DISABLEPANELS)))
							return FALSE;
						MenuItems = &Info.PluginMenu;
						break;
					case CPT_CONFIGURE:
						MenuItems = &Info.PluginConfig;
						break;
					case CPT_CMDLINE:
						if (!Info.CommandPrefix || !*Info.CommandPrefix)
							return FALSE;
						break;
					case CPT_INTERNAL:
						break;
				}

				if ((Data->CallFlags & CPT_MASK)==CPT_MENU || (Data->CallFlags & CPT_MASK)==CPT_CONFIGURE)
				{
					bool ItemFound = false;
					if (Data->ItemGuid==nullptr)
					{
						if (MenuItems->Count==1)
						{
							Data->FoundGuid=MenuItems->Guids[0];
							Data->ItemGuid=&Data->FoundGuid;
							ItemFound=true;
						}
					}
					else
					{
						for (size_t i = 0; i < MenuItems->Count; i++)
						{
							if (*Data->ItemGuid == MenuItems->Guids[i])
							{
								Data->FoundGuid=*Data->ItemGuid;
								Data->ItemGuid=&Data->FoundGuid;
								ItemFound=true;
								break;
							}
						}
					}
					if (!ItemFound)
						return FALSE;
				}

				Result=TRUE;
			}
		}
		else
		{
			if (!Data->pPlugin)
				return FALSE;

			PluginHandle* hPlugin=nullptr;
			Panel *ActivePanel=nullptr;

			switch ((Data->CallFlags & CPT_MASK))
			{
				case CPT_MENU:
				{
					ActivePanel = Global->CtrlObject->Cp()->ActivePanel();
					int OpenCode=OPEN_PLUGINSMENU;
					intptr_t Item=0;
					OpenDlgPluginData pd={sizeof(OpenDlgPluginData)};

					if (Editor)
					{
						OpenCode=OPEN_EDITOR;
					}
					else if (Viewer)
					{
						OpenCode=OPEN_VIEWER;
					}
					else if (Dialog)
					{
						OpenCode=OPEN_DIALOG;
						pd.hDlg=(HANDLE)Global->WindowManager->GetCurrentWindow().get();
						Item=(intptr_t)&pd;
					}

					hPlugin=Open(Data->pPlugin,OpenCode,Data->FoundGuid,Item);

					Result=TRUE;
					break;
				}

				case CPT_CONFIGURE:
					Global->CtrlObject->Plugins->ConfigureCurrent(Data->pPlugin,Data->FoundGuid);
					return TRUE;

				case CPT_CMDLINE:
				{
					ActivePanel=Global->CtrlObject->Cp()->ActivePanel();
					string command=Data->Command; // ����� ����� ������
					OpenCommandLineInfo info={sizeof(OpenCommandLineInfo),command.data()};
					hPlugin=Open(Data->pPlugin,OPEN_COMMANDLINE,FarGuid,(intptr_t)&info);

					Result=TRUE;
					break;
				}
				case CPT_INTERNAL:
					//TODO: ������ CallPlugin
					//WARNING: ������, ��� �� ����������� ��� ������������ MacroState
					break;
			}

			if (hPlugin && !Editor && !Viewer && !Dialog)
			{
				//BUGBUG: �������� ������? ����� �� ���?
				//BUGBUG: � ProcessCommandLine ������� ����� Open, � � CPT_MENU - �����
				if (ActivePanel->ProcessPluginEvent(FE_CLOSE,nullptr))
				{
					ClosePanel(hPlugin);
					return FALSE;
				}

				Panel *NewPanel=Global->CtrlObject->Cp()->ChangePanel(ActivePanel,FILE_PANEL,TRUE,TRUE);
				NewPanel->SetPluginMode(hPlugin,L"",true);
				NewPanel->Update(0);
				NewPanel->Show();
			}

			// restore title for old plugins only.
			#ifndef NO_WRAPPER
			if (Data->pPlugin->IsOemPlugin() && Editor && Global->WindowManager->GetCurrentEditor())
			{
				Global->WindowManager->GetCurrentEditor()->SetPluginTitle(nullptr);
			}
			#endif // NO_WRAPPER
		}
	}

	return Result;
}

Plugin *PluginManager::FindPlugin(const GUID& SysID) const
{
	auto Iterator = m_Plugins.find(SysID);
	return Iterator == m_Plugins.cend()? nullptr : Iterator->second.get();
}

PluginHandle* PluginManager::Open(Plugin *pPlugin,int OpenFrom,const GUID& Guid,intptr_t Item)
{
	OpenInfo Info = {sizeof(Info)};
	Info.OpenFrom = static_cast<OPENFROM>(OpenFrom);
	Info.Guid = &Guid;
	Info.Data = Item;

	auto hPlugin = pPlugin->Open(&Info);
	if (hPlugin)
	{
		PluginHandle *handle = new PluginHandle;
		handle->hPlugin = hPlugin;
		handle->pPlugin = pPlugin;
		return handle;
	}

	return nullptr;
}

void PluginManager::GetContentPlugins(const std::vector<const wchar_t*>& ColNames, std::vector<Plugin*>& Plugins) const
{
	size_t Count = ColNames.size();
	std::for_each(CONST_RANGE(SortedPlugins, i)
	{
		if (i->has<iGetContentData>() && i->has<iGetContentFields>())
		{
			GetContentFieldsInfo Info = { sizeof(GetContentFieldsInfo),Count,ColNames.data() };
			if (i->GetContentFields(&Info))
				Plugins.emplace_back(i);
		}
	});
}

void PluginManager::GetContentData(
	const std::vector<Plugin*>& Plugins,
	const string& Name,
	const std::vector<const wchar_t*>& ColNames,
	std::vector<const wchar_t*>& ColValues,
	std::unordered_map<string,string>& ContentData
) const
{
	const NTPath FilePath(Name);
	size_t Count = ColNames.size();
	std::for_each(CONST_RANGE(Plugins, i)
	{
		GetContentDataInfo GetInfo = { sizeof(GetContentDataInfo), FilePath.data(), Count, ColNames.data(), ColValues.data() };
		ColValues.assign(ColValues.size(), nullptr);

		if (i->GetContentData(&GetInfo) && GetInfo.Values)
		{
			for (size_t k=0; k<Count; k++)
			{
				if (GetInfo.Values[k])
					ContentData[ColNames[k]] += GetInfo.Values[k];
			}

			if (i->has<iFreeContentData>())
			{
				i->FreeContentData(&GetInfo);
			}
		}
	});
}

const GUID& PluginManager::GetGUID(const PluginHandle* hPlugin)
{
	return hPlugin->pPlugin->GetGUID();
}

void PluginManager::RefreshPluginsList()
{
	if(!UnloadedPlugins.empty())
	{
		UnloadedPlugins.remove_if([&](CONST_REFERENCE(UnloadedPlugins) i) -> bool
		{
			if (!i->Active())
			{
				i->Unload(true);
				RemovePlugin(i);
				return true;
			}
			return false;
		});
	}
}

void PluginManager::UndoRemove(Plugin* plugin)
{
	auto i = std::find(UnloadedPlugins.begin(), UnloadedPlugins.end(), plugin);
	if(i != UnloadedPlugins.end())
		UnloadedPlugins.erase(i);
}
