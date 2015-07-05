#pragma once

/*
filefilterparams.hpp

��������� ��������� �������
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

#include "filemasks.hpp"
#include "hilight.hpp"

enum
{
	DEFAULT_SORT_GROUP = 10000,
};

enum enumFileFilterFlagsType
{
	FFFT_FIRST = 0, //������ ���� ������

	FFFT_LEFTPANEL = FFFT_FIRST,
	FFFT_RIGHTPANEL,
	FFFT_FINDFILE,
	FFFT_COPY,
	FFFT_SELECT,
	FFFT_CUSTOM,

	FFFT_COUNT, //������ ���� ���������
};

enum enumFileFilterFlags
{
	FFF_NONE    = 0x00000000,
	FFF_INCLUDE = 0x00000001,
	FFF_EXCLUDE = 0x00000002,
	FFF_STRONG  = 0x10000000
};

enum enumFDateType
{
	FDATE_MODIFIED=0,
	FDATE_CREATED,
	FDATE_OPENED,
	FDATE_CHANGED,

	FDATE_COUNT, // ������ ��������� !!!
};

class FileFilterParams: noncopyable, swapable<FileFilterParams>
{
public:
	FileFilterParams();
	FileFilterParams(FileFilterParams&& rhs) noexcept;
	MOVE_OPERATOR_BY_SWAP(FileFilterParams);

	void swap(FileFilterParams& rhs) noexcept
	{
		using std::swap;
		m_strTitle.swap(rhs.m_strTitle);
		FMask.swap(rhs.FMask);
		swap(FDate, rhs.FDate);
		swap(FSize, rhs.FSize);
		swap(FHardLinks, rhs.FHardLinks);
		swap(FAttr, rhs.FAttr);
		swap(FHighlight, rhs.FHighlight);
		FFlags.swap(rhs.FFlags);
	}

	FileFilterParams Clone() const;

	void SetTitle(const string& Title);
	void SetMask(bool Used, const string& Mask);
	void SetDate(bool Used, DWORD DateType, FILETIME DateAfter, FILETIME DateBefore, bool bRelative);
	void SetSize(bool Used, const string& SizeAbove, const string& SizeBelow);
	void SetHardLinks(bool Used,DWORD HardLinksAbove, DWORD HardLinksBelow);
	void SetAttr(bool Used, DWORD AttrSet, DWORD AttrClear);
	void SetColors(const HighlightFiles::highlight_item& Colors);
	void SetSortGroup(int SortGroup) { FHighlight.SortGroup = SortGroup; }
	void SetContinueProcessing(bool bContinueProcessing) { FHighlight.bContinueProcessing = bContinueProcessing; }
	void SetFlags(enumFileFilterFlagsType FType, DWORD Flags) { FFlags[FType] = Flags; }
	void ClearAllFlags() { FFlags.fill(0); }

	const string& GetTitle() const;
	const string& GetMask() const { return FMask.strMask; }
	bool IsMaskUsed() const { return FMask.Used; }
	bool  GetDate(DWORD *DateType, FILETIME *DateAfter, FILETIME *DateBefore, bool *bRelative) const;
	bool IsSizeUsed() const {return FSize.Used;}
	const string& GetSizeAbove() const {return FSize.SizeAbove;}
	const string& GetSizeBelow() const {return FSize.SizeBelow;}
	bool  GetHardLinks(DWORD *HardLinksAbove, DWORD *HardLinksBelow) const;
	bool  GetAttr(DWORD *AttrSet, DWORD *AttrClear) const;
	HighlightFiles::highlight_item GetColors() const;
	wchar_t GetMarkChar() const;
	int   GetSortGroup() const { return FHighlight.SortGroup; }
	bool  GetContinueProcessing() const { return FHighlight.bContinueProcessing; }
	DWORD GetFlags(enumFileFilterFlagsType FType) const { return FFlags[FType]; }
	void RefreshMask() {if(FMask.Used) FMask.FilterMask.Set(FMask.strMask, FMF_SILENT);}


	// ������ ����� ���������� "�������" � ������ ��� �����������:
	// �������� �� ���� fd ��� ������� �������������� �������.
	// ���������� true  - ��������;
	//            false - �� ��������.
	bool FileInFilter(const FileListItem* fli, unsigned __int64 CurrentTime) const;
	bool FileInFilter(const os::FAR_FIND_DATA& fde, unsigned __int64 CurrentTime,const string* FullName=nullptr) const; //Used in dirinfo, copy, findfile
	bool FileInFilter(const PluginPanelItem& fd, unsigned __int64 CurrentTime) const;


private:
	string m_strTitle;

	struct fmask:noncopyable, swapable<fmask>
	{
		bool Used;
		string strMask;
		filemasks FilterMask; // ��������� ���������������� �����.

		fmask():Used(false) {}
		fmask(fmask&& rhs) noexcept: Used(false) { *this = std::move(rhs); }
		MOVE_OPERATOR_BY_SWAP(fmask);

		void swap(fmask& rhs) noexcept
		{
			using std::swap;
			swap(Used, rhs.Used);
			strMask.swap(rhs.strMask);
			FilterMask.swap(rhs.FilterMask);
		}
	} FMask;

	struct
	{
		uint64_t DateAfter;
		uint64_t DateBefore;
		enumFDateType DateType;
		bool Used;
		bool bRelative;
	} FDate;

	struct f_size
	{
		// ctor required, VC doesn't support C++03 value initialization for non-POD types
		f_size():
			SizeAboveReal(),
			SizeBelowReal(),
			Used()
		{}

		unsigned __int64 SizeAboveReal; // ����� ������ ����� ������ � ������
		unsigned __int64 SizeBelowReal; // ����� ������ ����� ������ � ������
		string SizeAbove; // ����� ������ ����� ������ ��� ��� ��� ����
		string SizeBelow; // ����� ������ ����� ������ ��� ��� ��� ����
		bool Used;
	} FSize;

	struct // ����� ��������� � �������, ����� ������� ���������� ������� ������. ���� ��� ������� ���������� ������ ���� Used � �������� �������� ������� "������ ������ ��� ����"
	{
		bool Used;
		DWORD CountAbove;
		DWORD CountBelow;
	} FHardLinks;

	struct
	{
		bool Used;
		DWORD AttrSet;
		DWORD AttrClear;
	} FAttr;

	struct
	{
		HighlightFiles::highlight_item Colors;
		int SortGroup;
		bool bContinueProcessing;
	} FHighlight;

	std::array<DWORD, FFFT_COUNT> FFlags;
};

bool FileFilterConfig(FileFilterParams *FF, bool ColorConfig=false);

//���������������� ������� ��� �������� ����� ���� ��������� ��������.
string MenuString(const FileFilterParams* FF, bool bHighlightType=false, int Hotkey=0, bool bPanelType=false, const wchar_t *FMask=nullptr, const wchar_t *Title=nullptr);
