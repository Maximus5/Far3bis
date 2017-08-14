﻿/*
console_session.cpp


*/
/*
Copyright © 2017 Far Group
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

#include "desktop.hpp"
#include "global.hpp"
#include "manager.hpp"
#include "interf.hpp"
#include "config.hpp"
#include "console.hpp"
#include "colormix.hpp"
#include "constitle.hpp"
#include "scrbuf.hpp"
#include "ctrlobj.hpp"
#include "cmdline.hpp"

class context: noncopyable, public i_context
{
public:
	void Activate() override
	{
		if (m_Activated)
			return;

		m_Activated = true;
		++Global->SuppressIndicators;
		++Global->SuppressClock;

		Global->WindowManager->ModalDesktopWindow();
		Global->WindowManager->PluginCommit();
	}

	void DrawCommand(const string& Command) override
	{
		Global->CtrlObject->CmdLine()->DrawFakeCommand(Command);
		ScrollScreen(1);

		m_Command = Command;
		m_ShowCommand = true;

		DoPrologue();
	}

	void Consolise(bool SetTextColour = true) override
	{
		assert(m_Activated);

		if (m_Consolised)
			return;
		m_Consolised = true;

		Global->ScrBuf->MoveCursor(0, WhereY());
		SetInitialCursorType();

		if (!m_Command.empty())
			ConsoleTitle::SetFarTitle(m_Command);

		// BUGBUG, implement better & safer way to do this
		const auto LockCount = Global->ScrBuf->GetLockCount();
		Global->ScrBuf->SetLockCount(0);

		Global->ScrBuf->Flush();

		// BUGBUG, implement better & safer way to do this
		Global->ScrBuf->SetLockCount(LockCount);

		if (SetTextColour)
			Console().SetTextAttributes(colors::PaletteColorToFarColor(COL_COMMANDLINEUSERSCREEN));
	}

	void DoPrologue() override
	{
		Global->CtrlObject->Desktop->TakeSnapshot();

		const auto XPos = 0;
		const auto YPos = ScrY - (Global->Opt->ShowKeyBar? 1 : 0);

		GotoXY(XPos, YPos);
		m_Finalised = false;
	}

	void DoEpilogue() override
	{
		if (!m_Activated)
			return;

		if (m_Finalised)
			return;

		if (m_Consolised)
		{
			if (Global->Opt->ShowKeyBar)
			{
				Console().Write(L"\n");
			}
			Console().Commit();
			Global->ScrBuf->FillBuf();

			m_Consolised = false;
		}

		// Empty command means that user simply pressed Enter in command line, in this case we don't want additional scrolling
		// ShowCommand is false when there is no "command" - class instantiated by FCTL_GETUSERSCREEN.
		if (!m_Command.empty() || !m_ShowCommand)
		{
			ScrollScreen(1);
		}

		Global->CtrlObject->Desktop->TakeSnapshot();

		m_Finalised = true;
	}

	~context() override
	{
		if (!m_Activated)
			return;

		Global->WindowManager->UnModalDesktopWindow();
		Global->WindowManager->PluginCommit();
		--Global->SuppressClock;
		--Global->SuppressIndicators;
	}

private:
	string m_Command;
	bool m_ShowCommand{};
	bool m_Activated{};
	bool m_Finalised{};
	bool m_Consolised{};
};

void console_session::EnterPluginContext()
{
	if (!m_PluginContextInvocations)
	{
		m_PluginContext = GetContext();
		m_PluginContext->Activate();
	}
	else
	{
		m_PluginContext->DoEpilogue();
	}

	m_PluginContext->DoPrologue();
	m_PluginContext->Consolise(!m_PluginContextInvocations);

	++m_PluginContextInvocations;
}

void console_session::LeavePluginContext()
{
	if (m_PluginContextInvocations)
		--m_PluginContextInvocations;

	if (m_PluginContext)
	{
		m_PluginContext->DoEpilogue();
	}
	else
	{
		// FCTL_SETUSERSCREEN without corresponding FCTL_GETUSERSCREEN
		// Old (1.x) behaviour emulation:
		if (Global->Opt->ShowKeyBar)
		{
			Console().Write(L"\n");
		}
		Console().Commit();
		Global->ScrBuf->FillBuf();
		ScrollScreen(1);
		Global->CtrlObject->Desktop->TakeSnapshot();
	}

	if (m_PluginContextInvocations)
		return;

	m_PluginContext.reset();
}

std::shared_ptr<i_context> console_session::GetContext()
{
	if (auto Result = m_Context.lock())
	{
		return Result;
	}
	else
	{
		Result = std::make_shared<context>();
		m_Context = Result;
		return Result;
	}
}
