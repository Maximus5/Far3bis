/*
options.cpp

��������� �������������� ���� (����� hmenu.cpp � ����������� �����������)
*/
/*
Copyright (c) 1996 Eugene Roshal
Copyright (c) 2000 Far Group
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

#include "options.hpp"
#include "lang.hpp"
#include "keys.hpp"
#include "hmenu.hpp"
#include "vmenu.hpp"
#include "filepanels.hpp"
#include "panel.hpp"
#include "chgmmode.hpp"
#include "filelist.hpp"
#include "hilight.hpp"
#include "cmdline.hpp"
#include "manager.hpp"
#include "ctrlobj.hpp"
#include "history.hpp"
#include "imports.hpp"
#include "message.hpp"
#include "hotplug.hpp"
#include "config.hpp"
#include "usermenu.hpp"
#include "datetime.hpp"
#include "setcolor.hpp"
#include "plist.hpp"
#include "filetype.hpp"
#include "ffolders.hpp"
#include "strmix.hpp"

enum enumMenus {
	MENU_LEFT,
	MENU_FILES,
	MENU_COMMANDS,
	MENU_OPTIONS,
	MENU_RIGHT
};

enum enumLeftMenu {
	MENU_LEFT_BRIEFVIEW,
	MENU_LEFT_MEDIUMVIEW,
	MENU_LEFT_FULLVIEW,
	MENU_LEFT_WIDEVIEW,
	MENU_LEFT_DETAILEDVIEW,
	MENU_LEFT_DIZVIEW,
	MENU_LEFT_LONGVIEW,
	MENU_LEFT_OWNERSVIEW,
	MENU_LEFT_LINKSVIEW,
	MENU_LEFT_ALTERNATIVEVIEW,
	MENU_LEFT_INFOPANEL = MENU_LEFT_ALTERNATIVEVIEW+2,
	MENU_LEFT_TREEPANEL,
	MENU_LEFT_QUICKVIEW,
	MENU_LEFT_SORTMODES = MENU_LEFT_QUICKVIEW+2,
	MENU_LEFT_LONGNAMES,
	MENU_LEFT_TOGGLEPANEL,
	MENU_LEFT_REREAD,
	MENU_LEFT_CHANGEDRIVE
};

//currently left == right

enum enumFilesMenu {
	MENU_FILES_VIEW,
	MENU_FILES_EDIT,
	MENU_FILES_COPY,
	MENU_FILES_MOVE,
	MENU_FILES_CREATEFOLDER,
	MENU_FILES_DELETE,
	MENU_FILES_WIPE,
	MENU_FILES_ADD = MENU_FILES_WIPE+2,
	MENU_FILES_EXTRACT,
	MENU_FILES_ARCHIVECOMMANDS,
	MENU_FILES_ATTRIBUTES = MENU_FILES_ARCHIVECOMMANDS+2,
	MENU_FILES_APPLYCOMMAND,
	MENU_FILES_DESCRIBE,
	MENU_FILES_SELECTGROUP = MENU_FILES_DESCRIBE+2,
	MENU_FILES_UNSELECTGROUP,
	MENU_FILES_INVERTSELECTION,
	MENU_FILES_RESTORESELECTION
};

enum enumCommandsMenu {
	MENU_COMMANDS_FINDFILE,
	MENU_COMMANDS_HISTORY,
	MENU_COMMANDS_VIDEOMODE,
	MENU_COMMANDS_FINDFOLDER,
	MENU_COMMANDS_VIEWHISTORY,
	MENU_COMMANDS_FOLDERHISTORY,
	MENU_COMMANDS_SWAPPANELS = MENU_COMMANDS_FOLDERHISTORY+2,
	MENU_COMMANDS_TOGGLEPANELS,
	MENU_COMMANDS_COMPAREFOLDERS,
	MENU_COMMANDS_EDITUSERMENU = MENU_COMMANDS_COMPAREFOLDERS+2,
	MENU_COMMANDS_FILEASSOCIATIONS,
	MENU_COMMANDS_FOLDERSHORTCUTS,
	MENU_COMMANDS_FILTER,
	MENU_COMMANDS_PLUGINCOMMANDS = MENU_COMMANDS_FILTER+2,
	MENU_COMMANDS_WINDOWSLIST,
	MENU_COMMANDS_PROCESSLIST,
	MENU_COMMANDS_HOTPLUGLIST
};

enum enumOptionsMenu {
	MENU_OPTIONS_SYSTEMSETTINGS,
	MENU_OPTIONS_PANELSETTINGS,
	MENU_OPTIONS_INTERFACESETTINGS,
	MENU_OPTIONS_LANGUAGES,
	MENU_OPTIONS_PLUGINSCONFIG,
	MENU_OPTIONS_DIALOGSETTINGS,
	MENU_OPTIONS_CONFIRMATIONS = MENU_OPTIONS_DIALOGSETTINGS+2,
	MENU_OPTIONS_PLUGINCONFIRMATIONS,
	MENU_OPTIONS_FILEPANELMODES,
	MENU_OPTIONS_FILEDESCRIPTIONS,
	MENU_OPTIONS_FOLDERINFOFILES,
	MENU_OPTIONS_VIEWERSETTINGS = MENU_OPTIONS_FOLDERINFOFILES+2,
	MENU_OPTIONS_EDITORSETTINGS,
	MENU_OPTIONS_COLORS = MENU_OPTIONS_EDITORSETTINGS+2,
	MENU_OPTIONS_FILESHIGHLIGHTING,
	MENU_OPTIONS_SAVESETUP = MENU_OPTIONS_FILESHIGHLIGHTING+2
};

void SetLeftRightMenuChecks (MenuDataEx *pMenu, bool bLeft)
{
	Panel *pPanel = bLeft?CtrlObject->Cp()->LeftPanel:CtrlObject->Cp()->RightPanel;

	switch ( pPanel->GetType() ) {

	case FILE_PANEL:
		{
       		int MenuLine = pPanel->GetViewMode()-VIEW_0;

			if ( MenuLine <= MENU_LEFT_ALTERNATIVEVIEW )
			{
				if ( MenuLine == 0 )
					pMenu[MENU_LEFT_ALTERNATIVEVIEW].SetCheck(1);
          		else
            		pMenu[MenuLine-1].SetCheck(1);
			}
		}

		break;

	case INFO_PANEL:
		pMenu[MENU_LEFT_INFOPANEL].SetCheck(1);
		break;

	case TREE_PANEL:
		pMenu[MENU_LEFT_TREEPANEL].SetCheck(1);
		break;

	case QVIEW_PANEL:
		pMenu[MENU_LEFT_QUICKVIEW].SetCheck(1);
		break;
	}

	pMenu[MENU_LEFT_LONGNAMES].SetCheck(!pPanel->GetShowShortNamesMode());
}

void ShellOptions(int LastCommand,MOUSE_EVENT_RECORD *MouseEvent)
{
	MenuDataEx LeftMenu[]=
  {
    (const wchar_t *)MMenuBriefView,LIF_SELECTED,KEY_CTRL1,
    (const wchar_t *)MMenuMediumView,0,KEY_CTRL2,
    (const wchar_t *)MMenuFullView,0,KEY_CTRL3,
    (const wchar_t *)MMenuWideView,0,KEY_CTRL4,
    (const wchar_t *)MMenuDetailedView,0,KEY_CTRL5,
    (const wchar_t *)MMenuDizView,0,KEY_CTRL6,
    (const wchar_t *)MMenuLongDizView,0,KEY_CTRL7,
    (const wchar_t *)MMenuOwnersView,0,KEY_CTRL8,
    (const wchar_t *)MMenuLinksView,0,KEY_CTRL9,
    (const wchar_t *)MMenuAlternativeView,0,KEY_CTRL0,
    L"",LIF_SEPARATOR,0,
    (const wchar_t *)MMenuInfoPanel,0,KEY_CTRLL,
    (const wchar_t *)MMenuTreePanel,0,KEY_CTRLT,
    (const wchar_t *)MMenuQuickView,0,KEY_CTRLQ,
    L"",LIF_SEPARATOR,0,
    (const wchar_t *)MMenuSortModes,0,KEY_CTRLF12,
    (const wchar_t *)MMenuLongNames,0,KEY_CTRLN,
    (const wchar_t *)MMenuTogglePanel,0,KEY_CTRLF1,
    (const wchar_t *)MMenuReread,0,KEY_CTRLR,
    (const wchar_t *)MMenuChangeDrive,0,KEY_ALTF1,
  };
 
	MenuDataEx FilesMenu[]=
  {
    (const wchar_t *)MMenuView,LIF_SELECTED,KEY_F3,
    (const wchar_t *)MMenuEdit,0,KEY_F4,
    (const wchar_t *)MMenuCopy,0,KEY_F5,
    (const wchar_t *)MMenuMove,0,KEY_F6,
    (const wchar_t *)MMenuCreateFolder,0,KEY_F7,
    (const wchar_t *)MMenuDelete,0,KEY_F8,
    (const wchar_t *)MMenuWipe,0,KEY_ALTDEL,
    L"",LIF_SEPARATOR,0,
    (const wchar_t *)MMenuAdd,0,KEY_SHIFTF1,
    (const wchar_t *)MMenuExtract,0,KEY_SHIFTF2,
    (const wchar_t *)MMenuArchiveCommands,0,KEY_SHIFTF3,
    L"",LIF_SEPARATOR,0,
    (const wchar_t *)MMenuAttributes,0,KEY_CTRLA,
    (const wchar_t *)MMenuApplyCommand,0,KEY_CTRLG,
    (const wchar_t *)MMenuDescribe,0,KEY_CTRLZ,
    L"",LIF_SEPARATOR,0,
    (const wchar_t *)MMenuSelectGroup,0,KEY_ADD,
    (const wchar_t *)MMenuUnselectGroup,0,KEY_SUBTRACT,
    (const wchar_t *)MMenuInvertSelection,0,KEY_MULTIPLY,
    (const wchar_t *)MMenuRestoreSelection,0,KEY_CTRLM,
  };


	MenuDataEx CmdMenu[]=
  {
  /* 00 */(const wchar_t *)MMenuFindFile,LIF_SELECTED,KEY_ALTF7,
  /* 01 */(const wchar_t *)MMenuHistory,0,KEY_ALTF8,
  /* 02 */(const wchar_t *)MMenuVideoMode,0,KEY_ALTF9,
  /* 03 */(const wchar_t *)MMenuFindFolder,0,KEY_ALTF10,
  /* 04 */(const wchar_t *)MMenuViewHistory,0,KEY_ALTF11,
  /* 05 */(const wchar_t *)MMenuFoldersHistory,0,KEY_ALTF12,
  /* 06 */L"",LIF_SEPARATOR,0,
  /* 07 */(const wchar_t *)MMenuSwapPanels,0,KEY_CTRLU,
  /* 08 */(const wchar_t *)MMenuTogglePanels,0,KEY_CTRLO,
  /* 09 */(const wchar_t *)MMenuCompareFolders,0,0,
  /* 10 */L"",LIF_SEPARATOR,0,
  /* 11 */(const wchar_t *)MMenuUserMenu,0,0,
  /* 12 */(const wchar_t *)MMenuFileAssociations,0,0,
  /* 13 */(const wchar_t *)MMenuFolderShortcuts,0,0,
  /* 14 */(const wchar_t *)MMenuFilter,0,KEY_CTRLI,
  /* 15 */L"",LIF_SEPARATOR,0,
  /* 16 */(const wchar_t *)MMenuPluginCommands,0,KEY_F11,
  /* 17 */(const wchar_t *)MMenuWindowsList,0,KEY_F12,
  /* 18 */(const wchar_t *)MMenuProcessList,0,KEY_CTRLW,
  /* 19 */(const wchar_t *)MMenuHotPlugList,0,0,
  };


	MenuDataEx OptionsMenu[]=
  {
   /* 00 */(const wchar_t *)MMenuSystemSettings,LIF_SELECTED,0,
   /* 01 */(const wchar_t *)MMenuPanelSettings,0,0,
   /* 02 */(const wchar_t *)MMenuInterface,0,0,
   /* 03 */(const wchar_t *)MMenuLanguages,0,0,
   /* 04 */(const wchar_t *)MMenuPluginsConfig,0,0,
   /* 05 */(const wchar_t *)MMenuDialogSettings,0,0,
   /* 06 */L"",LIF_SEPARATOR,0,
   /* 07 */(const wchar_t *)MMenuConfirmation,0,0,
           L"Plugin confirmation", 0, 0,   			
   /* 08 */(const wchar_t *)MMenuFilePanelModes,0,0,
   /* 09 */(const wchar_t *)MMenuFileDescriptions,0,0,
   /* 10 */(const wchar_t *)MMenuFolderInfoFiles,0,0,
   /* 11 */L"",LIF_SEPARATOR,0,
   /* 12 */(const wchar_t *)MMenuViewer,0,0,
   /* 13 */(const wchar_t *)MMenuEditor,0,0,
   /* 14 */L"",LIF_SEPARATOR,0,
   /* 15 */(const wchar_t *)MMenuColors,0,0,
   /* 16 */(const wchar_t *)MMenuFilesHighlighting,0,0,
   /* 17 */L"",LIF_SEPARATOR,0,
   /* 18 */(const wchar_t *)MMenuSaveSetup,0,KEY_SHIFTF9,
  };


	MenuDataEx RightMenu[]=
  {
    (const wchar_t *)MMenuBriefView,LIF_SELECTED,KEY_CTRL1,
    (const wchar_t *)MMenuMediumView,0,KEY_CTRL2,
    (const wchar_t *)MMenuFullView,0,KEY_CTRL3,
    (const wchar_t *)MMenuWideView,0,KEY_CTRL4,
    (const wchar_t *)MMenuDetailedView,0,KEY_CTRL5,
    (const wchar_t *)MMenuDizView,0,KEY_CTRL6,
    (const wchar_t *)MMenuLongDizView,0,KEY_CTRL7,
    (const wchar_t *)MMenuOwnersView,0,KEY_CTRL8,
    (const wchar_t *)MMenuLinksView,0,KEY_CTRL9,
    (const wchar_t *)MMenuAlternativeView,0,KEY_CTRL0,
    L"",LIF_SEPARATOR,0,
    (const wchar_t *)MMenuInfoPanel,0,KEY_CTRLL,
    (const wchar_t *)MMenuTreePanel,0,KEY_CTRLT,
    (const wchar_t *)MMenuQuickView,0,KEY_CTRLQ,
    L"",LIF_SEPARATOR,0,
    (const wchar_t *)MMenuSortModes,0,KEY_CTRLF12,
    (const wchar_t *)MMenuLongNames,0,KEY_CTRLN,
    (const wchar_t *)MMenuTogglePanelRight,0,KEY_CTRLF2,
    (const wchar_t *)MMenuReread,0,KEY_CTRLR,
    (const wchar_t *)MMenuChangeDriveRight,0,KEY_ALTF2,
  };


	HMenuData MainMenu[]=
  {
		MSG(MMenuLeftTitle),1,LeftMenu,countof(LeftMenu),L"LeftRightMenu",
		MSG(MMenuFilesTitle),0,FilesMenu,countof(FilesMenu),L"FilesMenu",
		MSG(MMenuCommandsTitle),0,CmdMenu,countof(CmdMenu),L"CmdMenu",
		MSG(MMenuOptionsTitle),0,OptionsMenu,countof(OptionsMenu),L"OptMenu",
		MSG(MMenuRightTitle),0,RightMenu,countof(RightMenu),L"LeftRightMenu"
  };

	static int LastHItem=-1,LastVItem=0;
	int HItem,VItem;

	// ��������
	CmdMenu[MENU_COMMANDS_HOTPLUGLIST].SetDisable(!ifn.bSetupAPIFunctions);

	if ( Opt.Policies.DisabledOptions )
	{
		for(size_t I = 0; I < countof(OptionsMenu); ++I)
		{
			if( I >= MENU_OPTIONS_CONFIRMATIONS )
				OptionsMenu[I].SetDisable((Opt.Policies.DisabledOptions >> (I-1)) & 1);
			else
				OptionsMenu[I].SetDisable((Opt.Policies.DisabledOptions >> I) & 1);
		}
	}

    SetLeftRightMenuChecks(LeftMenu, true);
    SetLeftRightMenuChecks(RightMenu, false);

	// ��������� �� ����

	{
		HMenu HOptMenu(MainMenu,countof(MainMenu));
		
		HOptMenu.SetHelp(L"Menus");
		HOptMenu.SetPosition(0,0,ScrX,0);

		if ( LastCommand )
		{

			MenuDataEx *VMenuTable[] = {LeftMenu, FilesMenu, CmdMenu, OptionsMenu, RightMenu};

			int HItemToShow = LastHItem;
			
			if ( HItemToShow == -1 )
			{
				if ( CtrlObject->Cp()->ActivePanel == CtrlObject->Cp()->RightPanel &&
					 CtrlObject->Cp()->ActivePanel->IsVisible() )
					HItemToShow = 4;
				else
					HItemToShow = 0;
			}

			MainMenu[0].Selected = 0;
			MainMenu[HItemToShow].Selected = 1;

			VMenuTable[HItemToShow][0].SetSelect(0);
			VMenuTable[HItemToShow][LastVItem].SetSelect(1);

			HOptMenu.Show();

			{
				ChangeMacroMode MacroMode(MACRO_MAINMENU);
				HOptMenu.ProcessKey(KEY_DOWN);
			}

		}
		else
		{
			if ( CtrlObject->Cp()->ActivePanel==CtrlObject->Cp()->RightPanel &&
				 CtrlObject->Cp()->ActivePanel->IsVisible() )
			{
				MainMenu[0].Selected = 0;
				MainMenu[4].Selected = 1;
			}
		}

		if (MouseEvent!=NULL)
		{
			ChangeMacroMode MacroMode(MACRO_MAINMENU);
      
			HOptMenu.Show();
			HOptMenu.ProcessMouse(MouseEvent);
		}

		{
			ChangeMacroMode MacroMode(MACRO_MAINMENU);
			HOptMenu.Process();
		}

		HOptMenu.GetExitCode(HItem,VItem);
	}

	// "���������� ������ ����"
	switch ( HItem )  {

		case MENU_LEFT:
		case MENU_RIGHT:
		{
			Panel *pPanel = (HItem == MENU_LEFT)?CtrlObject->Cp()->LeftPanel:CtrlObject->Cp()->RightPanel;

      		if ( VItem >= MENU_LEFT_BRIEFVIEW && VItem <= MENU_LEFT_ALTERNATIVEVIEW ) 
      		{
      			CtrlObject->Cp()->ChangePanelToFilled(pPanel, FILE_PANEL);
        		int NewViewMode = (VItem == MENU_LEFT_ALTERNATIVEVIEW)?VIEW_0:VIEW_1+VItem;

        		pPanel->SetViewMode(NewViewMode);
			}
      		else
      		{
        		switch ( VItem ) {
          			case MENU_LEFT_INFOPANEL: // Info panel
            			CtrlObject->Cp()->ChangePanelToFilled(pPanel, INFO_PANEL);
			            break;

					case MENU_LEFT_TREEPANEL: // Tree panel
						CtrlObject->Cp()->ChangePanelToFilled(pPanel, TREE_PANEL);
						break;

					case MENU_LEFT_QUICKVIEW: // Quick view
						CtrlObject->Cp()->ChangePanelToFilled(pPanel, QVIEW_PANEL);
						break;

					case MENU_LEFT_SORTMODES: // Sort modes
						pPanel->ProcessKey(KEY_CTRLF12);
						break;

					case MENU_LEFT_LONGNAMES: // Show long names
						pPanel->ProcessKey(KEY_CTRLN);
						break;

					case MENU_LEFT_TOGGLEPANEL: // Panel On/Off
						if ( HItem == MENU_LEFT )
							FrameManager->ProcessKey(KEY_CTRLF1);
						else
							FrameManager->ProcessKey(KEY_CTRLF2);
			            break;

					case MENU_LEFT_REREAD: // Re-read
						pPanel->ProcessKey(KEY_CTRLR);
						break;

					case MENU_LEFT_CHANGEDRIVE: // Change drive
						pPanel->ChangeDisk();
						break;
				}
			}

			break;
		}

		case MENU_FILES:
		{
			switch ( VItem ) {
				case MENU_FILES_VIEW:  // View
					FrameManager->ProcessKey(KEY_F3);
					break;

				case MENU_FILES_EDIT:  // Edit
					FrameManager->ProcessKey(KEY_F4);
					break;

				case MENU_FILES_COPY:  // Copy
					FrameManager->ProcessKey(KEY_F5);
					break;

				case MENU_FILES_MOVE:  // Rename or move
					FrameManager->ProcessKey(KEY_F6);
					break;

				case MENU_FILES_CREATEFOLDER:  // Make folder
					FrameManager->ProcessKey(KEY_F7);
					break;

				case MENU_FILES_DELETE:  // Delete
					FrameManager->ProcessKey(KEY_F8);
					break;

				case MENU_FILES_WIPE:  // Wipe
					FrameManager->ProcessKey(KEY_ALTDEL);
					break;

				case MENU_FILES_ADD:  // Add to archive
					CtrlObject->Cp()->ActivePanel->ProcessKey(KEY_SHIFTF1);
					break;

				case MENU_FILES_EXTRACT:  // Extract files
					CtrlObject->Cp()->ActivePanel->ProcessKey(KEY_SHIFTF2);
					break;

				case MENU_FILES_ARCHIVECOMMANDS:  // Archive commands
					CtrlObject->Cp()->ActivePanel->ProcessKey(KEY_SHIFTF3);
					break;

				case MENU_FILES_ATTRIBUTES: // File attributes
					CtrlObject->Cp()->ActivePanel->ProcessKey(KEY_CTRLA);
					break;

				case MENU_FILES_APPLYCOMMAND: // Apply command
					CtrlObject->Cp()->ActivePanel->ProcessKey(KEY_CTRLG);
					break;

				case MENU_FILES_DESCRIBE: // Describe files
					CtrlObject->Cp()->ActivePanel->ProcessKey(KEY_CTRLZ);
					break;

				case MENU_FILES_SELECTGROUP: // Select group
					CtrlObject->Cp()->ActivePanel->ProcessKey(KEY_ADD);
					break;

				case MENU_FILES_UNSELECTGROUP: // Unselect group
					CtrlObject->Cp()->ActivePanel->ProcessKey(KEY_SUBTRACT);
					break;

				case MENU_FILES_INVERTSELECTION: // Invert selection
					CtrlObject->Cp()->ActivePanel->ProcessKey(KEY_MULTIPLY);
					break;

				case MENU_FILES_RESTORESELECTION: // Restore selection
					CtrlObject->Cp()->ActivePanel->RestoreSelection();
					break;
			}

      		break;
		}

		case MENU_COMMANDS:
		{
			switch ( VItem ) {

				case MENU_COMMANDS_FINDFILE: // Find file
					FrameManager->ProcessKey(KEY_ALTF7);
					break;

				case MENU_COMMANDS_HISTORY: // History
					FrameManager->ProcessKey(KEY_ALTF8);
					break;

				case MENU_COMMANDS_VIDEOMODE: // Video mode
					FrameManager->ProcessKey(KEY_ALTF9);
					break;

				case MENU_COMMANDS_FINDFOLDER: // Find folder
					FrameManager->ProcessKey(KEY_ALTF10);
					break;

				case MENU_COMMANDS_VIEWHISTORY: // File view history
					FrameManager->ProcessKey(KEY_ALTF11);
					break;

				case MENU_COMMANDS_FOLDERHISTORY: // Folders history
					FrameManager->ProcessKey(KEY_ALTF12);
					break;

				case MENU_COMMANDS_SWAPPANELS: // Swap panels
					FrameManager->ProcessKey(KEY_CTRLU);
					break;

				case MENU_COMMANDS_TOGGLEPANELS: // Panels On/Off
					FrameManager->ProcessKey(KEY_CTRLO);
					break;

				case MENU_COMMANDS_COMPAREFOLDERS: // Compare folders
					CtrlObject->Cp()->ActivePanel->CompareDir();
					break;

				case MENU_COMMANDS_EDITUSERMENU: // Edit user menu
					ProcessUserMenu(true);
					break;

				case MENU_COMMANDS_FILEASSOCIATIONS: // File associations
					EditFileTypes();
					break;

				case MENU_COMMANDS_FOLDERSHORTCUTS: // Folder shortcuts
					ShowFolderShortcut();
					break;

				case MENU_COMMANDS_FILTER: // File panel filter
					CtrlObject->Cp()->ActivePanel->EditFilter();
					break;

				case MENU_COMMANDS_PLUGINCOMMANDS: // Plugin commands
					FrameManager->ProcessKey(KEY_F11);
					break;

				case MENU_COMMANDS_WINDOWSLIST: // Screens list
					FrameManager->ProcessKey(KEY_F12);
					break;

				case MENU_COMMANDS_PROCESSLIST: // Task list
					ShowProcessList();
					break;

				case MENU_COMMANDS_HOTPLUGLIST: // HotPlug list
					ShowHotplugDevice();
					break;
			}

			break;
		}

		case MENU_OPTIONS:
		{
			switch ( VItem ) {

				case MENU_OPTIONS_SYSTEMSETTINGS:   // System settings
					SystemSettings();
					break;

				case MENU_OPTIONS_PANELSETTINGS:   // Panel settings
					PanelSettings();
					break;

				case MENU_OPTIONS_INTERFACESETTINGS:   // Interface settings
					InterfaceSettings();
					break;

				case MENU_OPTIONS_LANGUAGES:   // Languages
				{
					VMenu *LangMenu, *HelpMenu;

					if ( Language::Select(FALSE, &LangMenu) )
					{
						Lang.Close();

						if ( !Lang.Init(g_strFarPath, true, MNewFileName) )
						{
							Message(MSG_WARNING, 1, L"Error", L"Cannot load language data", L"Ok");
							exit(0);
						}

						Language::Select(TRUE,&HelpMenu);
						delete HelpMenu;

						LangMenu->Hide();

						CtrlObject->Plugins.ReloadLanguage();
						SetEnvironmentVariable(L"FARLANG",Opt.strLanguage);
						
						PrepareStrFTime();
						PrepareUnitStr();

						FrameManager->InitKeyBar();
						CtrlObject->Cp()->RedrawKeyBar();
						CtrlObject->Cp()->SetScreenPosition();
					}

					delete LangMenu; //???? BUGBUG

					break;
				}

				case MENU_OPTIONS_PLUGINSCONFIG:   // Plugins configuration
					CtrlObject->Plugins.Configure();
					break;

				case MENU_OPTIONS_DIALOGSETTINGS:   // Dialog settings (police=5)
					DialogSettings();
					break;
				
				case MENU_OPTIONS_CONFIRMATIONS:   // Confirmations
					SetConfirmations();
					break;

				case MENU_OPTIONS_PLUGINCONFIRMATIONS:
					SetPluginConfirmations();
					break;

				case MENU_OPTIONS_FILEPANELMODES:   // File panel modes
					FileList::SetFilePanelModes();
					break;

				case MENU_OPTIONS_FILEDESCRIPTIONS:   // File descriptions
					SetDizConfig();
					break;

				case MENU_OPTIONS_FOLDERINFOFILES:   // Folder description files
					SetFolderInfoFiles();
					break;

				case MENU_OPTIONS_VIEWERSETTINGS:  // Viewer settings
					ViewerConfig(Opt.ViOpt);
					break;

				case MENU_OPTIONS_EDITORSETTINGS:  // Editor settings
					EditorConfig(Opt.EdOpt);
					break;

				case MENU_OPTIONS_COLORS:  // Colors
					SetColors();
					break;

				case MENU_OPTIONS_FILESHIGHLIGHTING:  // Files highlighting
					CtrlObject->HiFiles->HiEdit(0);
					break;

				case MENU_OPTIONS_SAVESETUP:  // Save setup
					SaveConfig(1);
					break;
			}

			break;
		}
	}

	int _CurrentFrame=FrameManager->GetCurrentFrame()->GetType();

	// TODO:����� ��� �� ����� ��������, ����� ������ ������� ����� ���� ������������� �������
	//      ��� ��, ���, ������ ��������/������ ����� ���� �� �������������
	
	if ( !(_CurrentFrame == MODALTYPE_VIEWER || _CurrentFrame == MODALTYPE_EDITOR) )
		CtrlObject->CmdLine->Show();

	if ( HItem != -1 && VItem != -1 )
 	{
		LastHItem = HItem;
		LastVItem = VItem;
	}
}
