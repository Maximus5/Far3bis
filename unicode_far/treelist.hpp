﻿#ifndef TREELIST_HPP_B2FDA185_E801_437B_A9D7_F4D3CE6D40A4
#define TREELIST_HPP_B2FDA185_E801_437B_A9D7_F4D3CE6D40A4
#pragma once

/*
treelist.hpp

Tree panel
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

#include "panel.hpp"

enum
{
	MODALTREE_ACTIVE  =1,
	MODALTREE_PASSIVE =2,
	MODALTREE_FREE    =3
};


class TreeList: public Panel
{
public:
	struct TreeItem: ::noncopyable, swapable<TreeItem>
	{
		string strName;
		std::vector<int> Last;
		size_t Depth;

		TreeItem():
			Last(MAX_PATH/2),
			Depth(0)
		{
		}

		TreeItem(const string& Name):
			strName(Name),
			Last(MAX_PATH/2),
			Depth(0)
		{
		}

		TreeItem(TreeItem&& rhs) noexcept: Depth() { *this = std::move(rhs); }

		MOVE_OPERATOR_BY_SWAP(TreeItem);

		void swap(TreeItem& rhs) noexcept
		{
			using std::swap;
			strName.swap(rhs.strName);
			Last.swap(rhs.Last);
			swap(Depth, rhs.Depth);
		}

		operator string() const { return strName; }
	};

	TreeList(window_ptr Owner, bool IsPanel = true);

	virtual int ProcessKey(const Manager::Key& Key) override;
	virtual int ProcessMouse(const MOUSE_EVENT_RECORD *MouseEvent) override;
	virtual int GoToFile(const string& Name, BOOL OnlyPartName = FALSE) override;
	virtual int FindPartName(const string& Name, int Next, int Direct = 1) override;
	virtual void Update(int Mode) override;
	virtual const string& GetCurDir() const override;

	void SetRootDir(const string& NewRootDir);
	void ProcessEnter();
	int GetExitCode() const { return m_ExitCode; }
	const TreeItem* GetItem(size_t Index) const;

	static void AddTreeName(const string& Name);
	static void DelTreeName(const string& Name);
	static void RenTreeName(const string& SrcName, const string& DestName);
	static void ReadSubTree(const string& Path);
	static void ClearCache();
	static void ReadCache(const string& TreeRoot);
	static void FlushCache();

private:
	virtual ~TreeList();

	virtual __int64 VMProcess(int OpCode, void *vParam = nullptr, __int64 iParam = 0) override;
	virtual bool SetCurDir(const string& NewDir, bool ClosePanel, bool IsUpdated = true) override;
	virtual int GetCurName(string &strName, string &strShortName) const override;
	virtual void UpdateViewPanel() override;
	virtual void MoveToMouse(const MOUSE_EVENT_RECORD *MouseEvent) override;
	virtual bool GetPlainString(string& Dest, int ListPos) const override;
	virtual int GoToFile(long idxItem) override;
	virtual long FindFile(const string& Name, BOOL OnlyPartName = FALSE) override;
	virtual long FindFirst(const string& Name) override;
	virtual long FindNext(int StartPos, const string& Name) override;
	virtual size_t GetFileCount() const override { return m_ListData.size(); }
	virtual int GetFileName(string &strName, int Pos, DWORD &FileAttr) const override;
	virtual void SetTitle() override;
	virtual string GetTitle() const override;
	virtual void SetFocus() override;
	virtual void KillFocus() override;
	virtual void UpdateKeyBar() override;
	virtual int GetCurrentPos() const override;
	virtual int GetSelName(string *strName, DWORD &FileAttr, string *ShortName = nullptr, os::FAR_FIND_DATA *fd = nullptr) override;
	virtual void DisplayObject() override;
	virtual size_t GetSelCount() const override;

	int ReadTree();
	void DisplayTree(int Fast);
	void DisplayTreeName(const wchar_t *Name, size_t Pos);
	void Up(int Count);
	void Down(int Count);
	void Scroll(int Count);
	void CorrectPosition();
	bool FillLastData();
	int SetDirPosition(const string& NewDir);
	void GetRoot();
	Panel* GetRootPanel();
	void SyncDir();
	void SaveTreeFile();
	int ReadTreeFile();
	int GetNextNavPos() const;
	int GetPrevNavPos() const;
	bool SaveState();
	bool RestoreState();

	std::vector<TreeItem> m_ListData;
	std::vector<TreeItem> m_SavedListData;
	const string m_Empty; // bugbug
	string m_Root;
	size_t m_WorkDir;
	size_t m_SavedWorkDir;
	long m_GetSelPosition;
	int m_ExitCode; // актуально только для дерева, вызванного из копира!
};

#endif // TREELIST_HPP_B2FDA185_E801_437B_A9D7_F4D3CE6D40A4
