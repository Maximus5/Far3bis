﻿#ifndef HELP_HPP_B6683B98_B047_4D77_AD11_1AB7392B9A2E
#define HELP_HPP_B6683B98_B047_4D77_AD11_1AB7392B9A2E
#pragma once

/*
help.hpp

Помощь
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

#include "modal.hpp"

class HelpRecord;

class Help:public Modal
{
	struct private_tag {};

public:
	static help_ptr create(const string& Topic,const wchar_t *Mask=nullptr,UINT64 Flags=0);
	Help(private_tag);
	virtual ~Help() override;

	virtual int  ProcessKey(const Manager::Key& Key) override;
	virtual int  ProcessMouse(const MOUSE_EVENT_RECORD *MouseEvent) override;
	virtual void InitKeyBar() override;
	virtual void SetScreenPosition() override;
	virtual void ResizeConsole() override;
	virtual bool CanFastHide() const override; // Введена для нужд CtrlAltShift
	virtual int GetTypeAndName(string &strType, string &strName) override;
	virtual int GetType() const override { return windowtype_help; }
	virtual long long VMProcess(int OpCode, void* vParam, long long iParam) override;

	BOOL GetError() const {return ErrorHelp;}
	static bool MkTopic(const class Plugin* pPlugin, const string& HelpTopic, string &strTopic);
	static string MakeLink(const string& path, const string& topic);

	struct StackHelpData;

private:
	virtual void DisplayObject() override;
	virtual string GetTitle() const override { return {}; }
	void init(const string& Topic, const wchar_t *Mask, UINT64 Flags);
	int  ReadHelp(const string& Mask);
	void AddLine(const string& Line);
	void AddTitle(const string& Title);
	static void HighlightsCorrection(string &strStr);
	void FastShow();
	void DrawWindowFrame() const;
	void OutString(const wchar_t *Str);
	int  StringLen(const string& Str);
	void CorrectPosition() const;
	bool IsReferencePresent();
	bool GetTopic(int realX, int realY, string& strTopic);
	void MoveToReference(int Forward,int CurScreen);
	void ReadDocumentsHelp(int TypeIndex);
	void Search(os::fs::file& HelpFile,uintptr_t nCodePage);
	int JumpTopic(const string& JumpTopic);
	int JumpTopic();
	int CanvasHeight() const { return ObjHeight() - 1 - 1;  }
	int HeaderHeight() const { return FixCount? FixCount + 1 : 0; }
	int BodyHeight() const { return CanvasHeight() - HeaderHeight();  }
	int CanvasWidth() const { return ObjWidth() - 1 - 1; }

	std::unique_ptr<StackHelpData> StackData;
	std::stack<StackHelpData, std::vector<StackHelpData>> Stack; // стек возврата
	std::vector<HelpRecord> HelpList; // "хелп" в памяти.
	string  strFullHelpPathName;
	string strCtrlColorChar;    // CtrlColorChar - опция! для спецсимвола-
	string strCurPluginContents; // помним PluginContents (для отображения в заголовке)
	string strCtrlStartPosChar;
	string strLastSearchStr;

	int FixCount;             // количество строк непрокручиваемой области

	int MouseDownX, MouseDownY, BeforeMouseDownX, BeforeMouseDownY;
	int MsX, MsY;

	// символа - для атрибутов
	FarColor CurColor;             // CurColor - текущий цвет отрисовки
	int CtrlTabSize;          // CtrlTabSize - опция! размер табуляции

	DWORD LastStartPos;
	DWORD StartPos;

	bool MouseDown;
	bool IsNewTopic;
	bool m_TopicFound;
	bool ErrorHelp;
	bool LastSearchCase, LastSearchWholeWords, LastSearchRegexp;
};

#endif // HELP_HPP_B6683B98_B047_4D77_AD11_1AB7392B9A2E
