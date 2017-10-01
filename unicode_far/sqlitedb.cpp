﻿/*
sqlitedb.cpp

обёртка sqlite api для c++.
*/
/*
Copyright © 2011 Far Group
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

#include "sqlitedb.hpp"
#include "sqlite.hpp"
#include "sqlite_unicode.hpp"
#include "pathmix.hpp"
#include "config.hpp"
#include "synchro.hpp"
#include "components.hpp"
#include "encoding.hpp"

namespace
{
	SCOPED_ACTION(components::component)([]
	{
		return components::component::info{ L"SQLite"s, WIDE(SQLITE_VERSION) };
	});

	SCOPED_ACTION(components::component)([]
	{
		return components::component::info{ L"SQLite Unicode extension"s, sqlite_unicode::SQLite_Unicode_Version };
	});

	static string GetDatabasePath(const string& FileName, bool Local)
	{
		if (FileName != L":memory:")
		{
			auto Result = Local? Global->Opt->LocalProfilePath : Global->Opt->ProfilePath;
			AddEndSlash(Result);
			return Result + FileName;
		}

		return FileName;
	}
}

int SQLiteDb::library_load()
{
	return sqlite_unicode::sqlite3_unicode_load();
}

void SQLiteDb::library_free()
{
	return sqlite_unicode::sqlite3_unicode_free();
}

void SQLiteDb::SQLiteStmt::stmt_deleter::operator()(sqlite::sqlite3_stmt* object) const
{
	sqlite::sqlite3_finalize(object);
}

SQLiteDb::SQLiteStmt& SQLiteDb::SQLiteStmt::Reset()
{
	m_Param = 1;
	sqlite::sqlite3_clear_bindings(m_Stmt.get());
	sqlite::sqlite3_reset(m_Stmt.get());
	return *this;
}

bool SQLiteDb::SQLiteStmt::Step() const
{
	return sqlite::sqlite3_step(m_Stmt.get()) == SQLITE_ROW;
}

bool SQLiteDb::SQLiteStmt::FinalStep() const
{
	return sqlite::sqlite3_step(m_Stmt.get()) == SQLITE_DONE;
}

SQLiteDb::SQLiteStmt& SQLiteDb::SQLiteStmt::BindImpl(std::nullptr_t)
{
	sqlite::sqlite3_bind_null(m_Stmt.get(), m_Param++);
	return *this;
}

SQLiteDb::SQLiteStmt& SQLiteDb::SQLiteStmt::BindImpl(int Value)
{
	sqlite::sqlite3_bind_int(m_Stmt.get(), m_Param++, Value);
	return *this;
}

SQLiteDb::SQLiteStmt& SQLiteDb::SQLiteStmt::BindImpl(long long Value)
{
	sqlite::sqlite3_bind_int64(m_Stmt.get(), m_Param++, Value);
	return *this;
}

SQLiteDb::SQLiteStmt& SQLiteDb::SQLiteStmt::BindImpl(const string_view& Value, bool bStatic)
{
	sqlite::sqlite3_bind_text16(m_Stmt.get(), m_Param++, Value.raw_data(), static_cast<int>(Value.size() * sizeof(wchar_t)), bStatic? sqlite::static_destructor : sqlite::transient_destructor);
	return *this;
}

SQLiteDb::SQLiteStmt& SQLiteDb::SQLiteStmt::BindImpl(const bytes_view& Value, bool bStatic)
{
	sqlite::sqlite3_bind_blob(m_Stmt.get(), m_Param++, Value.data(), static_cast<int>(Value.size()), bStatic? sqlite::static_destructor : sqlite::transient_destructor);
	return *this;
}

const wchar_t* SQLiteDb::SQLiteStmt::GetColText(int Col) const
{
	return static_cast<const wchar_t*>(sqlite::sqlite3_column_text16(m_Stmt.get(), Col));
}

const char* SQLiteDb::SQLiteStmt::GetColTextUTF8(int Col) const
{
	return reinterpret_cast<const char*>(sqlite::sqlite3_column_text(m_Stmt.get(), Col));
}

int SQLiteDb::SQLiteStmt::GetColInt(int Col) const
{
	return sqlite::sqlite3_column_int(m_Stmt.get(), Col);
}

unsigned long long SQLiteDb::SQLiteStmt::GetColInt64(int Col) const
{
	return sqlite::sqlite3_column_int64(m_Stmt.get(), Col);
}

bytes_view SQLiteDb::SQLiteStmt::GetColBlob(int Col) const
{
	return bytes_view(sqlite::sqlite3_column_blob(m_Stmt.get(), Col), sqlite::sqlite3_column_bytes(m_Stmt.get(), Col));
}

SQLiteDb::column_type SQLiteDb::SQLiteStmt::GetColType(int Col) const
{
	switch (sqlite::sqlite3_column_type(m_Stmt.get(), Col))
	{
	case SQLITE_INTEGER:
		return column_type::integer;
	case SQLITE_TEXT:
		return column_type::string;
	case SQLITE_BLOB:
		return column_type::blob;
	default:
		return column_type::unknown;
	}
}


SQLiteDb::SQLiteDb(initialiser Initialiser, const string& DbName, bool Local, bool WAL)
{
	Initialize(Initialiser, DbName, Local, WAL);
}

static bool can_create_file(const string& fname)
{
	return os::fs::file(fname, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE)? true : false;
}

void SQLiteDb::db_closer::operator()(sqlite::sqlite3* Object) const
{
	sqlite::sqlite3_close(Object);
}

bool SQLiteDb::Open(const string& DbFile, bool Local, bool WAL)
{
	const auto& v1_opener = [](const string& Name, database_ptr& Db)
	{
		return sqlite::sqlite3_open16(Name.data(), &ptr_setter(Db)) == SQLITE_OK;
	};

	const auto& v2_opener = [WAL](const string& Name, database_ptr& Db)
	{
		return sqlite::sqlite3_open_v2(encoding::utf8::get_bytes(Name).data(), &ptr_setter(Db), WAL? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK;
	};

	m_Path = GetDatabasePath(DbFile, Local);
	const auto mem_db = DbFile == L":memory:";

	if (!Global->Opt->ReadOnlyConfig || mem_db)
	{
		if (!mem_db && db_exists < 0)
		{
			db_exists = os::fs::is_file(m_Path)? +1 : 0;
		}
		if (!v1_opener(m_Path, m_Db))
			return false;

		sqlite::sqlite3_busy_timeout(m_Db.get(), 1000);
		return true;
	}

	// copy db to memory
	//
	if (!v1_opener(L":memory:", m_Db))
		return false;

	bool ok = true, copied = false;

	if (os::fs::is_file(m_Path))
	{
		database_ptr db_source;

		if (db_exists < 0)
			db_exists = +1;

		if (WAL && !can_create_file(concat(m_Path, L'.', GuidToStr(CreateUuid())))) // can't open db -- copy to %TEMP%
		{
			string strTmp;
			os::fs::GetTempPath(strTmp);
			append(strTmp, str(GetCurrentProcessId()), L'-', DbFile);
			ok = copied = os::fs::copy_file(m_Path, strTmp, nullptr, nullptr, nullptr, 0);
			if (ok)
			{
				os::fs::set_file_attributes(strTmp, FILE_ATTRIBUTE_NORMAL);
				m_Path = strTmp;
				ok = v1_opener(m_Path, db_source);
			}
		}
		else
		{
			ok = v2_opener(m_Path, db_source);
		}

		if (ok)
		{
			sqlite::sqlite3_busy_timeout(db_source.get(), 1000);
			const auto db_backup = sqlite::sqlite3_backup_init(m_Db.get(), "main", db_source.get(), "main");
			ok = (nullptr != db_backup);
			if (ok)
			{
				sqlite::sqlite3_backup_step(db_backup, -1);
				sqlite::sqlite3_backup_finish(db_backup);
				ok = sqlite::sqlite3_errcode(m_Db.get()) == SQLITE_OK;
			}
		}
	}

	if (copied)
		os::fs::delete_file(m_Path);

	m_Path = L":memory:";
	if (!ok)
		Close();
	return ok;
}

void SQLiteDb::Initialize(initialiser Initialiser, const string& DbName, bool Local, bool WAL)
{
	os::mutex m(os::make_name<os::mutex>(Local? Global->Opt->LocalProfilePath : Global->Opt->ProfilePath, DbName).data());
	SCOPED_ACTION(std::lock_guard<os::mutex>)(m);

	m_Name = DbName;

	const auto& OpenAndInitialise = [&](const string& Name)
	{
		return Open(Name, Local, WAL) && Initialiser(db_initialiser(this));
	};

	if (!OpenAndInitialise(DbName))
	{
		Close();
		++init_status;

		bool in_memory = Global->Opt->ReadOnlyConfig ||
			!(os::fs::move_file(m_Path, m_Path + L".bad", MOVEFILE_REPLACE_EXISTING) && OpenAndInitialise(DbName));

		if (in_memory)
		{
			Close();
			++init_status;
			OpenAndInitialise(L":memory:"s);
		}
	}
}

int SQLiteDb::GetInitStatus(string& name, bool full_name) const
{
	name = (full_name && !m_Path.empty() && m_Path != L":memory:") ? m_Path : m_Name;
	return init_status;
}

const wchar_t* SQLiteDb::GetErrorMessage(int InitStatus)
{
	switch (InitStatus)
	{
	case 0:  return L"no errors";
	case 1:  return L"database file is renamed to *.bad and new one is created";
	case 2:  return L"database is opened in memory";
	default: return L"unknown error";
	}
}

bool SQLiteDb::Exec(const char *Command) const
{
	return sqlite::sqlite3_exec(m_Db.get(), Command, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SQLiteDb::BeginTransaction()
{
	return Exec("BEGIN TRANSACTION;");
}

bool SQLiteDb::EndTransaction()
{
	return Exec("END TRANSACTION;");
}

bool SQLiteDb::RollbackTransaction()
{
	return Exec("ROLLBACK TRANSACTION;");
}

SQLiteDb::SQLiteStmt SQLiteDb::create_stmt(const wchar_t* Stmt) const
{
	sqlite::sqlite3_stmt* pStmt;
	if (sqlite::sqlite3_prepare16_v2(m_Db.get(), Stmt, -1, &pStmt, nullptr) != SQLITE_OK)
	{
		throw MAKE_FAR_EXCEPTION(L"SQLiteDb::create_stmt failed");
	}
	return SQLiteStmt(pStmt);
}

bool SQLiteDb::Changes() const
{
	return sqlite::sqlite3_changes(m_Db.get()) != 0;
}

unsigned long long SQLiteDb::LastInsertRowID() const
{
	return sqlite::sqlite3_last_insert_rowid(m_Db.get());
}

void SQLiteDb::Close()
{
	// sqlite3_close() returns SQLITE_BUSY and leaves the connection option
	// if there are unfinalized prepared statements or unfinished sqlite3_backups
	m_Statements.clear();
	m_Db.reset();
}

bool SQLiteDb::SetWALJournalingMode() const
{
	return Exec("PRAGMA journal_mode = WAL;");
}

bool SQLiteDb::EnableForeignKeysConstraints() const
{
	return Exec("PRAGMA foreign_keys = ON;");
}

int SQLiteDb::GetLastErrorCode() const
{
	return sqlite::sqlite3_errcode(m_Db.get());
}

string SQLiteDb::GetLastErrorString() const
{
	return static_cast<const wchar_t*>(sqlite::sqlite3_errmsg16(m_Db.get()));
}
