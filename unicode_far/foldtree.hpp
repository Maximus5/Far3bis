﻿#ifndef FOLDTREE_HPP_B257EC6E_953F_44BB_9F98_D55AEB1D584A
#define FOLDTREE_HPP_B257EC6E_953F_44BB_9F98_D55AEB1D584A
#pragma once

/*
foldtree.hpp

Поиск каталога по Alt-F10
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
#include "config.hpp"

class TreeList;
class EditControl;
class SaveScreen;

class FolderTree:public Modal
{
	struct private_tag {};

public:
	static foldertree_ptr create(string &strResultFolder, int ModalMode, int IsStandalone = TRUE, bool IsFullScreen = true);

	FolderTree(private_tag, int ModalMode, int IsStandalone, bool IsFullScreen);

	virtual int ProcessKey(const Manager::Key& Key) override;
	virtual int ProcessMouse(const MOUSE_EVENT_RECORD *MouseEvent) override;
	virtual void InitKeyBar() override;
	virtual void SetScreenPosition() override;
	virtual void ResizeConsole() override;
	/* $ Введена для нужд CtrlAltShift OT */
	virtual bool CanFastHide() const override;
	virtual int GetTypeAndName(string &strType, string &strName) override;
	virtual int GetType() const override { return windowtype_findfolder; }

private:
	virtual string GetTitle() const override { return {}; }
	virtual void DisplayObject() override;

	void init(string &strResultFolder);
	void DrawEdit();
	void SetCoords();

	std::shared_ptr<TreeList> Tree;
	std::unique_ptr<EditControl> FindEdit;
	int ModalMode;
	bool IsFullScreen;
	int IsStandalone;
	string strNewFolder;
	string strLastName;
};

#endif // FOLDTREE_HPP_B257EC6E_953F_44BB_9F98_D55AEB1D584A
