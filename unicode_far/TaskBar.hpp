﻿#ifndef TASKBAR_HPP_2522B9DF_D677_4AA9_8777_B5A1F588D4C1
#define TASKBAR_HPP_2522B9DF_D677_4AA9_8777_B5A1F588D4C1
#pragma once

/*
TaskBar.hpp

Windows 7 taskbar support
*/
/*
Copyright © 2009 Far Group
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

#include "farwinapi.hpp"

class taskbar: noncopyable
{
public:
	TBPFLAG GetProgressState() const;
	void SetProgressState(TBPFLAG tbpFlags);
	void SetProgressValue(UINT64 Completed, UINT64 Total);
	static void Flash();

private:
	friend taskbar& Taskbar();

	taskbar();

	TBPFLAG State;

	os::com::ptr<ITaskbarList3> mTaskbarList;
};

taskbar& Taskbar();

class IndeterminateTaskBar: noncopyable
{
public:
	IndeterminateTaskBar(bool EndFlash = true);
	~IndeterminateTaskBar();

private:
	bool EndFlash;
};

template<TBPFLAG T>
class TaskBarState: noncopyable
{
public:
	TaskBarState():PrevState(Taskbar().GetProgressState())
	{
		if (PrevState!=TBPF_ERROR && PrevState!=TBPF_PAUSED)
		{
			if (PrevState==TBPF_INDETERMINATE||PrevState==TBPF_NOPROGRESS)
			{
				Taskbar().SetProgressValue(1,1);
			}
			Taskbar().SetProgressState(T);
			Taskbar().Flash();
		}
	}

	~TaskBarState()
	{
		Taskbar().SetProgressState(PrevState);
	}

private:
	TBPFLAG PrevState;
};

using TaskBarPause = TaskBarState<TBPF_PAUSED>;
using TaskBarError = TaskBarState<TBPF_ERROR>;

#endif // TASKBAR_HPP_2522B9DF_D677_4AA9_8777_B5A1F588D4C1
