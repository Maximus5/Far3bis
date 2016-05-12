﻿#ifndef NOTIFICATION_HPP_B0BB0D31_61E8_49C3_AA4F_E8C1D7D71A25
#define NOTIFICATION_HPP_B0BB0D31_61E8_49C3_AA4F_E8C1D7D71A25
#pragma once

/*
notification.hpp

*/
/*
Copyright © 2013 Far Group
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

#include "synchro.hpp"

enum event_id
{
	update_intl,
	update_power,
	update_devices,
	update_environment,

	elevation_dialog,
	plugin_synchro,

	event_id_count
};

class wm_listener;

namespace detail
{
	class i_event_handler
	{
	public:
		virtual ~i_event_handler() = default;
		virtual void operator()(const any&) const = 0;
	};

	class event_handler: public i_event_handler
	{
	public:
		typedef std::function<void()> handler_type;

		event_handler(const handler_type& Handler):
			m_Handler(Handler)
		{
		}

		virtual void operator()(const any&) const override
		{
			m_Handler();
		}

	private:
		handler_type m_Handler;
	};

	class parametrized_event_handler: public i_event_handler
	{
	public:
		typedef std::function<void(const any&)> handler_type;

		parametrized_event_handler(const handler_type& Handler):
			m_Handler(Handler)
		{
		}

		virtual void operator()(const any& Payload) const override
		{
			m_Handler(Payload);
		}

	private:
		handler_type m_Handler;
	};
}

class message_manager: noncopyable
{
public:
	typedef std::multimap<string, const detail::i_event_handler*> handlers_map;

	handlers_map::iterator subscribe(event_id EventId, const detail::i_event_handler& EventHandler);
	handlers_map::iterator subscribe(const string& EventName, const detail::i_event_handler& EventHandler);
	void unsubscribe(handlers_map::iterator HandlerIterator);
	void notify(event_id EventId, any&& Payload = any());
	void notify(const string& EventName, any&& Payload = any());
	bool dispatch();

	class suppress: noncopyable
	{
	public:
		suppress();
		~suppress();

	private:
		message_manager& m_owner;
	};

private:
	friend message_manager& MessageManager();

	typedef SyncedQueue<std::pair<string, any>> message_queue;

	message_manager();

	message_queue m_Messages;
	handlers_map m_Handlers;
	std::unique_ptr<wm_listener> m_Window;
	std::atomic_ulong m_suppressions;
};

message_manager& MessageManager();

namespace detail
{
	template<class T>
	class listener_t: noncopyable
	{
	public:
		template<class id_type>
		listener_t(const id_type& EventId, const typename T::handler_type& EventHandler):
			m_Handler(EventHandler),
			m_Iterator(MessageManager().subscribe(EventId, m_Handler))
		{
		}

		~listener_t()
		{
			MessageManager().unsubscribe(m_Iterator);
		}

	private:
		T m_Handler;
		message_manager::handlers_map::iterator m_Iterator;
	};
}

typedef detail::listener_t<detail::event_handler> listener;
typedef detail::listener_t<detail::parametrized_event_handler> listener_ex;

#endif // NOTIFICATION_HPP_B0BB0D31_61E8_49C3_AA4F_E8C1D7D71A25
