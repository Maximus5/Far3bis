/*
wm_listener.cpp

��������� ������� ���������
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

#include "headers.hpp"
#pragma hdrstop

#include "wm_listener.hpp"
#include "config.hpp"
#include "imports.hpp"
#include "notification.hpp"

static LRESULT CALLBACK WndProc(HWND Hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg)
	{
	case WM_CLOSE:
		DestroyWindow(Hwnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_DEVICECHANGE:
		{
			//bool Arrival=false;
			switch(wParam)
			{
			case DBT_DEVICEARRIVAL:
				//Arrival=true;
			case DBT_DEVICEREMOVECOMPLETE:
				{

					auto Pbh = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);
					if(Pbh->dbch_devicetype==DBT_DEVTYP_VOLUME)
					{
						// currently we don't care what actually happened, "just a notification" is OK

						//auto Pdv=reinterpret_cast<PDEV_BROADCAST_VOLUME>(Pbh);
						//bool Media = Pdv->dbcv_flags & DBTF_MEDIA != 0;
						MessageManager().notify(update_devices);
					}
				}
				break;

			}
		}
		break;

	case WM_SETTINGCHANGE:
		if(lParam)
		{
			if (!StrCmp(reinterpret_cast<LPCWSTR>(lParam),L"Environment"))
			{
				if (Global->Opt->UpdateEnvironment) 
				{
					MessageManager().notify(update_environment);
				}
			}
			else if (!StrCmp(reinterpret_cast<LPCWSTR>(lParam),L"intl"))
			{
				MessageManager().notify(update_intl);
			}
		}
		break;

	case WM_POWERBROADCAST:
		switch(wParam)
		{
		case PBT_APMPOWERSTATUSCHANGE: // change status

		case PBT_POWERSETTINGCHANGE:   // change percent
			MessageManager().notify(update_power);
			break;
		// TODO:
		// PBT_APMSUSPEND & PBT_APMRESUMEAUTOMATIC handlers

		}

		break;

	}
	return DefWindowProc(Hwnd, Msg, wParam, lParam);
}

wm_listener::wm_listener():
	m_Hwnd(nullptr),
	m_exitEvent(Event::automatic, Event::nonsignaled)
{
	Check();
}

wm_listener::~wm_listener()
{
	m_exitEvent.Set();
	if(m_Hwnd)
	{
		SendMessage(m_Hwnd,WM_CLOSE, 0, 0);
	}
}

void wm_listener::Check()
{
	if (!m_Thread.joinable() || m_Thread.Signaled())
	{
		Event ReadyEvent(Event::automatic, Event::nonsignaled);
		m_Thread = Thread(&Thread::join, &wm_listener::WindowThreadRoutine, this, &ReadyEvent);
		ReadyEvent.Wait();
	}
}

void wm_listener::WindowThreadRoutine(const Event* ReadyEvent)
{
	WNDCLASSEX wc={sizeof(wc)};
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = L"FarHiddenWindowClass";
	UnregisterClass(wc.lpszClassName, nullptr);
	if(RegisterClassEx(&wc))
	{
		m_Hwnd = CreateWindowEx(0, wc.lpszClassName, nullptr, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, nullptr, nullptr);
		ReadyEvent->Set();
		if(m_Hwnd)
		{
			// for PBT_POWERSETTINGCHANGE
			HPOWERNOTIFY hpn=Imports().RegisterPowerSettingNotification(m_Hwnd,&GUID_BATTERY_PERCENTAGE_REMAINING,DEVICE_NOTIFY_WINDOW_HANDLE);

			MSG Msg;
			while(!m_exitEvent.Signaled() && GetMessage(&Msg, nullptr, 0, 0) > 0)
			{
				TranslateMessage(&Msg);
				DispatchMessage(&Msg);
			}

			if (hpn) // for PBT_POWERSETTINGCHANGE
				Imports().UnregisterPowerSettingNotification(hpn);

		}
		UnregisterClass(wc.lpszClassName, nullptr);
	}
}
