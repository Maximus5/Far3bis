﻿/*
filefilterparams.cpp

Параметры Файлового фильтра
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

#include "farcolor.hpp"
#include "filemasks.hpp"
#include "dialog.hpp"
#include "filelist.hpp"
#include "filefilterparams.hpp"
#include "colormix.hpp"
#include "message.hpp"
#include "interf.hpp"
#include "datetime.hpp"
#include "strmix.hpp"
#include "console.hpp"
#include "flink.hpp"
#include "lang.hpp"
#include "locale.hpp"
#include "fileattr.hpp"
#include "DlgGuid.hpp"

FileFilterParams::FileFilterParams():
	FDate(),
	FSize(),
	FHardLinks(),
	FAttr(),
	FHighlight(),
	FFlags()
{
	SetMask(true, L"*");

	std::for_each(RANGE(FHighlight.Colors.Color, i)
	{
		MAKE_OPAQUE(i.FileColor.ForegroundColor);
		MAKE_OPAQUE(i.MarkColor.ForegroundColor);
	});

	FHighlight.SortGroup=DEFAULT_SORT_GROUP;
}

FileFilterParams FileFilterParams::Clone() const
{
	FileFilterParams Result;
	Result.m_strTitle = m_strTitle;
	Result.SetMask(IsMaskUsed(), GetMask());
	Result.FSize = FSize;
	Result.FDate = FDate;
	Result.FAttr = FAttr;
	Result.FHardLinks = FHardLinks;
	Result.FHighlight.Colors = FHighlight.Colors;
	Result.FHighlight.SortGroup = FHighlight.SortGroup;
	Result.FHighlight.bContinueProcessing = FHighlight.bContinueProcessing;
	Result.FFlags = FFlags;
	return Result;
}

void FileFilterParams::SetTitle(const string& Title)
{
	m_strTitle = Title;
}

void FileFilterParams::SetMask(bool Used, const string& Mask)
{
	FMask.Used = Used;
	FMask.strMask = Mask;
	if (Used)
	{
		FMask.FilterMask.Set(FMask.strMask, FMF_SILENT);
	}
}

void FileFilterParams::SetDate(bool Used, enumFDateType DateType, FILETIME DateAfter, FILETIME DateBefore, bool bRelative)
{
	FDate.Used=Used;
	FDate.DateType = DateType;

	if (DateType>=FDATE_COUNT)
		FDate.DateType=FDATE_MODIFIED;

	FDate.DateAfter = FileTimeToUI64(DateAfter);
	FDate.DateBefore = FileTimeToUI64(DateBefore);
	FDate.bRelative=bRelative;
}

void FileFilterParams::SetSize(bool Used, const string& SizeAbove, const string& SizeBelow)
{
	FSize.Used=Used;
	FSize.SizeAbove = SizeAbove;
	FSize.SizeBelow = SizeBelow;
	FSize.SizeAboveReal=ConvertFileSizeString(FSize.SizeAbove);
	FSize.SizeBelowReal=ConvertFileSizeString(FSize.SizeBelow);
}

void FileFilterParams::SetHardLinks(bool Used, DWORD HardLinksAbove, DWORD HardLinksBelow)
{
	FHardLinks.Used=Used;
	FHardLinks.CountAbove=HardLinksAbove;
	FHardLinks.CountBelow=HardLinksBelow;
}

void FileFilterParams::SetAttr(bool Used, DWORD AttrSet, DWORD AttrClear)
{
	FAttr.Used=Used;
	FAttr.AttrSet=AttrSet;
	FAttr.AttrClear=AttrClear;
}

void FileFilterParams::SetColors(const highlight::element& Colors)
{
	FHighlight.Colors = Colors;
}

const string& FileFilterParams::GetTitle() const
{
	return m_strTitle;
}

bool FileFilterParams::GetDate(DWORD *DateType, FILETIME *DateAfter, FILETIME *DateBefore, bool *bRelative) const
{
	if (DateType)
		*DateType=FDate.DateType;

	if (DateAfter)
	{
		*DateAfter = UI64ToFileTime(FDate.DateAfter);
	}

	if (DateBefore)
	{
		*DateBefore = UI64ToFileTime(FDate.DateBefore);
	}

	if (bRelative)
		*bRelative=FDate.bRelative;

	return FDate.Used;
}

bool FileFilterParams::GetHardLinks(DWORD *HardLinksAbove, DWORD *HardLinksBelow) const
{
	if (HardLinksAbove)
		*HardLinksAbove = FHardLinks.CountAbove;
	if (HardLinksBelow)
		*HardLinksBelow = FHardLinks.CountBelow;
	return FHardLinks.Used;
}


bool FileFilterParams::GetAttr(DWORD *AttrSet, DWORD *AttrClear) const
{
	if (AttrSet)
		*AttrSet=FAttr.AttrSet;

	if (AttrClear)
		*AttrClear=FAttr.AttrClear;

	return FAttr.Used;
}

highlight::element FileFilterParams::GetColors() const
{
	return FHighlight.Colors;
}

wchar_t FileFilterParams::GetMarkChar() const
{
	return FHighlight.Colors.Mark.Char;
}

struct filter_file_object
{
	string Name;
	unsigned long long Size;
	FILETIME CreationTime;
	FILETIME ModificationTime;
	FILETIME AccessTime;
	FILETIME ChangeTime;
	DWORD Attributes;
	int NumberOfLinks;
};

bool FileFilterParams::FileInFilter(const FileListItem* fli, const FileList* Owner, unsigned long long CurrentTime) const
{
	filter_file_object Object;
	Object.Attributes = fli->FileAttr;
	Object.Size = fli->FileSize;
	Object.CreationTime = fli->CreationTime;
	Object.ModificationTime = fli->WriteTime;
	Object.AccessTime = fli->AccessTime;
	Object.ChangeTime = fli->ChangeTime;
	Object.Name = fli->strName;

	return FileInFilter(Object, CurrentTime, [&](filter_file_object& Item)
	{
		Item.NumberOfLinks = fli->NumberOfLinks(Owner);
	});
}

bool FileFilterParams::FileInFilter(const os::FAR_FIND_DATA& fde, unsigned long long CurrentTime,const string* FullName) const
{
	filter_file_object Object;
	Object.Attributes = fde.dwFileAttributes;
	Object.Size = fde.nFileSize;
	Object.CreationTime = fde.ftCreationTime;
	Object.ModificationTime = fde.ftLastWriteTime;
	Object.AccessTime = fde.ftLastAccessTime;
	Object.ChangeTime = fde.ftChangeTime;
	Object.Name = fde.strFileName;

	return FileInFilter(Object, CurrentTime, [&](filter_file_object& Item)
	{
		Item.NumberOfLinks = FullName? GetNumberOfLinks(*FullName) : 1;
	});
}

bool FileFilterParams::FileInFilter(const PluginPanelItem& fd, unsigned long long CurrentTime) const
{
	filter_file_object Object;
	Object.Attributes = fd.FileAttributes;
	Object.Size = fd.FileSize;
	Object.CreationTime = fd.CreationTime;
	Object.ModificationTime = fd.LastWriteTime;
	Object.AccessTime = fd.LastAccessTime;
	Object.ChangeTime = fd.ChangeTime;
	Object.Name = fd.FileName;
	Object.NumberOfLinks = fd.NumberOfLinks;

	return FileInFilter(Object, CurrentTime, nullptr);
}

bool FileFilterParams::FileInFilter(filter_file_object& Object, unsigned long long CurrentTime, const std::function<void(filter_file_object&)>& Getter) const
{
	// Режим проверки атрибутов файла включен?
	if (FAttr.Used)
	{
		// Проверка попадания файла по установленным атрибутам
		if ((Object.Attributes & FAttr.AttrSet) != FAttr.AttrSet)
			return false;

		// Проверка попадания файла по отсутствующим атрибутам
		if (Object.Attributes & FAttr.AttrClear)
			return false;
	}

	// Режим проверки размера файла включен?
	if (FSize.Used)
	{
		if (!FSize.SizeAbove.empty())
		{
			if (Object.Size < FSize.SizeAboveReal) // Размер файла меньше минимального разрешённого по фильтру?
				return false;                          // Не пропускаем этот файл
		}

		if (!FSize.SizeBelow.empty())
		{
			if (Object.Size > FSize.SizeBelowReal) // Размер файла больше максимального разрешённого по фильтру?
				return false;                          // Не пропускаем этот файл
		}
	}

	// Режим проверки количества жестких ссылок на файл включен?
	// Пока что, при включенном условии, срабатывание происходит при случае "ссылок больше чем одна"
	if (FHardLinks.Used)
	{
		if (Object.Attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			return false;
		}

		if (Getter)
			Getter(Object);

		if (Object.NumberOfLinks < 2)
		{
			return false;
		}
	}

	// Режим проверки времени файла включен?
	if (FDate.Used)
	{
		auto before = FDate.DateBefore, after = FDate.DateAfter;

		if (after || before)
		{
			const FILETIME* ft = nullptr;

			switch (FDate.DateType)
			{
				case FDATE_CREATED:
					ft = &Object.CreationTime;
					break;
				case FDATE_OPENED:
					ft = &Object.AccessTime;
					break;
				case FDATE_CHANGED:
					ft = &Object.ChangeTime;
					break;
				case FDATE_MODIFIED:
					ft = &Object.ModificationTime;
					break;

				// dummy label to make compiler happy
				case FDATE_COUNT:
					break;
			}

			if (ft)
			{
				if (FDate.bRelative)
				{
					if (after)
						after = CurrentTime - after;

					if (before)
						before = CurrentTime - before;
				}

				const auto ftime = FileTimeToUI64(*ft);

				// Есть введённая пользователем начальная дата?
				if (after)
				{
					// Дата файла меньше начальной даты по фильтру?
					if (ftime < after)
						// Не пропускаем этот файл
						return false;
				}

				// Есть введённая пользователем конечная дата?
				if (before)
				{
					// Дата файла больше конечной даты по фильтру?
					if (ftime > before)
						return false;
				}
			}
		}
	}

	// Режим проверки маски файла включен?
	if (FMask.Used)
	{
		// ЭТО ЕСТЬ УЗКОЕ МЕСТО ДЛЯ СКОРОСТНЫХ ХАРАКТЕРИСТИК Far Manager
		// при считывании директории

		// Файл не попадает под маску введённую в фильтре?
		if (!FMask.FilterMask.Compare(Object.Name))
			// Не пропускаем этот файл
			return false;
	}

	// Да! Файл выдержал все испытания и будет допущен к использованию
	// в вызвавшей эту функцию операции.
	return true;
}

//Централизованная функция для создания строк меню различных фильтров.
string MenuString(const FileFilterParams *FF, bool bHighlightType, wchar_t Hotkey, bool bPanelType, const wchar_t *FMask, const wchar_t *Title)
{
	string strDest;

	const wchar_t DownArrow = L'\x2193';
	const wchar_t *Name, *Mask;
	wchar_t MarkChar[]=L"' '";
	DWORD IncludeAttr, ExcludeAttr;
	bool UseMask, UseSize, UseHardLinks, UseDate, RelativeDate;

	if (bPanelType)
	{
		Name=Title;
		UseMask=true;
		Mask=FMask;
		IncludeAttr=0;
		ExcludeAttr=FILE_ATTRIBUTE_DIRECTORY;
		RelativeDate=UseDate=UseSize=UseHardLinks=false;
	}
	else
	{
		MarkChar[1] = FF->GetMarkChar();

		if (!MarkChar[1])
			*MarkChar=0;

		Name=FF->GetTitle().data();
		Mask = FF->GetMask().data();
		UseMask=FF->IsMaskUsed();

		if (!FF->GetAttr(&IncludeAttr,&ExcludeAttr))
			IncludeAttr=ExcludeAttr=0;

		UseSize=FF->IsSizeUsed();
		UseDate=FF->GetDate(nullptr,nullptr,nullptr,&RelativeDate);
		UseHardLinks=FF->GetHardLinks(nullptr,nullptr);
	}

	string Attr;

	enum_attributes([&](DWORD Attribute, wchar_t Character)
	{
		if (IncludeAttr & Attribute)
		{
			Attr.push_back(Character);
			Attr.push_back(L'+');
		}
		else if (ExcludeAttr & Attribute)
		{
			Attr.push_back(Character);
			Attr.push_back(L'-');
		}
		else
		{
			Attr.push_back(L'.');
			Attr.push_back(L'.');
		}
		return true;
	});

	wchar_t SizeDate[] = L"....";

	if (UseSize)
	{
		SizeDate[0]=L'S';
	}

	if (UseDate)
	{
		if (RelativeDate)
			SizeDate[1]=L'R';
		else
			SizeDate[1]=L'D';
	}

	if (UseHardLinks)
	{
		SizeDate[2]=L'H';
	}

	if (bHighlightType)
	{
		if (FF->GetContinueProcessing())
			SizeDate[3]=DownArrow;
		strDest = format(L"{1:3} {0} {2} {3} {0} {4}", BoxSymbols[BS_V1], MarkChar, Attr, SizeDate, UseMask? Mask : L"");
	}
	else
	{
		SizeDate[3]=0;

		if (!Hotkey && !bPanelType)
		{
			strDest = format(L"{1:{2}.{2}} {0} {3} {4} {0} {5}", BoxSymbols[BS_V1], Name, 21 + (wcschr(Name, L'&')? 1 : 0), Attr, SizeDate, UseMask? Mask : L"");
		}
		else
		{
			if (Hotkey)
				strDest = format(L"&{1}. {2:18.18} {0} {3} {4} {0} {5}", BoxSymbols[BS_V1], Hotkey, Name, Attr, SizeDate, UseMask? Mask : L"");
			else
				strDest = format(L"   {1:18.18} {0} {2} {3} {0} {4}", BoxSymbols[BS_V1], Name, Attr, SizeDate, UseMask? Mask : L"");
		}
	}

	RemoveTrailingSpaces(strDest);
	return strDest;
}

enum enumFileFilterConfig
{
	ID_FF_TITLE,

	ID_FF_NAME,
	ID_FF_NAMEEDIT,

	ID_FF_SEPARATOR1,

	ID_FF_MATCHMASK,
	ID_FF_MASKEDIT,

	ID_FF_SEPARATOR2,

	ID_FF_MATCHSIZE,
	ID_FF_SIZEFROMSIGN,
	ID_FF_SIZEFROMEDIT,
	ID_FF_SIZETOSIGN,
	ID_FF_SIZETOEDIT,

	ID_FF_MATCHDATE,
	ID_FF_DATETYPE,
	ID_FF_DATERELATIVE,
	ID_FF_DATEBEFORESIGN,
	ID_FF_DATEBEFOREEDIT,
	ID_FF_DAYSBEFOREEDIT,
	ID_FF_TIMEBEFOREEDIT,
	ID_FF_DATEAFTERSIGN,
	ID_FF_DATEAFTEREDIT,
	ID_FF_DAYSAFTEREDIT,
	ID_FF_TIMEAFTEREDIT,
	ID_FF_CURRENT,
	ID_FF_BLANK,

	ID_FF_SEPARATOR3,
	ID_FF_SEPARATOR4,

	ID_FF_MATCHATTRIBUTES,
	ID_FF_READONLY,
	ID_FF_ARCHIVE,
	ID_FF_HIDDEN,
	ID_FF_SYSTEM,
	ID_FF_COMPRESSED,
	ID_FF_ENCRYPTED,
	ID_FF_NOTINDEXED,
	ID_FF_DIRECTORY,
	ID_FF_SPARSE,
	ID_FF_TEMP,
	ID_FF_OFFLINE,
	ID_FF_REPARSEPOINT,
	ID_FF_VIRTUAL,
	ID_FF_INTEGRITY_STREAM,
	ID_FF_NO_SCRUB_DATA,

	ID_HER_SEPARATOR1,
	ID_HER_MARK_TITLE,
	ID_HER_MARKEDIT,
	ID_HER_MARKTRANSPARENT,

	ID_HER_NORMALFILE,
	ID_HER_NORMALMARKING,
	ID_HER_SELECTEDFILE,
	ID_HER_SELECTEDMARKING,
	ID_HER_CURSORFILE,
	ID_HER_CURSORMARKING,
	ID_HER_SELECTEDCURSORFILE,
	ID_HER_SELECTEDCURSORMARKING,

	ID_HER_COLOREXAMPLE,
	ID_HER_CONTINUEPROCESSING,

	ID_FF_SEPARATOR5,

	ID_FF_HARDLINKS,

	ID_FF_SEPARATOR6,

	ID_FF_OK,
	ID_FF_RESET,
	ID_FF_CANCEL,
	ID_FF_MAKETRANSPARENT,
};

void HighlightDlgUpdateUserControl(FAR_CHAR_INFO *VBufColorExample, const highlight::element &Colors)
{
	const wchar_t *ptr;
	FarColor Color;
	const PaletteColors PalColor[] = {COL_PANELTEXT,COL_PANELSELECTEDTEXT,COL_PANELCURSOR,COL_PANELSELECTEDCURSOR};
	int VBufRow = 0;

	for (auto i: zip(Colors.Color, PalColor))
	{
		auto& CurColor= std::get<0>(i);
		const auto pal = std::get<1>(i);

		Color = CurColor.FileColor;

		if (!COLORVALUE(Color.BackgroundColor) && !COLORVALUE(Color.ForegroundColor))
		{
			FARCOLORFLAGS ExFlags = Color.Flags&FCF_EXTENDEDFLAGS;
			Color=colors::PaletteColorToFarColor(pal);
			Color.Flags|=ExFlags;

		}

		ptr=MSG(Colors.Mark.Char? lng::MHighlightExample2 : lng::MHighlightExample1);

		for (int k=0; k<15; k++)
		{
			VBufColorExample[15*VBufRow+k].Char=ptr[k];
			VBufColorExample[15*VBufRow+k].Attributes=Color;
		}

		if (Colors.Mark.Char)
		{
			// inherit only color mode, not style
			VBufColorExample[15*VBufRow+1].Attributes.Flags = Color.Flags&FCF_4BITMASK;
			VBufColorExample[15*VBufRow+1].Char = Colors.Mark.Char;
			if (COLORVALUE(CurColor.MarkColor.ForegroundColor) || COLORVALUE(CurColor.MarkColor.BackgroundColor))
			{
				VBufColorExample[15 * VBufRow + 1].Attributes = CurColor.MarkColor;
			}
			else
			{
				// apply all except color mode
				VBufColorExample[15 * VBufRow + 1].Attributes.Flags |= CurColor.MarkColor.Flags & FCF_EXTENDEDFLAGS;
			}
		}

		VBufColorExample[15 * VBufRow].Attributes = VBufColorExample[15 * VBufRow + 14].Attributes = colors::PaletteColorToFarColor(COL_PANELBOX);
		++VBufRow;
	}
}

void FilterDlgRelativeDateItemsUpdate(Dialog* Dlg, bool bClear)
{
	Dlg->SendMessage(DM_ENABLEREDRAW, FALSE, nullptr);

	if (Dlg->SendMessage(DM_GETCHECK, ID_FF_DATERELATIVE, nullptr))
	{
		Dlg->SendMessage(DM_SHOWITEM, ID_FF_DATEBEFOREEDIT, nullptr);
		Dlg->SendMessage(DM_SHOWITEM, ID_FF_DATEAFTEREDIT, nullptr);
		Dlg->SendMessage(DM_SHOWITEM, ID_FF_CURRENT, nullptr);
		Dlg->SendMessage(DM_SHOWITEM,ID_FF_DAYSBEFOREEDIT,ToPtr(1));
		Dlg->SendMessage(DM_SHOWITEM,ID_FF_DAYSAFTEREDIT,ToPtr(1));
	}
	else
	{
		Dlg->SendMessage(DM_SHOWITEM, ID_FF_DAYSBEFOREEDIT, nullptr);
		Dlg->SendMessage(DM_SHOWITEM, ID_FF_DAYSAFTEREDIT, nullptr);
		Dlg->SendMessage(DM_SHOWITEM,ID_FF_DATEBEFOREEDIT,ToPtr(1));
		Dlg->SendMessage(DM_SHOWITEM,ID_FF_DATEAFTEREDIT,ToPtr(1));
		Dlg->SendMessage(DM_SHOWITEM,ID_FF_CURRENT,ToPtr(1));
	}

	if (bClear)
	{
		Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_DATEAFTEREDIT,nullptr);
		Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_DAYSAFTEREDIT,nullptr);
		Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_TIMEAFTEREDIT,nullptr);
		Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_DATEBEFOREEDIT,nullptr);
		Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_TIMEBEFOREEDIT,nullptr);
		Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_DAYSBEFOREEDIT,nullptr);
	}

	Dlg->SendMessage(DM_ENABLEREDRAW, TRUE, nullptr);
}

intptr_t FileFilterConfigDlgProc(Dialog* Dlg,intptr_t Msg,intptr_t Param1,void* Param2)
{
	switch (Msg)
	{
		case DN_INITDIALOG:
		{
			FilterDlgRelativeDateItemsUpdate(Dlg, false);
			return TRUE;
		}
		case DN_BTNCLICK:
		{
			if (Param1==ID_FF_CURRENT || Param1==ID_FF_BLANK) //Current и Blank
			{
				string strDate, strTime;

				if (Param1==ID_FF_CURRENT)
				{
					ConvertDate(get_utc_time_as_filetime(), strDate, strTime, 12, FALSE, FALSE, 2);
				}
				else
				{
					strDate.clear();
					strTime.clear();
				}

				Dlg->SendMessage(DM_ENABLEREDRAW, FALSE, nullptr);
				int relative = (int)Dlg->SendMessage(DM_GETCHECK, ID_FF_DATERELATIVE, nullptr);
				int db = relative ? ID_FF_DAYSBEFOREEDIT : ID_FF_DATEBEFOREEDIT;
				int da = relative ? ID_FF_DAYSAFTEREDIT  : ID_FF_DATEAFTEREDIT;
				Dlg->SendMessage(DM_SETTEXTPTR,da, UNSAFE_CSTR(strDate));
				Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_TIMEAFTEREDIT, UNSAFE_CSTR(strTime));
				Dlg->SendMessage(DM_SETTEXTPTR,db, UNSAFE_CSTR(strDate));
				Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_TIMEBEFOREEDIT, UNSAFE_CSTR(strTime));
				Dlg->SendMessage(DM_SETFOCUS, da, nullptr);
				COORD r;
				r.X=r.Y=0;
				Dlg->SendMessage(DM_SETCURSORPOS,da,&r);
				Dlg->SendMessage(DM_ENABLEREDRAW, TRUE, nullptr);
				break;
			}
			else if (Param1==ID_FF_RESET) // Reset
			{
				Dlg->SendMessage(DM_ENABLEREDRAW, FALSE, nullptr);
				intptr_t ColorConfig = Dlg->SendMessage(DM_GETDLGDATA, 0, nullptr);
				Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_MASKEDIT,const_cast<wchar_t*>(L"*"));
				Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_SIZEFROMEDIT,nullptr);
				Dlg->SendMessage(DM_SETTEXTPTR,ID_FF_SIZETOEDIT,nullptr);

				for (int I=ID_FF_READONLY; I < ID_HER_SEPARATOR1 ; ++I)
				{
					Dlg->SendMessage(DM_SETCHECK,I,ToPtr(BSTATE_3STATE));
				}

				if (!ColorConfig)
					Dlg->SendMessage(DM_SETCHECK,ID_FF_DIRECTORY,ToPtr(BSTATE_UNCHECKED));

				FarListPos LPos={sizeof(FarListPos)};
				Dlg->SendMessage(DM_LISTSETCURPOS,ID_FF_DATETYPE,&LPos);
				Dlg->SendMessage(DM_SETCHECK,ID_FF_MATCHMASK,ToPtr(BSTATE_CHECKED));
				Dlg->SendMessage(DM_SETCHECK,ID_FF_MATCHSIZE,ToPtr(BSTATE_UNCHECKED));
				Dlg->SendMessage(DM_SETCHECK,ID_FF_HARDLINKS,ToPtr(BSTATE_UNCHECKED));
				Dlg->SendMessage(DM_SETCHECK,ID_FF_MATCHDATE,ToPtr(BSTATE_UNCHECKED));
				Dlg->SendMessage(DM_SETCHECK,ID_FF_DATERELATIVE,ToPtr(BSTATE_UNCHECKED));
				FilterDlgRelativeDateItemsUpdate(Dlg, true);
				Dlg->SendMessage(DM_SETCHECK,ID_FF_MATCHATTRIBUTES,ToPtr(ColorConfig?BSTATE_UNCHECKED:BSTATE_CHECKED));
				Dlg->SendMessage(DM_ENABLEREDRAW, TRUE, nullptr);
				break;
			}
			else if (Param1==ID_FF_MAKETRANSPARENT)
			{
				const auto Colors = reinterpret_cast<highlight::element*>(Dlg->SendMessage(DM_GETDLGDATA, 0, nullptr));

				std::for_each(RANGE(Colors->Color, i)
				{
					MAKE_TRANSPARENT(i.FileColor.ForegroundColor);
					MAKE_TRANSPARENT(i.FileColor.BackgroundColor);
					MAKE_TRANSPARENT(i.MarkColor.ForegroundColor);
					MAKE_TRANSPARENT(i.MarkColor.BackgroundColor);
				});

				Dlg->SendMessage(DM_SETCHECK,ID_HER_MARKTRANSPARENT,ToPtr(BSTATE_CHECKED));
				break;
			}
			else if (Param1==ID_FF_DATERELATIVE)
			{
				FilterDlgRelativeDateItemsUpdate(Dlg, true);
				break;
			}
		}
		case DN_CONTROLINPUT:

			if ((Msg==DN_BTNCLICK && Param1 >= ID_HER_NORMALFILE && Param1 <= ID_HER_SELECTEDCURSORMARKING)
			        || (Msg==DN_CONTROLINPUT && Param1==ID_HER_COLOREXAMPLE && ((INPUT_RECORD *)Param2)->EventType == MOUSE_EVENT && ((INPUT_RECORD *)Param2)->Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED))
			{
				const auto EditData = reinterpret_cast<highlight::element*>(Dlg->SendMessage(DM_GETDLGDATA, 0, nullptr));

				if (Msg==DN_CONTROLINPUT)
				{
					Param1 = ID_HER_NORMALFILE + ((INPUT_RECORD *)Param2)->Event.MouseEvent.dwMousePosition.Y*2;

					if (((INPUT_RECORD *)Param2)->Event.MouseEvent.dwMousePosition.X==1 && (EditData->Mark.Char))
						Param1 = ID_HER_NORMALMARKING + ((INPUT_RECORD *)Param2)->Event.MouseEvent.dwMousePosition.Y*2;
				}

				//Color[0=file, 1=mark][0=normal,1=selected,2=undercursor,3=selectedundercursor]
				Console().GetColorDialog(((Param1-ID_HER_NORMALFILE)&1)? EditData->Color[(Param1-ID_HER_NORMALFILE)/2].MarkColor : EditData->Color[(Param1-ID_HER_NORMALFILE)/2].FileColor, true, true);

				size_t Size = Dlg->SendMessage(DM_GETDLGITEM, ID_HER_COLOREXAMPLE, nullptr);
				block_ptr<FarDialogItem> Buffer(Size);
				FarGetDialogItem gdi = {sizeof(FarGetDialogItem), Size, Buffer.get()};
				Dlg->SendMessage(DM_GETDLGITEM,ID_HER_COLOREXAMPLE,&gdi);
				//MarkChar это FIXEDIT размером в 1 символ
				wchar_t MarkChar[2];
				FarDialogItemData item={sizeof(FarDialogItemData),1,MarkChar};
				Dlg->SendMessage(DM_GETTEXT,ID_HER_MARKEDIT,&item);
				EditData->Mark.Char = *MarkChar;
				HighlightDlgUpdateUserControl(gdi.Item->VBuf,*EditData);
				Dlg->SendMessage(DM_SETDLGITEM,ID_HER_COLOREXAMPLE,gdi.Item);
				return TRUE;
			}

			break;
		case DN_EDITCHANGE:

			if (Param1 == ID_HER_MARKEDIT)
			{
				const auto EditData = reinterpret_cast<highlight::element*>(Dlg->SendMessage(DM_GETDLGDATA, 0, nullptr));
				size_t Size = Dlg->SendMessage(DM_GETDLGITEM, ID_HER_COLOREXAMPLE, nullptr);
				block_ptr<FarDialogItem> Buffer(Size);
				FarGetDialogItem gdi = {sizeof(FarGetDialogItem), Size, Buffer.get()};
				Dlg->SendMessage(DM_GETDLGITEM,ID_HER_COLOREXAMPLE,&gdi);
				//MarkChar это FIXEDIT размером в 1 символ
				wchar_t MarkChar[2];
				FarDialogItemData item={sizeof(FarDialogItemData),1,MarkChar};
				Dlg->SendMessage(DM_GETTEXT,ID_HER_MARKEDIT,&item);
				EditData->Mark.Char = *MarkChar;
				HighlightDlgUpdateUserControl(gdi.Item->VBuf,*EditData);
				Dlg->SendMessage(DM_SETDLGITEM,ID_HER_COLOREXAMPLE,gdi.Item);
				return TRUE;
			}

			break;
		case DN_CLOSE:

			if (Param1 == ID_FF_OK && Dlg->SendMessage(DM_GETCHECK, ID_FF_MATCHSIZE, nullptr))
			{
				string Size = reinterpret_cast<const wchar_t*>(Dlg->SendMessage(DM_GETCONSTTEXTPTR, ID_FF_SIZEFROMEDIT, nullptr));
				bool Ok = Size.empty() || CheckFileSizeStringFormat(Size);
				if (Ok)
				{
					Size = reinterpret_cast<const wchar_t*>(Dlg->SendMessage(DM_GETCONSTTEXTPTR, ID_FF_SIZETOEDIT, nullptr));
					Ok = Size.empty() || CheckFileSizeStringFormat(Size);
				}
				if (!Ok)
				{
					intptr_t ColorConfig = Dlg->SendMessage(DM_GETDLGDATA, 0, nullptr);
					Message(MSG_WARNING,1,ColorConfig?MSG(lng::MFileHilightTitle):MSG(lng::MFileFilterTitle),MSG(lng::MBadFileSizeFormat),MSG(lng::MOk));
					return FALSE;
				}
			}

			break;
		default:
			break;
	}

	return Dlg->DefProc(Msg,Param1,Param2);
}

bool FileFilterConfig(FileFilterParams *FF, bool ColorConfig)
{
	// Временная маска.
	filemasks FileMask;
	// История для маски файлов
	const wchar_t FilterMasksHistoryName[] = L"FilterMasks";
	// История для имени фильтра
	const wchar_t FilterNameHistoryName[] = L"FilterName";
	// Маски для диалога настройки
	// Маска для ввода дней для относительной даты
	const wchar_t DaysMask[] = L"9999";
	string strDateMask, strTimeMask;
	// Определение параметров даты и времени в системе.
	wchar_t DateSeparator = locale::GetDateSeparator();
	wchar_t TimeSeparator = locale::GetTimeSeparator();
	wchar_t DecimalSeparator = locale::GetDecimalSeparator();
	int DateFormat = locale::GetDateFormat();

	switch (DateFormat)
	{
		case 0:
			// Маска даты для форматов DD.MM.YYYYY и MM.DD.YYYYY
			strDateMask = string(L"99") + DateSeparator + L"99" + DateSeparator + L"9999N";
			break;
		case 1:
			// Маска даты для форматов DD.MM.YYYYY и MM.DD.YYYYY
			strDateMask = string(L"99") + DateSeparator + L"99" + DateSeparator + L"9999N";
			break;
		default:
			// Маска даты для формата YYYYY.MM.DD
			strDateMask = string(L"N9999") + DateSeparator + L"99" + DateSeparator + L"99";
			break;
	}

	// Маска времени
	strTimeMask = string(L"99") + TimeSeparator + L"99" + TimeSeparator + L"99" + DecimalSeparator + L"999";
	const wchar_t VerticalLine[] = {BoxSymbols[BS_T_H1V1],BoxSymbols[BS_V1],BoxSymbols[BS_V1],BoxSymbols[BS_V1],BoxSymbols[BS_B_H1V1],0};
	FarDialogItem FilterDlgData[]=
	{
		{DI_DOUBLEBOX,3,1,76,23,0,nullptr,nullptr,DIF_SHOWAMPERSAND,MSG(lng::MFileFilterTitle)},

		{DI_TEXT,5,2,0,2,0,nullptr,nullptr,DIF_FOCUS,MSG(lng::MFileFilterName)},
		{DI_EDIT,5,2,74,2,0,FilterNameHistoryName,nullptr,DIF_HISTORY,L""},

		{DI_TEXT,-1,3,0,3,0,nullptr,nullptr,DIF_SEPARATOR,L""},

		{DI_CHECKBOX,5,4,0,4,0,nullptr,nullptr,DIF_AUTOMATION,MSG(lng::MFileFilterMatchMask)},
		{DI_EDIT,5,4,74,4,0,FilterMasksHistoryName,nullptr,DIF_HISTORY,L""},

		{DI_TEXT,-1,5,0,5,0,nullptr,nullptr,DIF_SEPARATOR,L""},

		{DI_CHECKBOX,5,6,0,6,0,nullptr,nullptr,DIF_AUTOMATION,MSG(lng::MFileFilterSize)},
		{DI_TEXT,7,7,8,7,0,nullptr,nullptr,0,MSG(lng::MFileFilterSizeFromSign)},
		{DI_EDIT,10,7,20,7,0,nullptr,nullptr,0,L""},
		{DI_TEXT,7,8,8,8,0,nullptr,nullptr,0,MSG(lng::MFileFilterSizeToSign)},
		{DI_EDIT,10,8,20,8,0,nullptr,nullptr,0,L""},

		{DI_CHECKBOX,24,6,0,6,0,nullptr,nullptr,DIF_AUTOMATION,MSG(lng::MFileFilterDate)},
		{DI_COMBOBOX,26,7,41,7,0,nullptr,nullptr,DIF_DROPDOWNLIST|DIF_LISTNOAMPERSAND,L""},
		{DI_CHECKBOX,26,8,0,8,0,nullptr,nullptr,0,MSG(lng::MFileFilterDateRelative)},
		{DI_TEXT,48,7,50,7,0,nullptr,nullptr,0,MSG(lng::MFileFilterDateBeforeSign)},
		{DI_FIXEDIT,51,7,61,7,0,nullptr,strDateMask.data(),DIF_MASKEDIT,L""},
		{DI_FIXEDIT,51,7,61,7,0,nullptr,DaysMask,DIF_MASKEDIT,L""},
		{DI_FIXEDIT,63,7,74,7,0,nullptr,strTimeMask.data(),DIF_MASKEDIT,L""},
		{DI_TEXT,48,8,50,8,0,nullptr,nullptr,0,MSG(lng::MFileFilterDateAfterSign)},
		{DI_FIXEDIT,51,8,61,8,0,nullptr,strDateMask.data(),DIF_MASKEDIT,L""},
		{DI_FIXEDIT,51,8,61,8,0,nullptr,DaysMask,DIF_MASKEDIT,L""},
		{DI_FIXEDIT,63,8,74,8,0,nullptr,strTimeMask.data(),DIF_MASKEDIT,L""},
		{DI_BUTTON,0,6,0,6,0,nullptr,nullptr,DIF_BTNNOCLOSE,MSG(lng::MFileFilterCurrent)},
		{DI_BUTTON,0,6,74,6,0,nullptr,nullptr,DIF_BTNNOCLOSE,MSG(lng::MFileFilterBlank)},

		{DI_TEXT,-1,9,0,9,0,nullptr,nullptr,DIF_SEPARATOR,L""},
		{DI_VTEXT,22,5,22,9,0,nullptr,nullptr,DIF_BOXCOLOR,VerticalLine},

		{DI_CHECKBOX, 5,10,0,10,0,nullptr,nullptr,DIF_AUTOMATION,MSG(lng::MFileFilterAttr)},
		{DI_CHECKBOX, 7,11,0,11,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrR)},
		{DI_CHECKBOX, 7,12,0,12,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrA)},
		{DI_CHECKBOX, 7,13,0,13,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrH)},
		{DI_CHECKBOX, 7,14,0,14,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrS)},
		{DI_CHECKBOX, 7,15,0,15,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrC)},
		{DI_CHECKBOX, 7,16,0,16,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrE)},
		{DI_CHECKBOX, 7,17,0,17,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrNI)},
		{DI_CHECKBOX, 7,18,0,18,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrD)},

		{DI_CHECKBOX,42,11,0,11,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrSparse)},
		{DI_CHECKBOX,42,12,0,12,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrT)},
		{DI_CHECKBOX,42,13,0,13,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrOffline)},
		{DI_CHECKBOX,42,14,0,14,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrReparse)},
		{DI_CHECKBOX,42,15,0,15,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrVirtual)},
		{DI_CHECKBOX,42,16,0,16,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrIntegrityStream)},
		{DI_CHECKBOX,42,17,0,17,0,nullptr,nullptr,DIF_3STATE,MSG(lng::MFileFilterAttrNoScrubData)},

		{DI_TEXT,-1,17,0,17,0,nullptr,nullptr,DIF_SEPARATOR,MSG(lng::MHighlightColors)},
		{DI_TEXT,7,18,0,18,0,nullptr,nullptr,0,MSG(lng::MHighlightMarkChar)},
		{DI_FIXEDIT,5,18,5,18,0,nullptr,nullptr,0,L""},
		{DI_CHECKBOX,0,18,0,18,0,nullptr,nullptr,0,MSG(lng::MHighlightTransparentMarkChar)},

		{DI_BUTTON,5,19,0,19,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,MSG(lng::MHighlightFileName1)},
		{DI_BUTTON,0,19,0,19,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,MSG(lng::MHighlightMarking1)},
		{DI_BUTTON,5,20,0,20,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,MSG(lng::MHighlightFileName2)},
		{DI_BUTTON,0,20,0,20,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,MSG(lng::MHighlightMarking2)},
		{DI_BUTTON,5,21,0,21,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,MSG(lng::MHighlightFileName3)},
		{DI_BUTTON,0,21,0,21,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,MSG(lng::MHighlightMarking3)},
		{DI_BUTTON,5,22,0,22,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,MSG(lng::MHighlightFileName4)},
		{DI_BUTTON,0,22,0,22,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,MSG(lng::MHighlightMarking4)},

		{DI_USERCONTROL,73-15-1,19,73-2,22,0,nullptr,nullptr,DIF_NOFOCUS,L""},
		{DI_CHECKBOX,5,23,0,23,0,nullptr,nullptr,0,MSG(lng::MHighlightContinueProcessing)},

		{DI_TEXT,-1,19,0,19,0,nullptr,nullptr,DIF_SEPARATOR,L""},

		{DI_CHECKBOX,5,20,0,20,0,nullptr,nullptr,0,MSG(lng::MFileHardLinksCount)},//добавляем новый чекбокс в панель
		{DI_TEXT,-1,21,0,21,0,nullptr,nullptr,DIF_SEPARATOR,L""},// и разделитель

		{DI_BUTTON,0,22,0,22,0,nullptr,nullptr,DIF_DEFAULTBUTTON|DIF_CENTERGROUP,MSG(lng::MOk)},
		{DI_BUTTON,0,22,0,22,0,nullptr,nullptr,DIF_CENTERGROUP|DIF_BTNNOCLOSE,MSG(lng::MFileFilterReset)},
		{DI_BUTTON,0,22,0,22,0,nullptr,nullptr,DIF_CENTERGROUP,MSG(lng::MFileFilterCancel)},
		{DI_BUTTON,0,22,0,22,0,nullptr,nullptr,DIF_CENTERGROUP|DIF_BTNNOCLOSE,MSG(lng::MFileFilterMakeTransparent)},
	};
	FilterDlgData[0].Data=MSG(ColorConfig? lng::MFileHilightTitle : lng::MFileFilterTitle);
	auto FilterDlg = MakeDialogItemsEx(FilterDlgData);

	if (ColorConfig)
	{
		FilterDlg[ID_FF_TITLE].Y2+=5;

		for (int i=ID_FF_NAME; i<=ID_FF_SEPARATOR1; i++)
			FilterDlg[i].Flags|=DIF_HIDDEN;

		for (int i=ID_FF_MATCHMASK; i < ID_HER_SEPARATOR1; i++)
		{
			FilterDlg[i].Y1-=2;
			FilterDlg[i].Y2-=2;
		}

		for (int i=ID_FF_SEPARATOR5; i<=ID_FF_MAKETRANSPARENT; i++)
		{
			FilterDlg[i].Y1+=5;
			FilterDlg[i].Y2+=5;
		}
	}
	else
	{
		for (int i=ID_HER_SEPARATOR1; i<=ID_HER_CONTINUEPROCESSING; i++)
			FilterDlg[i].Flags|=DIF_HIDDEN;

		FilterDlg[ID_FF_MAKETRANSPARENT].Flags=DIF_HIDDEN;
	}

	const auto& AmpFixup = [&](size_t Index)
	{
		return contains(FilterDlg[Index].strData, L'&')? 1 : 0;
	};

	FilterDlg[ID_FF_NAMEEDIT].X1 = FilterDlg[ID_FF_NAME].X1 + FilterDlg[ID_FF_NAME].strData.size() - AmpFixup(ID_FF_NAME) + 1;
	FilterDlg[ID_FF_MASKEDIT].X1 = FilterDlg[ID_FF_MATCHMASK].X1 + FilterDlg[ID_FF_MATCHMASK].strData.size() - AmpFixup(ID_FF_MATCHMASK) + 5;
	FilterDlg[ID_FF_BLANK].X1 = FilterDlg[ID_FF_BLANK].X2 - FilterDlg[ID_FF_BLANK].strData.size() + AmpFixup(ID_FF_BLANK) - 3;
	FilterDlg[ID_FF_CURRENT].X2 = FilterDlg[ID_FF_BLANK].X1 - 2;
	FilterDlg[ID_FF_CURRENT].X1 = FilterDlg[ID_FF_CURRENT].X2 - FilterDlg[ID_FF_CURRENT].strData.size() + AmpFixup(ID_FF_CURRENT) - 3;
	FilterDlg[ID_HER_MARKTRANSPARENT].X1 = FilterDlg[ID_HER_MARK_TITLE].X1 + FilterDlg[ID_HER_MARK_TITLE].strData.size() - AmpFixup(ID_HER_MARK_TITLE) + 1;

	for (int i = ID_HER_NORMALMARKING; i <= ID_HER_SELECTEDCURSORMARKING; i += 2)
		FilterDlg[i].X1 = FilterDlg[ID_HER_NORMALFILE].X1 + FilterDlg[ID_HER_NORMALFILE].strData.size() - AmpFixup(ID_HER_NORMALFILE) + 1;

	FAR_CHAR_INFO VBufColorExample[15*4]={};
	auto Colors = FF->GetColors();
	HighlightDlgUpdateUserControl(VBufColorExample,Colors);
	FilterDlg[ID_HER_COLOREXAMPLE].VBuf=VBufColorExample;
	FilterDlg[ID_HER_MARKEDIT].strData.assign(Colors.Mark.Char ? 1 : 0, Colors.Mark.Char);
	FilterDlg[ID_HER_MARKTRANSPARENT].Selected = Colors.Mark.Transparent;
	FilterDlg[ID_HER_CONTINUEPROCESSING].Selected=(FF->GetContinueProcessing()?1:0);
	FilterDlg[ID_FF_NAMEEDIT].strData = FF->GetTitle();
	FilterDlg[ID_FF_MATCHMASK].Selected = FF->IsMaskUsed();
	FilterDlg[ID_FF_MASKEDIT].strData = FF->GetMask();

	if (!FilterDlg[ID_FF_MATCHMASK].Selected)
		FilterDlg[ID_FF_MASKEDIT].Flags|=DIF_DISABLE;

	FilterDlg[ID_FF_MATCHSIZE].Selected = FF->IsSizeUsed();
	FilterDlg[ID_FF_SIZEFROMEDIT].strData = FF->GetSizeAbove();
	FilterDlg[ID_FF_SIZETOEDIT].strData = FF->GetSizeBelow();
	FilterDlg[ID_FF_HARDLINKS].Selected=FF->GetHardLinks(nullptr,nullptr)?1:0; //пока что мы проверяем только флаг использования данного условия

	if (!FilterDlg[ID_FF_MATCHSIZE].Selected)
		for (int i=ID_FF_SIZEFROMSIGN; i <= ID_FF_SIZETOEDIT; i++)
			FilterDlg[i].Flags|=DIF_DISABLE;

	// Лист для комбобокса времени файла
	FarList DateList={sizeof(FarList)};
	FarListItem TableItemDate[FDATE_COUNT]={};
	// Настройка списка типов дат файла
	DateList.Items=TableItemDate;
	DateList.ItemsNumber=FDATE_COUNT;

	for (int i=0; i < FDATE_COUNT; ++i)
		TableItemDate[i].Text=MSG(lng::MFileFilterWrited+i);

	DWORD DateType;
	FILETIME DateAfter, DateBefore;
	bool bRelative;
	FilterDlg[ID_FF_MATCHDATE].Selected=FF->GetDate(&DateType,&DateAfter,&DateBefore,&bRelative)?1:0;
	FilterDlg[ID_FF_DATERELATIVE].Selected=bRelative?1:0;
	FilterDlg[ID_FF_DATETYPE].ListItems=&DateList;
	TableItemDate[DateType].Flags=LIF_SELECTED;

	if (bRelative)
	{
		ConvertRelativeDate(DateAfter, FilterDlg[ID_FF_DAYSAFTEREDIT].strData, FilterDlg[ID_FF_TIMEAFTEREDIT].strData);
		ConvertRelativeDate(DateBefore, FilterDlg[ID_FF_DAYSBEFOREEDIT].strData, FilterDlg[ID_FF_TIMEBEFOREEDIT].strData);
	}
	else
	{
		ConvertDate(DateAfter,FilterDlg[ID_FF_DATEAFTEREDIT].strData,FilterDlg[ID_FF_TIMEAFTEREDIT].strData,12,FALSE,FALSE,2);
		ConvertDate(DateBefore,FilterDlg[ID_FF_DATEBEFOREEDIT].strData,FilterDlg[ID_FF_TIMEBEFOREEDIT].strData,12,FALSE,FALSE,2);
	}

	if (!FilterDlg[ID_FF_MATCHDATE].Selected)
		for (int i=ID_FF_DATETYPE; i <= ID_FF_BLANK; i++)
			FilterDlg[i].Flags|=DIF_DISABLE;

	static const std::pair<enumFileFilterConfig, DWORD> AttributeMapping[] =
	{
		{ ID_FF_READONLY, FILE_ATTRIBUTE_READONLY },
		{ ID_FF_ARCHIVE, FILE_ATTRIBUTE_ARCHIVE },
		{ ID_FF_HIDDEN, FILE_ATTRIBUTE_HIDDEN },
		{ ID_FF_SYSTEM, FILE_ATTRIBUTE_SYSTEM },
		{ ID_FF_COMPRESSED, FILE_ATTRIBUTE_COMPRESSED },
		{ ID_FF_ENCRYPTED, FILE_ATTRIBUTE_ENCRYPTED },
		{ ID_FF_NOTINDEXED, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED },
		{ ID_FF_DIRECTORY, FILE_ATTRIBUTE_DIRECTORY },
		{ ID_FF_SPARSE, FILE_ATTRIBUTE_SPARSE_FILE },
		{ ID_FF_TEMP, FILE_ATTRIBUTE_TEMPORARY },
		{ ID_FF_OFFLINE, FILE_ATTRIBUTE_OFFLINE },
		{ ID_FF_REPARSEPOINT, FILE_ATTRIBUTE_REPARSE_POINT },
		{ ID_FF_VIRTUAL, FILE_ATTRIBUTE_VIRTUAL },
		{ ID_FF_INTEGRITY_STREAM, FILE_ATTRIBUTE_INTEGRITY_STREAM },
		{ ID_FF_NO_SCRUB_DATA, FILE_ATTRIBUTE_NO_SCRUB_DATA },
	};

	DWORD AttrSet, AttrClear;
	FilterDlg[ID_FF_MATCHATTRIBUTES].Selected=FF->GetAttr(&AttrSet,&AttrClear)?1:0;

	for (const auto& i: AttributeMapping)
	{
		FilterDlg[i.first].Selected = AttrSet & i.second? BSTATE_CHECKED : AttrClear & i.second? BSTATE_UNCHECKED : BSTATE_3STATE;
	}

	if (!FilterDlg[ID_FF_MATCHATTRIBUTES].Selected)
	{
		for (int i=ID_FF_READONLY; i < ID_HER_SEPARATOR1; i++)
			FilterDlg[i].Flags|=DIF_DISABLE;
	}

	const auto Dlg = Dialog::create(FilterDlg, FileFilterConfigDlgProc, ColorConfig ? &Colors : nullptr);
	Dlg->SetHelp(ColorConfig?L"HighlightEdit":L"Filter");
	Dlg->SetPosition(-1,-1,FilterDlg[ID_FF_TITLE].X2+4,FilterDlg[ID_FF_TITLE].Y2+2);
	Dlg->SetId(ColorConfig?HighlightConfigId:FiltersConfigId);
	Dlg->SetAutomation(ID_FF_MATCHMASK,ID_FF_MASKEDIT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHSIZE,ID_FF_SIZEFROMSIGN,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHSIZE,ID_FF_SIZEFROMEDIT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHSIZE,ID_FF_SIZETOSIGN,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHSIZE,ID_FF_SIZETOEDIT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_DATETYPE,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_DATERELATIVE,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_DATEAFTERSIGN,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_DATEAFTEREDIT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_DAYSAFTEREDIT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_TIMEAFTEREDIT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_DATEBEFORESIGN,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_DATEBEFOREEDIT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_DAYSBEFOREEDIT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_TIMEBEFOREEDIT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_CURRENT,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
	Dlg->SetAutomation(ID_FF_MATCHDATE,ID_FF_BLANK,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);

	for (const auto& i: AttributeMapping)
	{
		Dlg->SetAutomation(ID_FF_MATCHATTRIBUTES, i.first, DIF_DISABLE, DIF_NONE, DIF_NONE, DIF_DISABLE);
	}

	for (;;)
	{
		Dlg->ClearDone();
		Dlg->Process();
		int ExitCode=Dlg->GetExitCode();

		if (ExitCode==ID_FF_OK) // Ok
		{
			// Если введённая пользователем маска не корректна, тогда вернёмся в диалог
			if (FilterDlg[ID_FF_MATCHMASK].Selected && !FileMask.Set(FilterDlg[ID_FF_MASKEDIT].strData,0))
				continue;

			Colors.Mark.Transparent = FilterDlg[ID_HER_MARKTRANSPARENT].Selected == BSTATE_CHECKED;

			FF->SetColors(Colors);
			FF->SetContinueProcessing(FilterDlg[ID_HER_CONTINUEPROCESSING].Selected!=0);
			FF->SetTitle(FilterDlg[ID_FF_NAMEEDIT].strData);
			FF->SetMask(FilterDlg[ID_FF_MATCHMASK].Selected!=0,
			            FilterDlg[ID_FF_MASKEDIT].strData);
			FF->SetSize(FilterDlg[ID_FF_MATCHSIZE].Selected!=0,
			            FilterDlg[ID_FF_SIZEFROMEDIT].strData,
			            FilterDlg[ID_FF_SIZETOEDIT].strData);
			FF->SetHardLinks(FilterDlg[ID_FF_HARDLINKS].Selected!=0,0,0); //пока устанавливаем только флаг использования признака
			bRelative = FilterDlg[ID_FF_DATERELATIVE].Selected!=0;

			FilterDlg[ID_FF_TIMEBEFOREEDIT].strData[8] = TimeSeparator;
			FilterDlg[ID_FF_TIMEAFTEREDIT].strData[8] = TimeSeparator;

			StrToDateTime(FilterDlg[bRelative?ID_FF_DAYSAFTEREDIT:ID_FF_DATEAFTEREDIT].strData,FilterDlg[ID_FF_TIMEAFTEREDIT].strData,DateAfter,DateFormat,DateSeparator,TimeSeparator,bRelative);
			StrToDateTime(FilterDlg[bRelative?ID_FF_DAYSBEFOREEDIT:ID_FF_DATEBEFOREEDIT].strData,FilterDlg[ID_FF_TIMEBEFOREEDIT].strData,DateBefore,DateFormat,DateSeparator,TimeSeparator,bRelative);
			FF->SetDate(FilterDlg[ID_FF_MATCHDATE].Selected!=0,
			            static_cast<enumFDateType>(FilterDlg[ID_FF_DATETYPE].ListPos),
			            DateAfter,
			            DateBefore,
			            bRelative);
			AttrSet=0;
			AttrClear=0;

			for(const auto& i: AttributeMapping)
			{
				switch (FilterDlg[i.first].Selected)
				{
				case BSTATE_CHECKED:
					AttrSet |= i.second;
					break;

				case BSTATE_UNCHECKED:
					AttrClear |= i.second;
					break;

				default:
					break;
				}
			}

			FF->SetAttr(FilterDlg[ID_FF_MATCHATTRIBUTES].Selected!=0,
			            AttrSet,
			            AttrClear);
			return true;
		}
		else
			break;
	}

	return false;
}
