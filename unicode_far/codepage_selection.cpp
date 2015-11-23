/*
codepage_selection.hpp
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

#include "codepage_selection.hpp"
#include "codepage.hpp"
#include "vmenu2.hpp"
#include "keys.hpp"
#include "language.hpp"
#include "dialog.hpp"
#include "interf.hpp"
#include "config.hpp"
#include "configdb.hpp"
#include "FarDlgBuilder.hpp"
#include "DlgGuid.hpp"
#include "constitle.hpp"
#include "strmix.hpp"

codepages& Codepages()
{
	static codepages cp;
	return cp;
}

// ���� ��� �������� ����� ������� �������
static const wchar_t NamesOfCodePagesKey[] = L"CodePages.Names";
static const wchar_t FavoriteCodePagesKey[] = L"CodePages.Favorites";

// �������� ������ �������� ������� �� ������� ���������
ENUM(CodePagesCallbackCallSource)
{
	CodePageSelect,
	CodePagesFill,
	CodePagesFill2,
	CodePageCheck
};

// ����������� �������� ���� ������� �������
enum StandardCodePagesMenuItems
{
	SearchAll = BIT(0), // Find-in-Files dialog
	AutoCP    = BIT(1), // show <Autodetect> item
	OEM       = BIT(2), // show OEM codepage
	ANSI      = BIT(3), // show ANSI codepage
	UTF8      = BIT(4), // show UTF-8 codepage
	UTF16LE   = BIT(5), // show UTF-16 LE codepage
	UTF16BE   = BIT(6), // show UTF-16 BE codepage
	VOnly     = BIT(7), // show only viewer-supported codepages
	DefaultCP = BIT(8), // show <Default> item

	AllStandard = OEM | ANSI | UTF8 | UTF16BE | UTF16LE
};

codepages::codepages():
	dialog(nullptr),
	control(0),
	DialogBuilderList(),
	currentCodePage(0),
	favoriteCodePages(0),
	normalCodePages(0),
	selectedCodePages(false),
	CallbackCallSource(CodePageSelect)
{
}

codepages::~codepages()
{}

// �������� ������� �������� ��� �������� � ����
inline uintptr_t codepages::GetMenuItemCodePage(size_t Position)
{
	const auto DataPtr = CodePagesMenu->GetUserDataPtr<uintptr_t>(Position);
	return DataPtr? *DataPtr : 0;
}

inline size_t codepages::GetListItemCodePage(size_t Position)
{
	const auto DataPtr = dialog->GetListItemDataPtr<uintptr_t>(control, Position);
	return DataPtr? *DataPtr : 0;
}

// ��������� �������� ��� ��� ������� � �������� ����������� ������� ������� (������������ ������ ��� ������������ �� �������������)
inline bool codepages::IsPositionStandard(UINT position)
{
	return position <= (UINT)CodePagesMenu->size() - favoriteCodePages - (favoriteCodePages?1:0) - normalCodePages - (normalCodePages?1:0);
}

// ��������� �������� ��� ��� ������� � �������� ��������� ������� ������� (������������ ������ ��� ������������ �� �������������)
inline bool codepages::IsPositionFavorite(UINT position)
{
	return !IsPositionStandard(position) && !IsPositionNormal(position);
}

// ��������� �������� ��� ��� ������� � �������� ������������ ������� ������� (������������ ������ ��� ������������ �� �������������)
inline bool codepages::IsPositionNormal(UINT position)
{
	return position >= static_cast<UINT>(CodePagesMenu->size() - normalCodePages);
}

// ��������� ������ ��� ����������� ������������� ������� ��������
string codepages::FormatCodePageString(uintptr_t CodePage, const string& CodePageName, bool IsCodePageNameCustom) const
{
	string result;
	if (static_cast<intptr_t>(CodePage) >= 0)  // CodePage != CP_DEFAULT, CP_REDETECT
	{
		result = std::to_wstring(CodePage);
		result.resize(std::max(result.size(), size_t(5)), L' ');
		result += BoxSymbols[BS_V1];
		result += (!IsCodePageNameCustom || CallbackCallSource == CodePagesFill || CallbackCallSource == CodePagesFill2? L' ' : L'*');
	}
	result += CodePageName;
	return result;
}

// ��������� ������� ��������
void codepages::AddCodePage(const string& codePageName, uintptr_t codePage, size_t position, bool enabled, bool checked, bool IsCodePageNameCustom)
{
	if (CallbackCallSource == CodePagesFill)
	{
		// ��������� ������� ������������ ��������
		if (position == size_t(-1))
		{
			FarListInfo info = { sizeof(FarListInfo) };
			dialog->SendMessage(DM_LISTINFO, control, &info);
			position = info.ItemsNumber;
		}

		// ��������� �������
		FarListInsert item = { sizeof(FarListInsert), static_cast<intptr_t>(position) };

		string name = FormatCodePageString(codePage, codePageName, IsCodePageNameCustom);
		item.Item.Text = name.data();

		if (selectedCodePages && checked)
		{
			item.Item.Flags |= LIF_CHECKED;
		}

		if (!enabled)
		{
			item.Item.Flags |= LIF_GRAYED;
		}

		dialog->SendMessage(DM_LISTINSERT, control, &item);
		dialog->SetListItemData(control, position, codePage);
	}
	else if (CallbackCallSource == CodePagesFill2)
	{
		// ��������� �������
		DialogBuilderListItem2 item;

		item.Text = FormatCodePageString(codePage, codePageName, IsCodePageNameCustom);
		item.Flags = LIF_NONE;

		if (selectedCodePages && checked)
		{
			item.Flags |= LIF_CHECKED;
		}

		if (!enabled)
		{
			item.Flags |= LIF_GRAYED;
		}

		item.ItemValue = static_cast<int>(codePage);

		// ��������� ������� ������������ ��������
		if (position == size_t(-1) || position >= DialogBuilderList->size())
		{
			DialogBuilderList->emplace_back(item);
		}
		else
		{
			DialogBuilderList->emplace(DialogBuilderList->begin() + position, item);
		}
	}
	else
	{
		// ������ ����� ������� ����
		MenuItemEx item(FormatCodePageString(codePage, codePageName, IsCodePageNameCustom));
		if (!enabled)
			item.Flags |= MIF_GRAYED;
		item.UserData = codePage;

		// ��������� ����� ������� � ����
		if (position == size_t(-1))
			CodePagesMenu->AddItem(item);
		else
			CodePagesMenu->AddItem(item, static_cast<int>(position));

		// ���� ���� ������������� ������ �� ����������� �������
		if (currentCodePage == codePage)
		{
			if ((CodePagesMenu->GetSelectPos() == -1 || GetMenuItemCodePage() != codePage))
			{
				CodePagesMenu->SetSelectPos(static_cast<int>(position == size_t(-1)? CodePagesMenu->size() - 1 : position), 1);
			}
		}
	}
}

// ��������� ����������� ������� ��������
void codepages::AddStandardCodePage(const wchar_t *codePageName, uintptr_t codePage, int position, bool enabled)
{
	bool checked = false;

	if (selectedCodePages && codePage != CP_DEFAULT)
	{
		if (GetFavorite(codePage) & CPST_FIND)
			checked = true;
	}

	AddCodePage(codePageName, codePage, position, enabled, checked, false);
}

// ��������� �����������
void codepages::AddSeparator(LPCWSTR Label, size_t position)
{
	if (CallbackCallSource == CodePagesFill)
	{
		if (position == size_t(-1))
		{
			FarListInfo info = { sizeof(FarListInfo) };
			dialog->SendMessage(DM_LISTINFO, control, &info);
			position = info.ItemsNumber;
		}

		FarListInsert item = { sizeof(FarListInsert), static_cast<intptr_t>(position) };
		item.Item.Text = Label;
		item.Item.Flags = LIF_SEPARATOR;
		dialog->SendMessage(DM_LISTINSERT, control, &item);
	}
	else if (CallbackCallSource == CodePagesFill2)
	{
		// ��������� �������
		DialogBuilderListItem2 item;
		item.Text = Label;
		item.Flags = LIF_SEPARATOR;
		item.ItemValue = 0;

		// ��������� ������� ������������ ��������
		if (position == size_t(-1) || position >= DialogBuilderList->size())
		{
			DialogBuilderList->emplace_back(item);
		}
		else
		{
			DialogBuilderList->emplace(DialogBuilderList->begin() + position, item);
		}
	}
	else
	{
		MenuItemEx item;
		item.strName = Label;
		item.Flags = MIF_SEPARATOR;

		if (position == size_t(-1))
			CodePagesMenu->AddItem(item);
		else
			CodePagesMenu->AddItem(item, static_cast<int>(position));
	}
}

// �������� ���������� ��������� � ������
size_t codepages::size() const
{
	if (CallbackCallSource == CodePageSelect)
	{
		return CodePagesMenu->size();
	}
	else if (CallbackCallSource == CodePagesFill2)
	{
		return DialogBuilderList->size();
	}
	else
	{
		FarListInfo info = { sizeof(FarListInfo) };
		dialog->SendMessage(DM_LISTINFO, control, &info);
		return info.ItemsNumber;
	}
}

// �������� ������� ��� ������� ������� � ������ ���������� �� ������ ������� ��������
size_t codepages::GetCodePageInsertPosition(uintptr_t codePage, size_t start, size_t length)
{
	const auto GetCodePage = [this](size_t position) -> uintptr_t
	{
		switch (CallbackCallSource)
		{
		case CodePageSelect: return GetMenuItemCodePage(position);
		case CodePagesFill2: return (*DialogBuilderList)[position].ItemValue;
		default: return GetListItemCodePage(position);
		}
	};

	const auto iRange = make_irange(start, start + length);
	return *std::find_if(CONST_RANGE(iRange, i) { return GetCodePage(i) >= codePage; });
}

// ��������� ��� ����������� ������� ��������
void codepages::AddCodePages(DWORD codePages)
{
	// default & re-detect
	//
	uintptr_t cp_auto = CP_DEFAULT;
	if (0 != (codePages & ::DefaultCP))
	{
		AddStandardCodePage(MSG(MDefaultCP), CP_DEFAULT, -1, true);
		cp_auto = CP_REDETECT;
	}

	AddStandardCodePage(MSG(MEditOpenAutoDetect), cp_auto, -1, (codePages & AutoCP) != 0);

	if (codePages & SearchAll)
		AddStandardCodePage(MSG(MFindFileAllCodePages), CP_SET, -1, true);

	// system codepages
	//
	AddSeparator(MSG(MGetCodePageSystem));
	AddStandardCodePage(L"ANSI", GetACP(), -1, (codePages & ::ANSI) != 0);

	if (GetOEMCP() != GetACP())
		AddStandardCodePage(L"OEM", GetOEMCP(), -1, (codePages & ::OEM) != 0);

	// unicode codepages
	//
	AddSeparator(MSG(MGetCodePageUnicode));
	AddStandardCodePage(L"UTF-8", CP_UTF8, -1, (codePages & ::UTF8) != 0);
	AddStandardCodePage(L"UTF-16 (Little endian)", CP_UNICODE, -1, (codePages & ::UTF16LE) != 0);
	AddStandardCodePage(L"UTF-16 (Big endian)", CP_REVERSEBOM, -1, (codePages & ::UTF16BE) != 0);

	// other codepages
	//
	FOR(const auto& i, InstalledCodepages())
	{
		UINT cp = i.first;
		if (IsStandardCodePage(cp))
			continue;

		string CodepageName;
		UINT len = 0;
		std::tie(len, CodepageName) = GetCodePageInfo(cp);
		if (!len || (len > 2 && (codePages & ::VOnly)))
			continue;

		bool IsCodePageNameCustom = false;
		FormatCodePageName(cp, CodepageName, IsCodePageNameCustom);

		long long selectType = GetFavorite(cp);

		// ��������� ������� �������� ���� � ����������, ���� � ��������� ������� ��������
		if (selectType & CPST_FAVORITE)
		{
			// ���� ���� ��������� ����������� ����� ���������� � ����������� ��������� ��������
			if (!favoriteCodePages)
				AddSeparator(MSG(MGetCodePageFavorites), size() - normalCodePages - (normalCodePages?1:0));

			// ��������� ������� �������� � ���������
			AddCodePage(
				CodepageName, cp,
				GetCodePageInsertPosition(
				cp, size() - normalCodePages - favoriteCodePages - (normalCodePages?1:0), favoriteCodePages
				),
				true, (selectType & CPST_FIND) != 0, IsCodePageNameCustom
				);
			// ����������� ������� ��������� ������ ��������
			favoriteCodePages++;
		}
		else if (CallbackCallSource == CodePagesFill || CallbackCallSource == CodePagesFill2 || !Global->Opt->CPMenuMode)
		{
			// ��������� ����������� ����� ������������ � ���������� ��������� ��������
			if (!normalCodePages)
				AddSeparator(MSG(MGetCodePageOther));

			// ��������� ������� �������� � ����������
			AddCodePage(
				CodepageName, cp,
				GetCodePageInsertPosition(cp, size() - normalCodePages, normalCodePages),
				true, (selectType & CPST_FIND) != 0, IsCodePageNameCustom
				);
			// ����������� ������� ��������� ������ ��������
			normalCodePages++;
		}
	}
}

// ��������� ����������/�������� �/�� ������ ��������� ������ ��������
void codepages::SetFavorite(bool State)
{
	if (Global->Opt->CPMenuMode && State)
		return;

	UINT itemPosition = CodePagesMenu->GetSelectPos();
	uintptr_t codePage = GetMenuItemCodePage();

	if ((State && IsPositionNormal(itemPosition)) || (!State && IsPositionFavorite(itemPosition)))
	{
		// �������� ������� ��������� ����� � �������
		long long selectType = GetFavorite(codePage);

		// �������/��������� � ������� ���������� � ��������� ������� ��������
		if (State)
			SetFavorite(codePage, CPST_FAVORITE | (selectType & CPST_FIND ? CPST_FIND : 0));
		else if (selectType & CPST_FIND)
			SetFavorite(codePage, CPST_FIND);
		else
			DeleteFavorite(codePage);

		// ������ ����� ������� ����
		MenuItemEx newItem(CodePagesMenu->current().strName);
		newItem.UserData = codePage;
		// ��������� ������� �������
		size_t position = CodePagesMenu->GetSelectPos();
		// ������� ������ ����� ����
		CodePagesMenu->DeleteItem(CodePagesMenu->GetSelectPos());

		// ��������� ����� ���� � ����� �����
		if (State)
		{
			// ��������� �����������, ���� ��������� ������� ������� ��� �� ����
			// � ����� ���������� ��������� ���������� ������� ��������
			if (!favoriteCodePages && normalCodePages>1)
				AddSeparator(MSG(MGetCodePageFavorites), CodePagesMenu->size() - normalCodePages);

			// ���� �������, ���� �������� �������
			const auto newPosition = GetCodePageInsertPosition(
				codePage,
				CodePagesMenu->size() - normalCodePages - favoriteCodePages,
				favoriteCodePages
				);
			// ��������� ������� �������� � ���������
			CodePagesMenu->AddItem(newItem, static_cast<int>(newPosition));

			// ������� �����������, ���� ��� ������������ ������� �������
			if (normalCodePages == 1)
				CodePagesMenu->DeleteItem(static_cast<int>(CodePagesMenu->size() - 1));

			// �������� �������� ���������� � ��������� ������� �������
			favoriteCodePages++;
			normalCodePages--;
			position++;
		}
		else
		{
			// ������� �����������, ���� ����� �������� �� ��������� �� �����
			// ��������� ������� ��������
			if (favoriteCodePages == 1 && normalCodePages>0)
				CodePagesMenu->DeleteItem(static_cast<int>(CodePagesMenu->size() - normalCodePages - 2));

			// ��������� ������� � ���������� �������, ������ ���� ��� ������������
			if (!Global->Opt->CPMenuMode)
			{
				// ��������� �����������, ���� �� ���� �� ����� ���������� ������� ��������
				if (!normalCodePages)
					AddSeparator(MSG(MGetCodePageOther));

				// ��������� ������� �������� � ����������
				CodePagesMenu->AddItem(
					newItem,
					static_cast<int>(GetCodePageInsertPosition(
					codePage,
					CodePagesMenu->size() - normalCodePages,
					normalCodePages
					))
					);
				normalCodePages++;
			}
			// ���� � ������ ������� ���������� ������ �� ������� ��������� ��������� �������, �� ������� � �����������
			else if (favoriteCodePages == 1)
				CodePagesMenu->DeleteItem(static_cast<int>(CodePagesMenu->size() - normalCodePages - 1));

			favoriteCodePages--;

			if (position == CodePagesMenu->size() - normalCodePages - 1)
				position--;
		}

		// ������������� ������� � ����
		CodePagesMenu->SetSelectPos(static_cast<int>(position >= CodePagesMenu->size()? CodePagesMenu->size() - 1 : position), 1);

		// ���������� ����
		if (Global->Opt->CPMenuMode)
			CodePagesMenu->SetPosition(-1, -1, 0, 0);
	}
}

// ��������� ���� ������ ������ ��������
void codepages::FillCodePagesVMenu(bool bShowUnicode, bool bViewOnly, bool bShowAutoDetect)
{
	uintptr_t codePage = currentCodePage;

	if (CodePagesMenu->GetSelectPos() != -1 && static_cast<size_t>(CodePagesMenu->GetSelectPos()) < CodePagesMenu->size() - normalCodePages)
		currentCodePage = GetMenuItemCodePage();

	// ������� ����
	favoriteCodePages = normalCodePages = 0;
	CodePagesMenu->clear();

	string title = MSG(MGetCodePageTitle);
	if (Global->Opt->CPMenuMode)
		title += L" *";
	CodePagesMenu->SetTitle(title);

	// ��������� ������� ��������
	AddCodePages(::OEM | ::ANSI | ::UTF8
		| (bShowUnicode ? (::UTF16BE | ::UTF16LE) : 0)
		| (bViewOnly ? ::VOnly : 0)
		| (bShowAutoDetect ? ::AutoCP : 0)
		);
	// ��������������� ������������ ������� ��������
	currentCodePage = codePage;
	// ������������� ����
	CodePagesMenu->SetPosition(-1, -1, 0, 0);
	// ���������� ����
}

// ����������� ��� ������� ��������
string& codepages::FormatCodePageName(uintptr_t CodePage, string& CodePageName) const
{
	bool IsCodePageNameCustom;
	return FormatCodePageName(CodePage, CodePageName, IsCodePageNameCustom);
}

// ����������� ��� ������� ��������
string& codepages::FormatCodePageName(uintptr_t CodePage, string& CodePageName, bool &IsCodePageNameCustom) const
{
	string strCodePage = std::to_wstring(CodePage);
	string CurrentCodePageName;

	// �������� �������� �������� ������������� ��� ������� ��������
	if (ConfigProvider().GeneralCfg()->GetValue(NamesOfCodePagesKey, strCodePage, CurrentCodePageName, L""))
	{
		IsCodePageNameCustom = true;
		if (CurrentCodePageName == CodePageName)
		{
			ConfigProvider().GeneralCfg()->DeleteValue(NamesOfCodePagesKey, strCodePage);
			IsCodePageNameCustom = false;
		}
		else
		{
			CodePageName = CurrentCodePageName;
		}
	}

	return CodePageName;
}

// ������ ��������� ������� �������������� ����� ������� ��������
enum EditCodePagesDialogControls
{
	EDITCP_BORDER,
	EDITCP_EDIT,
	EDITCP_SEPARATOR,
	EDITCP_OK,
	EDITCP_CANCEL,
	EDITCP_RESET,
};

// ������� ��� ������� �������������� ����� ������� ��������
intptr_t codepages::EditDialogProc(Dialog* Dlg, intptr_t Msg, intptr_t Param1, void* Param2)
{
	if (Msg == DN_CLOSE)
	{
		if (Param1 == EDITCP_OK || Param1 == EDITCP_RESET)
		{
			string strCodePageName;
			uintptr_t CodePage = GetMenuItemCodePage();
			string strCodePage = std::to_wstring(CodePage);

			if (Param1 == EDITCP_OK)
			{
				strCodePageName = reinterpret_cast<const wchar_t*>(Dlg->SendMessage(DM_GETCONSTTEXTPTR, EDITCP_EDIT, nullptr));
			}
			// ���� ��� ������� �������� ������, �� �������, ��� ��� �� ������
			if (strCodePageName.empty())
				ConfigProvider().GeneralCfg()->DeleteValue(NamesOfCodePagesKey, strCodePage);
			else
				ConfigProvider().GeneralCfg()->SetValue(NamesOfCodePagesKey, strCodePage, strCodePageName);
			// �������� ���������� � ������� ��������
			string CodepageName;
			UINT len = 0;
			std::tie(len, CodepageName) = GetCodePageInfo(static_cast<UINT>(CodePage));
			if (len)
			{
				// ��������� ��� ������ ��������
				bool IsCodePageNameCustom = false;
				FormatCodePageName(CodePage, CodepageName, IsCodePageNameCustom);
				// ��������� ��� ������� ��������
				int Position = CodePagesMenu->GetSelectPos();
				CodePagesMenu->DeleteItem(Position);
				MenuItemEx NewItem(FormatCodePageString(CodePage, CodepageName, IsCodePageNameCustom));
				NewItem.UserData = CodePage;
				CodePagesMenu->AddItem(NewItem, Position);
				CodePagesMenu->SetSelectPos(Position, 1);
			}
		}
	}
	return Dlg->DefProc(Msg, Param1, Param2);
}

// ����� ��������� ����� ������� ��������
void codepages::EditCodePageName()
{
	UINT Position = CodePagesMenu->GetSelectPos();
	if (IsPositionStandard(Position))
		return;
	string CodePageName = CodePagesMenu->at(Position).strName;
	size_t BoxPosition = CodePageName.find(BoxSymbols[BS_V1]);
	if (BoxPosition == string::npos)
		return;
	CodePageName.erase(0, BoxPosition + 2);
	FarDialogItem EditDialogData[] =
	{
		{ DI_DOUBLEBOX, 3, 1, 50, 5, 0, nullptr, nullptr, 0, MSG(MGetCodePageEditCodePageName) },
		{ DI_EDIT, 5, 2, 48, 2, 0, L"CodePageName", nullptr, DIF_FOCUS | DIF_HISTORY, CodePageName.data() },
		{ DI_TEXT, -1, 3, 0, 3, 0, nullptr, nullptr, DIF_SEPARATOR, L"" },
		{ DI_BUTTON, 0, 4, 0, 3, 0, nullptr, nullptr, DIF_DEFAULTBUTTON | DIF_CENTERGROUP, MSG(MOk) },
		{ DI_BUTTON, 0, 4, 0, 3, 0, nullptr, nullptr, DIF_CENTERGROUP, MSG(MCancel) },
		{ DI_BUTTON, 0, 4, 0, 3, 0, nullptr, nullptr, DIF_CENTERGROUP, MSG(MGetCodePageResetCodePageName) }
	};
	auto EditDialog = MakeDialogItemsEx(EditDialogData);
	const auto Dlg = Dialog::create(EditDialog, &codepages::EditDialogProc, this);
	Dlg->SetPosition(-1, -1, 54, 7);
	Dlg->SetHelp(L"EditCodePageNameDlg");
	Dlg->Process();
}

bool codepages::SelectCodePage(uintptr_t& CodePage, bool bShowUnicode, bool bViewOnly, bool bShowAutoDetect)
{
	bool Result = false;
	CallbackCallSource = CodePageSelect;
	currentCodePage = CodePage;
	// ������ ����
	CodePagesMenu = VMenu2::create(L"", nullptr, 0, ScrY - 4);
	CodePagesMenu->SetBottomTitle(MSG(!Global->Opt->CPMenuMode?MGetCodePageBottomTitle:MGetCodePageBottomShortTitle));
	CodePagesMenu->SetMenuFlags(VMENU_WRAPMODE | VMENU_AUTOHIGHLIGHT);
	CodePagesMenu->SetHelp(L"CodePagesMenu");
	CodePagesMenu->SetId(CodePagesMenuId);
	// ��������� ������� ��������
	FillCodePagesVMenu(bShowUnicode, bViewOnly, bShowAutoDetect);
	// ���������� ����

	// ���� ��������� ��������� ����
	intptr_t r = CodePagesMenu->Run([&](const Manager::Key& RawKey)->int
	{
		const auto ReadKey = RawKey.FarKey();
		int KeyProcessed = 1;
		switch (ReadKey)
		{
			// ��������� �������/������ ��������� ������ ��������
		case KEY_CTRLH:
		case KEY_RCTRLH:
			Global->Opt->CPMenuMode = !Global->Opt->CPMenuMode;
			CodePagesMenu->SetBottomTitle(MSG(!Global->Opt->CPMenuMode?MGetCodePageBottomTitle:MGetCodePageBottomShortTitle));
			FillCodePagesVMenu(bShowUnicode, bViewOnly, bShowAutoDetect);
			break;
			// ��������� �������� ������� �������� �� ������ ���������
		case KEY_DEL:
		case KEY_NUMDEL:
			SetFavorite(false);
			break;
			// ��������� ���������� ������� �������� � ������ ���������
		case KEY_INS:
		case KEY_NUMPAD0:
			SetFavorite(true);
			break;
			// ����������� ��� ������� ��������
		case KEY_F4:
			EditCodePageName();
			break;
		default:
			KeyProcessed = 0;
		}
		return KeyProcessed;
	});

	// �������� ��������� ������� ��������
	if (r >= 0)
	{
		CodePage = GetMenuItemCodePage();
		Result = true;
	}
	CodePagesMenu.reset();
	return Result;
}

// ��������� ������ ��������� ��������
void codepages::FillCodePagesList(std::vector<DialogBuilderListItem2> &List, bool allowAuto, bool allowAll, bool allowDefault, bool bViewOnly)
{
	CallbackCallSource = CodePagesFill2;
	// ������������� ���������� ��� ������� �� ��������
	DialogBuilderList = &List;
	favoriteCodePages = normalCodePages = 0;
	selectedCodePages = !allowAuto && allowAll;
	// ��������� ����������� �������� � ������
	AddCodePages
		((allowDefault ? ::DefaultCP : 0)
		| (allowAuto ? ::AutoCP : 0)
		| (allowAll ? ::SearchAll : 0)
		| (bViewOnly ? ::VOnly : 0)
		| ::AllStandard
		);
	DialogBuilderList = nullptr;
}


// ��������� ������ ��������� ��������
UINT codepages::FillCodePagesList(Dialog* Dlg, UINT controlId, uintptr_t codePage, bool allowAuto, bool allowAll, bool allowDefault, bool bViewOnly)
{
	CallbackCallSource = CodePagesFill;
	// ������������� ���������� ��� ������� �� ��������
	dialog = Dlg;
	control = controlId;
	currentCodePage = codePage;
	favoriteCodePages = normalCodePages = 0;
	selectedCodePages = !allowAuto && allowAll;
	// ��������� ����������� �������� � ������
	AddCodePages
		((allowDefault ? ::DefaultCP : 0)
		| (allowAuto ? ::AutoCP : 0)
		| (allowAll ? ::SearchAll : 0)
		| (bViewOnly ? ::VOnly : 0)
		| ::AllStandard
		);

	if (CallbackCallSource == CodePagesFill)
	{
		// ���� ���� �������� �������
		FarListInfo info = { sizeof(FarListInfo) };
		Dlg->SendMessage(DM_LISTINFO, control, &info);

		for (size_t i = 0; i != info.ItemsNumber; ++i)
		{
			if (GetListItemCodePage(i) == codePage)
			{
				FarListGetItem Item = { sizeof(FarListGetItem), static_cast<intptr_t>(i) };
				dialog->SendMessage(DM_LISTGETITEM, control, &Item);
				dialog->SendMessage(DM_SETTEXTPTR, control, const_cast<wchar_t*>(Item.Item.Text));
				FarListPos Pos = { sizeof(FarListPos), static_cast<intptr_t>(i), -1 };
				dialog->SendMessage(DM_LISTSETCURPOS, control, &Pos);
				break;
			}
		}
	}

	// ���������� ����� ��������� ������ ��������
	return favoriteCodePages;
}

bool codepages::IsCodePageSupported(uintptr_t CodePage, size_t MaxCharSize) const
{
	if (CodePage == CP_DEFAULT || IsStandardCodePage(CodePage))
		return true;

	const auto CharSize = GetCodePageInfo(static_cast<UINT>(CodePage)).first;
	return CharSize != 0 && CharSize <= MaxCharSize;
}

long long codepages::GetFavorite(uintptr_t cp)
{
	long long value = 0;
	ConfigProvider().GeneralCfg()->GetValue(FavoriteCodePagesKey, std::to_wstring(cp), value, 0);
	return value;
}

void codepages::SetFavorite(uintptr_t cp, long long value)
{
	ConfigProvider().GeneralCfg()->SetValue(FavoriteCodePagesKey, std::to_wstring(cp), value);
}

void codepages::DeleteFavorite(uintptr_t cp)
{
	ConfigProvider().GeneralCfg()->DeleteValue(FavoriteCodePagesKey, std::to_wstring(cp));
}

GeneralConfig::int_values_enumerator codepages::GetFavoritesEnumerator()
{
	return ConfigProvider().GeneralCfg()->GetIntValuesEnumerator(FavoriteCodePagesKey);
}


//################################################################################################

F8CP::F8CP(bool viewer):
	m_AcpName(MSG(Global->OnlyEditorViewerUsed? (viewer? MSingleViewF8 : MSingleEditF8) : (viewer? MViewF8 : MEditF8))),
	m_OemName(MSG(Global->OnlyEditorViewerUsed? (viewer? MSingleViewF8DOS : MSingleEditF8DOS) : (viewer? MViewF8DOS : MEditF8DOS))),
	m_UtfName(L"UTF-8")
{

	UINT defcp = viewer ? Global->Opt->ViOpt.DefaultCodePage : Global->Opt->EdOpt.DefaultCodePage;

	string cps(viewer ? Global->Opt->ViOpt.strF8CPs : Global->Opt->EdOpt.strF8CPs);
	if (cps != L"-1")
	{
		std::unordered_set<UINT> used_cps;
		std::vector<string> f8list;
		split(f8list, cps, 0);
		std::for_each(CONST_RANGE(f8list, str_cp)
		{
			string s(str_cp);
			ToUpper(s);
			UINT cp = 0;
			if (s == L"ANSI" || s == L"ACP" || s == L"WIN")
				cp = GetACP();
			else if (s == L"OEM" || s == L"OEMCP" || s == L"DOS")
				cp = GetOEMCP();
			else if (s == L"UTF8" || s == L"UTF-8")
				cp = CP_UTF8;
			else if (s == L"DEFAULT")
				cp = defcp;
			else {
				try { cp = std::stoul(s); }
				catch (std::exception&) { cp = 0; }
			}
			if (cp && Codepages().IsCodePageSupported(cp, viewer ? 2:20) && used_cps.find(cp) == used_cps.end())
			{
				m_F8CpOrder.push_back(cp);
				used_cps.insert(cp);
			}
		});
	}
	if (m_F8CpOrder.empty())
	{
		UINT acp = GetACP(), oemcp = GetOEMCP();
		if (cps != L"-1")
			defcp = acp;
		m_F8CpOrder.push_back(defcp);
		if (acp != defcp)
			m_F8CpOrder.push_back(acp);
		if (oemcp != defcp && oemcp != acp)
			m_F8CpOrder.push_back(oemcp);
	}
}

uintptr_t F8CP::NextCP(uintptr_t cp) const
{
	UINT next_cp = m_F8CpOrder[0];
	if (cp <= std::numeric_limits<UINT>::max())
	{
		auto curr = std::find(ALL_CONST_RANGE(m_F8CpOrder), static_cast<UINT>(cp));
		if (curr != m_F8CpOrder.cend() && ++curr != m_F8CpOrder.cend())
			next_cp = *curr;
	}
	return next_cp;
}

const string& F8CP::NextCPname(uintptr_t cp) const
{
	UINT next_cp = static_cast<UINT>(NextCP(cp));
	if (next_cp == GetACP())
		return m_AcpName;
	else if (next_cp == GetOEMCP())
		return m_OemName;
	else if (next_cp == CP_UTF8)
		return m_UtfName;
	else
		return m_Number = std::to_wstring(next_cp);
}
