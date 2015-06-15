#pragma once

/*
config.hpp

������������
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

#include "panel.hpp"
#include "palette.hpp"

class GeneralConfig;

enum
{
	CASR_PANEL  = 0x0001,
	CASR_EDITOR = 0x0002,
	CASR_VIEWER = 0x0004,
	CASR_HELP   = 0x0008,
	CASR_DIALOG = 0x0010,
};

enum ExcludeCmdHistoryType
{
	EXCLUDECMDHISTORY_NOTWINASS    = 0x00000001,  // �� �������� � ������� ������� ���������� Windows
	EXCLUDECMDHISTORY_NOTFARASS    = 0x00000002,  // �� �������� � ������� ������� ���������� ���������� ������
	EXCLUDECMDHISTORY_NOTPANEL     = 0x00000004,  // �� �������� � ������� ������� ���������� � ������
	EXCLUDECMDHISTORY_NOTCMDLINE   = 0x00000008,  // �� �������� � ������� ������� ���������� � ���.������
	//EXCLUDECMDHISTORY_NOTAPPLYCMD   = 0x00000010,  // �� �������� � ������� ������� ���������� �� "Apply Command"
};

enum QUOTEDNAMETYPE
{
	QUOTEDNAME_INSERT         = 0x00000001,            // �������� ��� ������ � ��������� ������, � �������� � ���������
	QUOTEDNAME_CLIPBOARD      = 0x00000002,            // �������� ��� ��������� � ����� ������
};

enum
{
	DMOUSEBUTTON_LEFT = 0x00000001,
	DMOUSEBUTTON_RIGHT = 0x00000002,
};

enum
{
	VMENUCLICK_IGNORE = 0,
	VMENUCLICK_CANCEL = 1,
	VMENUCLICK_APPLY = 2,
};

enum DIZUPDATETYPE
{
	DIZ_NOT_UPDATE,
	DIZ_UPDATE_IF_DISPLAYED,
	DIZ_UPDATE_ALWAYS
};

struct column;
struct FARConfigItem;

class Option
{
public:
	template<class T>
	explicit Option(const T& Value): m_Value(Value) {}
	virtual ~Option(){}

	virtual string toString() const = 0;
	virtual void fromString(const string& value) = 0;
	virtual string ExInfo() const = 0;
	virtual string typeToString() const = 0;
	virtual bool IsDefault(const any& Default) const = 0;
	virtual void SetDefault(const any& Default) = 0;
	virtual bool Edit(class DialogBuilder* Builder, int Width, int Param) = 0;
	virtual void Export(FarSettingsItem& To) const = 0;

	bool Changed() const { return m_Value.touched(); }

protected:
	template<class T>
	const T& GetT() const { return any_cast<T>(m_Value); }
	template<class T>
	void SetT(const T& NewValue) { if (GetT<T>() != NewValue) m_Value = NewValue; }

private:
	friend class Options;

	virtual bool StoreValue(GeneralConfig* Storage, const string& KeyName, const string& ValueName, bool always) const = 0;
	virtual bool ReceiveValue(GeneralConfig* Storage, const string& KeyName, const string& ValueName, const any& Default) = 0;

	void MakeUnchanged() { decltype(m_Value)(std::move(m_Value.value())).swap(m_Value); }

	monitored<any> m_Value;
};

template<class base_type, class derived>
class opt_traits: public Option
{
public:
	typedef base_type type;

	opt_traits(): Option(base_type()) {}
	opt_traits(const base_type& Value): Option(Value) {}
	opt_traits(const derived& Value): Option(Value.Get()) {}

	const base_type& Get() const { return GetT<base_type>(); }
	void Set(const base_type& Value) { SetT(Value); }

	virtual bool IsDefault(const any& Default) const override { return Get() == any_cast<base_type>(Default); }
	virtual void SetDefault(const any& Default) override { Set(any_cast<base_type>(Default)); }

	virtual bool ReceiveValue(GeneralConfig* Storage, const string& KeyName, const string& ValueName, const any& Default) override;
	virtual bool StoreValue(GeneralConfig* Storage, const string& KeyName, const string& ValueName, bool always) const override;

	//operator const base_type&() const { return Get(); }
};

class BoolOption: public opt_traits<bool, BoolOption>
{
public:
	BoolOption() {}
	BoolOption(const bool& Value): opt_traits(Value) {}
	BoolOption(const BoolOption& Value): opt_traits(Value) {}

	virtual string toString() const override { return Get() ? L"true" : L"false"; }
	virtual void fromString(const string& value) override;
	virtual string ExInfo() const override { return string(); }
	virtual string typeToString() const override { return L"boolean"; }
	virtual bool Edit(class DialogBuilder* Builder, int Width, int Param) override;
	virtual void Export(FarSettingsItem& To) const override;

	template<class T>
	BoolOption& operator=(const T& Value) { Set(Value); return *this; }

	operator bool() const { return Get(); }
};

class Bool3Option: public opt_traits<long long, Bool3Option>
{
public:
	Bool3Option() {}
	Bool3Option(const int& Value): opt_traits(Value){}
	Bool3Option(const Bool3Option& Value): opt_traits(Value){}

	virtual string toString() const override { int v = Get(); return v ? (v == 1 ? L"true" : L"other") : L"false"; }
	virtual void fromString(const string& value) override;
	virtual string ExInfo() const override { return string(); }
	virtual string typeToString() const override { return L"3-state"; }
	virtual bool Edit(class DialogBuilder* Builder, int Width, int Param) override;
	virtual void Export(FarSettingsItem& To) const override;

	template<class T>
	Bool3Option& operator=(const T& Value) { Set(Value); return *this; }

	operator FARCHECKEDSTATE() const { return static_cast<FARCHECKEDSTATE>(Get()); }
};

class IntOption: public opt_traits<long long, IntOption>
{
public:
	IntOption() {}
	IntOption(long long Value): opt_traits(Value){}
	IntOption(const IntOption& Value): opt_traits(Value){}

	virtual string toString() const override { return std::to_wstring(Get()); }
	virtual void fromString(const string& value) override;
	virtual string ExInfo() const override;
	virtual string typeToString() const override { return L"integer"; }
	virtual bool Edit(class DialogBuilder* Builder, int Width, int Param) override;
	virtual void Export(FarSettingsItem& To) const override;

	template<class T>
	IntOption& operator=(const T& Value) { Set(Value); return *this; }

	IntOption& operator|=(long long Value){ Set(Get() | Value); return *this; }
	IntOption& operator&=(long long Value){Set(Get()&Value); return *this;}
	IntOption& operator%=(long long Value){Set(Get()%Value); return *this;}
	IntOption& operator^=(long long Value){Set(Get()^Value); return *this;}
	IntOption& operator--(){Set(Get()-1); return *this;}
	IntOption& operator++(){Set(Get()+1); return *this;}
	IntOption operator--(int){long long Current = Get(); Set(Current-1); return Current;}
	IntOption operator++(int){long long Current = Get(); Set(Current+1); return Current;}

	operator long long() const { return Get(); }
};

class StringOption: public opt_traits<string, StringOption>
{
public:
	StringOption() {}
	StringOption(const StringOption& Value): opt_traits(Value) {}
	StringOption(const string& Value): opt_traits(Value) {}

	virtual string toString() const override { return Get(); }
	virtual void fromString(const string& value) override { Set(value); }
	virtual string ExInfo() const override { return string(); }
	virtual string typeToString() const override { return L"string"; }
	virtual bool Edit(class DialogBuilder* Builder, int Width, int Param) override;
	virtual void Export(FarSettingsItem& To) const override;

	template<class T>
	StringOption& operator=(const T& Value) { Set(Value); return *this; }

	StringOption& operator+=(const string& Value) {Set(Get()+Value); return *this;}
	wchar_t operator[] (size_t index) const { return Get()[index]; }
	const wchar_t* data() const { return Get().data(); }
	void clear() { Set(string()); }
	bool empty() const { return Get().empty(); }
	size_t size() const { return Get().size(); }

	operator const string&() const { return Get(); }
};

class Options: noncopyable
{
	enum farconfig_mode
	{
		cfg_roaming,
		cfg_local,
	};

public:
	struct ViewerOptions;
	struct EditorOptions;

	Options();
	~Options();
	void ShellOptions(bool LastCommand, const MOUSE_EVENT_RECORD *MouseEvent);
	void Load(const std::vector<std::pair<string, string>>& Overridden);
	void Save(bool Manual);
	const Option* GetConfigValue(const wchar_t *Key, const wchar_t *Name) const;
	const Option* GetConfigValue(size_t Root, const wchar_t* Name) const;
	bool AdvancedConfig(farconfig_mode Mode = cfg_roaming);
	void LocalViewerConfig(Options::ViewerOptions &ViOptRef) {return ViewerConfig(ViOptRef, true);}
	void LocalEditorConfig(Options::EditorOptions &EdOptRef) {return EditorConfig(EdOptRef, true);}
	void SetSearchColumns(const string& Columns, const string& Widths);

	struct PanelOptions
	{
		IntOption m_Type;
		BoolOption Visible;
		IntOption ViewMode;
		IntOption SortMode;
		BoolOption ReverseSortOrder;
		BoolOption SortGroups;
		BoolOption ShowShortNames;
		BoolOption NumericSort;
		BoolOption CaseSensitiveSort;
		BoolOption SelectedFirst;
		BoolOption DirectoriesFirst;
		StringOption Folder;
		StringOption CurFile;
	};

	struct AutoCompleteOptions
	{
		BoolOption ShowList;
		BoolOption ModalList;
		BoolOption AppendCompletion;

		Bool3Option UseFilesystem;
		Bool3Option UseHistory;
		Bool3Option UsePath;
		Bool3Option UseEnvironment;
	};

	struct PluginConfirmation
	{
		Bool3Option OpenFilePlugin;
		BoolOption StandardAssociation;
		BoolOption EvenIfOnlyOnePlugin;
		BoolOption SetFindList;
		BoolOption Prefix;
	};

	struct Confirmation
	{
		BoolOption Copy;
		BoolOption Move;
		BoolOption RO;
		BoolOption Drag;
		BoolOption Delete;
		BoolOption DeleteFolder;
		BoolOption Exit;
		BoolOption Esc;
		BoolOption EscTwiceToInterrupt;
		BoolOption RemoveConnection;
		BoolOption AllowReedit;
		BoolOption HistoryClear;
		BoolOption RemoveSUBST;
		BoolOption RemoveHotPlug;
		BoolOption DetachVHD;
	};

	struct DizOptions
	{
		StringOption strListNames;
		BoolOption ROUpdate;
		IntOption UpdateMode;
		BoolOption SetHidden;
		IntOption StartPos;
		BoolOption AnsiByDefault;
		BoolOption SaveInUTF;
	};

	struct CodeXLAT
	{
		CodeXLAT(): Layouts(), CurrentLayout() {}

		HKL Layouts[10];
		StringOption strLayouts;
		StringOption Rules[3]; // �������:
		// [0] "���� ���������� ������ ���������"
		// [1] "���� ���������� ������ ����������� ������"
		// [2] "���� ���������� ������ �� ���/lat"
		StringOption Table[2]; // [0] non-english �����, [1] english �����
		StringOption strWordDivForXlat;
		IntOption Flags;
		mutable int CurrentLayout;
	};

	struct EditorOptions
	{
		IntOption TabSize;
		IntOption ExpandTabs;
		BoolOption PersistentBlocks;
		BoolOption DelRemovesBlocks;
		BoolOption AutoIndent;
		BoolOption AutoDetectCodePage;
		IntOption DefaultCodePage;
		StringOption strF8CPs;
		BoolOption CursorBeyondEOL;
		BoolOption BSLikeDel;
		IntOption CharCodeBase;
		BoolOption SavePos;
		BoolOption SaveShortPos;
		BoolOption AllowEmptySpaceAfterEof;
		IntOption ReadOnlyLock;
		IntOption UndoSize;
		BoolOption UseExternalEditor;
		IntOption FileSizeLimit;
		BoolOption ShowKeyBar;
		BoolOption ShowTitleBar;
		BoolOption ShowScrollBar;
		BoolOption EditOpenedForWrite;
		BoolOption SearchSelFound;
		BoolOption SearchCursorAtEnd;
		BoolOption SearchRegexp;
		BoolOption SearchPickUpWord;
		Bool3Option ShowWhiteSpace;

		StringOption strWordDiv;

		BoolOption KeepEOL;
		BoolOption AddUnicodeBOM;
	};

	struct ViewerOptions
	{
		enum
		{
			eMinLineSize = 1*1000,
			eDefLineSize = 10*1000,
			eMaxLineSize = 100*1000
		};

		BoolOption AutoDetectCodePage;
		IntOption   DefaultCodePage;
		StringOption strF8CPs;
		IntOption   MaxLineSize; // 1000..100000, default=10000
		BoolOption PersistentBlocks;
		BoolOption  SaveCodepage;
		BoolOption SavePos;
		BoolOption  SaveShortPos;
		BoolOption SaveWrapMode;
		BoolOption  SearchEditFocus; // auto-focus on edit text/hex window
		BoolOption  SearchRegexp;
		Bool3Option SearchWrapStop; // [NonStop] / {Start-End} / [Full Cycle]
		BoolOption  ShowArrows;
		BoolOption ShowKeyBar;
		BoolOption  ShowScrollbar;
		BoolOption ShowTitleBar;
		IntOption   TabSize;
		BoolOption  UseExternalViewer;
		BoolOption  ViewerIsWrap; // (Wrap|WordWarp)=1 | UnWrap=0
		BoolOption  ViewerWrap; // Wrap=0|WordWarp=1
		BoolOption Visible0x00;
		IntOption  ZeroChar;
	};

	struct PoliciesOptions
	{
		BoolOption ShowHiddenDrives; // ���������� ������� ���������� �����
	};

	struct DialogsOptions
	{
		BoolOption EditBlock;            // ���������� ����� � ������� �����
		BoolOption EditHistory;          // ��������� � �������?
		BoolOption AutoComplete;         // ��������� ��������������?
		BoolOption EULBsClear;           // = 1 - BS � �������� ��� UnChanged ������ ������� ����� ������ �����, ��� � Del
		IntOption EditLine;             // ����� ���������� � ������ ����� (������ ��� ����... ��������� ��������� ����������)
		IntOption MouseButton;          // ���������� ���������� ������/����� ������ ���� ��� ������ �������� ���� �������
		BoolOption DelRemovesBlocks;
		IntOption CBoxMaxHeight;        // ������������ ������ ������������ ������ (�� ���������=8)
	};

	struct VMenuOptions
	{
		IntOption LBtnClick;
		IntOption RBtnClick;
		IntOption MBtnClick;
	};

	struct CommandLineOptions
	{
		BoolOption EditBlock;
		BoolOption DelRemovesBlocks;
		BoolOption AutoComplete;
		BoolOption UsePromptFormat;
		StringOption strPromptFormat;
	};

	struct NowellOptions
	{
		// ����� ��������� Move ������� R/S/H ��������, ����� �������� - ���������� �������
		BoolOption MoveRO;
	};

	struct ScreenSizes
	{
		// �� ������� ���. �������� ������� ��� ������������ ������
		IntOption DeltaX;
		IntOption DeltaY;
	};

	struct LoadPluginsOptions
	{
		string strCustomPluginsPath;  // ���� ��� ������ ��������, ��������� � /p
		string strPersonalPluginsPath;
		bool MainPluginDir; // true - ������������ ����������� ���� � �������� ��������
		bool PluginsCacheOnly; // set by '/co' switch, not saved
		bool PluginsPersonal;

		BoolOption SilentLoadPlugin;
#ifndef NO_WRAPPER
		BoolOption OEMPluginsSupport;
#endif // NO_WRAPPER
		BoolOption ScanSymlinks;
	};

	struct FindFileOptions
	{
		IntOption FileSearchMode;
		BoolOption FindFolders;
		BoolOption FindSymLinks;
		BoolOption UseFilter;
		BoolOption FindAlternateStreams;
		StringOption strSearchInFirstSize;

		StringOption strSearchOutFormat;
		StringOption strSearchOutFormatWidth;

		std::vector<column> OutColumns;
	};

	struct InfoPanelOptions
	{
		IntOption ComputerNameFormat;
		IntOption UserNameFormat;
		BoolOption ShowPowerStatus;
		StringOption strShowStatusInfo;
		StringOption strFolderInfoFiles;
		BoolOption ShowCDInfo;
	};

	struct TreeOptions
	{
		BoolOption TurnOffCopmletely;   // Turn OFF SlowlyAndBuglyTreeView

		IntOption MinTreeCount;         // ����������� ���������� ����� ��� ���������� ������ � �����.
		BoolOption AutoChangeFolder;    // ��������� ����� ��� ����������� �� ������
		IntOption TreeFileAttr;         // �������� �������� ��� ������-�������

#if defined(TREEFILE_PROJECT)
		BoolOption LocalDisk;           // ������� ���� ��������� ����� ��� ��������� ������
		BoolOption NetDisk;             // ������� ���� ��������� ����� ��� ������� ������
		BoolOption NetPath;             // ������� ���� ��������� ����� ��� ������� �����
		BoolOption RemovableDisk;       // ������� ���� ��������� ����� ��� ������� ������
		BoolOption CDDisk;              // ������� ���� ��������� ����� ��� CD/DVD/BD/etc ������

		StringOption strLocalDisk;      // ������ ����� �����-�������� ��� ��������� ������
		StringOption strNetDisk;        // ������ ����� �����-�������� ��� ������� ������
		StringOption strNetPath;        // ������ ����� �����-�������� ��� ������� �����
		StringOption strRemovableDisk;  // ������ ����� �����-�������� ��� ������� ������
		StringOption strCDDisk;         // ������ ����� �����-�������� ��� CD/DVD/BD/etc ������

		StringOption strExceptPath;     // ��� ������������� ����� �� �������

		StringOption strSaveLocalPath;  // ���� ��������� ��������� �����
		StringOption strSaveNetPath;    // ���� ��������� ������� �����
#endif
	};

	struct CopyMoveOptions
	{
		BoolOption UseSystemCopy;         // ������������ ��������� ������� �����������
		BoolOption CopyOpened;            // ���������� �������� �� ������ �����
		BoolOption CopyShowTotal;         // �������� ����� ��������� �����������
		BoolOption MultiCopy;             // "��������� �����������������/�����������/�������� ������"
		IntOption CopySecurityOptions; // ��� �������� Move - ��� ������ � ������ "Copy access rights"
		IntOption CopyTimeRule;          // $ 30.01.2001 VVM  ���������� ����� �����������,���������� ����� � ������� ��������
		IntOption BufferSize;
	};

	struct DeleteOptions
	{
		BoolOption ShowTotal;         // �������� ����� ��������� ��������
		BoolOption HighlightSelected;
		IntOption  ShowSelected;
	};

	struct MacroOptions
	{
		int DisableMacro; // ��������� /m ��� /ma ��� /m....
		// config
		StringOption strKeyMacroCtrlDot, strKeyMacroRCtrlDot; // ��� KEY_CTRLDOT/KEY_RCTRLDOT
		StringOption strKeyMacroCtrlShiftDot, strKeyMacroRCtrlShiftDot; // ��� KEY_CTRLSHIFTDOT/KEY_RCTRLSHIFTDOT
		// internal
		DWORD KeyMacroCtrlDot, KeyMacroRCtrlDot;
		DWORD KeyMacroCtrlShiftDot, KeyMacroRCtrlShiftDot;
		StringOption strMacroCONVFMT; // ������ �������������� double � ������
		StringOption strDateFormat; // ��� $Date
		BoolOption ShowPlayIndicator; // �������� ����� 'P' �� ����� ������������ �������
	};

	struct KnownModulesIDs
	{
		struct GuidOption
		{
			GUID Id;
			StringOption StrId;
			const wchar_t* Default;
		};

		GuidOption Network;
		GuidOption Emenu;
		GuidOption Arclite;
		GuidOption Luamacro;
		GuidOption Netbox;
	};

	struct ExecuteOptions
	{
		BoolOption RestoreCPAfterExecute;
		BoolOption ExecuteUseAppPath;
		BoolOption ExecuteFullTitle;
		BoolOption ExecuteSilentExternal;
		StringOption strExecuteBatchType;
		StringOption strExcludeCmds;
		StringOption strComSpecParams;
		BoolOption   UseHomeDir; // cd ~
		StringOption strHomeDir; // cd ~
		StringOption strNotQuotedShell;
	};

	palette Palette;
	BoolOption Clock;
	BoolOption Mouse;
	BoolOption ShowKeyBar;
	BoolOption ScreenSaver;
	IntOption ScreenSaverTime;
	BoolOption UseVk_oem_x;
	BoolOption ShowHidden;
	BoolOption ShortcutAlwaysChdir;
	BoolOption Highlight;
	BoolOption RightClickSelect;

	BoolOption SelectFolders;
	BoolOption ReverseSort;
	BoolOption SortFolderExt;
	BoolOption DeleteToRecycleBin;
	IntOption WipeSymbol; // ������ ����������� ��� "ZAP-��������"

	CopyMoveOptions CMOpt;

	DeleteOptions DelOpt;

	BoolOption MultiMakeDir; // ����� �������� ���������� ��������� �� ���� �����

	BoolOption UseRegisteredTypes;

	BoolOption ViewerEditorClock;
	BoolOption SaveViewHistory;
	IntOption ViewHistoryCount;
	IntOption ViewHistoryLifetime;

	StringOption strExternalEditor;
	EditorOptions EdOpt;
	StringOption strExternalViewer;
	ViewerOptions ViOpt;

	// alias for EdOpt.strWordDiv
	StringOption& strWordDiv;
	StringOption strQuotedSymbols;
	IntOption QuotedName;
	BoolOption AutoSaveSetup;
	IntOption ChangeDriveMode;
	BoolOption ChangeDriveDisconnectMode;

	BoolOption SaveHistory;
	IntOption HistoryCount;
	IntOption HistoryLifetime;
	BoolOption SaveFoldersHistory;
	IntOption FoldersHistoryCount;
	IntOption FoldersHistoryLifetime;
	IntOption DialogsHistoryCount;
	IntOption DialogsHistoryLifetime;

	FindFileOptions FindOpt;

	IntOption LeftHeightDecrement;
	IntOption RightHeightDecrement;
	IntOption WidthDecrement;

	BoolOption ShowColumnTitles;
	BoolOption ShowPanelStatus;
	BoolOption ShowPanelTotals;
	BoolOption ShowPanelFree;
	BoolOption PanelDetailedJunction;
	BoolOption ShowUnknownReparsePoint;
	BoolOption HighlightColumnSeparator;
	BoolOption DoubleGlobalColumnSeparator;

	BoolOption ShowPanelScrollbar;
	BoolOption ShowMenuScrollbar;
	BoolOption ShowScreensNumber;
	BoolOption ShowSortMode;
	BoolOption ShowMenuBar;
	StringOption FormatNumberSeparators;
	BoolOption CleanAscii;
	BoolOption NoGraphics;

	Confirmation Confirm;
	PluginConfirmation PluginConfirm;

	DizOptions Diz;

	BoolOption ShellRightLeftArrowsRule;
	PanelOptions LeftPanel;
	PanelOptions RightPanel;
	BoolOption LeftFocus;

	AutoCompleteOptions AutoComplete;

	// ���� ����� ���������� ������������� �� ��������� ������.
	IntOption  AutoUpdateLimit;
	BoolOption AutoUpdateRemoteDrive;

	StringOption strLanguage;
	BoolOption SetIcon;
	BoolOption SetAdminIcon;
	IntOption PanelRightClickRule;
	IntOption PanelCtrlAltShiftRule;
	// ��������� Ctrl-F. ���� = 0, �� ���������� ���� ��� ����, ����� - � ������ ����������� �� ������
	BoolOption PanelCtrlFRule;
	/*
	��������� Ctrl-Alt-Shift
	��� ���������� - ������� ��������:
	0 - Panel
	1 - Edit
	2 - View
	3 - Help
	4 - Dialog
	*/
	IntOption AllCtrlAltShiftRule;

	IntOption CASRule; // 18.12.2003 - ������� ��������� ����� � ������ CAS (������� #1).
	/*
	  ������ ��������� Esc ��� ��������� ������:
	    =1 - �� �������� ��������� � History, ���� ����� Ctrl-E/Ctrl/-X
	         ������ ESC (��������� - ��� VC).
	    =0 - ��������� ��� � ���� - �������� ��������� � History
	*/
	BoolOption CmdHistoryRule;

	IntOption ExcludeCmdHistory;

	BoolOption SubstPluginPrefix; // 1 = ��������������� ������� ������� (��� Ctrl-[ � ��� ��������)
	BoolOption SetAttrFolderRules;

	BoolOption ExceptUsed;
	StringOption strExceptEventSvc;
	/*
	������� �� ���� ������ ��������� ����������
	Alt-����� ��� ����������� ������� � �������� "`-=[]\;',./" �
	�������������� Alt-, Ctrl-, Alt-Shift-, Ctrl-Shift-, Ctrl-Alt-
	*/
	BoolOption ShiftsKeyRules;
	IntOption CursorSize[4];

	CodeXLAT XLat;

	StringOption ConsoleDetachKey; // ���������� ������ ��� ������ Far'������ ������� �� ����������� ���������������� �������� � ��� �����������.

	StringOption strHelpLanguage;
	BoolOption FullScreenHelp;
	IntOption HelpTabSize;

	IntOption HelpURLRules; // =0 ��������� ����������� ������� URL-����������
	BoolOption HelpSearchRegexp;

	// ���������� ���������� ����� � �� ���������� ������ ���. ��� �������������� "����������" "�������" ������.
	BoolOption RememberLogicalDrives;
	BoolOption FlagPosixSemantics;

	IntOption MsWheelDelta; // ������ �������� ��� ���������
	IntOption MsWheelDeltaView;
	IntOption MsWheelDeltaEdit;
	IntOption MsWheelDeltaHelp;
	// �������������� ���������
	IntOption MsHWheelDelta;
	IntOption MsHWheelDeltaView;
	IntOption MsHWheelDeltaEdit;

	/*
	������� �����:
	    0 - ���� ����������, �� ���������� ������� ����� ��� GetSubstName()
	    1 - ���� ����������, �� ���������� ��� ��������� ��� GetSubstName()
	*/
	IntOption SubstNameRule;

	/* $ 23.05.2001 AltF9
	  + ���� ��������� ������� ��������  ������ ���������� Alt-F9
	       (��������� ������� ������) � ������� ������. �� ��������� - 1.
	    0 - ������������ ��������, ����������� � FAR ������ 1.70 beta 3 �
	       ����, �.�. ������������ 25/50 �����.
	    1 - ������������ ������������������� �������� - ���� FAR Manager
	       ����� ������������� � ����������� �� ����������� ��������� ������
	       ����������� ���� � �������.*/
	BoolOption AltF9;

	BoolOption ClearType;

	Bool3Option PgUpChangeDisk;
	BoolOption ShowDotsInRoot;
	BoolOption ShowCheckingFile;
	BoolOption CloseCDGate;       // ���������������� CD
	BoolOption UpdateEnvironment;

	ExecuteOptions Exec;

	IntOption PluginMaxReadData;
	BoolOption ScanJunction;

	IntOption RedrawTimeout;
	IntOption DelThreadPriority; // ��������� �������� ��������, �� ��������� = THREAD_PRIORITY_NORMAL

	LoadPluginsOptions LoadPlug;

	DialogsOptions Dialogs;
	VMenuOptions VMenu;
	CommandLineOptions CmdLine;
	PoliciesOptions Policies;
	NowellOptions Nowell;
	ScreenSizes ScrSize;
	MacroOptions Macro;

	IntOption FindCodePage;

	TreeOptions Tree;
	InfoPanelOptions InfoPanel;

	BoolOption CPMenuMode;
	StringOption strNoAutoDetectCP;
	// ������������� ����� ������� �������� ����� ��������� �� �������������� nsUniversalDetectorEx.
	// ���������� ��������� ������� �� ����� �� �������, ������� UTF-8 ����� ������������ ���� ����
	// 65001 ����� ������������. ���� UniversalDetector ������ �������� �� ����� ������, ��� �����
	// �������� �� ������������� ANSI ��� OEM, � ����������� �� ��������.
	// ������: L"1250,1252,1253,1255,855,10005,28592,28595,28597,28598,38598,65001"
	// ���� ������ ������ ������� ���������� ������� ������� � UCD ������� �� �����.
	// ���� "-1", �� � ����������� CPMenuMode (Ctrl-H � ���� ������� ������� ���������� UCD ���� �����
	// ���������, ���� ����� ���������� ������ ��������� � ��������� (OEM ANSI) ������� ��������.

	StringOption strTitleAddons;
	StringOption strEditorTitleFormat;
	StringOption strViewerTitleFormat;

	IntOption StoredElevationMode;

	BoolOption StoredWindowMode;

	string ProfilePath;
	string LocalProfilePath;
	string TemplateProfilePath;
	string GlobalUserMenuDir;
	KnownModulesIDs KnownIDs;

	StringOption strBoxSymbols;

	BoolOption SmartFolderMonitor; // def: 0=always monitor panel folder(s), 1=only when FAR has input focus

	int ReadOnlyConfig;
	int UseExceptionHandler;
	int ElevationMode;
	int WindowMode;

	const std::vector<PanelViewSettings>& ViewSettings;

	class farconfig;

private:
	void InitConfig();
	void InitConfigData();
	intptr_t AdvancedConfigDlgProc(class Dialog* Dlg, intptr_t Msg, intptr_t Param1, void* Param2);
	void SystemSettings();
	void PanelSettings();
	void InterfaceSettings();
	void DialogSettings();
	void VMenuSettings();
	void CmdlineSettings();
	void SetConfirmations();
	void PluginsManagerSettings();
	void SetDizConfig();
	void ViewerConfig(ViewerOptions &ViOptRef, bool Local = false);
	void EditorConfig(EditorOptions &EdOptRef, bool Local = false);
	void SetFolderInfoFiles();
	void InfoPanelSettings();
	static void MaskGroupsSettings();
	void AutoCompleteSettings();
	void TreeSettings();
	void SetFilePanelModes();
	void SetViewSettings(size_t Index, PanelViewSettings&& Data);
	void AddViewSettings(size_t Index, PanelViewSettings&& Data);
	void DeleteViewSettings(size_t Index);
	void ReadPanelModes();
	void SavePanelModes(bool always);

	std::vector<farconfig> Config;
	farconfig_mode CurrentConfig;
	std::vector<PanelViewSettings> m_ViewSettings;
	bool m_ViewSettingsChanged;
};

string GetFarIniString(const string& AppName, const string& KeyName, const string& Default);
int GetFarIniInt(const string& AppName, const string& KeyName, int Default);

clock_t GetRedrawTimeout();
