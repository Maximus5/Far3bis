﻿/*
plugins.cpp

Работа с плагинами (низкий уровень, кое-что повыше в filelist.cpp)
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

#include "plugins.hpp"
#include "keys.hpp"
#include "scantree.hpp"
#include "chgprior.hpp"
#include "constitle.hpp"
#include "filepanels.hpp"
#include "panel.hpp"
#include "vmenu.hpp"
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
#include "lang.hpp"
#include "language.hpp"
#include "desktop.hpp"
#include "PluginA.hpp"
#include "string_utils.hpp"
#include "cvtname.hpp"
#include "delete.hpp"

static const wchar_t PluginsFolderName[] = L"Plugins";

static void ReadUserBackgound(SaveScreen *SaveScr)
{
	if (Global->KeepUserScreen)
	{
		if (SaveScr)
			SaveScr->Discard();

		Global->ScrBuf->FillBuf();
		Global->CtrlObject->Desktop->TakeSnapshot();
	}
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
	if (pPlugin->IsOemPlugin() && starts_with_icase(pPlugin->GetModuleName(), Global->g_strFarPath))
		strPluginKey.erase(0, Global->g_strFarPath.size());
#endif // NO_WRAPPER
}

static wchar_t GetPluginHotKey(Plugin *pPlugin, const GUID& Guid, hotkey_type HotKeyType)
{
	string strPluginKey;
	GetHotKeyPluginKey(pPlugin, strPluginKey);
	const auto strHotKey = ConfigProvider().PlHotkeyCfg()->GetHotkey(strPluginKey, Guid, HotKeyType);
	return strHotKey.empty()? L'\0' : strHotKey.front();
}

bool PluginManager::plugin_less::operator ()(const Plugin* a, const Plugin *b) const
{
	return StrCmpI(PointToName(a->GetModuleName()),PointToName(b->GetModuleName())) < 0;
}

static void CallPluginSynchroEvent(const any& Payload)
{
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
	Plugin *Luamacro=nullptr; // обеспечить выгрузку данного плагина последним.

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

Plugin* PluginManager::AddPlugin(std::unique_ptr<Plugin>&& pPlugin)
{
	const auto Result = m_Plugins.emplace(pPlugin->GetGUID(), nullptr);
	if (!Result.second)
	{
		pPlugin->Unload(true);
		return nullptr;
	}
	Result.first->second = std::move(pPlugin);

	const auto PluginPtr = Result.first->second.get();

	SortedPlugins.emplace(PluginPtr);
#ifndef NO_WRAPPER
	if (PluginPtr->IsOemPlugin())
	{
		OemPluginsCount++;
	}
#endif // NO_WRAPPER
	return PluginPtr;
}

bool PluginManager::UpdateId(Plugin *pPlugin, const GUID& Id)
{
	const auto Iterator = m_Plugins.find(pPlugin->GetGUID());
	// important, do not delete Plugin instance
	Iterator->second.release();
	m_Plugins.erase(Iterator);
	pPlugin->SetGuid(Id);
	const auto Result = m_Plugins.emplace(pPlugin->GetGUID(), nullptr);
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
	std::unique_ptr<Plugin> pPlugin;

	std::any_of(CONST_RANGE(PluginFactories, i) { return (pPlugin = i->CreatePlugin(FileName)) != nullptr; });

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

		if (!Result)
		{
			return nullptr;
		}

		const auto PluginPtr = AddPlugin(std::move(pPlugin));

		if (!PluginPtr)
		{
			return nullptr;
		}

		if (bDataLoaded && !PluginPtr->Load())
		{
			PluginPtr->Unload(true);
			RemovePlugin(PluginPtr);
			return nullptr;
		}
		return PluginPtr;
	}
	return nullptr;
}

Plugin* PluginManager::LoadPluginExternal(const string& ModuleName, bool LoadToMem)
{
	auto pPlugin = FindPlugin(ModuleName);

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

		if (os::GetFindDataEx(ModuleName, FindData))
		{
			pPlugin = LoadPlugin(ModuleName, FindData, LoadToMem);
		}
	}
	return pPlugin;
}

int PluginManager::UnloadPlugin(Plugin *pPlugin, int From)
{
	int nResult = FALSE;

	if (pPlugin && (From != iExitFAR))   //схитрим, если упали в EXITFAR, не полезем в рекурсию, мы и так в Unload
	{
		for(int i = static_cast<int>(Global->WindowManager->GetWindowCount()-1); i >= 0; --i)
		{
			const auto Window = Global->WindowManager->GetWindow(i);
			if((Window->GetType()==windowtype_dialog && std::static_pointer_cast<Dialog>(Window)->GetPluginOwner() == pPlugin) || Window->GetType()==windowtype_help)
			{
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
			const auto ActivePanel = Global->CtrlObject->Cp()->ActivePanel();
			ActivePanel->Update(UPDATE_KEEP_SELECTION);
			ActivePanel->Redraw();
			const auto AnotherPanel = Global->CtrlObject->Cp()->PassivePanel();
			AnotherPanel->Update(UPDATE_KEEP_SELECTION|UPDATE_SECONDARY);
			AnotherPanel->Redraw();
		}

		UnloadedPlugins.emplace_back(pPlugin);
	}

	return nResult;
}

bool PluginManager::IsPluginUnloaded(const Plugin* pPlugin) const
{
	return contains(UnloadedPlugins, pPlugin);
}

int PluginManager::UnloadPluginExternal(Plugin* pPlugin)
{
	//BUGBUG нужны проверки на легальность выгрузки
	if(pPlugin->Active())
	{
		if(!IsPluginUnloaded(pPlugin))
		{
			UnloadedPlugins.emplace_back(pPlugin);
		}
		return TRUE;
	}

	UnloadedPlugins.remove(pPlugin);
	const auto Result = pPlugin->Unload(true);
	RemovePlugin(pPlugin);
	return Result;
}

Plugin *PluginManager::FindPlugin(const string& ModuleName) const
{
	const auto ItemIterator = std::find_if(CONST_RANGE(SortedPlugins, i)
	{
		return equal_icase(i->GetModuleName(), ModuleName);
	});
	return ItemIterator == SortedPlugins.cend()? nullptr : *ItemIterator;
}

void PluginManager::LoadFactories()
{
	PluginFactories.emplace_back(std::make_unique<native_plugin_factory>(this));
#ifndef NO_WRAPPER
	if (Global->Opt->LoadPlug.OEMPluginsSupport)
		PluginFactories.emplace_back(CreateOemPluginFactory(this));
#endif // NO_WRAPPER

	ScanTree ScTree(false, true, Global->Opt->LoadPlug.ScanSymlinks);
	os::FAR_FIND_DATA FindData;
	ScTree.SetFindPath(Global->g_strFarPath + L"\\Adapters", L"*");

	string filename;
	while (ScTree.GetNextName(FindData, filename))
	{
		if (CmpName(L"*.dll", filename.data(), false) && !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			if (auto CustomModel = CreateCustomPluginFactory(this, filename))
			{
				PluginFactories.emplace_back(std::move(CustomModel));
			}
		}
	}
}

void PluginManager::LoadPlugins()
{
	SCOPED_ACTION(IndeterminateTaskBar)(false);
	m_PluginsLoaded = false;

	LoadFactories();

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

		// сначала подготовим список
		if (Global->Opt->LoadPlug.MainPluginDir) // только основные и персональные?
		{
			strPluginsDir=Global->g_strFarPath+PluginsFolderName;
			// ...а персональные есть?
			if (Global->Opt->LoadPlug.PluginsPersonal && !Global->Opt->LoadPlug.strPersonalPluginsPath.empty())
				append(strPluginsDir, L';', Global->Opt->LoadPlug.strPersonalPluginsPath);
		}
		else if (!Global->Opt->LoadPlug.strCustomPluginsPath.empty())  // только "заказные" пути?
		{
			strPluginsDir = Global->Opt->LoadPlug.strCustomPluginsPath;
		}

		// теперь пройдемся по всему ранее собранному списку
		for (const auto& i: split<std::vector<string>>(strPluginsDir, STLF_UNIQUE))
		{
			// расширяем значение пути
			strFullName = Unquote(os::env::expand_strings(i)); //??? здесь ХЗ

			if (!IsAbsolutePath(strFullName))
			{
				strPluginsDir = Global->g_strFarPath;
				strPluginsDir += strFullName;
				strFullName = strPluginsDir;
			}

			// Получим реальное значение полного длинного пути
			strFullName = ConvertNameToLong(ConvertNameToFull(strFullName));
			strPluginsDir = strFullName;

			// ставим на поток очередной путь из списка...
			ScTree.SetFindPath(strPluginsDir,L"*");

			// ...и пройдемся по нему
			while (ScTree.GetNextName(FindData,strFullName))
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

plugin_panel* PluginManager::OpenFilePlugin(const string* Name, OPERATION_MODES OpMode, OPENFILEPLUGINTYPE Type)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);

	// We're conditionally messing with the title down there.
	// However, we save & restore it unconditionally, as plugins could mess with it too.

	string OldTitle(ConsoleTitle::GetTitle());
	SCOPE_EXIT
	{
		// Invalidate cache
		ConsoleTitle::SetFarTitle({});
		// Set & flush
		ConsoleTitle::SetFarTitle(OldTitle, true);
	};

	if (Global->Opt->ShowCheckingFile)
	{
		OldTitle = ConsoleTitle::GetTitle();
		ConsoleTitle::SetFarTitle(msg(lng::MCheckingFileInPlugin), true);
	}
	plugin_panel* hResult = nullptr;

	using PluginInfo = std::pair<plugin_panel, HANDLE>;
	std::list<PluginInfo> items;

	string strFullName;

	if (Name)
	{
		strFullName = ConvertNameToFull(*Name);
		Name = &strFullName;
	}

	bool ShowMenu = Global->Opt->PluginConfirm.OpenFilePlugin==BSTATE_3STATE? !(Type == OFP_NORMAL || Type == OFP_SEARCH) : Global->Opt->PluginConfirm.OpenFilePlugin != 0;
	bool ShowWarning = OpMode == OPM_NONE;
	 //у анси плагинов OpMode нет.
	if(Type==OFP_ALTERNATIVE) OpMode|=OPM_PGDN;
	if(Type==OFP_COMMANDS) OpMode|=OPM_COMMANDS;

	AnalyseInfo Info={sizeof(Info), Name? Name->data() : nullptr, nullptr, 0, OpMode};
	std::vector<BYTE> Buffer(Global->Opt->PluginMaxReadData);

	bool DataRead = false;
	for (const auto& i: SortedPlugins)
	{
		if (!i->has(iOpenFilePlugin) && !(i->has(iAnalyse) && i->has(iOpen)))
			continue;

		if(Name && !DataRead)
		{
			if (const auto File = os::fs::file(*Name, FILE_READ_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN))
			{
				size_t DataSize = 0;
				if (File.Read(Buffer.data(), Buffer.size(), DataSize))
				{
					Info.Buffer = Buffer.data();
					Info.BufferSize = DataSize;
					DataRead = true;
				}
			}

			if(!DataRead)
			{
				if(ShowWarning)
				{
					Global->CatchError();
					Message(MSG_WARNING|MSG_ERRORTYPE,
						L"",
						{
							msg(lng::MOpenPluginCannotOpenFile),
							*Name
						},
						{ lng::MOk });
				}
				break;
			}
		}

		if (i->has(iOpenFilePlugin))
		{
			if (Global->Opt->ShowCheckingFile)
			{
				ConsoleTitle::SetFarTitle(concat(msg(lng::MCheckingFileInPlugin), L" - ["_sv, PointToName(i->GetModuleName()), L"]..."_sv), true);
			}

			const auto hPlugin = i->OpenFilePlugin(Name? Name->data() : nullptr, (BYTE*)Info.Buffer, Info.BufferSize, OpMode);

			if (hPlugin == PANEL_STOP)   //сразу на выход, плагин решил нагло обработать все сам (Autorun/PictureView)!!!
			{
				hResult = reinterpret_cast<plugin_panel*>(PANEL_STOP);
				break;
			}

			if (hPlugin)
			{
				items.emplace_back(plugin_panel{ i, hPlugin }, nullptr);
			}
		}
		else
		{
			if (const auto analyse = i->Analyse(&Info))
			{
				items.emplace_back(plugin_panel{ i, nullptr }, analyse);
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
			const auto menu = VMenu2::create(msg(lng::MPluginConfirmationTitle), nullptr, 0, ScrY - 4);
			menu->SetPosition(-1, -1, 0, 0);
			menu->SetHelp(L"ChoosePluginMenu");
			menu->SetMenuFlags(VMENU_SHOWAMPERSAND | VMENU_WRAPMODE);

			std::for_each(CONST_RANGE(items, i)
			{
				menu->AddItem(i.first.plugin()->GetTitle());
			});

			if (Global->Opt->PluginConfirm.StandardAssociation && Type == OFP_NORMAL)
			{
				MenuItemEx mitem;
				mitem.Flags |= MIF_SEPARATOR;
				menu->AddItem(mitem);
				menu->AddItem(msg(lng::MMenuPluginStdAssociation));
			}

			int ExitCode = menu->Run();
			if (ExitCode == -1)
				hResult = static_cast<plugin_panel*>(PANEL_STOP);
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

		if (pResult != items.end() && pResult->first.panel() == nullptr)
		{
			pAnalyse = pResult;
			OpenAnalyseInfo oainfo = { sizeof(OpenAnalyseInfo), &Info, pResult->second };

			OpenInfo oInfo = {sizeof(oInfo)};
			oInfo.OpenFrom = OPEN_ANALYSE;
			oInfo.Guid = &FarGuid;
			oInfo.Data = (intptr_t)&oainfo;

			HANDLE h = pResult->first.plugin()->Open(&oInfo);

			if (h == PANEL_STOP)
			{
				hResult = static_cast<plugin_panel*>(PANEL_STOP);
				pResult = items.end();
			}
			else if (h)
			{
				pResult->first.set_panel(h);
			}
			else
			{
				pResult = items.end();
			}
		}
	}

	FOR_CONST_RANGE(items, i)
	{
		if (i != pResult && i->first.panel())
		{
			ClosePanelInfo ci = {sizeof ci, i->first.panel() };
			i->first.plugin()->ClosePanel(&ci);
		}

		if (i != pAnalyse && i->second)
		{
			CloseAnalyseInfo ci = {sizeof ci, i->second };
			i->first.plugin()->CloseAnalyse(&ci);
		}
	}

	if (pResult != items.end())
	{
		hResult = std::make_unique<plugin_panel>(std::move(pResult->first)).release();
	}

	return hResult;
}

plugin_panel* PluginManager::OpenFindListPlugin(const PluginPanelItem *PanelItem, size_t ItemsNumber)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	std::list<plugin_panel> items;
	auto pResult = items.end();

	for (const auto& i: SortedPlugins)
	{
		if (!i->has(iSetFindList))
			continue;

		OpenInfo Info = {sizeof(Info)};
		Info.OpenFrom = OPEN_FINDLIST;
		Info.Guid = &FarGuid;
		Info.Data = 0;

		if (const auto hPlugin = i->Open(&Info))
		{
			items.emplace_back(i, hPlugin);
		}

		if (!items.empty() && !Global->Opt->PluginConfirm.SetFindList)
			break;
	}

	if (!items.empty())
	{
		if (items.size()>1)
		{
			const auto menu = VMenu2::create(msg(lng::MPluginConfirmationTitle), nullptr, 0, ScrY - 4);
			menu->SetPosition(-1, -1, 0, 0);
			menu->SetHelp(L"ChoosePluginMenu");
			menu->SetMenuFlags(VMENU_SHOWAMPERSAND | VMENU_WRAPMODE);

			std::for_each(CONST_RANGE(items, i)
			{
				menu->AddItem(i.plugin()->GetTitle());
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
		Info.hPanel = pResult->panel();
		Info.PanelItem = PanelItem;
		Info.ItemsNumber = ItemsNumber;

		if (!pResult->plugin()->SetFindList(&Info))
		{
			pResult = items.end();
		}
	}

	FOR_CONST_RANGE(items, i)
	{
		if (i!=pResult)
		{
			if (i->panel())
			{
				ClosePanelInfo Info = {sizeof(Info)};
				Info.hPanel = i->panel();
				i->plugin()->ClosePanel(&Info);
			}
		}
	}

	if (pResult != items.end())
	{
		return std::make_unique<plugin_panel>(std::move(*pResult)).release();
	}

	return nullptr;
}


void PluginManager::ClosePanel(const plugin_panel* hPlugin)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	ClosePanelInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	hPlugin->plugin()->ClosePanel(&Info);
	delete hPlugin;
}


int PluginManager::ProcessEditorInput(const INPUT_RECORD *Rec) const
{
	ProcessEditorInputInfo Info={sizeof(Info)};
	Info.Rec=*Rec;

	return std::any_of(CONST_RANGE(SortedPlugins, i) {return i->has(iProcessEditorInput) && i->ProcessEditorInput(&Info);});
}


int PluginManager::ProcessEditorEvent(int Event, void *Param, const Editor* EditorInstance) const
{
	int nResult = 0;
	if (const auto Container = EditorInstance->GetOwner())
	{
		if (Event == EE_REDRAW)
		{
			const auto FED = std::dynamic_pointer_cast<FileEditor>(Container).get();
			FED->AutoDeleteColors();
		}

		ProcessEditorEventInfo Info = {sizeof(Info)};
		Info.Event = Event;
		Info.Param = Param;
		Info.EditorID = EditorInstance->GetId();

		SCOPED_ACTION(auto)(Container->GetPinner());
		std::for_each(CONST_RANGE(SortedPlugins, i)
		{
			if (i->has(iProcessEditorEvent))
				nResult = i->ProcessEditorEvent(&Info);
		});
	}

	return nResult;
}


int PluginManager::ProcessSubscribedEditorEvent(int Event, void *Param, const Editor* EditorInstance, const std::unordered_set<GUID, uuid_hash, uuid_equal>& PluginIds) const
{
	int nResult = 0;
	if (const auto Container = EditorInstance->GetOwner())
	{
		ProcessEditorEventInfo Info = {sizeof(Info)};
		Info.Event = Event;
		Info.Param = Param;
		Info.EditorID = EditorInstance->GetId();

		SCOPED_ACTION(auto)(Container->GetPinner());
		std::for_each(CONST_RANGE(SortedPlugins, i)
		{
			if (i->has(iProcessEditorEvent) && PluginIds.count(i->GetGUID()))
			{
				nResult = i->ProcessEditorEvent(&Info);
			}
		});
	}

	return nResult;
}


int PluginManager::ProcessViewerEvent(int Event, void *Param, const Viewer* ViewerInstance) const
{
	int nResult = 0;
	if (const auto Container = ViewerInstance->GetOwner())
	{
		ProcessViewerEventInfo Info = {sizeof(Info)};
		Info.Event = Event;
		Info.Param = Param;
		Info.ViewerID = ViewerInstance->GetId();

		SCOPED_ACTION(auto)(Container->GetPinner());
		std::for_each(CONST_RANGE(SortedPlugins, i)
		{
			if (i->has(iProcessViewerEvent))
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

	return std::any_of(CONST_RANGE(SortedPlugins, i) {return i->has(iProcessDialogEvent) && i->ProcessDialogEvent(&Info);});
}

int PluginManager::ProcessConsoleInput(ProcessConsoleInputInfo *Info) const
{
	int nResult = 0;

	for (const auto& i: SortedPlugins)
	{
		if (i->has(iProcessConsoleInput))
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


int PluginManager::GetFindData(const plugin_panel* hPlugin, PluginPanelItem **pPanelData, size_t *pItemsNumber, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	GetFindDataInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.OpMode = OpMode;
	int Result = hPlugin->plugin()->GetFindData(&Info);
	*pPanelData = Info.PanelItem;
	*pItemsNumber = Info.ItemsNumber;
	return Result;
}


void PluginManager::FreeFindData(const plugin_panel* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, bool FreeUserData)
{
	if (FreeUserData)
		FreePluginPanelItemsUserData(const_cast<plugin_panel*>(hPlugin),PanelItem,ItemsNumber);

	FreeFindDataInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	hPlugin->plugin()->FreeFindData(&Info);
}


int PluginManager::GetVirtualFindData(const plugin_panel* hPlugin, PluginPanelItem **pPanelData, size_t *pItemsNumber, const string& Path)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	*pItemsNumber=0;

	GetVirtualFindDataInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.Path = Path.data();
	int Result = hPlugin->plugin()->GetVirtualFindData(&Info);
	*pPanelData = Info.PanelItem;
	*pItemsNumber = Info.ItemsNumber;
	return Result;
}


void PluginManager::FreeVirtualFindData(const plugin_panel* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber)
{
	FreeFindDataInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	return hPlugin->plugin()->FreeVirtualFindData(&Info);
}


int PluginManager::SetDirectory(const plugin_panel* hPlugin, const string& Dir, int OpMode, const UserDataItem *UserData)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SetDirectoryInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.Dir = Dir.data();
	Info.OpMode = OpMode;
	if (UserData)
	{
		Info.UserData = *UserData;
	}
	return hPlugin->plugin()->SetDirectory(&Info);
}


int PluginManager::GetFile(const plugin_panel* hPlugin, PluginPanelItem *PanelItem, const string& DestPath, string &strResultName, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	std::unique_ptr<SaveScreen> SaveScr;
	int Found=FALSE;
	Global->KeepUserScreen=FALSE;

	if (!(OpMode & OPM_FIND))
		SaveScr = std::make_unique<SaveScreen>(); //???

	SCOPED_ACTION(UndoGlobalSaveScrPtr)(SaveScr.get());

	GetFilesInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = 1;
	Info.Move = 0;
	Info.DestPath = DestPath.data();
	Info.OpMode = OpMode;

	int GetCode = hPlugin->plugin()->GetFiles(&Info);

	string strFindPath = Info.DestPath;
	AddEndSlash(strFindPath);
	strFindPath += L'*';
	const auto Find = os::fs::enum_files(strFindPath);
	const auto ItemIterator = std::find_if(CONST_RANGE(Find, i) { return !(i.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); });
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


int PluginManager::DeleteFiles(const plugin_panel* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SaveScreen SaveScr;
	Global->KeepUserScreen=FALSE;

	DeleteFilesInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	Info.OpMode = OpMode;

	int Code = hPlugin->plugin()->DeleteFiles(&Info);

	ReadUserBackgound(&SaveScr);

	return Code;
}


int PluginManager::MakeDirectory(const plugin_panel* hPlugin, const wchar_t **Name, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SaveScreen SaveScr;
	Global->KeepUserScreen=FALSE;

	MakeDirectoryInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.Name = *Name;
	Info.OpMode = OpMode;

	int Code = hPlugin->plugin()->MakeDirectory(&Info);

	*Name = Info.Name;

	ReadUserBackgound(&SaveScr);

	return Code;
}


int PluginManager::ProcessHostFile(const plugin_panel* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SaveScreen SaveScr;
	Global->KeepUserScreen=FALSE;

	ProcessHostFileInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	Info.OpMode = OpMode;

	int Code = hPlugin->plugin()->ProcessHostFile(&Info);

	ReadUserBackgound(&SaveScr);

	return Code;
}


int PluginManager::GetFiles(const plugin_panel* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, bool Move, const wchar_t **DestPath, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);

	GetFilesInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	Info.Move = Move;
	Info.DestPath = *DestPath;
	Info.OpMode = OpMode;

	int Result = hPlugin->plugin()->GetFiles(&Info);
	*DestPath = Info.DestPath;
	return Result;
}


int PluginManager::PutFiles(const plugin_panel* hPlugin, PluginPanelItem *PanelItem, size_t ItemsNumber, bool Move, int OpMode)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	SaveScreen SaveScr;
	Global->KeepUserScreen=FALSE;

	static string strCurrentDirectory;
	strCurrentDirectory = os::GetCurrentDirectory();
	PutFilesInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.PanelItem = PanelItem;
	Info.ItemsNumber = ItemsNumber;
	Info.Move = Move;
	Info.SrcPath = strCurrentDirectory.data();
	Info.OpMode = OpMode;

	int Code = hPlugin->plugin()->PutFiles(&Info);

	ReadUserBackgound(&SaveScr);

	return Code;
}

void PluginManager::GetOpenPanelInfo(const plugin_panel* hPlugin, OpenPanelInfo *Info)
{
	if (!Info)
		return;

	*Info = {};

	Info->StructSize = sizeof(OpenPanelInfo);
	Info->hPanel = hPlugin->panel();
	hPlugin->plugin()->GetOpenPanelInfo(Info);

	if (Info->CurDir && *Info->CurDir && (Info->Flags & OPIF_REALNAMES) && (Global->CtrlObject->Cp()->ActivePanel()->GetPluginHandle() == hPlugin) && ParsePath(Info->CurDir)!=PATH_UNKNOWN)
		os::SetCurrentDirectory(Info->CurDir, false);
}


int PluginManager::ProcessKey(const plugin_panel* hPlugin,const INPUT_RECORD *Rec, bool Pred)
{

	ProcessPanelInputInfo Info={sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.Rec=*Rec;

#ifndef NO_WRAPPER
	if (Pred && hPlugin->plugin()->IsOemPlugin())
		Info.Rec.EventType |= 0x4000;
#endif
	return hPlugin->plugin()->ProcessPanelInput(&Info);
}


int PluginManager::ProcessEvent(const plugin_panel* hPlugin, int Event, void *Param)
{
	ProcessPanelEventInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.Event = Event;
	Info.Param = Param;

	return hPlugin->plugin()->ProcessPanelEvent(&Info);
}


int PluginManager::Compare(const plugin_panel* hPlugin, const PluginPanelItem *Item1, const PluginPanelItem *Item2, unsigned int Mode)
{
	CompareInfo Info = {sizeof(Info)};
	Info.hPanel = hPlugin->panel();
	Info.Item1 = Item1;
	Info.Item2 = Item2;
	Info.Mode = static_cast<OPENPANELINFO_SORTMODES>(Mode);

	return hPlugin->plugin()->Compare(&Info);
}

void PluginManager::ConfigureCurrent(Plugin *pPlugin, const GUID& Guid)
{
	ConfigureInfo Info = {sizeof(Info)};
	Info.Guid = &Guid;

	if (pPlugin->Configure(&Info))
	{
		panel_ptr Panels[] =
		{
			Global->CtrlObject->Cp()->LeftPanel(),
			Global->CtrlObject->Cp()->RightPanel(),
		};

		std::for_each(CONST_RANGE(Panels, i)
		{
			if (i->GetMode() == panel_mode::PLUGIN_PANEL)
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

static string AddHotkey(const string& Item, wchar_t Hotkey)
{
	return concat(!Hotkey?  L" "s : Hotkey == L'&'? L"&&&"s : L"&"s + Hotkey, L"  ", Item);
}

/* $ 29.05.2001 IS
   ! При настройке "параметров внешних модулей" закрывать окно с их
     списком только при нажатии на ESC
*/
void PluginManager::Configure(int StartPos)
{
		const auto PluginList = VMenu2::create(msg(lng::MPluginConfigTitle), nullptr, 0, ScrY - 4);
		PluginList->SetMenuFlags(VMENU_WRAPMODE);
		PluginList->SetHelp(L"PluginsConfig");
		PluginList->SetId(PluginsConfigMenuId);

		while (!Global->CloseFAR)
		{
			bool NeedUpdateItems = true;
			bool HotKeysPresent = ConfigProvider().PlHotkeyCfg()->HotkeysPresent(hotkey_type::config_menu);

			if (NeedUpdateItems)
			{
				PluginList->clear();
				LoadIfCacheAbsent();
				string strName;
				GUID guid;

				for (const auto& i: SortedPlugins)
				{
					bool bCached = i->CheckWorkFlags(PIWF_CACHED);
					unsigned long long id = 0;

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
							if (!ConfigProvider().PlCacheCfg()->GetPluginsConfigMenuItem(id, J, strName, guid))
								break;
						}
						else
						{
							if (J >= Info.PluginConfig.Count)
								break;

							strName = NullToEmpty(Info.PluginConfig.Strings[J]);
							guid = Info.PluginConfig.Guids[J];
						}

						const auto Hotkey = GetPluginHotKey(i, guid, hotkey_type::config_menu);
						MenuItemEx ListItem;
#ifndef NO_WRAPPER
						if (i->IsOemPlugin())
							ListItem.Flags=LIF_CHECKED|L'A';
#endif // NO_WRAPPER
						if (!HotKeysPresent)
							ListItem.strName = strName;
						else
							ListItem.strName = AddHotkey(strName, Hotkey);

						PluginMenuItemData item = { i, guid };

						ListItem.UserData = item;

						PluginList->AddItem(ListItem);
					}
				}

				PluginList->AssignHighlights(FALSE);
				PluginList->SetBottomTitle(msg(lng::MPluginHotKeyBottom));
				PluginList->SortItems(false, HotKeysPresent? 3 : 0);
				PluginList->SetSelectPos(StartPos,1);
				NeedUpdateItems = false;
			}

			string strPluginModuleName;

			PluginList->Run([&](const Manager::Key& RawKey)
			{
				const auto Key=RawKey();
				int SelPos=PluginList->GetSelectPos();
				const auto item = PluginList->GetUserDataPtr<PluginMenuItemData>(SelPos);
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
							strTitle = PluginList->current().strName.substr(nOffset);
							RemoveExternalSpaces(strTitle);

							if (SetHotKeyDialog(item->pPlugin, item->Guid, hotkey_type::config_menu, strTitle))
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

				const auto item = PluginList->GetUserDataPtr<PluginMenuItemData>(StartPos);
				ConfigureCurrent(item->pPlugin, item->Guid);
			}
		}
}

int PluginManager::CommandsMenu(int ModalType,int StartPos,const wchar_t *HistoryName)
{
	if (ModalType == windowtype_dialog || ModalType == windowtype_menu)
	{
		const auto dlg = std::static_pointer_cast<Dialog>(Global->WindowManager->GetCurrentWindow());
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
		const auto PluginList = VMenu2::create(msg(lng::MPluginCommandsMenuTitle), nullptr, 0, ScrY - 4);
		PluginList->SetMenuFlags(VMENU_WRAPMODE);
		PluginList->SetHelp(L"PluginCommands");
		PluginList->SetId(PluginsMenuId);
		bool NeedUpdateItems = true;

		while (NeedUpdateItems)
		{
			bool HotKeysPresent = ConfigProvider().PlHotkeyCfg()->HotkeysPresent(hotkey_type::plugins_menu);

			if (NeedUpdateItems)
			{
				PluginList->clear();
				LoadIfCacheAbsent();
				string strName;
				GUID guid;

				for (const auto& i: SortedPlugins)
				{
					bool bCached = i->CheckWorkFlags(PIWF_CACHED);
					unsigned long long IFlags;
					unsigned long long id = 0;

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
							if (!ConfigProvider().PlCacheCfg()->GetPluginsMenuItem(id, J, strName, guid))
								break;
						}
						else
						{
							if (J >= Info.PluginMenu.Count)
								break;

							strName = NullToEmpty(Info.PluginMenu.Strings[J]);
							guid = Info.PluginMenu.Guids[J];
						}

						const auto Hotkey = GetPluginHotKey(i, guid, hotkey_type::plugins_menu);
						MenuItemEx ListItem;
#ifndef NO_WRAPPER
						if (i->IsOemPlugin())
							ListItem.Flags=LIF_CHECKED|L'A';
#endif // NO_WRAPPER
						if (!HotKeysPresent)
							ListItem.strName = strName;
						else
							ListItem.strName = AddHotkey(strName, Hotkey);

						PluginMenuItemData itemdata;
						itemdata.pPlugin = i;
						itemdata.Guid = guid;

						ListItem.UserData = itemdata;

						PluginList->AddItem(ListItem);
					}
				}

				PluginList->AssignHighlights(FALSE);
				PluginList->SetBottomTitle(msg(lng::MPluginHotKeyBottom));
				PluginList->SortItems(false, HotKeysPresent? 3 : 0);
				PluginList->SetSelectPos(StartPos,1);
				NeedUpdateItems = false;
			}

			PluginList->Run([&](const Manager::Key& RawKey)
			{
				const auto Key=RawKey();
				int SelPos=PluginList->GetSelectPos();
				const auto ItemPtr = PluginList->GetUserDataPtr<PluginMenuItemData>(SelPos);
				int KeyProcessed = 1;

				switch (Key)
				{
					case KEY_SHIFTF1:
						// Вызываем нужный топик, который передали в CommandsMenu()
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
							strTitle = PluginList->current().strName.substr(nOffset);
							RemoveExternalSpaces(strTitle);

							if (SetHotKeyDialog(ItemPtr->pPlugin, ItemPtr->Guid, hotkey_type::plugins_menu, strTitle))
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

							if (ItemPtr->pPlugin->has(iConfigure))
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
		item = *PluginList->GetUserDataPtr<PluginMenuItemData>(ExitCode);
	}

	const auto ActivePanel = Global->CtrlObject->Cp()->ActivePanel();
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

	const auto hPlugin = Open(item.pPlugin, OpenCode, item.Guid, Item);

	if (hPlugin && !Editor && !Viewer && !Dialog)
	{
		if (ActivePanel->ProcessPluginEvent(FE_CLOSE,nullptr))
		{
			ClosePanel(hPlugin);
			return FALSE;
		}

		const auto NewPanel = Global->CtrlObject->Cp()->ChangePanel(ActivePanel, panel_type::FILE_PANEL, TRUE, TRUE);
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

bool PluginManager::SetHotKeyDialog(Plugin *pPlugin, const GUID& Guid, hotkey_type HotKeyType, const string& DlgPluginTitle)
{
	string strPluginKey;
	GetHotKeyPluginKey(pPlugin, strPluginKey);
	string strHotKey = ConfigProvider().PlHotkeyCfg()->GetHotkey(strPluginKey, Guid, HotKeyType);

	DialogBuilder Builder(lng::MPluginHotKeyTitle, L"SetHotKeyDialog");
	Builder.AddText(lng::MPluginHotKey);
	Builder.AddTextAfter(Builder.AddFixEditField(strHotKey, 1), DlgPluginTitle.data());
	Builder.AddOKCancel();
	if(Builder.ShowDialog())
	{
		if (!strHotKey.empty() && strHotKey.front() != L' ')
			ConfigProvider().PlHotkeyCfg()->SetHotkey(strPluginKey, Guid, HotKeyType, strHotKey);
		else
			ConfigProvider().PlHotkeyCfg()->DelHotkey(strPluginKey, Guid, HotKeyType);
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
		unsigned long long id = ConfigProvider().PlCacheCfg()->GetCacheID(pPlugin->GetCacheName());
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
	DialogBuilder Builder(lng::MPluginInformation, L"ShowPluginInfo");
	Builder.AddText(lng::MPluginModuleTitle);
	Builder.AddConstEditField(pPlugin->GetTitle(), Width);
	Builder.AddText(lng::MPluginDescription);
	Builder.AddConstEditField(pPlugin->GetDescription(), Width);
	Builder.AddText(lng::MPluginAuthor);
	Builder.AddConstEditField(pPlugin->GetAuthor(), Width);
	Builder.AddText(lng::MPluginVersion);
	Builder.AddConstEditField(pPlugin->GetVersionString(), Width);
	Builder.AddText(lng::MPluginModulePath);
	Builder.AddConstEditField(pPlugin->GetModuleName(), Width);
	Builder.AddText(lng::MPluginGUID);
	Builder.AddConstEditField(strPluginGuid, Width);
	Builder.AddText(lng::MPluginItemGUID);
	Builder.AddConstEditField(strItemGuid, Width);
	Builder.AddText(lng::MPluginPrefix);
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
	const auto Res = reinterpret_cast<wchar_t*>(BufReserve(Buf, Count, Rest, Size));
	if (Res)
	{
		wcscpy(Res, Str.data());
	}
	return Res;
}


void ItemsToBuf(PluginMenuItem& Menu, const std::vector<string>& NamesArray, const std::vector<GUID>& GuidsArray, char*& Buf, size_t& Rest, size_t& Size)
{
	Menu.Count = NamesArray.size();
	Menu.Strings = nullptr;
	Menu.Guids = nullptr;

	if (Menu.Count)
	{
		const auto Items = reinterpret_cast<wchar_t**>(BufReserve(Buf, Menu.Count * sizeof(wchar_t*), Rest, Size));
		const auto Guids = reinterpret_cast<GUID*>(BufReserve(Buf, Menu.Count * sizeof(GUID), Rest, Size));
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
				Guids[i] = GuidsArray[i];
			}
		}
	}
}

size_t PluginManager::GetPluginInformation(Plugin *pPlugin, FarGetPluginInformation *pInfo, size_t BufferSize)
{
	if(IsPluginUnloaded(pPlugin)) return 0;
	string Prefix;
	PLUGIN_FLAGS Flags = 0;

	using menu_items = std::pair<std::vector<string>, std::vector<GUID>>;
	menu_items MenuItems, DiskItems, ConfItems;

	if (pPlugin->CheckWorkFlags(PIWF_CACHED))
	{
		unsigned long long id = ConfigProvider().PlCacheCfg()->GetCacheID(pPlugin->GetCacheName());
		Flags = ConfigProvider().PlCacheCfg()->GetFlags(id);
		Prefix = ConfigProvider().PlCacheCfg()->GetCommandPrefix(id);

		string Name;
		GUID Guid;

		const auto& ReadCache = [&](const auto& Getter, auto& Items)
		{
			for (size_t i = 0; std::invoke(Getter, ConfigProvider().PlCacheCfg(), id, i, Name, Guid); ++i)
			{
				Items.first.emplace_back(Name);
				Items.second.emplace_back(Guid);
			}
		};

		ReadCache(&PluginsCacheConfig::GetPluginsMenuItem, MenuItems);
		ReadCache(&PluginsCacheConfig::GetDiskMenuItem, DiskItems);
		ReadCache(&PluginsCacheConfig::GetPluginsConfigMenuItem, ConfItems);
	}
	else
	{
		PluginInfo Info = {sizeof(Info)};
		if (pPlugin->GetPluginInfo(&Info))
		{
			Flags = Info.Flags;
			Prefix = NullToEmpty(Info.CommandPrefix);

			const auto& CopyData = [](const PluginMenuItem& Item, menu_items& Items)
			{
				for (size_t i = 0; i < Item.Count; i++)
				{
					Items.first.emplace_back(Item.Strings[i]);
					Items.second.emplace_back(Item.Guids[i]);
				}
			};

			CopyData(Info.PluginMenu, MenuItems);
			CopyData(Info.DiskMenu, DiskItems);
			CopyData(Info.PluginConfig, ConfItems);
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

	ItemsToBuf(pInfo->PInfo->DiskMenu, DiskItems.first, DiskItems.second, Buffer, Rest, Size);
	ItemsToBuf(pInfo->PInfo->PluginMenu, MenuItems.first, MenuItems.second, Buffer, Rest, Size);
	ItemsToBuf(pInfo->PInfo->PluginConfig, ConfItems.first, ConfItems.second, Buffer, Rest, Size);

	return Size;
}

bool PluginManager::GetDiskMenuItem(Plugin *pPlugin, size_t PluginItem, bool &ItemPresent, wchar_t& PluginHotkey, string &strPluginText, GUID &Guid) const
{
	LoadIfCacheAbsent();

	ItemPresent = false;

	if (pPlugin->CheckWorkFlags(PIWF_CACHED))
	{
		ItemPresent = ConfigProvider().PlCacheCfg()->GetDiskMenuItem(ConfigProvider().PlCacheCfg()->GetCacheID(pPlugin->GetCacheName()), PluginItem, strPluginText, Guid) && !strPluginText.empty();
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
		PluginHotkey = GetPluginHotKey(pPlugin, Guid, hotkey_type::drive_menu);
	}

	return true;
}

int PluginManager::UseFarCommand(plugin_panel* hPlugin,int CommandType)
{
	OpenPanelInfo Info;
	GetOpenPanelInfo(hPlugin,&Info);

	if (!(Info.Flags & OPIF_REALNAMES))
		return FALSE;

	switch (CommandType)
	{
		case PLUGIN_FARGETFILE:
		case PLUGIN_FARGETFILES:
			return !hPlugin->plugin()->has(iGetFiles) || (Info.Flags & OPIF_EXTERNALGET);
		case PLUGIN_FARPUTFILES:
			return !hPlugin->plugin()->has(iPutFiles) || (Info.Flags & OPIF_EXTERNALPUT);
		case PLUGIN_FARDELETEFILES:
			return !hPlugin->plugin()->has(iDeleteFiles) || (Info.Flags & OPIF_EXTERNALDELETE);
		case PLUGIN_FARMAKEDIRECTORY:
			return !hPlugin->plugin()->has(iMakeDirectory) || (Info.Flags & OPIF_EXTERNALMKDIR);
	}

	return TRUE;
}


void PluginManager::ReloadLanguage() const
{
	std::for_each(ALL_CONST_RANGE(SortedPlugins), std::mem_fn(&Plugin::CloseLang));
	ConfigProvider().PlCacheCfg()->DiscardCache();
}

void PluginManager::LoadIfCacheAbsent() const
{
	if (ConfigProvider().PlCacheCfg()->IsCacheEmpty())
	{
		std::for_each(ALL_CONST_RANGE(SortedPlugins), std::mem_fn(&Plugin::Load));
	}
}

//template parameters must have external linkage
struct PluginData
{
	Plugin* pPlugin;
	unsigned long long PluginFlags;
};

int PluginManager::ProcessCommandLine(const string& CommandParam, panel_ptr Target)
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

	for (const auto& i: SortedPlugins)
	{
		unsigned long long PluginFlags = 0;

		if (i->CheckWorkFlags(PIWF_CACHED))
		{
			unsigned long long id = ConfigProvider().PlCacheCfg()->GetCacheID(i->GetCacheName());
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

			if (starts_with_icase(strPrefix, string_view(PrStart, Len)))
			{
				if (i->Load() && i->has(iOpen))
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

	const auto ActivePanel = Global->CtrlObject->Cp()->ActivePanel();
	const auto CurPanel = Target? Target : ActivePanel;

	if (CurPanel->ProcessPluginEvent(FE_CLOSE,nullptr))
		return FALSE;

	auto PData = items.begin();

	if (items.size()>1)
	{
		const auto menu = VMenu2::create(msg(lng::MPluginConfirmationTitle), nullptr, 0, ScrY - 4);
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
	const auto hPlugin = Open(PData->pPlugin, OPEN_COMMANDLINE, FarGuid, (intptr_t)&info);

	if (hPlugin)
	{
		const auto NewPanel = Global->CtrlObject->Cp()->ChangePanel(CurPanel, panel_type::FILE_PANEL, TRUE, TRUE);
		NewPanel->SetPluginMode(hPlugin,L"",!Target || Target == ActivePanel);
		NewPanel->Update(0);
		NewPanel->Show();
	}

	return TRUE;
}


/* $ 27.09.2000 SVS
  Функция CallPlugin - найти плагин по ID и запустить
  в зачаточном состоянии!
*/
int PluginManager::CallPlugin(const GUID& SysID,int OpenFrom, void *Data,void **Ret)
{
	if (const auto Dlg = std::dynamic_pointer_cast<Dialog>(Global->WindowManager->GetCurrentWindow()))
	{
		if (Dlg->CheckDialogMode(DMODE_NOPLUGINS))
		{
			return FALSE;
		}
	}

	Plugin *pPlugin = FindPlugin(SysID);

	if (pPlugin)
	{
		if (pPlugin->has(iOpen) && !Global->ProcessException)
		{
			const auto hNewPlugin = Open(pPlugin, OpenFrom, FarGuid, (intptr_t)Data);
			bool process=false;

			if (OpenFrom == OPEN_FROMMACRO)
			{
				if (hNewPlugin)
				{
					if (os::memory::is_pointer(hNewPlugin->panel()) && hNewPlugin->panel() != INVALID_HANDLE_VALUE)
					{
						const auto fmc = reinterpret_cast<FarMacroCall*>(hNewPlugin->panel());
						if (fmc->Count > 0 && fmc->Values[0].Type == FMVT_PANEL)
						{
							process = true;
							hNewPlugin->set_panel(fmc->Values[0].Pointer);
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
				const auto NewPanel = Global->CtrlObject->Cp()->ChangePanel(Global->CtrlObject->Cp()->ActivePanel(), panel_type::FILE_PANEL, TRUE, TRUE);
				NewPanel->SetPluginMode(hNewPlugin,L"", true);
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
				const auto handle = reinterpret_cast<plugin_panel*>(hNewPlugin);
				if (OpenFrom == OPEN_FROMMACRO && process)
					*Ret = ToPtr(1);
				else
				{
					*Ret = hNewPlugin? handle->panel(): nullptr;
					delete handle;
				}
			}

			return TRUE;
		}
	}
	return FALSE;
}

// поддержка макрофункций plugin.call, plugin.cmd, plugin.config и т.п
bool PluginManager::CallPluginItem(const GUID& Guid, CallPluginInfo *Data)
{
	auto Result = false;

	if (Global->ProcessException)
		return false;

	const auto curType = Global->WindowManager->GetCurrentWindow()->GetType();

	if (curType==windowtype_dialog && std::static_pointer_cast<Dialog>(Global->WindowManager->GetCurrentWindow())->CheckDialogMode(DMODE_NOPLUGINS))
		return false;

	const auto Editor = curType == windowtype_editor;
	const auto Viewer = curType == windowtype_viewer;
	const auto Dialog = curType == windowtype_dialog;

	if (Data->CallFlags & CPT_CHECKONLY)
	{
		Data->pPlugin = FindPlugin(Guid);
		if (!Data->pPlugin || !Data->pPlugin->Load())
			return false;

		// Разрешен ли вызов данного типа в текущей области (предварительная проверка)
		switch (Data->CallFlags & CPT_MASK)
		{
		case CPT_MENU:
			if (!Data->pPlugin->has(iOpen))
				return false;
			break;

		case CPT_CONFIGURE:
			//TODO: Автокомплит не влияет?
			if (curType!=windowtype_panels)
				return false;

			if (!Data->pPlugin->has(iConfigure))
				return false;
			break;

		case CPT_CMDLINE:
			//TODO: Автокомплит не влияет?
			if (curType!=windowtype_panels)
				return false;

			//TODO: OpenPanel или OpenFilePlugin?
			if (!Data->pPlugin->has(iOpen))
				return false;
			break;

		case CPT_INTERNAL:
			//TODO: Уточнить функцию
			if (!Data->pPlugin->has(iOpen))
				return false;
			break;

		default:
			break;
		}

		PluginInfo Info{sizeof(Info)};
		if (!Data->pPlugin->GetPluginInfo(&Info))
			return false;

		auto IFlags = Info.Flags;
		PluginMenuItem* MenuItems = nullptr;

		// Разрешен ли вызов данного типа в текущей области
		switch (Data->CallFlags & CPT_MASK)
		{
		case CPT_MENU:
			if (
				(Editor && !(IFlags & PF_EDITOR)) ||
				(Viewer && !(IFlags & PF_VIEWER)) ||
				(Dialog && !(IFlags & PF_DIALOG)) ||
				(!Editor && !Viewer && !Dialog && (IFlags & PF_DISABLEPANELS)))
				return false;

			MenuItems = &Info.PluginMenu;
			break;

		case CPT_CONFIGURE:
			MenuItems = &Info.PluginConfig;
			break;

		case CPT_CMDLINE:
			if (!Info.CommandPrefix || !*Info.CommandPrefix)
				return false;
			break;

		case CPT_INTERNAL:
			break;

		default:
			break;
		}

		if ((Data->CallFlags & CPT_MASK)==CPT_MENU || (Data->CallFlags & CPT_MASK)==CPT_CONFIGURE)
		{
			auto ItemFound = false;
			if (!Data->ItemGuid)
			{
				if (MenuItems->Count == 1)
				{
					Data->FoundGuid = MenuItems->Guids[0];
					Data->ItemGuid = &Data->FoundGuid;
					ItemFound = true;
				}
			}
			else
			{
				if (contains(make_range(MenuItems->Guids, MenuItems->Count), *Data->ItemGuid))
				{
					Data->FoundGuid = *Data->ItemGuid;
					Data->ItemGuid = &Data->FoundGuid;
					ItemFound = true;
				}
			}
			if (!ItemFound)
				return false;
		}

		return true;
	}

	if (!Data->pPlugin)
		return false;

	plugin_panel* hPlugin = nullptr;
	panel_ptr ActivePanel;

	switch (Data->CallFlags & CPT_MASK)
	{
	case CPT_MENU:
		{
			ActivePanel = Global->CtrlObject->Cp()->ActivePanel();
			auto OpenCode = OPEN_PLUGINSMENU;
			intptr_t Item = 0;
			OpenDlgPluginData pd { sizeof(pd) };

			if (Editor)
			{
				OpenCode = OPEN_EDITOR;
			}
			else if (Viewer)
			{
				OpenCode = OPEN_VIEWER;
			}
			else if (Dialog)
			{
				OpenCode = OPEN_DIALOG;
				pd.hDlg = static_cast<HANDLE>(Global->WindowManager->GetCurrentWindow().get());
				Item = reinterpret_cast<intptr_t>(&pd);
			}

			hPlugin=Open(Data->pPlugin,OpenCode,Data->FoundGuid,Item);
			Result = true;
		}
		break;

	case CPT_CONFIGURE:
		Global->CtrlObject->Plugins->ConfigureCurrent(Data->pPlugin,Data->FoundGuid);
		return true;

	case CPT_CMDLINE:
		{
			ActivePanel=Global->CtrlObject->Cp()->ActivePanel();
			string command=Data->Command; // Нужна копия строки
			OpenCommandLineInfo info={sizeof(OpenCommandLineInfo),command.data()};
			hPlugin=Open(Data->pPlugin,OPEN_COMMANDLINE,FarGuid,(intptr_t)&info);
			Result = true;
		}
		break;

	case CPT_INTERNAL:
		//TODO: бывший CallPlugin
		//WARNING: учесть, что он срабатывает без переключения MacroState
		break;

	default:
		break;
	}

	if (hPlugin && !Editor && !Viewer && !Dialog)
	{
		//BUGBUG: Закрытие панели? Нужно ли оно?
		//BUGBUG: В ProcessCommandLine зовется перед Open, а в CPT_MENU - после
		if (ActivePanel->ProcessPluginEvent(FE_CLOSE, nullptr))
		{
			ClosePanel(hPlugin);
			return false;
		}

		const auto NewPanel = Global->CtrlObject->Cp()->ChangePanel(ActivePanel, panel_type::FILE_PANEL, TRUE, TRUE);
		NewPanel->SetPluginMode(hPlugin, L"", true);
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

	return Result;
}

Plugin *PluginManager::FindPlugin(const GUID& SysID) const
{
	const auto Iterator = m_Plugins.find(SysID);
	return Iterator == m_Plugins.cend()? nullptr : Iterator->second.get();
}

plugin_panel* PluginManager::Open(Plugin *pPlugin,int OpenFrom,const GUID& Guid,intptr_t Item)
{
	OpenInfo Info = {sizeof(Info)};
	Info.OpenFrom = static_cast<OPENFROM>(OpenFrom);
	Info.Guid = &Guid;
	Info.Data = Item;

	if (const auto hPlugin = pPlugin->Open(&Info))
	{
		const auto handle = new plugin_panel(pPlugin, hPlugin);
		return handle;
	}

	return nullptr;
}

std::vector<Plugin*> PluginManager::GetContentPlugins(const std::vector<const wchar_t*>& ColNames) const
{
	GetContentFieldsInfo Info = { sizeof(GetContentFieldsInfo), ColNames.size(), ColNames.data() };
	std::vector<Plugin*> Plugins;
	std::copy_if(ALL_CONST_RANGE(SortedPlugins), std::back_inserter(Plugins), [&Info](Plugin* p)
	{
		return p->has(iGetContentData) && p->has(iGetContentFields) && p->GetContentFields(&Info);
	});
	return Plugins;
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

			if (i->has(iFreeContentData))
			{
				i->FreeContentData(&GetInfo);
			}
		}
	});
}

const GUID& PluginManager::GetGUID(const plugin_panel* hPlugin)
{
	return hPlugin->plugin()->GetGUID();
}

void PluginManager::RefreshPluginsList()
{
	if(!UnloadedPlugins.empty())
	{
		UnloadedPlugins.remove_if([this](const auto& i)
		{
			if (!i->Active())
			{
				i->Unload(true);
				// gcc bug, this-> required
				this->RemovePlugin(i);
				return true;
			}
			return false;
		});
	}
}

void PluginManager::UndoRemove(Plugin* plugin)
{
	const auto i = std::find(UnloadedPlugins.begin(), UnloadedPlugins.end(), plugin);
	if(i != UnloadedPlugins.end())
		UnloadedPlugins.erase(i);
}

plugin_panel::plugin_panel(Plugin* PluginInstance, HANDLE Panel):
	m_Plugin(PluginInstance),
	m_PluginActivity(m_Plugin->keep_activity()),
	m_Panel(Panel)
{
}

plugin_panel::~plugin_panel() = default;

void plugin_panel::delayed_delete(const string& Name)
{
	m_DelayedDeleters.emplace_back(Name);
}
