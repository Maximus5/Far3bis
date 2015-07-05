#pragma once

/*
FarDlgBuilder.hpp

������������ ��������������� �������� - ������ ��� ����������� ������������ � FAR
*/
/*
Copyright � 2010 Far Group
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

#include "DlgBuilder.hpp"
#include "dialog.hpp"
#include "config.hpp"

struct DialogItemEx;

// ������� ����������� ������ � �������.
struct DialogBuilderListItem2
{
	// �������, ������� ����� �������� � �������.
	string Text;

	LISTITEMFLAGS Flags;

	// ��������, ������� ����� �������� � ���� Value ��� ������ ���� �������.
	int ItemValue;
};

template<class T>
struct ListControlBinding: public DialogItemBinding<T>
{
	int& Value;
	string *Text;
	FarList *List;

	ListControlBinding(int& aValue, string *aText, FarList *aList)
		: Value(aValue), Text(aText), List(aList)
	{
	}

	virtual ~ListControlBinding()
	{
		if (List)
		{
			delete [] List->Items;
		}
		delete List;
	}

	virtual void SaveValue(T *Item, int RadioGroupIndex) override
	{
		if (List)
		{
			FarListItem &ListItem = List->Items[Item->ListPos];
			Value = ListItem.Reserved[0];
		}
		if (Text)
		{
			*Text = Item->strData;
		}
	}
};

/*
����� ��� ������������� ���������� ��������, ������������ ������ ���� FAR.
���������� FAR'������ ����� string ��� ������ � ���������� ������.

��� ����, ����� �������� ������� ������������ ����������
��������� �� �����������, ����� ������������ ����� DialogItemEx::Indent().

������������ automation (��������� ������ ������ �������� � ����������� �� ���������
�������). ����������� ��� ������ ������ LinkFlags().
*/
class DialogBuilder: noncopyable, public DialogBuilderBase<DialogItemEx>
{
public:
	DialogBuilder(LNGID TitleMessageId, const wchar_t *HelpTopic = nullptr, Dialog::dialog_handler handler = nullptr);
	DialogBuilder();
	~DialogBuilder();

	// ��������� ���� ���� DI_FIXEDIT ��� �������������� ���������� ��������� ��������.
	virtual DialogItemEx *AddIntEditField(int *Value, int Width) override;
	virtual DialogItemEx *AddIntEditField(IntOption& Value, int Width);
	virtual DialogItemEx *AddHexEditField(IntOption& Value, int Width);

	// ��������� ���� ���� DI_EDIT ��� �������������� ���������� ���������� ��������.
	DialogItemEx *AddEditField(string& Value, int Width, const wchar_t *HistoryID = nullptr, FARDIALOGITEMFLAGS Flags = 0);
	DialogItemEx *AddEditField(StringOption& Value, int Width, const wchar_t *HistoryID = nullptr, FARDIALOGITEMFLAGS Flags = 0);

	// ��������� ���� ���� DI_FIXEDIT ��� �������������� ���������� ���������� ��������.
	DialogItemEx *AddFixEditField(string& Value, int Width, const wchar_t *Mask = nullptr);
	DialogItemEx *AddFixEditField(StringOption& Value, int Width, const wchar_t *Mask = nullptr);

	// ��������� ������������ ���� ���� DI_EDIT ��� �������� ���������� ���������� ��������.
	DialogItemEx *AddConstEditField(const string& Value, int Width, FARDIALOGITEMFLAGS Flags = 0);

	// ��������� ���������� ������ � ���������� ����������.
	DialogItemEx *AddComboBox(int& Value, string *Text, int Width, const DialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddComboBox(IntOption& Value, string *Text, int Width, const DialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddComboBox(int& Value, string *Text, int Width, const std::vector<DialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddComboBox(IntOption& Value, string *Text, int Width, const std::vector<DialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);

	DialogItemEx *AddListBox(int& Value, int Width, int Height, const DialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListBox(IntOption& Value, int Width, int Height, const DialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListBox(int& Value, int Width, int Height, const std::vector<DialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListBox(IntOption& Value, int Width, int Height, const std::vector<DialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);

	DialogItemEx *AddCheckbox(int TextMessageId, BOOL *Value, int Mask = 0, bool ThreeState = false)
	{
		return DialogBuilderBase<DialogItemEx>::AddCheckbox(TextMessageId, Value, Mask, ThreeState);
	}
	DialogItemEx *AddCheckbox(int TextMessageId, IntOption& Value, int Mask=0, bool ThreeState=false);
	DialogItemEx *AddCheckbox(int TextMessageId, Bool3Option& Value);
	DialogItemEx *AddCheckbox(int TextMessageId, BoolOption& Value);
	DialogItemEx *AddCheckbox(const wchar_t* Caption, BoolOption& Value);

	void AddRadioButtons(IntOption& Value, int OptionCount, const int MessageIDs[], bool FocusOnSelected=false);

	// ��������� ��������� ��������� Parent � Target. ����� Parent->Selected �����
	// false, ������������� ����� Flags � �������� Target; ����� ����� true -
	// ���������� �����.
	// ���� LinkLabels ����������� � true, �� ��������� ��������, ����������� � �������� Target
	// �������� AddTextBefore � AddTextAfter, ����� ����������� � ��������� Parent.
	void LinkFlags(DialogItemEx *Parent, DialogItemEx *Target, FARDIALOGITEMFLAGS Flags, bool LinkLabels=true);

	void AddOKCancel();
	void AddOKCancel(int OKMessageId, int CancelMessageId);
	void AddOK();

	void SetDialogMode(DWORD Flags);

	int AddTextWrap(const wchar_t *text, bool center=false, int width=0);

	void SetId(const GUID& Id);
	const GUID& GetId() const {return m_Id;}

protected:
	virtual void InitDialogItem(DialogItemEx *Item, const wchar_t* Text) override;
	virtual int TextWidth(const DialogItemEx &Item) override;
	virtual const wchar_t* GetLangString(int MessageID) override;
	virtual intptr_t DoShowDialog() override;
	virtual DialogItemBinding<DialogItemEx> *CreateCheckBoxBinding(BOOL* Value, int Mask) override;
	virtual DialogItemBinding<DialogItemEx> *CreateRadioButtonBinding(int *Value) override;

	DialogItemBinding<DialogItemEx> *CreateCheckBoxBinding(IntOption &Value, int Mask);
	DialogItemBinding<DialogItemEx> *CreateCheckBoxBinding(Bool3Option& Value);
	DialogItemBinding<DialogItemEx> *CreateCheckBoxBinding(BoolOption& Value);
	DialogItemBinding<DialogItemEx> *CreateRadioButtonBinding(IntOption& Value);

	DialogItemEx *AddListControl(FARDIALOGITEMTYPES Type, int& Value, string *Text, int Width, int Height, const DialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListControl(FARDIALOGITEMTYPES Type, IntOption& Value, string *Text, int Width, int Height, const DialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListControl(FARDIALOGITEMTYPES Type, int& Value, string *Text, int Width, int Height, const std::vector<DialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListControl(FARDIALOGITEMTYPES Type, IntOption& Value, string *Text, int Width, int Height, const std::vector<DialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);

private:
	static void LinkFlagsByID(DialogItemEx *Parent, DialogItemEx* Target, FARDIALOGITEMFLAGS Flags);

	const wchar_t *m_HelpTopic;
	DWORD m_Mode;
	GUID m_Id;
	bool m_IdExist;
	Dialog::dialog_handler m_handler;
};
