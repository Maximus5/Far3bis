﻿#ifndef FARDLGBUILDER_HPP_4AD5C50D_B9AC_49DE_B34B_BAD22219BCBD
#define FARDLGBUILDER_HPP_4AD5C50D_B9AC_49DE_B34B_BAD22219BCBD
#pragma once

/*
FarDlgBuilder.hpp

Динамическое конструирование диалогов - версия для внутреннего употребления в FAR
*/
/*
Copyright © 2010 Far Group
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

enum class lng: int;
struct DialogItemEx;

// Элемент выпадающего списка в диалоге.
struct FarDialogBuilderListItem
{
	// Строчка из LNG-файла, которая будет показана в диалоге.
	lng MessageId;

	// Значение, которое будет записано в поле Value при выборе этой строчки.
	int ItemValue;
};

// Элемент выпадающего списка в диалоге.
struct FarDialogBuilderListItem2
{
	// Строчка, которая будет показана в диалоге.
	string Text;

	LISTITEMFLAGS Flags;

	// Значение, которое будет записано в поле Value при выборе этой строчки.
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

	virtual ~ListControlBinding() override
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
Класс для динамического построения диалогов, используемый внутри кода FAR.
Использует FAR'овский класс string для работы с текстовыми полями.

Для того, чтобы сместить элемент относительно дефолтного
положения по горизонтали, можно использовать метод DialogItemEx::Indent().

Поддерживает automation (изменение флагов одного элемента в зависимости от состояния
другого). Реализуется при помощи метода LinkFlags().
*/
class DialogBuilder: noncopyable, public DialogBuilderBase<DialogItemEx>
{
	using base = DialogBuilderBase<DialogItemEx>;
public:
	DialogBuilder(lng TitleMessageId, const wchar_t *HelpTopic = nullptr, Dialog::dialog_handler handler = nullptr);
	DialogBuilder();
	~DialogBuilder();

	// Добавляет поле типа DI_FIXEDIT для редактирования указанного числового значения.
	virtual DialogItemEx *AddIntEditField(int *Value, int Width) override;
	virtual DialogItemEx *AddIntEditField(IntOption& Value, int Width);
	virtual DialogItemEx *AddHexEditField(IntOption& Value, int Width);

	// Добавляет поле типа DI_EDIT для редактирования указанного строкового значения.
	DialogItemEx *AddEditField(string& Value, int Width, const wchar_t *HistoryID = nullptr, FARDIALOGITEMFLAGS Flags = 0);
	DialogItemEx *AddEditField(StringOption& Value, int Width, const wchar_t *HistoryID = nullptr, FARDIALOGITEMFLAGS Flags = 0);

	// Добавляет поле типа DI_FIXEDIT для редактирования указанного строкового значения.
	DialogItemEx *AddFixEditField(string& Value, int Width, const wchar_t *Mask = nullptr);
	DialogItemEx *AddFixEditField(StringOption& Value, int Width, const wchar_t *Mask = nullptr);

	// Добавляет неизменяемое поле типа DI_EDIT для посмотра указанного строкового значения.
	DialogItemEx *AddConstEditField(const string& Value, int Width, FARDIALOGITEMFLAGS Flags = 0);

	// Добавляет выпадающий список с указанными значениями.
	DialogItemEx *AddComboBox(int& Value, string *Text, int Width, const FarDialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddComboBox(IntOption& Value, string *Text, int Width, const FarDialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddComboBox(int& Value, string *Text, int Width, const std::vector<FarDialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddComboBox(IntOption& Value, string *Text, int Width, const std::vector<FarDialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);

	DialogItemEx *AddListBox(int& Value, int Width, int Height, const FarDialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListBox(IntOption& Value, int Width, int Height, const FarDialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListBox(int& Value, int Width, int Height, const std::vector<FarDialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListBox(IntOption& Value, int Width, int Height, const std::vector<FarDialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);

	decltype(auto) AddCheckbox(lng TextMessageId, int *Value, int Mask = 0, bool ThreeState = false)
	{
		return base::AddCheckbox(static_cast<int>(TextMessageId), Value, Mask, ThreeState);
	}
	DialogItemEx *AddCheckbox(lng TextMessageId, IntOption& Value, int Mask=0, bool ThreeState=false);
	DialogItemEx *AddCheckbox(lng TextMessageId, Bool3Option& Value);
	DialogItemEx *AddCheckbox(lng TextMessageId, BoolOption& Value);
	DialogItemEx *AddCheckbox(const wchar_t* Caption, BoolOption& Value);

	void AddRadioButtons(IntOption& Value, int OptionCount, const lng MessageIDs[], bool FocusOnSelected=false);

	using base::AddText;

	decltype(auto) AddText(lng LabelId)
	{
		return base::AddText(static_cast<int>(LabelId));
	}

	decltype(auto) AddTextBefore(DialogItemEx* RelativeTo, lng LabelId)
	{
		return base::AddTextBefore(RelativeTo, static_cast<int>(LabelId));
	}

	using base::AddTextAfter;

	decltype(auto) AddTextAfter(DialogItemEx* RelativeTo, lng LabelId, int skip = 1)
	{
		return base::AddTextAfter(RelativeTo, static_cast<int>(LabelId), skip);
	}

	using base::AddSeparator;

	decltype(auto) AddSeparator(lng LabelId)
	{
		return base::AddSeparator(static_cast<int>(LabelId));
	}

	// Связывает состояние элементов Parent и Target. Когда Parent->Selected равно
	// false, устанавливает флаги Flags у элемента Target; когда равно true -
	// сбрасывает флаги.
	// Если LinkLabels установлено в true, то текстовые элементы, добавленные к элементу Target
	// методами AddTextBefore и AddTextAfter, также связываются с элементом Parent.
	void LinkFlags(DialogItemEx *Parent, DialogItemEx *Target, FARDIALOGITEMFLAGS Flags, bool LinkLabels=true);

	void AddOKCancel();
	void AddOKCancel(lng OKMessageId, lng CancelMessageId);
	void AddOKCancel(lng OKMessageId, lng ExtraMessageId, lng CancelMessageId);
	void AddOK();

	void SetDialogMode(DWORD Flags);

	int AddTextWrap(const wchar_t *text, bool center=false, int width=0);

	void SetId(const GUID& Id);
	const GUID& GetId() const {return m_Id;}

protected:
	const wchar_t* GetLangString(lng MessageID);
	virtual void InitDialogItem(DialogItemEx *Item, const wchar_t* Text) override;
	virtual int TextWidth(const DialogItemEx &Item) override;
	virtual intptr_t DoShowDialog() override;
	virtual DialogItemBinding<DialogItemEx> *CreateCheckBoxBinding(BOOL* Value, int Mask) override;
	virtual DialogItemBinding<DialogItemEx> *CreateRadioButtonBinding(int *Value) override;

	DialogItemBinding<DialogItemEx> *CreateCheckBoxBinding(IntOption &Value, int Mask);
	DialogItemBinding<DialogItemEx> *CreateCheckBoxBinding(Bool3Option& Value);
	DialogItemBinding<DialogItemEx> *CreateCheckBoxBinding(BoolOption& Value);
	DialogItemBinding<DialogItemEx> *CreateRadioButtonBinding(IntOption& Value);

	DialogItemEx *AddListControl(FARDIALOGITEMTYPES Type, int& Value, string *Text, int Width, int Height, const FarDialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListControl(FARDIALOGITEMTYPES Type, IntOption& Value, string *Text, int Width, int Height, const FarDialogBuilderListItem *Items, size_t ItemCount, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListControl(FARDIALOGITEMTYPES Type, int& Value, string *Text, int Width, int Height, const std::vector<FarDialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);
	DialogItemEx *AddListControl(FARDIALOGITEMTYPES Type, IntOption& Value, string *Text, int Width, int Height, const std::vector<FarDialogBuilderListItem2> &Items, FARDIALOGITEMFLAGS Flags = DIF_NONE);

private:
	static void LinkFlagsByID(DialogItemEx *Parent, DialogItemEx* Target, FARDIALOGITEMFLAGS Flags);
	virtual const wchar_t* GetLangString(int MessageID) override;

	const wchar_t *m_HelpTopic;
	DWORD m_Mode;
	GUID m_Id;
	bool m_IdExist;
	Dialog::dialog_handler m_handler;
};

#endif // FARDLGBUILDER_HPP_4AD5C50D_B9AC_49DE_B34B_BAD22219BCBD
