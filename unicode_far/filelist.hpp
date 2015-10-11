#pragma once

/*
filelist.hpp

�������� ������ - ����� �������
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
#include "dizlist.hpp"
#include "hilight.hpp"
#include "filesystemwatcher.hpp"

class FileFilter;

struct FileListItem: noncopyable, swapable<FileListItem>
{
	string strName;
	char Selected;
	char PrevSelected;
	char ShowFolderSize;
	char ShortNamePresent;
	const HighlightFiles::highlight_item* Colors;

	DWORD NumberOfLinks;
	DWORD NumberOfStreams;
	UINT64 UserFlags;
	void* UserData;
	FARPANELITEMFREECALLBACK Callback;

	size_t Position;
	int SortGroup;
	const wchar_t *DizText;
	bool DeleteDiz;
	string strOwner;
	wchar_t **CustomColumnData;
	size_t CustomColumnNumber;
	DWORD CRC32;

	//BUGBUG!!
	DWORD FileAttr;
	FILETIME CreationTime;
	FILETIME AccessTime;
	FILETIME WriteTime;
	FILETIME ChangeTime;

	unsigned __int64 FileSize;
	unsigned __int64 AllocationSize;
	unsigned __int64 StreamsSize;

	string strShortName;

	DWORD ReparseTag;

	std::unordered_map<string,string> ContentData;

	FileListItem():
		Selected(),
		PrevSelected(),
		ShowFolderSize(),
		ShortNamePresent(),
		Colors(),
		NumberOfLinks(),
		NumberOfStreams(),
		UserFlags(),
		UserData(),
		Callback(),
		Position(),
		SortGroup(),
		DizText(),
		DeleteDiz(),
		CustomColumnData(),
		CustomColumnNumber(),
		CRC32(),
		FileAttr(),
		CreationTime(),
		AccessTime(),
		WriteTime(),
		ChangeTime(),
		FileSize(),
		AllocationSize(),
		StreamsSize(),
		ReparseTag()
	{
	}

	FileListItem(FileListItem&& rhs) noexcept:
		Selected(),
		PrevSelected(),
		ShowFolderSize(),
		ShortNamePresent(),
		Colors(),
		NumberOfLinks(),
		NumberOfStreams(),
		UserFlags(),
		UserData(),
		Callback(),
		Position(),
		SortGroup(),
		DizText(),
		DeleteDiz(),
		CustomColumnData(),
		CustomColumnNumber(),
		CRC32(),
		FileAttr(),
		CreationTime(),
		AccessTime(),
		WriteTime(),
		ChangeTime(),
		FileSize(),
		AllocationSize(),
		StreamsSize(),
		ReparseTag()
	{
		*this = std::move(rhs);
	}

	~FileListItem();

	MOVE_OPERATOR_BY_SWAP(FileListItem);

	void swap(FileListItem& rhs) noexcept
	{
		using std::swap;
		strName.swap(rhs.strName);
		swap(Selected, rhs.Selected);
		swap(PrevSelected, rhs.PrevSelected);
		swap(ShowFolderSize, rhs.ShowFolderSize);
		swap(ShortNamePresent, rhs.ShortNamePresent);
		swap(Colors, rhs.Colors);
		swap(NumberOfLinks, rhs.NumberOfLinks);
		swap(NumberOfStreams, rhs.NumberOfStreams);
		swap(UserFlags, rhs.UserFlags);
		swap(UserData, rhs.UserData);
		swap(Callback, rhs.Callback);
		swap(Position, rhs.Position);
		swap(SortGroup, rhs.SortGroup);
		swap(DizText, rhs.DizText);
		swap(DeleteDiz, rhs.DeleteDiz);
		strOwner.swap(rhs.strOwner);
		swap(CustomColumnData, rhs.CustomColumnData);
		swap(CustomColumnNumber, rhs.CustomColumnNumber);
		swap(CRC32, rhs.CRC32);
		swap(FileAttr, rhs.FileAttr);
		swap(CreationTime, rhs.CreationTime);
		swap(AccessTime, rhs.AccessTime);
		swap(WriteTime, rhs.WriteTime);
		swap(ChangeTime, rhs.ChangeTime);
		swap(FileSize, rhs.FileSize);
		swap(AllocationSize, rhs.AllocationSize);
		swap(StreamsSize, rhs.StreamsSize);
		strShortName.swap(rhs.strShortName);
		swap(ReparseTag, rhs.ReparseTag);
		ContentData.swap(rhs.ContentData);
	}
};

struct PluginsListItem: noncopyable, swapable<PluginsListItem>
{
	PluginsListItem(PluginHandle* hPlugin, const string& HostFile, const string& PrevOriginalCurDir, int Modified, int PrevViewMode, int PrevSortMode, bool PrevSortOrder, bool PrevNumericSort, bool PrevCaseSensitiveSort, bool PrevDirectoriesFirst, const PanelViewSettings& PrevViewSettings):
		m_Plugin(hPlugin),
		m_HostFile(HostFile),
		m_PrevOriginalCurDir(PrevOriginalCurDir),
		m_Modified(Modified),
		m_PrevViewMode(PrevViewMode),
		m_PrevSortMode(PrevSortMode),
		m_PrevSortOrder(PrevSortOrder),
		m_PrevNumericSort(PrevNumericSort),
		m_PrevCaseSensitiveSort(PrevCaseSensitiveSort),
		m_PrevDirectoriesFirst(PrevDirectoriesFirst),
		m_PrevViewSettings(PrevViewSettings.clone())
	{}

	PluginsListItem(PluginsListItem&& rhs) noexcept:
		m_Plugin(),
		m_HostFile(),
		m_PrevOriginalCurDir(),
		m_Modified(),
		m_PrevViewMode(),
		m_PrevSortMode(),
		m_PrevSortOrder(),
		m_PrevNumericSort(),
		m_PrevCaseSensitiveSort(),
		m_PrevDirectoriesFirst(),
		m_PrevViewSettings()
	{
		*this = std::move(rhs);
	}

	MOVE_OPERATOR_BY_SWAP(PluginsListItem);

	void swap(PluginsListItem& rhs) noexcept
	{
		using std::swap;
		swap(m_Plugin, rhs.m_Plugin);
		m_HostFile.swap(rhs.m_HostFile);
		m_PrevOriginalCurDir.swap(rhs.m_PrevOriginalCurDir);
		swap(m_Modified, rhs.m_Modified);
		swap(m_PrevViewMode, rhs.m_PrevViewMode);
		swap(m_PrevSortMode, rhs.m_PrevSortMode);
		swap(m_PrevSortOrder, rhs.m_PrevSortOrder);
		swap(m_PrevNumericSort, rhs.m_PrevNumericSort);
		swap(m_PrevCaseSensitiveSort, rhs.m_PrevCaseSensitiveSort);
		swap(m_PrevDirectoriesFirst, rhs.m_PrevDirectoriesFirst);
		m_PrevViewSettings.swap(rhs.m_PrevViewSettings);
	}

	PluginHandle* m_Plugin;
	string m_HostFile;
	string m_PrevOriginalCurDir;
	int m_Modified;
	int m_PrevViewMode;
	int m_PrevSortMode;
	bool m_PrevSortOrder;
	bool m_PrevNumericSort;
	bool m_PrevCaseSensitiveSort;
	bool m_PrevDirectoriesFirst;
	PanelViewSettings m_PrevViewSettings;
};

ENUM(OPENFILEPLUGINTYPE);

class FileList:public Panel
{
public:
	FileList(window_ptr Owner);

	virtual int ProcessKey(const Manager::Key& Key) override;
	virtual int ProcessMouse(const MOUSE_EVENT_RECORD *MouseEvent) override;
	virtual __int64 VMProcess(int OpCode,void *vParam=nullptr,__int64 iParam=0) override;
	virtual void MoveToMouse(const MOUSE_EVENT_RECORD *MouseEvent) override;
	virtual void SetFocus() override;
	virtual void Update(int Mode) override;
	virtual bool UpdateIfChanged(bool Idle) override;
	virtual void UpdateIfRequired() override;
	virtual int SendKeyToPlugin(DWORD Key,bool Pred=false) override;
	virtual void StartFSWatcher(bool got_focus=false, bool check_time=true) override;
	virtual void StopFSWatcher() override;
	virtual void SortFileList(int KeepPosition) override;
	virtual void SetViewMode(int ViewMode) override;
	virtual void SetSortMode(int SortMode, bool KeepOrder = false) override;
	virtual void SetCustomSortMode(int SortMode, bool KeepOrder = false, bool InvertByDefault = false) override;
	virtual void ChangeSortOrder(bool Reverse) override;
	virtual void ChangeNumericSort(bool Mode) override;
	virtual void ChangeCaseSensitiveSort(bool Mode) override;
	virtual void ChangeDirectoriesFirst(bool Mode) override;
	virtual bool SetCurDir(const string& NewDir,bool ClosePanel,bool IsUpdated=true) override;
	virtual int GetPrevSortMode() const override;
	virtual bool GetPrevSortOrder() const override;
	virtual int GetPrevViewMode() const override;
	virtual bool GetPrevNumericSort() const override;
	virtual bool GetPrevCaseSensitiveSort() const override;
	virtual bool GetPrevDirectoriesFirst() const override;
	virtual int GetFileName(string &strName, int Pos, DWORD &FileAttr) const override;
	virtual int GetCurrentPos() const override;
	virtual int FindPartName(const string& Name,int Next,int Direct=1) override;
	virtual bool GetPlainString(string& Dest, int ListPos) const override;
	virtual int GoToFile(long idxItem) override;
	virtual int GoToFile(const string& Name,BOOL OnlyPartName=FALSE) override;
	virtual long FindFile(const string& Name,BOOL OnlyPartName=FALSE) override;
	virtual int IsSelected(const string& Name) override;
	virtual int IsSelected(size_t idxItem) override;
	virtual long FindFirst(const string& Name) override;
	virtual long FindNext(int StartPos, const string& Name) override;
	virtual void UpdateViewPanel() override;
	virtual void CompareDir() override;
	virtual void ClearSelection() override;
	virtual void SaveSelection() override;
	virtual void RestoreSelection() override;
	virtual void EditFilter() override;
	virtual bool FileInFilter(size_t idxItem) override;
	virtual bool FilterIsEnabled() override;
	virtual void ReadDiz(PluginPanelItem *ItemList=nullptr,int ItemLength=0, DWORD dwFlags=0) override;
	virtual void DeleteDiz(const string& Name, const string& ShortName) override;
	virtual void FlushDiz() override;
	virtual void GetDizName(string &strDizName) const override;
	virtual void CopyDiz(const string& Name, const string& ShortName, const string& DestName, const string& DestShortName, DizList *DestDiz) override;
	virtual bool IsDizDisplayed() override;
	virtual bool IsColumnDisplayed(int Type) override;
	virtual int GetColumnsCount() const override { return m_Columns; }
	virtual void SetReturnCurrentFile(int Mode) override;
	virtual void GetOpenPanelInfo(OpenPanelInfo *Info) const override;
	virtual void SetPluginMode(PluginHandle* hPlugin,const string& PluginFile,bool SendOnFocus=false) override;
	virtual size_t GetSelCount() const override;
	virtual int GetSelName(string *strName, DWORD &FileAttr, string *strShortName = nullptr, os::FAR_FIND_DATA *fde = nullptr) override;
	virtual void UngetSelName() override;
	virtual void ClearLastGetSelection() override;
	virtual unsigned __int64 GetLastSelectedSize() const override;
	virtual PluginHandle* GetPluginHandle() const override;
	virtual size_t GetRealSelCount() const override;
	virtual void SetPluginModified() override;
	virtual int ProcessPluginEvent(int Event,void *Param) override;
	virtual void SetTitle() override;
	virtual size_t GetFileCount() const override { return m_ListData.size(); }
	virtual void UpdateKeyBar() override;
	virtual void IfGoHome(wchar_t Drive) override;
	virtual bool GetSelectedFirstMode() const override { return SelectedFirst; }

	const FileListItem* GetItem(size_t Index) const;
	const FileListItem* GetLastSelectedItem() const;

	PluginHandle* OpenFilePlugin(const string* FileName,int PushPrev, OPENFILEPLUGINTYPE Type);
	void ProcessHostFile();
	bool GetPluginInfo(PluginInfo *PInfo);
	void PluginGetPanelInfo(PanelInfo &Info);
	size_t PluginGetPanelItem(int ItemNumber,FarGetPluginPanelItem *Item);
	size_t PluginGetSelectedPanelItem(int ItemNumber,FarGetPluginPanelItem *Item);
	void PluginGetColumnTypesAndWidths(string& strColumnTypes,string& strColumnWidths);
	void PluginBeginSelection();
	void PluginSetSelection(int ItemNumber,bool Selection);
	void PluginClearSelection(int SelectedItemNumber);
	void PluginEndSelection();
	int PluginPanelHelp(const PluginHandle* hPlugin) const;
	void ResetLastUpdateTime() {LastUpdateTime = 0;}

	static size_t FileListToPluginItem2(const FileListItem& fi,FarGetPluginPanelItem* pi);
	static int FileNameToPluginItem(const string& Name,PluginPanelItem& pi);
	static void FileListToPluginItem(const FileListItem& fi,PluginPanelItem& pi);
	static void PluginToFileListItem(const PluginPanelItem& pi,FileListItem& fi);
	static bool IsModeFullScreen(int Mode);
	static string &AddPluginPrefix(const FileList *SrcPanel,string &strPrefix);

	struct PrevDataItem;

protected:
	virtual void ClearAllItem() override;

private:
	virtual ~FileList();
	virtual void SetSelectedFirstMode(bool Mode) override;
	virtual void DisplayObject() override;
	virtual int GetCurName(string &strName, string &strShortName) const override;
	virtual int GetCurBaseName(string &strName, string &strShortName) const override;

	void ApplySortMode(int Mode);
	void DeleteListData(std::vector<FileListItem>& ListData);
	void ToBegin();
	void ToEnd();
	void MoveCursor(int offset);
	void Scroll(int offset);
	void CorrectPosition();
	void ShowFileList(int Fast);
	void ShowList(int ShowStatus,int StartColumn);
	void SetShowColor(int Position, bool FileColor = true) const;
	FarColor GetShowColor(int Position, bool FileColor = true) const;
	void ShowSelectedSize();
	void ShowTotalSize(const OpenPanelInfo &Info);
	int ConvertName(const wchar_t *SrcName, string &strDest, int MaxLength, unsigned __int64 RightAlign, int ShowStatus, DWORD dwFileAttr) const;
	void Select(FileListItem& SelItem, int Selection);
	long SelectFiles(int Mode,const wchar_t *Mask=nullptr);
	void ProcessEnter(bool EnableExec,bool SeparateWindow, bool EnableAssoc, bool RunAs, OPENFILEPLUGINTYPE Type);
	// ChangeDir ���������� false, e��� �� ������ ��������� �������� ����
	bool ChangeDir(const string& NewDir,bool ResolvePath=false,bool IsUpdated=true,const FileListItem *CurPtr=nullptr);
	void CountDirSize(UINT64 PluginFlags);
	void ReadFileNames(int KeepSelection, int UpdateEvenIfPanelInvisible, int DrawMessage);
	void UpdatePlugin(int KeepSelection, int UpdateEvenIfPanelInvisible);
	void MoveSelection(std::vector<FileListItem>& From, std::vector<FileListItem>& To);
	void PushPlugin(PluginHandle* hPlugin,const string& HostFile);
	int PopPlugin(int EnableRestoreViewMode);
	void PopPrevData(const string& DefaultName,bool Closed,bool UsePrev,bool Position,bool SetDirectorySuccess);
	void CopyFiles(bool bMoved=false);
	void CopyNames(bool FillPathName, bool UNC);
	void SelectSortMode();
	bool ApplyCommand();
	void DescribeFiles();
	std::vector<PluginPanelItem> CreatePluginItemList(bool AddTwoDot = true);
	PluginHandle* OpenPluginForFile(const string* FileName,DWORD FileAttr, OPENFILEPLUGINTYPE Type);
	int PreparePanelView(PanelViewSettings *PanelView);
	int PrepareColumnWidths(std::vector<column>& Columns, bool FullScreen, bool StatusLine);
	void PrepareViewSettings(int ViewMode, const OpenPanelInfo *PlugInfo);
	void PluginDelete();
	void PutDizToPlugin(FileList *DestPanel, const std::vector<PluginPanelItem>& ItemList, int Delete, int Move, DizList *SrcDiz);
	void PluginGetFiles(const wchar_t **DestPath,int Move);
	void PluginToPluginFiles(int Move);
	void PluginHostGetFiles();
	void PluginPutFilesToNew();
	int PluginPutFilesToAnother(int Move,Panel *AnotherPanel);
	void ProcessPluginCommand();
	void PluginClearSelection(const std::vector<PluginPanelItem>& ItemList);
	void ProcessCopyKeys(int Key);
	void ReadSortGroups(bool UpdateFilterCurrentTime=true);
	int ProcessOneHostFile(const FileListItem* Item);
	void HighlightBorder(int Level, int ListPos) const;
	void InitFSWatcher(bool CheckTree);
	bool IsColumnDisplayed(std::function<bool(const column&)> Compare);

	static void DeletePluginItemList(std::vector<PluginPanelItem> &ItemList);
	static void AddParentPoint(FileListItem *CurPtr, size_t CurFilePos, const FILETIME* Times = nullptr, const string& Owner = string());

	std::unique_ptr<FileFilter> m_Filter;
	DizList Diz;
	int DizRead;
	/* $ 09.11.2001 IS
	     ����������� � ����������� ������, ������� ������������ ��� ������
	     �����, ������� �� ���������� � ������. �� ��������� - �������� ������.
	*/
	wchar_t openBracket[2], closeBracket[2];

	string strOriginalCurDir;
	string strPluginDizName;
	std::vector<FileListItem> m_ListData;
	PluginHandle* m_hPlugin;
	std::list<PrevDataItem> PrevDataList;
	std::list<PluginsListItem> PluginsList;
	FileSystemWatcher FSWatcher;
	long UpperFolderTopFile,LastCurFile;
	long ReturnCurrentFile;
	size_t SelFileCount;
	long GetSelPosition,LastSelPosition;
	size_t TotalFileCount;
	unsigned __int64 SelFileSize;
	unsigned __int64 TotalFileSize;
	unsigned __int64 FreeDiskSize;
	clock_t LastUpdateTime;
	int m_Height;
	int m_Columns;

	int ColumnsInGlobal;

	int LeftPos;
	int ShiftSelection;
	int MouseSelection;
	bool SelectedFirst;
	int empty; // ��������� �� ��������� ������ �������
	int AccessTimeUpdateRequired;

	int UpdateRequired,UpdateRequiredMode,UpdateDisabled;
	int SortGroupsRead;
	int InternalProcessKey;

	long CacheSelIndex,CacheSelPos;
	long CacheSelClearIndex,CacheSelClearPos;

	wchar_t CustomSortIndicator[2];
};
