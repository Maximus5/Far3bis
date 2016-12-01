﻿/*
configdb.cpp

хранение настроек в базе sqlite.
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

#include "configdb.hpp"
#include "sqlitedb.hpp"
#include "strmix.hpp"
#include "encoding.hpp"
#include "pathmix.hpp"
#include "config.hpp"
#include "datetime.hpp"
#include "tinyxml.hpp"
#include "farversion.hpp"
#include "console.hpp"
#include "language.hpp"
#include "message.hpp"
#include "synchro.hpp"
#include "regex_helpers.hpp"

class representation_source
{
public:
	representation_source(const string& File):
		m_File(File),
		m_Root(nullptr)
	{
		const auto RootName = "farconfig";

		const auto XmlFile = file_ptr(_wfopen(NTPath(File).data(), L"rb"));
		if (!XmlFile)
			return;

		if (m_Document.LoadFile(XmlFile.get()) != tinyxml::XML_SUCCESS)
			return;

		const auto root = m_Document.FirstChildElement(RootName);
		SetRoot(*root);
	}

	tinyxml::XMLHandle GetRoot() const { return m_Root; }

	void SetRoot(tinyxml::XMLHandle Root) { m_Root = Root; }

	void PrintError() const { m_Document.PrintError(); }

private:
	tinyxml::XMLDocument m_Document;
	string m_File;
	tinyxml::XMLHandle m_Root;
};

static auto& CreateChild(tinyxml::XMLElement& Parent, const char* Name)
{
	const auto e = Parent.GetDocument()->NewElement(Name);
	Parent.LinkEndChild(e);
	return *e;
}

class representation_destination
{
public:
	representation_destination():
		m_Root()
	{
		const auto RootName = "farconfig";

		m_Document.SetBOM(true);
		m_Document.LinkEndChild(m_Document.NewDeclaration());
		const auto root = m_Document.NewElement(RootName);
		m_Document.LinkEndChild(root);
		SetRoot(*root);
	}

	tinyxml::XMLElement& GetRoot() const { return *m_Root; }

	void SetRoot(tinyxml::XMLElement& Root) { m_Root = &Root; }

	bool Save(const string& File)
	{
		const file_ptr XmlFile(_wfopen(NTPath(File).data(), L"w"));
		return XmlFile && m_Document.SaveFile(XmlFile.get()) == tinyxml::XML_SUCCESS;
	}

private:
	tinyxml::XMLDocument m_Document;
	tinyxml::XMLElement* m_Root;
};

namespace {

class xml_enum: noncopyable, public enumerator<xml_enum, const tinyxml::XMLElement*>
{
	IMPLEMENTS_ENUMERATOR(xml_enum);

public:
	xml_enum(tinyxml::XMLHandle base, const char* name):
		m_name(name),
		m_base(base.ToNode())
	{}

	xml_enum(const tinyxml::XMLNode& base, const char* name):
		m_name(name),
		m_base(&base)
	{}

private:
	bool get(size_t index, value_type& value) const
	{
		value = index? value->NextSiblingElement(m_name) :
		        m_base? m_base->FirstChildElement(m_name) : nullptr;

		return value? true : false;
	}

	const char* m_name;
	const tinyxml::XMLNode* m_base;
};

class iGeneralConfigDb: public GeneralConfig, public SQLiteDb
{
protected:
	iGeneralConfigDb(const wchar_t* DbName, bool local)
	{
		Initialize(DbName, local);
	}

private:
	virtual bool InitializeImpl(const string& DbName, bool Local) override
	{
		static constexpr auto Schema =
			"CREATE TABLE IF NOT EXISTS general_config(key TEXT NOT NULL, name TEXT NOT NULL, value BLOB, PRIMARY KEY (key, name));"
		;

		static constexpr stmt_init_t Statements[] =
		{
			{ stmtUpdateValue, L"UPDATE general_config SET value=?3 WHERE key=?1 AND name=?2;" },
			{ stmtInsertValue, L"INSERT INTO general_config VALUES (?1,?2,?3);" },
			{ stmtGetValue, L"SELECT value FROM general_config WHERE key=?1 AND name=?2;" },
			{ stmtDelValue, L"DELETE FROM general_config WHERE key=?1 AND name=?2;" },
			{ stmtEnumValues, L"SELECT name, value FROM general_config WHERE key=?1;" },
		};
		CheckStmt<stmt_count>(Statements);

		return Open(DbName, Local)
		    && Exec(Schema)
		    && PrepareStatements(Statements)
		;
	}

	virtual bool SetValue(const string& Key, const string& Name, const string& Value) override
	{
		return SetValueT(Key, Name, Value);
	}

	virtual bool SetValue(const string& Key, const string& Name, const wchar_t* Value) override
	{
		return SetValueT(Key, Name, Value);
	}

	virtual bool SetValue(const string& Key, const string& Name, unsigned long long Value) override
	{
		return SetValueT(Key, Name, Value);
	}

	virtual bool SetValue(const string& Key, const string& Name, const blob_view& Value) override
	{
		return SetValueT(Key, Name, Value);
	}

	virtual bool GetValue(const string& Key, const string& Name, bool& Value, bool Default) override
	{
		long long Data = Default;
		const auto Result = GetValue(Key, Name, Data, Data);
		Value = Data != 0;
		return Result;
	}

	virtual bool GetValue(const string& Key, const string& Name, long long& Value, long long Default) override
	{
		return GetValueT<column_type::integer>(Key, Name, Value, Default, &SQLiteStmt::GetColInt64);
	}

	virtual bool GetValue(const string& Key, const string& Name, string& Value, const wchar_t* Default) override
	{
		return GetValueT<column_type::string>(Key, Name, Value, Default, &SQLiteStmt::GetColText);
	}

	virtual bool GetValue(const string& Key, const string& Name, string& Value, const string& Default) override
	{
		return GetValueT<column_type::string>(Key, Name, Value, Default, &SQLiteStmt::GetColText);
	}

	virtual bool DeleteValue(const string& Key, const string& Name) override
	{
		return ExecuteStatement(stmtDelValue, Key, Name);
	}

	virtual bool EnumValues(const string& Key, DWORD Index, string &Name, string &Value) override
	{
		return EnumValuesT(Key, Index, Name, Value, &SQLiteStmt::GetColText);
	}

	virtual bool EnumValues(const string& Key, DWORD Index, string &Name, long long& Value) override
	{
		return EnumValuesT(Key, Index, Name, Value, &SQLiteStmt::GetColInt64);
	}

	virtual void Export(representation_destination& Representation) override
	{
		auto& root = CreateChild(Representation.GetRoot(), GetKeyName());

		auto stmtEnumAllValues = create_stmt(L"SELECT key, name, value FROM general_config ORDER BY key, name;");

		while (stmtEnumAllValues.Step())
		{
			auto& e = CreateChild(root, "setting");

			e.SetAttribute("key", stmtEnumAllValues.GetColTextUTF8(0));
			e.SetAttribute("name", stmtEnumAllValues.GetColTextUTF8(1));

			switch (static_cast<column_type>(stmtEnumAllValues.GetColType(2)))
			{
			case column_type::integer:
				e.SetAttribute("type", "qword");
				e.SetAttribute("value", to_hex_string(stmtEnumAllValues.GetColInt64(2)).data());
				break;

			case column_type::string:
				e.SetAttribute("type", "text");
				e.SetAttribute("value", stmtEnumAllValues.GetColTextUTF8(2));
				break;

			case column_type::blob:
			case column_type::unknown:
				{
					e.SetAttribute("type", "hex");
					const auto Blob = stmtEnumAllValues.GetColBlob(2);
					e.SetAttribute("value", BlobToHexString(Blob.data(), Blob.size()).data());
				}
			}
		}
	}

	virtual void Import(const representation_source& Representation) override
	{
		SCOPED_ACTION(auto)(ScopedTransaction());
		for(const auto& e: xml_enum(Representation.GetRoot().FirstChildElement(GetKeyName()), "setting"))
		{
			const auto key = e->Attribute("key");
			const auto name = e->Attribute("name");
			const auto type = e->Attribute("type");
			const auto value = e->Attribute("value");

			if (!key || !name || !type || !value)
				continue;

			const auto Key = encoding::utf8::get_chars(key);
			const auto Name = encoding::utf8::get_chars(name);

			if (!strcmp(type,"qword"))
			{
				SetValue(Key, Name, strtoull(value, nullptr, 16));
				continue;
			}
			
			if (!strcmp(type,"text"))
			{
				SetValue(Key, Name, encoding::utf8::get_chars(value));
				continue;
			}
			
			if (!strcmp(type,"hex"))
			{
				const auto Blob = HexStringToBlob(value);
				SetValue(Key, Name, make_blob_view(Blob.data(), Blob.size()));
				continue;
			}
		}
	}

	virtual const char* GetKeyName() const = 0;

	template<column_type TypeId, class getter_t, class T, class DT>
	bool GetValueT(const string& Key, const string& Name, T& Value, const DT& Default, getter_t Getter)
	{
		const auto Stmt = AutoStatement(stmtGetValue);
		if (!Stmt->Bind(Key, Name).Step() || Stmt->GetColType(0) != TypeId)
		{
			Value = Default;
			return false;
		}

		Value = std::invoke(Getter, Stmt, 0);
		return true;
	}

	template<class T>
	bool SetValueT(const string& Key, const string& Name, const T Value)
	{
		bool b = ExecuteStatement(stmtUpdateValue, Key, Name, Value);
		if (!b || !Changes())
			b = ExecuteStatement(stmtInsertValue, Key, Name, Value);
		return b;
	}

	template<class T, class getter_t>
	bool EnumValuesT(const string& Key, DWORD Index, string& Name, T& Value, getter_t Getter)
	{
		auto Stmt = AutoStatement(stmtEnumValues);
		if (Index == 0)
			Stmt->Reset().Bind(transient(Key));

		if (!Stmt->Step())
			return false;

		Name = Stmt->GetColText(0);
		Value = std::invoke(Getter, Stmt, 1);
		Stmt.release();
		return true;
	}

	enum statement_id
	{
		stmtUpdateValue,
		stmtInsertValue,
		stmtGetValue,
		stmtDelValue,
		stmtEnumValues,

		stmt_count
	};
};

class GeneralConfigDb: public iGeneralConfigDb
{
public:
	GeneralConfigDb():iGeneralConfigDb(L"generalconfig.db", false) {}

private:
	virtual const char* GetKeyName() const override {return "generalconfig";}
};

class LocalGeneralConfigDb: public iGeneralConfigDb
{
public:
	LocalGeneralConfigDb():iGeneralConfigDb(L"localconfig.db", true) {}

private:
	virtual const char* GetKeyName() const override {return "localconfig";}
};

class HierarchicalConfigDb: public HierarchicalConfig, public SQLiteDb
{
public:
	explicit HierarchicalConfigDb(const string& DbName, bool Local = false):
		// If a thread with same event name is running, we will open that event here
		AsyncDone(Event::manual, Event::signaled, make_name<Event>(GetPath(), GetName()).data())
	{
		// and wait for the signal
		AsyncDone.Wait();
		Initialize(DbName, Local);
	}

	virtual ~HierarchicalConfigDb() { HierarchicalConfigDb::EndTransaction(); AsyncDone.Set(); }

protected:
	virtual void AsyncFinish() override
	{
		AsyncDone.Reset();
		ConfigProvider().AddThread(Thread(&Thread::detach, &HierarchicalConfigDb::AsyncDelete, this));
	}

	virtual bool InitializeImpl(const string& DbName, bool Local) override
	{
		static constexpr auto Schema =
			"CREATE TABLE IF NOT EXISTS table_keys(id INTEGER PRIMARY KEY, parent_id INTEGER NOT NULL, name TEXT NOT NULL, description TEXT, FOREIGN KEY(parent_id) REFERENCES table_keys(id) ON UPDATE CASCADE ON DELETE CASCADE, UNIQUE (parent_id,name));"
			"CREATE TABLE IF NOT EXISTS table_values(key_id INTEGER NOT NULL, name TEXT NOT NULL, value BLOB, FOREIGN KEY(key_id) REFERENCES table_keys(id) ON UPDATE CASCADE ON DELETE CASCADE, PRIMARY KEY (key_id, name), CHECK (key_id <> 0));"
			//root key (needs to be before the transaction start)
			"INSERT OR IGNORE INTO table_keys VALUES (0,0,\"\",\"Root - do not edit\");"
		;

		static constexpr stmt_init_t Statements[] =
		{
			{ stmtCreateKey, L"INSERT INTO table_keys VALUES (NULL,?1,?2,?3);" },
			{ stmtFindKey, L"SELECT id FROM table_keys WHERE parent_id=?1 AND name=?2 AND id<>0;" },
			{ stmtSetKeyDescription, L"UPDATE table_keys SET description=?1 WHERE id=?2 AND id<>0 AND description<>?1;" },
			{ stmtSetValue, L"INSERT OR REPLACE INTO table_values VALUES (?1,?2,?3);" },
			{ stmtGetValue, L"SELECT value FROM table_values WHERE key_id=?1 AND name=?2;" },
			{ stmtEnumKeys, L"SELECT name FROM table_keys WHERE parent_id=?1 AND id<>0;" },
			{ stmtEnumValues, L"SELECT name, value FROM table_values WHERE key_id=?1;" },
			{ stmtDelValue, L"DELETE FROM table_values WHERE key_id=?1 AND name=?2;" },
			{ stmtDeleteTree, L"DELETE FROM table_keys WHERE id=?1 AND id<>0;" },
		};
		CheckStmt<stmt_count>(Statements);

		return Open(DbName, Local)
		    && EnableForeignKeysConstraints()
		    && Exec(Schema)
		    && PrepareStatements(Statements)
		    && BeginTransaction()
		;
	}

	virtual bool Flush() override
	{
		const bool b = EndTransaction();
		BeginTransaction();
		return b;
	}

	virtual key CreateKey(const key& Root, const string& Name, const string* Description) override
	{
		if (ExecuteStatement(stmtCreateKey, Root.get(), Name, Description? Description->data() : nullptr))
			return make_key(LastInsertRowID());

		const auto Key = FindByName(Root, Name);
		if (Key.get() && Description)
			SetKeyDescription(Key, *Description);
		return Key;
	}

	virtual key FindByName(const key& Root, const string& Name) override
	{
		const auto Stmt = AutoStatement(stmtFindKey);
		if (!Stmt->Bind(Root.get(), Name).Step())
			return make_key(0);

		return make_key(Stmt->GetColInt64(0));
	}

	virtual bool SetKeyDescription(const key& Root, const string& Description) override
	{
		return ExecuteStatement(stmtSetKeyDescription, Description, Root.get());
	}

	virtual bool SetValue(const key& Root, const string& Name, const string& Value) override
	{
		return SetValueT(Root, Name, Value);
	}

	virtual bool SetValue(const key& Root, const string& Name, const wchar_t* Value) override
	{
		return SetValueT(Root, Name, Value);
	}

	virtual bool SetValue(const key& Root, const string& Name, unsigned long long Value) override
	{
		return SetValueT(Root, Name, Value);
	}

	virtual bool SetValue(const key& Root, const string& Name, const blob_view& Value) override
	{
		return SetValueT(Root, Name, Value);
	}

	virtual bool GetValue(const key& Root, const string& Name, unsigned long long& Value) override
	{
		return GetValueT(Root, Name, Value, &SQLiteStmt::GetColInt64);
	}

	virtual bool GetValue(const key& Root, const string& Name, string &Value) override
	{
		return GetValueT(Root, Name, Value, &SQLiteStmt::GetColText);
	}

	virtual bool GetValue(const key& Root, const string& Name, writable_blob_view& Value) override
	{
		return GetValueT(Root, Name, Value, &SQLiteStmt::GetColBlob);
	}

	virtual bool DeleteKeyTree(const key& Key) override
	{
		//All subtree is automatically deleted because of foreign key constraints
		return ExecuteStatement(stmtDeleteTree, Key.get());
	}

	virtual bool DeleteValue(const key& Root, const string& Name) override
	{
		return ExecuteStatement(stmtDelValue, Root.get(), Name);
	}

	virtual bool EnumKeys(const key& Root, DWORD Index, string& Name) override
	{
		auto Stmt = AutoStatement(stmtEnumKeys);
		if (Index == 0)
			Stmt->Reset().Bind(Root.get());

		if (!Stmt->Step())
			return false;

		Name = Stmt->GetColText(0);
		Stmt.release();
		return true;
	}

	virtual bool EnumValues(const key& Root, DWORD Index, string& Name, int& Type) override
	{
		auto Stmt = AutoStatement(stmtEnumValues);
		if (Index == 0)
			Stmt->Reset().Bind(Root.get());

		if (!Stmt->Step())
			return false;

		Name = Stmt->GetColText(0);
		Type = static_cast<int>(Stmt->GetColType(1));
		Stmt.release();
		return true;
	}

	virtual void SerializeBlob(const char* Name, const void* Blob, size_t Size, tinyxml::XMLElement& e)
	{
		e.SetAttribute("type", "hex");
		e.SetAttribute("value", BlobToHexString(Blob, Size).data());
	}

	virtual void Export(representation_destination& Representation) override
	{
		Export(Representation, root_key(), CreateChild(Representation.GetRoot(), "hierarchicalconfig"));
	}

	virtual std::vector<char> DeserializeBlob(const char* Name, const char* Type, const char* Value, const tinyxml::XMLElement& e)
	{
		return HexStringToBlob(Value);
	}

	virtual void Import(const representation_source& Representation) override
	{
		SCOPED_ACTION(auto)(ScopedTransaction());
		for (const auto& e: xml_enum(Representation.GetRoot().FirstChildElement("hierarchicalconfig"), "key"))
		{
			Import(root_key(), *e);
		}
	}

	void Export(representation_destination& Representation, const key& Key, tinyxml::XMLElement& XmlKey)
	{
		{
			const auto Stmt = AutoStatement(stmtEnumValues);
			Stmt->Bind(Key.get());
			while (Stmt->Step())
			{
				auto& e = CreateChild(XmlKey, "value");

				const auto name = Stmt->GetColTextUTF8(0);
				e.SetAttribute("name", name);

				switch (static_cast<column_type>(Stmt->GetColType(1)))
				{
				case column_type::integer:
					e.SetAttribute("type", "qword");
					e.SetAttribute("value", to_hex_string(Stmt->GetColInt64(1)).data());
					break;

				case column_type::string:
					e.SetAttribute("type", "text");
					e.SetAttribute("value", Stmt->GetColTextUTF8(1));
					break;

				case column_type::blob:
				case column_type::unknown:
					{
						const auto Blob = Stmt->GetColBlob(1);
						SerializeBlob(name, Blob.data(), Blob.size(), e);
					}
					break;
				}
			}
		}

		auto stmtEnumSubKeys = create_stmt(L"SELECT id, name, description FROM table_keys WHERE parent_id=?1 AND id<>0;");
		stmtEnumSubKeys.Bind(Key.get());
		while (stmtEnumSubKeys.Step())
		{
			auto& e = CreateChild(XmlKey, "key");

			e.SetAttribute("name", stmtEnumSubKeys.GetColTextUTF8(1));
			if (const auto description = stmtEnumSubKeys.GetColTextUTF8(2))
				e.SetAttribute("description", description);

			Export(Representation, make_key(stmtEnumSubKeys.GetColInt64(0)), e);
		}
	}

	void Import(const key& root, const tinyxml::XMLElement& key)
	{
		const auto key_name = key.Attribute("name");
		if (!key_name)
			return;
		const auto KeyName = encoding::utf8::get_chars(key_name);
		const auto key_description = key.Attribute("description");
		string KeyDescription;
		if (key_description)
		{
			KeyDescription = encoding::utf8::get_chars(key_description);
		}
		const auto Key = CreateKey(root, KeyName, key_description? &KeyDescription : nullptr);
		if (!Key.get())
			return;

		for (const auto& e: xml_enum(key, "value"))
		{
			const auto name = e->Attribute("name");
			const auto type = e->Attribute("type");
			const auto value = e->Attribute("value");

			if (!name || !type)
				continue;

			string Name = encoding::utf8::get_chars(name);

			if (value && !strcmp(type, "qword"))
			{
				SetValue(Key, Name, strtoull(value, nullptr, 16));
			}
			else if (value && !strcmp(type, "text"))
			{
				string Value = encoding::utf8::get_chars(value);
				SetValue(Key, Name, Value);
			}
			else if (value && !strcmp(type, "hex"))
			{
				const auto Blob = HexStringToBlob(value);
				SetValue(Key, Name, make_blob_view(Blob.data(), Blob.size()));
			}
			else
			{
				// custom types, value is optional
				const auto Blob = DeserializeBlob(name, type, value, *e);
				SetValue(Key, Name, make_blob_view(Blob.data(), Blob.size()));
			}
		}

		for (const auto& e: xml_enum(key, "key"))
		{
			Import(Key, *e);
		}
	}

	void AsyncDelete() const
	{
		delete this;
	}

	template<class T, class getter_t>
	bool GetValueT(const key& Root, const string& Name, T& Value, getter_t Getter)
	{
		const auto Stmt = AutoStatement(stmtGetValue);
		if (!Stmt->Bind(Root.get(), Name).Step())
			return false;

		Value = std::invoke(Getter, Stmt, 0);
		return true;
	}

	template<class T>
	bool SetValueT(const key& Root, const string& Name, const T& Value)
	{
		return ExecuteStatement(stmtSetValue, Root.get(), Name, Value);
	}

	enum statement_id
	{
		stmtCreateKey,
		stmtFindKey,
		stmtSetKeyDescription,
		stmtSetValue,
		stmtGetValue,
		stmtEnumKeys,
		stmtEnumValues,
		stmtDelValue,
		stmtDeleteTree,

		stmt_count
	};

	Event AsyncDone;
};

static constexpr std::pair<FARCOLORFLAGS, const wchar_t*> ColorFlagNames[] =
{
	{FCF_FG_4BIT,      L"fg4bit"   },
	{FCF_BG_4BIT,      L"bg4bit"   },
	{FCF_FG_BOLD,      L"bold"     },
	{FCF_FG_ITALIC,    L"italic"   },
	{FCF_FG_UNDERLINE, L"underline"},
};

class HighlightHierarchicalConfigDb: public HierarchicalConfigDb
{
public:
	using HierarchicalConfigDb::HierarchicalConfigDb;

private:
	virtual void SerializeBlob(const char* Name, const void* Blob, size_t Size, tinyxml::XMLElement& e) override
	{
		static constexpr const char* ColorKeys[] =
		{
			"NormalColor", "SelectedColor",
			"CursorColor", "SelectedCursorColor",
			"MarkCharNormalColor", "MarkCharSelectedColor",
			"MarkCharCursorColor", "MarkCharSelectedCursorColor",
		};

		if (std::any_of(CONST_RANGE(ColorKeys, i) { return !strcmp(Name, i); }))
		{
			auto& Color = *static_cast<const FarColor*>(Blob);
			e.SetAttribute("type", "color");
			e.SetAttribute("background", to_hex_string(Color.BackgroundColor).data());
			e.SetAttribute("foreground", to_hex_string(Color.ForegroundColor).data());
			e.SetAttribute("flags", encoding::utf8::get_bytes(FlagsToString(Color.Flags, ColorFlagNames)).data());
		}
		else
		{
			return HierarchicalConfigDb::SerializeBlob(Name, Blob, Size, e);
		}
	}

	virtual std::vector<char> DeserializeBlob(const char* Name, const char* Type, const char* Value, const tinyxml::XMLElement& e) override
	{
		if(!strcmp(Type, "color"))
		{
			FarColor Color{};

			if (const auto background = e.Attribute("background"))
				Color.BackgroundColor = std::strtoul(background, nullptr, 16);
			if (const auto foreground = e.Attribute("foreground"))
				Color.ForegroundColor = std::strtoul(foreground, nullptr, 16);
			if (const auto flags = e.Attribute("flags"))
				Color.Flags = StringToFlags(encoding::utf8::get_chars(flags), ColorFlagNames);

			std::vector<char> Blob(sizeof(FarColor));
			std::memcpy(Blob.data(), &Color, sizeof(Color));
			return Blob;
		}
		else
		{
			return HierarchicalConfigDb::DeserializeBlob(Name, Type, Value, e);
		}
	}
};

class ColorsConfigDb: public ColorsConfig, public SQLiteDb
{
public:
	ColorsConfigDb()
	{
		Initialize(L"colors.db");
	}

private:
	virtual bool InitializeImpl(const string& DbName, bool Local) override
	{
		static constexpr auto Schema =
			"CREATE TABLE IF NOT EXISTS colors(name TEXT NOT NULL PRIMARY KEY, value BLOB);"
		;

		static constexpr stmt_init_t Statements[] =
		{
			{ stmtUpdateValue, L"UPDATE colors SET value=?2 WHERE name=?1;" },
			{ stmtInsertValue, L"INSERT INTO colors VALUES (?1,?2);" },
			{ stmtGetValue, L"SELECT value FROM colors WHERE name=?1;" },
			{ stmtDelValue, L"DELETE FROM colors WHERE name=?1;" },
		};
		CheckStmt<stmt_count>(Statements);

		return Open(DbName, Local)
		    && Exec(Schema)
		    && PrepareStatements(Statements)
		;
	}

	virtual bool SetValue(const string& Name, const FarColor& Value) override
	{
		const auto Blob = make_blob_view(Value);
		bool b = ExecuteStatement(stmtUpdateValue, Name, Blob);
		if (!b || !Changes())
			b = ExecuteStatement(stmtInsertValue, Name, Blob);
		return b;
	}

	virtual bool GetValue(const string& Name, FarColor& Value) override
	{
		const auto Stmt = AutoStatement(stmtGetValue);
		if (!Stmt->Bind(Name).Step())
			return false;

		const auto Blob = Stmt->GetColBlob(0);
		if (Blob.size() != sizeof(Value))
			throw MAKE_FAR_EXCEPTION("incorrect blob size");
		Value = *reinterpret_cast<const FarColor*>(Blob.data());
		return true;
	}

	virtual void Export(representation_destination& Representation) override
	{
		auto& root = CreateChild(Representation.GetRoot(), "colors");

		auto stmtEnumAllValues = create_stmt(L"SELECT name, value FROM colors ORDER BY name;");

		while (stmtEnumAllValues.Step())
		{
			auto& e = CreateChild(root, "object");

			e.SetAttribute("name", stmtEnumAllValues.GetColTextUTF8(0));
			const auto Blob = stmtEnumAllValues.GetColBlob(1);
			if (Blob.size() != sizeof(FarColor))
				throw MAKE_FAR_EXCEPTION("incorrect blob size");
			auto& Color = *reinterpret_cast<const FarColor*>(Blob.data());
			e.SetAttribute("background", to_hex_string(Color.BackgroundColor).data());
			e.SetAttribute("foreground", to_hex_string(Color.ForegroundColor).data());
			e.SetAttribute("flags", encoding::utf8::get_bytes(FlagsToString(Color.Flags, ColorFlagNames)).data());
		}
	}

	virtual void Import(const representation_source& Representation) override
	{
		SCOPED_ACTION(auto)(ScopedTransaction());
		for (const auto& e: xml_enum(Representation.GetRoot().FirstChildElement("colors"), "object"))
		{
			const auto name = e->Attribute("name");
			const auto background = e->Attribute("background");
			const auto foreground = e->Attribute("foreground");
			const auto flags = e->Attribute("flags");

			if (!name)
				continue;

			const auto Name = encoding::utf8::get_chars(name);

			if(background && foreground && flags)
			{
				FarColor Color = {};
				Color.BackgroundColor = std::strtoul(background, nullptr, 16);
				Color.ForegroundColor = std::strtoul(foreground, nullptr, 16);
				Color.Flags = StringToFlags(encoding::utf8::get_chars(flags), ColorFlagNames);
				SetValue(Name, Color);
			}
			else
			{
				ExecuteStatement(stmtDelValue, Name);
			}
		}
	}

	enum statement_id
	{
		stmtUpdateValue,
		stmtInsertValue,
		stmtGetValue,
		stmtDelValue,

		stmt_count
	};
};

class AssociationsConfigDb: public AssociationsConfig, public SQLiteDb
{
public:
	AssociationsConfigDb()
	{
		Initialize(L"associations.db");
	}

private:
	virtual bool InitializeImpl(const string& DbName, bool Local) override
	{
		static constexpr auto Schema =
			"CREATE TABLE IF NOT EXISTS filetypes(id INTEGER PRIMARY KEY, weight INTEGER NOT NULL, mask TEXT, description TEXT);"
			"CREATE TABLE IF NOT EXISTS commands(ft_id INTEGER NOT NULL, type INTEGER NOT NULL, enabled INTEGER NOT NULL, command TEXT, FOREIGN KEY(ft_id) REFERENCES filetypes(id) ON UPDATE CASCADE ON DELETE CASCADE, PRIMARY KEY (ft_id, type));"
		;

		static constexpr stmt_init_t Statements[] =
		{
			{ stmtReorder, L"UPDATE filetypes SET weight=weight+1 WHERE weight>(CASE ?1 WHEN 0 THEN 0 ELSE (SELECT weight FROM filetypes WHERE id=?1) END);" },
			{ stmtAddType, L"INSERT INTO filetypes VALUES (NULL,(CASE ?1 WHEN 0 THEN 1 ELSE (SELECT weight FROM filetypes WHERE id=?1)+1 END),?2,?3);" },
			{ stmtGetMask, L"SELECT mask FROM filetypes WHERE id=?1;" },
			{ stmtGetDescription, L"SELECT description FROM filetypes WHERE id=?1;" },
			{ stmtUpdateType, L"UPDATE filetypes SET mask=?1, description=?2 WHERE id=?3;" },
			{ stmtSetCommand, L"INSERT OR REPLACE INTO commands VALUES (?1,?2,?3,?4);" },
			{ stmtGetCommand, L"SELECT command, enabled FROM commands WHERE ft_id=?1 AND type=?2;" },
			{ stmtEnumTypes, L"SELECT id, description FROM filetypes ORDER BY weight;" },
			{ stmtEnumMasks, L"SELECT id, mask FROM filetypes ORDER BY weight;" },
			{ stmtEnumMasksForType, L"SELECT id, mask FROM filetypes, commands WHERE id=ft_id AND type=?1 AND enabled<>0 ORDER BY weight;" },
			{ stmtDelType, L"DELETE FROM filetypes WHERE id=?1;" },
			{ stmtGetWeight, L"SELECT weight FROM filetypes WHERE id=?1;" },
			{ stmtSetWeight, L"UPDATE filetypes SET weight=?1 WHERE id=?2;" },
		};
		CheckStmt<stmt_count>(Statements);

		return Open(DbName, Local)
		    && EnableForeignKeysConstraints()
		    && Exec(Schema)
		    && PrepareStatements(Statements)
		;
	}

	virtual bool EnumMasks(DWORD Index, unsigned long long *id, string &strMask) override
	{
		auto Stmt = AutoStatement(stmtEnumMasks);
		if (Index == 0)
			Stmt->Reset();

		if (!Stmt->Step())
			return false;

		*id = Stmt->GetColInt64(0);
		strMask = Stmt->GetColText(1);
		Stmt.release();
		return true;
	}

	virtual bool EnumMasksForType(int Type, DWORD Index, unsigned long long *id, string &strMask) override
	{
		auto Stmt = AutoStatement(stmtEnumMasksForType);
		if (Index == 0)
			Stmt->Reset().Bind(Type);

		if (!Stmt->Step())
			return false;

		*id = Stmt->GetColInt64(0);
		strMask = Stmt->GetColText(1);
		Stmt.release();
		return true;
	}

	virtual bool GetMask(unsigned long long id, string &strMask) override
	{
		const auto Stmt = AutoStatement(stmtGetMask);
		if (!Stmt->Bind(id).Step())
			return false;

		strMask = Stmt->GetColText(0);
		return true;
	}

	virtual bool GetDescription(unsigned long long id, string &strDescription) override
	{
		const auto Stmt = AutoStatement(stmtGetDescription);
		if (!Stmt->Bind(id).Step())
			return false;

		strDescription = Stmt->GetColText(0);
		return true;
	}

	virtual bool GetCommand(unsigned long long id, int Type, string &strCommand, bool *Enabled = nullptr) override
	{
		const auto Stmt = AutoStatement(stmtGetCommand);
		if (!Stmt->Bind(id, Type).Step())
			return false;

		strCommand = Stmt->GetColText(0);
		if (Enabled)
			*Enabled = Stmt->GetColInt(1) != 0;
		return true;
	}

	virtual bool SetCommand(unsigned long long id, int Type, const string& Command, bool Enabled) override
	{
		return ExecuteStatement(stmtSetCommand, id, Type, Enabled, Command);
	}

	virtual bool SwapPositions(unsigned long long id1, unsigned long long id2) override
	{
		const auto Stmt = AutoStatement(stmtGetWeight);
		if (!Stmt->Bind(id1).Step())
			return false;

		const auto weight1 = Stmt->GetColInt64(0);
		Stmt->Reset();
		if (!Stmt->Bind(id2).Step())
			return false;

		const auto weight2 = Stmt->GetColInt64(0);
		Stmt->Reset();
		return ExecuteStatement(stmtSetWeight, weight1, id2) && ExecuteStatement(stmtSetWeight, weight2, id1);
	}

	virtual unsigned long long AddType(unsigned long long after_id, const string& Mask, const string& Description) override
	{
		return ExecuteStatement(stmtReorder, after_id) && ExecuteStatement(stmtAddType, after_id, Mask, Description)? LastInsertRowID() : 0;
	}

	virtual bool UpdateType(unsigned long long id, const string& Mask, const string& Description) override
	{
		return ExecuteStatement(stmtUpdateType, Mask, Description, id);
	}

	virtual bool DelType(unsigned long long id) override
	{
		return ExecuteStatement(stmtDelType, id);
	}

	virtual void Export(representation_destination& Representation) override
	{
		auto& root = CreateChild(Representation.GetRoot(), "associations");

		auto stmtEnumAllTypes = create_stmt(L"SELECT id, mask, description FROM filetypes ORDER BY weight;");
		auto stmtEnumCommandsPerFiletype = create_stmt(L"SELECT type, enabled, command FROM commands WHERE ft_id=?1 ORDER BY type;");

		while (stmtEnumAllTypes.Step())
		{
			auto& e = CreateChild(root, "filetype");

			e.SetAttribute("mask", stmtEnumAllTypes.GetColTextUTF8(1));
			e.SetAttribute("description", stmtEnumAllTypes.GetColTextUTF8(2));

			stmtEnumCommandsPerFiletype.Bind(stmtEnumAllTypes.GetColInt64(0));
			while (stmtEnumCommandsPerFiletype.Step())
			{
				auto& se = CreateChild(e, "command");

				se.SetAttribute("type", stmtEnumCommandsPerFiletype.GetColInt(0));
				se.SetAttribute("enabled", stmtEnumCommandsPerFiletype.GetColInt(1));
				se.SetAttribute("command", stmtEnumCommandsPerFiletype.GetColTextUTF8(2));
			}
			stmtEnumCommandsPerFiletype.Reset();
		}
	}

	virtual void Import(const representation_source& Representation) override
	{
		auto base = Representation.GetRoot().FirstChildElement("associations");
		if (!base.ToElement())
			return;

		SCOPED_ACTION(auto)(ScopedTransaction());
		Exec("DELETE FROM filetypes;"); //delete all before importing
		unsigned long long id = 0;
		for (const auto& e: xml_enum(base, "filetype"))
		{
			const auto mask = e->Attribute("mask");
			const auto description = e->Attribute("description");

			if (!mask)
				continue;

			const auto Mask = encoding::utf8::get_chars(mask);
			const auto Description = encoding::utf8::get_chars(NullToEmpty(description));

			id = AddType(id, Mask, Description);
			if (!id)
				continue;

			for (const auto& se: xml_enum(*e, "command"))
			{
				const auto command = se->Attribute("command");
				if (!command)
					continue;

				int type=0;
				if (se->QueryIntAttribute("type", &type) != tinyxml::XML_SUCCESS)
					continue;

				int enabled=0;
				if (se->QueryIntAttribute("enabled", &enabled) != tinyxml::XML_SUCCESS)
					continue;

				SetCommand(id, type, encoding::utf8::get_chars(command), enabled != 0);
			}

		}
	}

	enum statement_id
	{
		stmtReorder,
		stmtAddType,
		stmtGetMask,
		stmtGetDescription,
		stmtUpdateType,
		stmtSetCommand,
		stmtGetCommand,
		stmtEnumTypes,
		stmtEnumMasks,
		stmtEnumMasksForType,
		stmtDelType,
		stmtGetWeight,
		stmtSetWeight,

		stmt_count
	};
};

#if 1
#if   defined(_M_IA64) || defined(__ia64)|| defined(__ia64__)
#define PLATFORM_SUFFIX L"IA64"
#elif defined(_M_AMD64)|| defined(_M_X64)|| defined(__amd64)|| defined(__amd64__)|| defined(__x86_64)|| defined(__x86_64__)
#define PLATFORM_SUFFIX L"64"
#elif defined(_M_ARM)  || defined(__arm) || defined(__arm__)|| defined(_ARM_)
#define PLATFORM_SUFFIX L"ARM"
#elif defined(_M_IX86) || defined(__i386)|| defined(__i386__)
#define PLATFORM_SUFFIX L"32"
#endif
#else
#define PLATFORM_SUFFIX L""
#endif

class PluginsCacheConfigDb: public PluginsCacheConfig, public SQLiteDb
{
public:
	PluginsCacheConfigDb()
	{
		Initialize(L"plugincache" PLATFORM_SUFFIX L".db", true);
	}

	virtual bool DiscardCache() override
	{
		SCOPED_ACTION(auto)(ScopedTransaction());
		return Exec("DELETE FROM cachename;");
	}

private:
	virtual bool InitializeImpl(const string& DbName, bool Local) override
	{
		static constexpr auto Schema =
			"CREATE TABLE IF NOT EXISTS cachename(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);"
			"CREATE TABLE IF NOT EXISTS preload(cid INTEGER NOT NULL PRIMARY KEY, enabled INTEGER NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS signatures(cid INTEGER NOT NULL PRIMARY KEY, signature TEXT NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS guids(cid INTEGER NOT NULL PRIMARY KEY, guid TEXT NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS titles(cid INTEGER NOT NULL PRIMARY KEY, title TEXT NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS authors(cid INTEGER NOT NULL PRIMARY KEY, author TEXT NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS descriptions(cid INTEGER NOT NULL PRIMARY KEY, description TEXT NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS minfarversions(cid INTEGER NOT NULL PRIMARY KEY, version BLOB NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS pluginversions(cid INTEGER NOT NULL PRIMARY KEY, version BLOB NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS flags(cid INTEGER NOT NULL PRIMARY KEY, bitmask INTEGER NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS prefixes(cid INTEGER NOT NULL PRIMARY KEY, prefix TEXT NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE);"
			"CREATE TABLE IF NOT EXISTS exports(cid INTEGER NOT NULL, export TEXT NOT NULL, enabled INTEGER NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE, PRIMARY KEY (cid, export));"
			"CREATE TABLE IF NOT EXISTS menuitems(cid INTEGER NOT NULL, type INTEGER NOT NULL, number INTEGER NOT NULL, guid TEXT NOT NULL, name TEXT NOT NULL, FOREIGN KEY(cid) REFERENCES cachename(id) ON UPDATE CASCADE ON DELETE CASCADE, PRIMARY KEY (cid, type, number));"
		;

		static constexpr stmt_init_t Statements[] =
		{
			{ stmtCreateCache, L"INSERT INTO cachename VALUES (NULL,?1);," },
			{ stmtFindCacheName, L"SELECT id FROM cachename WHERE name=?1;" },
			{ stmtDelCache, L"DELETE FROM cachename WHERE name=?1;" },
			{ stmtCountCacheNames, L"SELECT count(name) FROM cachename;" },
			{ stmtGetPreloadState, L"SELECT enabled FROM preload WHERE cid=?1;" },
			{ stmtGetSignature, L"SELECT signature FROM signatures WHERE cid=?1;" },
			{ stmtGetExportState, L"SELECT enabled FROM exports WHERE cid=?1 and export=?2;" },
			{ stmtGetGuid, L"SELECT guid FROM guids WHERE cid=?1;" },
			{ stmtGetTitle, L"SELECT title FROM titles WHERE cid=?1;" },
			{ stmtGetAuthor, L"SELECT author FROM authors WHERE cid=?1;" },
			{ stmtGetPrefix, L"SELECT prefix FROM prefixes WHERE cid=?1;" },
			{ stmtGetDescription, L"SELECT description FROM descriptions WHERE cid=?1;" },
			{ stmtGetFlags, L"SELECT bitmask FROM flags WHERE cid=?1;" },
			{ stmtGetMinFarVersion, L"SELECT version FROM minfarversions WHERE cid=?1;" },
			{ stmtGetVersion, L"SELECT version FROM pluginversions WHERE cid=?1;" },
			{ stmtSetPreloadState, L"INSERT OR REPLACE INTO preload VALUES (?1,?2);" },
			{ stmtSetSignature, L"INSERT OR REPLACE INTO signatures VALUES (?1,?2);" },
			{ stmtSetExportState, L"INSERT OR REPLACE INTO exports VALUES (?1,?2,?3);" },
			{ stmtSetGuid, L"INSERT OR REPLACE INTO guids VALUES (?1,?2);" },
			{ stmtSetTitle, L"INSERT OR REPLACE INTO titles VALUES (?1,?2);" },
			{ stmtSetAuthor, L"INSERT OR REPLACE INTO authors VALUES (?1,?2);" },
			{ stmtSetPrefix, L"INSERT OR REPLACE INTO prefixes VALUES (?1,?2);" },
			{ stmtSetDescription, L"INSERT OR REPLACE INTO descriptions VALUES (?1,?2);" },
			{ stmtSetFlags, L"INSERT OR REPLACE INTO flags VALUES (?1,?2);," },
			{ stmtSetMinFarVersion, L"INSERT OR REPLACE INTO minfarversions VALUES (?1,?2);" },
			{ stmtSetVersion, L"INSERT OR REPLACE INTO pluginversions VALUES (?1,?2);" },
			{ stmtEnumCache, L"SELECT name FROM cachename ORDER BY name;" },
			{ stmtGetMenuItem, L"SELECT name, guid FROM menuitems WHERE cid=?1 AND type=?2 AND number=?3;" },
			{ stmtSetMenuItem, L"INSERT OR REPLACE INTO menuitems VALUES (?1,?2,?3,?4,?5);" },
		};
		CheckStmt<stmt_count>(Statements);

		return Open(DbName, Local, true)
		    && SetWALJournalingMode()
		    && EnableForeignKeysConstraints()
		    && Exec(Schema)
		    && PrepareStatements(Statements)
		;
	}

	virtual void Import(const representation_source&) override {}
	virtual void Export(representation_destination&) override {}

	virtual unsigned long long CreateCache(const string& CacheName) override
	{
		return ExecuteStatement(stmtCreateCache, CacheName)? LastInsertRowID() : 0;
	}

	virtual unsigned long long GetCacheID(const string& CacheName) const override
	{
		const auto Stmt = AutoStatement(stmtFindCacheName);
		return Stmt->Bind(CacheName).Step()?
		       Stmt->GetColInt64(0) :
		       0;
	}

	virtual bool DeleteCache(const string& CacheName) override
	{
		//All related entries are automatically deleted because of foreign key constraints
		return ExecuteStatement(stmtDelCache, CacheName);
	}

	virtual bool IsPreload(unsigned long long id) const override
	{
		const auto Stmt = AutoStatement(stmtGetPreloadState);
		return Stmt->Bind(id).Step() && Stmt->GetColInt(0) != 0;
	}

	virtual string GetSignature(unsigned long long id) const override
	{
		return GetTextFromID(stmtGetSignature, id);
	}

	virtual bool GetExportState(unsigned long long id, const wchar_t* ExportName) const override
	{
		if (!*ExportName)
			return false;

		const auto Stmt = AutoStatement(stmtGetExportState);
		return Stmt->Bind(id, ExportName).Step() && Stmt->GetColInt(0);
	}

	virtual string GetGuid(unsigned long long id) const override
	{
		return GetTextFromID(stmtGetGuid, id);
	}

	virtual string GetTitle(unsigned long long id) const override
	{
		return GetTextFromID(stmtGetTitle, id);
	}

	virtual string GetAuthor(unsigned long long id) const override
	{
		return GetTextFromID(stmtGetAuthor, id);
	}

	virtual string GetDescription(unsigned long long id) const override
	{
		return GetTextFromID(stmtGetDescription, id);
	}

	virtual bool GetMinFarVersion(unsigned long long id, VersionInfo *Version) const override
	{
		return GetVersionImpl(stmtGetMinFarVersion, id, Version);
	}

	virtual bool GetVersion(unsigned long long id, VersionInfo *Version) const override
	{
		return GetVersionImpl(stmtGetVersion, id, Version);
	}

	virtual bool GetDiskMenuItem(unsigned long long id, size_t index, string &Text, GUID& Guid) const override
	{
		return GetMenuItem(id, DRIVE_MENU, index, Text, Guid);
	}

	virtual bool GetPluginsMenuItem(unsigned long long id, size_t index, string &Text, GUID& Guid) const override
	{
		return GetMenuItem(id, PLUGINS_MENU, index, Text, Guid);
	}

	virtual bool GetPluginsConfigMenuItem(unsigned long long id, size_t index, string &Text, GUID& Guid) const override
	{
		return GetMenuItem(id, CONFIG_MENU, index, Text, Guid);
	}

	virtual string GetCommandPrefix(unsigned long long id) const override
	{
		return GetTextFromID(stmtGetPrefix, id);
	}

	virtual unsigned long long GetFlags(unsigned long long id) const override
	{
		const auto Stmt = AutoStatement(stmtGetFlags);
		return Stmt->Bind(id).Step()? Stmt->GetColInt64(0) : 0;
	}

	virtual bool SetPreload(unsigned long long id, bool Preload) override
	{
		return ExecuteStatement(stmtSetPreloadState, id, Preload);
	}

	virtual bool SetSignature(unsigned long long id, const string& Signature) override
	{
		return ExecuteStatement(stmtSetSignature, id, Signature);
	}

	virtual bool SetDiskMenuItem(unsigned long long id, size_t index, const string& Text, const GUID& Guid) override
	{
		return SetMenuItem(id, DRIVE_MENU, index, Text, Guid);
	}

	virtual bool SetPluginsMenuItem(unsigned long long id, size_t index, const string& Text, const GUID& Guid) override
	{
		return SetMenuItem(id, PLUGINS_MENU, index, Text, Guid);
	}

	virtual bool SetPluginsConfigMenuItem(unsigned long long id, size_t index, const string& Text, const GUID& Guid) override
	{
		return SetMenuItem(id, CONFIG_MENU, index, Text, Guid);
	}

	virtual bool SetCommandPrefix(unsigned long long id, const string& Prefix) override
	{
		return ExecuteStatement(stmtSetPrefix, id, Prefix);
	}

	virtual bool SetFlags(unsigned long long id, unsigned long long Flags) override
	{
		return ExecuteStatement(stmtSetFlags, id, Flags);
	}

	virtual bool SetExportState(unsigned long long id, const wchar_t* ExportName, bool Exists) override
	{
		return *ExportName && ExecuteStatement(stmtSetExportState, id, ExportName, Exists);
	}

	virtual bool SetMinFarVersion(unsigned long long id, const VersionInfo *Version) override
	{
		return ExecuteStatement(stmtSetMinFarVersion, id, make_blob_view(*Version));
	}

	virtual bool SetVersion(unsigned long long id, const VersionInfo *Version) override
	{
		return ExecuteStatement(stmtSetVersion, id, make_blob_view(*Version));
	}

	virtual bool SetGuid(unsigned long long id, const string& Guid) override
	{
		return ExecuteStatement(stmtSetGuid, id, Guid);
	}

	virtual bool SetTitle(unsigned long long id, const string& Title) override
	{
		return ExecuteStatement(stmtSetTitle, id, Title);
	}

	virtual bool SetAuthor(unsigned long long id, const string& Author) override
	{
		return ExecuteStatement(stmtSetAuthor, id, Author);
	}

	virtual bool SetDescription(unsigned long long id, const string& Description) override
	{
		return ExecuteStatement(stmtSetDescription, id, Description);
	}

	virtual bool EnumPlugins(DWORD index, string &CacheName) const override
	{
		auto Stmt = AutoStatement(stmtEnumCache);
		if (index == 0)
			Stmt->Reset();

		if (!Stmt->Step())
			return false;

		CacheName = Stmt->GetColText(0);
		Stmt.release();
		return true;
	}

	virtual bool IsCacheEmpty() const override
	{
		const auto Stmt = AutoStatement(stmtCountCacheNames);
		return Stmt->Step() && Stmt->GetColInt(0) == 0;
	}

	enum MenuItemTypeEnum
	{
		PLUGINS_MENU,
		CONFIG_MENU,
		DRIVE_MENU
	};

	bool GetMenuItem(unsigned long long id, MenuItemTypeEnum type, size_t index, string &Text, GUID& Guid) const
	{
		const auto Stmt = AutoStatement(stmtGetMenuItem);
		if (!Stmt->Bind(id, type, index).Step())
			return false;

		Text = Stmt->GetColText(0);
		return StrToGuid(Stmt->GetColText(1), Guid);
	}

	bool SetMenuItem(unsigned long long id, MenuItemTypeEnum type, size_t index, const string& Text, const GUID& Guid) const
	{
		return ExecuteStatement(stmtSetMenuItem, id, type, index, GuidToStr(Guid), Text);
	}

	string GetTextFromID(size_t StatementIndex, unsigned long long id) const
	{
		auto Stmt = AutoStatement(StatementIndex);
		return Stmt->Bind(id).Step()? Stmt->GetColText(0) : string{};
	}

	bool GetVersionImpl(size_t StatementIndex, unsigned long long id, VersionInfo *Version) const
	{
		const auto Stmt = AutoStatement(StatementIndex);
		if (!Stmt->Bind(id).Step())
			return false;

		const auto Blob = Stmt->GetColBlob(0);
		if (Blob.size() != sizeof(*Version))
			throw MAKE_FAR_EXCEPTION("incorrect blob size");
		*Version = *reinterpret_cast<const VersionInfo*>(Blob.data());
		return true;
	}

	enum statement_id
	{
		stmtCreateCache,
		stmtFindCacheName,
		stmtDelCache,
		stmtCountCacheNames,
		stmtGetPreloadState,
		stmtGetSignature,
		stmtGetExportState,
		stmtGetGuid,
		stmtGetTitle,
		stmtGetAuthor,
		stmtGetPrefix,
		stmtGetDescription,
		stmtGetFlags,
		stmtGetMinFarVersion,
		stmtGetVersion,
		stmtSetPreloadState,
		stmtSetSignature,
		stmtSetExportState,
		stmtSetGuid,
		stmtSetTitle,
		stmtSetAuthor,
		stmtSetPrefix,
		stmtSetDescription,
		stmtSetFlags,
		stmtSetMinFarVersion,
		stmtSetVersion,
		stmtEnumCache,
		stmtGetMenuItem,
		stmtSetMenuItem,

		stmt_count
	};
};

class PluginsHotkeysConfigDb: public PluginsHotkeysConfig, public SQLiteDb
{
public:
	PluginsHotkeysConfigDb()
	{
		Initialize(L"pluginhotkeys.db");
	}

private:
	virtual bool InitializeImpl(const string& DbName, bool Local) override
	{
		static constexpr auto Schema =
			"CREATE TABLE IF NOT EXISTS pluginhotkeys(pluginkey TEXT NOT NULL, menuguid TEXT NOT NULL, type INTEGER NOT NULL, hotkey TEXT, PRIMARY KEY(pluginkey, menuguid, type));"
			;

		static constexpr stmt_init_t Statements[] =
		{
			{ stmtGetHotkey, L"SELECT hotkey FROM pluginhotkeys WHERE pluginkey=?1 AND menuguid=?2 AND type=?3;" },
			{ stmtSetHotkey, L"INSERT OR REPLACE INTO pluginhotkeys VALUES (?1,?2,?3,?4);" },
			{ stmtDelHotkey, L"DELETE FROM pluginhotkeys WHERE pluginkey=?1 AND menuguid=?2 AND type=?3;" },
			{ stmtCheckForHotkeys, L"SELECT count(hotkey) FROM pluginhotkeys WHERE type=?1;" },
		};
		CheckStmt<stmt_count>(Statements);

		return Open(DbName, Local)
		    && Exec(Schema)
		    && PrepareStatements(Statements)
		;
	}

	virtual bool HotkeysPresent(hotkey_type HotKeyType) override
	{
		const auto Stmt = AutoStatement(stmtCheckForHotkeys);
		return Stmt->Bind(static_cast<std::underlying_type_t<hotkey_type>>(HotKeyType)).Step() && Stmt->GetColInt(0);
	}

	virtual string GetHotkey(const string& PluginKey, const GUID& MenuGuid, hotkey_type HotKeyType) override
	{
		const auto Stmt = AutoStatement(stmtGetHotkey);
		if (!Stmt->Bind(PluginKey, GuidToStr(MenuGuid), static_cast<std::underlying_type_t<hotkey_type>>(HotKeyType)).Step())
			return{};

		return Stmt->GetColText(0);
	}

	virtual bool SetHotkey(const string& PluginKey, const GUID& MenuGuid, hotkey_type HotKeyType, const string& HotKey) override
	{
		return ExecuteStatement(stmtSetHotkey, PluginKey, GuidToStr(MenuGuid), static_cast<std::underlying_type_t<hotkey_type>>(HotKeyType), HotKey);
	}

	virtual bool DelHotkey(const string& PluginKey, const GUID& MenuGuid, hotkey_type HotKeyType) override
	{
		return ExecuteStatement(stmtDelHotkey, PluginKey, GuidToStr(MenuGuid), static_cast<std::underlying_type_t<hotkey_type>>(HotKeyType));
	}

	virtual void Export(representation_destination& Representation) override
	{
		auto& root = CreateChild(Representation.GetRoot(), "pluginhotkeys");

		auto stmtEnumAllPluginKeys = create_stmt(L"SELECT pluginkey FROM pluginhotkeys GROUP BY pluginkey;");
		auto stmtEnumAllHotkeysPerKey = create_stmt(L"SELECT menuguid, type, hotkey FROM pluginhotkeys WHERE pluginkey=$1;");

		while (stmtEnumAllPluginKeys.Step())
		{
			auto& p = CreateChild(root, "plugin");

			string Key = stmtEnumAllPluginKeys.GetColText(0);
			p.SetAttribute("key", stmtEnumAllPluginKeys.GetColTextUTF8(0));

			stmtEnumAllHotkeysPerKey.Bind(Key);
			while (stmtEnumAllHotkeysPerKey.Step())
			{
				const char *type = nullptr;
				switch (static_cast<hotkey_type>(stmtEnumAllHotkeysPerKey.GetColInt(1)))
				{
				case hotkey_type::drive_menu: type = "drive"; break;
				case hotkey_type::config_menu: type = "config"; break;
				case hotkey_type::plugins_menu: type = "plugins"; break;
				}

				if (!type)
				{
					// TODO: log
					continue;
				}

				auto& e = CreateChild(p, "hotkey");
				e.SetAttribute("menu", type);
				e.SetAttribute("guid", stmtEnumAllHotkeysPerKey.GetColTextUTF8(0));
				const auto hotkey = stmtEnumAllHotkeysPerKey.GetColTextUTF8(2);
				e.SetAttribute("hotkey", NullToEmpty(hotkey));
			}
			stmtEnumAllHotkeysPerKey.Reset();
		}
	}

	virtual void Import(const representation_source& Representation) override
	{
		SCOPED_ACTION(auto)(ScopedTransaction());
		for (const auto& e: xml_enum(Representation.GetRoot().FirstChildElement("pluginhotkeys"), "plugin"))
		{
			const auto key = e->Attribute("key");

			if (!key)
				continue;

			const auto Key = encoding::utf8::get_chars(key);

			for (const auto& se: xml_enum(*e, "hotkey"))
			{
				const auto stype = se->Attribute("menu");
				const auto guid = se->Attribute("guid");
				const auto hotkey = se->Attribute("hotkey");

				GUID Guid;

				if (!stype || !guid || !StrToGuid(encoding::utf8::get_chars(guid), Guid))
					continue;

				const auto Hotkey = encoding::utf8::get_chars(hotkey);

				if (!strcmp(stype,"drive"))
					SetHotkey(Key, Guid, hotkey_type::drive_menu, Hotkey);
				else if (!strcmp(stype,"config"))
					SetHotkey(Key, Guid, hotkey_type::config_menu, Hotkey);
				else if (!strcmp(stype, "plugins"))
					SetHotkey(Key, Guid, hotkey_type::plugins_menu, Hotkey);
			}

		}
	}

	enum statement_id
	{
		stmtGetHotkey,
		stmtSetHotkey,
		stmtDelHotkey,
		stmtCheckForHotkeys,

		stmt_count
	};
};

class HistoryConfigCustom: public HistoryConfig, public SQLiteDb
{
public:
	virtual ~HistoryConfigCustom()
	{
		WaitAllAsync();
		StopEvent.Set();
	}

private:
	static unsigned long long DaysToUI64(int Days)
	{
		return Days * 24ull * 60ull * 60ull * 10000000ull;
	}

	Thread WorkThread;
	Event StopEvent;
	Event AsyncDeleteAddDone;
	Event AsyncCommitDone;
	Event AsyncWork;
	MultiWaiter AllWaiter;

	struct AsyncWorkItem
	{
		unsigned long long DeleteId;
		unsigned int TypeHistory;
		string HistoryName;
		string strName;
		int Type;
		bool Lock;
		string strGuid;
		string strFile;
		string strData;
	};

	SyncedQueue<std::unique_ptr<AsyncWorkItem>> WorkQueue;

	void WaitAllAsync() const { AllWaiter.Wait(); }
	void WaitCommitAsync() const { AsyncCommitDone.Wait(); }

	bool StartThread()
	{
		StopEvent = Event(Event::automatic, Event::nonsignaled);
		string EventName;
		if (GetPath() != L":memory:")
		{
			EventName = make_name<Event>(GetPath(), GetName());
		}
		AsyncDeleteAddDone = Event(Event::manual, Event::signaled, (EventName + L"_Delete").data());
		AsyncCommitDone = Event(Event::manual, Event::signaled, (EventName + L"_Commit").data());
		AllWaiter.Add(AsyncDeleteAddDone);
		AllWaiter.Add(AsyncCommitDone);
		AsyncWork = Event(Event::automatic, Event::nonsignaled);
		WorkThread = Thread(&Thread::join, &HistoryConfigCustom::ThreadProc, this);
		return true;
	}

	void ThreadProc()
	{
		MultiWaiter Waiter;
		Waiter.Add(AsyncWork);
		Waiter.Add(StopEvent);

		for (;;)
		{
			DWORD wait = Waiter.Wait(MultiWaiter::wait_any);

			if (wait != WAIT_OBJECT_0)
				break;

			bool bAddDelete=false, bCommit=false;

			{
				SCOPED_ACTION(auto)(WorkQueue.scoped_lock());

				decltype(WorkQueue)::value_type item;
				while (WorkQueue.try_pop(item))
				{
					if (item) //DeleteAndAddAsync
					{
						SQLiteDb::BeginTransaction();
						if (item->DeleteId)
							DeleteInternal(item->DeleteId);
						AddInternal(item->TypeHistory, item->HistoryName, item->strName, item->Type, item->Lock, item->strGuid, item->strFile, item->strData);
						SQLiteDb::EndTransaction();
						bAddDelete = true;
					}
					else // EndTransaction
					{
						SQLiteDb::EndTransaction();
						bCommit = true;
					}
				}
			}
			if (bAddDelete)
				AsyncDeleteAddDone.Set();
			if (bCommit)
				AsyncCommitDone.Set();
		}
	}

	bool AddInternal(unsigned int TypeHistory, const string& HistoryName, const string &Name, int Type, bool Lock, const string &strGuid, const string &strFile, const string &strData) const
	{
		return ExecuteStatement(stmtAdd, TypeHistory, HistoryName, Type, Lock, Name, GetCurrentUTCTimeInUI64(), strGuid, strFile, strData);
	}

	bool DeleteInternal(unsigned long long id) const
	{
		return ExecuteStatement(stmtDel, id);
	}

	unsigned long long GetPrevImpl(unsigned int TypeHistory, const string& HistoryName, unsigned long long id, string& Name, const std::function<unsigned long long()>& Fallback) const
	{
		WaitAllAsync();
		Name.clear();

		if (!id)
		{
			const auto GetNewestStmt = AutoStatement(stmtGetNewest);
			if (!GetNewestStmt->Bind(TypeHistory, HistoryName).Step())
				return 0;

			Name = GetNewestStmt->GetColText(1);
			return GetNewestStmt->GetColInt64(0);
		}

		const auto GetPrevStmt = AutoStatement(stmtGetPrev);
		if (!GetPrevStmt->Bind(id, TypeHistory, HistoryName).Step())
			return Fallback();

		Name = GetPrevStmt->GetColText(1);
		return GetPrevStmt->GetColInt64(0);
	}

	virtual bool BeginTransaction() override { WaitAllAsync(); return SQLiteDb::BeginTransaction(); }

	virtual bool EndTransaction() override
	{
		WorkQueue.push(nullptr);
		WaitAllAsync();
		AsyncCommitDone.Reset();
		AsyncWork.Set();
		return true;
	}

	virtual bool RollbackTransaction() override { WaitAllAsync(); return SQLiteDb::RollbackTransaction(); }

	virtual bool InitializeImpl(const string& DbName, bool Local) override
	{
		static constexpr auto Schema =
			//command,view,edit,folder,dialog history
			"CREATE TABLE IF NOT EXISTS history(id INTEGER PRIMARY KEY, kind INTEGER NOT NULL, key TEXT NOT NULL, type INTEGER NOT NULL, lock INTEGER NOT NULL, name TEXT NOT NULL, time INTEGER NOT NULL, guid TEXT NOT NULL, file TEXT NOT NULL, data TEXT NOT NULL);"
			"CREATE INDEX IF NOT EXISTS history_idx1 ON history (kind, key);"
			"CREATE INDEX IF NOT EXISTS history_idx2 ON history (kind, key, time);"
			"CREATE INDEX IF NOT EXISTS history_idx3 ON history (kind, key, lock DESC, time DESC);"
			"CREATE INDEX IF NOT EXISTS history_idx4 ON history (kind, key, time DESC);"
			//view,edit file positions and bookmarks history
			"CREATE TABLE IF NOT EXISTS editorposition_history(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE COLLATE NOCASE, time INTEGER NOT NULL, line INTEGER NOT NULL, linepos INTEGER NOT NULL, screenline INTEGER NOT NULL, leftpos INTEGER NOT NULL, codepage INTEGER NOT NULL);"
			"CREATE TABLE IF NOT EXISTS editorbookmarks_history(pid INTEGER NOT NULL, num INTEGER NOT NULL, line INTEGER NOT NULL, linepos INTEGER NOT NULL, screenline INTEGER NOT NULL, leftpos INTEGER NOT NULL, FOREIGN KEY(pid) REFERENCES editorposition_history(id) ON UPDATE CASCADE ON DELETE CASCADE, PRIMARY KEY (pid, num));"
			"CREATE INDEX IF NOT EXISTS editorposition_history_idx1 ON editorposition_history (time DESC);"
			"CREATE TABLE IF NOT EXISTS viewerposition_history(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE COLLATE NOCASE, time INTEGER NOT NULL, filepos INTEGER NOT NULL, leftpos INTEGER NOT NULL, hex INTEGER NOT NULL, codepage INTEGER NOT NULL);"
			"CREATE TABLE IF NOT EXISTS viewerbookmarks_history(pid INTEGER NOT NULL, num INTEGER NOT NULL, filepos INTEGER NOT NULL, leftpos INTEGER NOT NULL, FOREIGN KEY(pid) REFERENCES viewerposition_history(id) ON UPDATE CASCADE ON DELETE CASCADE, PRIMARY KEY (pid, num));"
			"CREATE INDEX IF NOT EXISTS viewerposition_history_idx1 ON viewerposition_history (time DESC);"
		;

		static constexpr stmt_init_t Statements[] =
		{
			{ stmtEnum, L"SELECT id, name, type, lock, time, guid, file, data FROM history WHERE kind=?1 AND key=?2 ORDER BY time;" },
			{ stmtEnumDesc, L"SELECT id, name, type, lock, time, guid, file, data FROM history WHERE kind=?1 AND key=?2 ORDER BY lock DESC, time DESC;" },
			{ stmtDel, L"DELETE FROM history WHERE id=?1;" },
			{ stmtDeleteOldUnlocked, L"DELETE FROM history WHERE kind=?1 AND key=?2 AND lock=0 AND time<?3 AND id NOT IN (SELECT id FROM history WHERE kind=?1 AND key=?2 ORDER BY lock DESC, time DESC LIMIT ?4);" },
			{ stmtEnumLargeHistories, L"SELECT key FROM (SELECT key, num FROM (SELECT key, count(id) as num FROM history WHERE kind=?1 GROUP BY key)) WHERE num > ?2;" },
			{ stmtAdd, L"INSERT INTO history VALUES (NULL,?1,?2,?3,?4,?5,?6,?7,?8,?9);" },
			{ stmtGetName, L"SELECT name FROM history WHERE id=?1;" },
			{ stmtGetNameAndType, L"SELECT name, type, guid, file, data FROM history WHERE id=?1;" },
			{ stmtGetNewestName, L"SELECT name FROM history WHERE kind=?1 AND key=?2 ORDER BY lock DESC, time DESC LIMIT 1;" },
			{ stmtCount, L"SELECT count(id) FROM history WHERE kind=?1 AND key=?2;" },
			{ stmtDelUnlocked, L"DELETE FROM history WHERE kind=?1 AND key=?2 AND lock=0;" },
			{ stmtGetLock, L"SELECT lock FROM history WHERE id=?1;" },
			{ stmtSetLock, L"UPDATE history SET lock=?1 WHERE id=?2" },
			{ stmtGetNext, L"SELECT a.id, a.name FROM history AS a, history AS b WHERE b.id=?1 AND a.kind=?2 AND a.key=?3 AND a.time>b.time ORDER BY a.time LIMIT 1;" },
			{ stmtGetPrev, L"SELECT a.id, a.name FROM history AS a, history AS b WHERE b.id=?1 AND a.kind=?2 AND a.key=?3 AND a.time<b.time ORDER BY a.time DESC LIMIT 1;" },
			{ stmtGetNewest, L"SELECT id, name FROM history WHERE kind=?1 AND key=?2 ORDER BY time DESC LIMIT 1;" },
			{ stmtSetEditorPos, L"INSERT OR REPLACE INTO editorposition_history VALUES (NULL,?1,?2,?3,?4,?5,?6,?7);" },
			{ stmtSetEditorBookmark, L"INSERT OR REPLACE INTO editorbookmarks_history VALUES (?1,?2,?3,?4,?5,?6);" },
			{ stmtGetEditorPos, L"SELECT id, line, linepos, screenline, leftpos, codepage FROM editorposition_history WHERE name=?1 COLLATE NOCASE;" },
			{ stmtGetEditorBookmark, L"SELECT line, linepos, screenline, leftpos FROM editorbookmarks_history WHERE pid=?1 AND num=?2;" },
			{ stmtSetViewerPos, L"INSERT OR REPLACE INTO viewerposition_history VALUES (NULL,?1,?2,?3,?4,?5,?6);" },
			{ stmtSetViewerBookmark, L"INSERT OR REPLACE INTO viewerbookmarks_history VALUES (?1,?2,?3,?4);" },
			{ stmtGetViewerPos, L"SELECT id, filepos, leftpos, hex, codepage FROM viewerposition_history WHERE name=?1 COLLATE NOCASE;" },
			{ stmtGetViewerBookmark, L"SELECT filepos, leftpos FROM viewerbookmarks_history WHERE pid=?1 AND num=?2;" },
			{ stmtDeleteOldEditor, L"DELETE FROM editorposition_history WHERE time<?1 AND id NOT IN (SELECT id FROM editorposition_history ORDER BY time DESC LIMIT ?2);" },
			{ stmtDeleteOldViewer, L"DELETE FROM viewerposition_history WHERE time<?1 AND id NOT IN (SELECT id FROM viewerposition_history ORDER BY time DESC LIMIT ?2);" },
		};
		CheckStmt<stmt_count>(Statements);

		return Open(DbName, Local, true)
		    && SetWALJournalingMode()
		    && EnableForeignKeysConstraints()
		    && Exec(Schema)
		    && PrepareStatements(Statements)
		    && StartThread()
		;
	}

	virtual bool Delete(unsigned long long id) override
	{
		WaitAllAsync();
		return DeleteInternal(id);
	}

	virtual bool Enum(DWORD index, unsigned int TypeHistory, const string& HistoryName, unsigned long long *id, string &Name, history_record_type* Type, bool *Lock, unsigned long long *Time, string &strGuid, string &strFile, string &strData, bool Reverse = false) override
	{
		WaitAllAsync();
		auto Stmt = AutoStatement(Reverse? stmtEnumDesc : stmtEnum);

		if (index == 0)
			Stmt->Reset().Bind(TypeHistory, transient(HistoryName));

		if (!Stmt->Step())
			return false;

		*id = Stmt->GetColInt64(0);
		Name = Stmt->GetColText(1);
		*Type = static_cast<history_record_type>(Stmt->GetColInt(2));
		*Lock = Stmt->GetColInt(3) != 0;
		*Time = Stmt->GetColInt64(4);
		strGuid = Stmt->GetColText(5);
		strFile = Stmt->GetColText(6);
		strData = Stmt->GetColText(7);
		Stmt.release();
		return true;
	}

	virtual bool DeleteAndAddAsync(unsigned long long DeleteId, unsigned int TypeHistory, const string& HistoryName, string Name, int Type, bool Lock, string &strGuid, string &strFile, string &strData) override
	{
		auto item = std::make_unique<AsyncWorkItem>();
		item->DeleteId=DeleteId;
		item->TypeHistory=TypeHistory;
		item->HistoryName=HistoryName;
		item->strName=Name;
		item->Type=Type;
		item->Lock=Lock;
		item->strGuid=strGuid;
		item->strFile=strFile;
		item->strData=strData;

		WorkQueue.push(std::move(item));

		WaitAllAsync();
		AsyncDeleteAddDone.Reset();
		AsyncWork.Set();
		return true;
	}

	virtual bool DeleteOldUnlocked(unsigned int TypeHistory, const string& HistoryName, int DaysToKeep, int MinimumEntries) override
	{
		WaitAllAsync();
		const auto older = GetCurrentUTCTimeInUI64() - DaysToUI64(DaysToKeep);
		return ExecuteStatement(stmtDeleteOldUnlocked, TypeHistory, HistoryName, older, MinimumEntries);
	}

	virtual bool EnumLargeHistories(DWORD index, int MinimumEntries, unsigned int TypeHistory, string &strHistoryName) override
	{
		WaitAllAsync();
		auto Stmt = AutoStatement(stmtEnumLargeHistories);
		if (index == 0)
			Stmt->Reset().Bind(TypeHistory, MinimumEntries);

		if (!Stmt->Step())
			return false;

		strHistoryName = Stmt->GetColText(0);
		Stmt.release();
		return true;
	}

	virtual bool GetNewest(unsigned int TypeHistory, const string& HistoryName, string& Name) override
	{
		WaitAllAsync();
		const auto Stmt = AutoStatement(stmtGetNewestName);
		if (!Stmt->Bind(TypeHistory, HistoryName).Step())
			return false;

		Name = Stmt->GetColText(0);
		return true;
	}

	virtual bool Get(unsigned long long id, string& Name) override
	{
		WaitAllAsync();
		const auto Stmt = AutoStatement(stmtGetName);
		if (!Stmt->Bind(id).Step())
			return false;

		Name = Stmt->GetColText(0);
		return true;
	}

	virtual bool Get(unsigned long long id, string& Name, history_record_type& Type, string &strGuid, string &strFile, string &strData) override
	{
		WaitAllAsync();
		const auto Stmt = AutoStatement(stmtGetNameAndType);
		if (!Stmt->Bind(id).Step())
			return false;

		Name = Stmt->GetColText(0);
		Type = static_cast<history_record_type>(Stmt->GetColInt(1));
		strGuid = Stmt->GetColText(2);
		strFile = Stmt->GetColText(3);
		strData = Stmt->GetColText(4);
		return true;
	}

	virtual DWORD Count(unsigned int TypeHistory, const string& HistoryName) override
	{
		WaitAllAsync();
		const auto Stmt = AutoStatement(stmtCount);
		return Stmt->Bind(TypeHistory, HistoryName).Step()? static_cast<DWORD>(Stmt-> GetColInt(0)) : 0;
	}

	virtual bool FlipLock(unsigned long long id) override
	{
		WaitAllAsync();
		return ExecuteStatement(stmtSetLock, !IsLocked(id), id);
	}

	virtual bool IsLocked(unsigned long long id) override
	{
		WaitAllAsync();
		const auto Stmt = AutoStatement(stmtGetLock);
		return Stmt->Bind(id).Step() && Stmt->GetColInt(0) != 0;
	}

	virtual bool DeleteAllUnlocked(unsigned int TypeHistory, const string& HistoryName) override
	{
		WaitAllAsync();
		return ExecuteStatement(stmtDelUnlocked, TypeHistory, HistoryName);
	}

	virtual unsigned long long GetNext(unsigned int TypeHistory, const string& HistoryName, unsigned long long id, string& Name) override
	{
		WaitAllAsync();
		Name.clear();

		if (!id)
			return 0;

		const auto Stmt = AutoStatement(stmtGetNext);
		if (!Stmt->Bind(id, TypeHistory, HistoryName).Step())
			return 0;

		Name = Stmt->GetColText(1);
		return Stmt->GetColInt64(0);
	}

	virtual unsigned long long GetPrev(unsigned int TypeHistory, const string& HistoryName, unsigned long long id, string& Name) override
	{
		return GetPrevImpl(TypeHistory, HistoryName, id, Name, [&]() { return Get(id, Name)? id : 0; });
	}

	virtual unsigned long long CyclicGetPrev(unsigned int TypeHistory, const string& HistoryName, unsigned long long id, string& Name) override
	{
		return GetPrevImpl(TypeHistory, HistoryName, id, Name, [&]() { return 0; });
	}

	virtual unsigned long long SetEditorPos(const string& Name, int Line, int LinePos, int ScreenLine, int LeftPos, uintptr_t CodePage) override
	{
		WaitCommitAsync();
		return ExecuteStatement(stmtSetEditorPos, Name, GetCurrentUTCTimeInUI64(), Line, LinePos, ScreenLine, LeftPos, CodePage)? LastInsertRowID() : 0;
	}

	virtual unsigned long long GetEditorPos(const string& Name, int *Line, int *LinePos, int *ScreenLine, int *LeftPos, uintptr_t *CodePage) override
	{
		WaitCommitAsync();
		const auto Stmt = AutoStatement(stmtGetEditorPos);
		if (!Stmt->Bind(Name).Step())
			return 0;

		*Line = Stmt->GetColInt(1);
		*LinePos = Stmt->GetColInt(2);
		*ScreenLine = Stmt->GetColInt(3);
		*LeftPos = Stmt->GetColInt(4);
		*CodePage = Stmt->GetColInt(5);
		return Stmt->GetColInt64(0);
	}

	virtual bool SetEditorBookmark(unsigned long long id, size_t i, int Line, int LinePos, int ScreenLine, int LeftPos) override
	{
		WaitCommitAsync();
		return ExecuteStatement(stmtSetEditorBookmark, id, i, Line, LinePos, ScreenLine, LeftPos);
	}

	virtual bool GetEditorBookmark(unsigned long long id, size_t i, int *Line, int *LinePos, int *ScreenLine, int *LeftPos) override
	{
		WaitCommitAsync();
		const auto Stmt = AutoStatement(stmtGetEditorBookmark);
		if (!Stmt->Bind(id, i).Step())
			return false;

		*Line = Stmt->GetColInt(0);
		*LinePos = Stmt->GetColInt(1);
		*ScreenLine = Stmt->GetColInt(2);
		*LeftPos = Stmt->GetColInt(3);
		return true;
	}

	virtual unsigned long long SetViewerPos(const string& Name, long long FilePos, long long LeftPos, int Hex_Wrap, uintptr_t CodePage) override
	{
		WaitCommitAsync();
		return ExecuteStatement(stmtSetViewerPos, Name, GetCurrentUTCTimeInUI64(), FilePos, LeftPos, Hex_Wrap, CodePage)? LastInsertRowID() : 0;
	}

	virtual unsigned long long GetViewerPos(const string& Name, long long *FilePos, long long *LeftPos, int *Hex, uintptr_t *CodePage) override
	{
		WaitCommitAsync();
		const auto Stmt = AutoStatement(stmtGetViewerPos);

		if (!Stmt->Bind(Name).Step())
			return 0;

		*FilePos = Stmt->GetColInt64(1);
		*LeftPos = Stmt->GetColInt64(2);
		*Hex = Stmt->GetColInt(3);
		*CodePage = Stmt->GetColInt(4);
		return Stmt->GetColInt64(0);
	}

	virtual bool SetViewerBookmark(unsigned long long id, size_t i, long long FilePos, long long LeftPos) override
	{
		WaitCommitAsync();
		return ExecuteStatement(stmtSetViewerBookmark, id, i, FilePos, LeftPos);
	}

	virtual bool GetViewerBookmark(unsigned long long id, size_t i, long long *FilePos, long long *LeftPos) override
	{
		WaitCommitAsync();
		const auto Stmt = AutoStatement(stmtGetViewerBookmark);
		if (!Stmt->Bind(id, i).Step())
			return false;

		*FilePos = Stmt->GetColInt64(0);
		*LeftPos = Stmt->GetColInt64(1);
		return true;
	}

	virtual void DeleteOldPositions(int DaysToKeep, int MinimumEntries) override
	{
		WaitCommitAsync();
		const auto older = GetCurrentUTCTimeInUI64() - DaysToUI64(DaysToKeep);
		ExecuteStatement(stmtDeleteOldEditor, older, MinimumEntries);
		ExecuteStatement(stmtDeleteOldViewer, older, MinimumEntries);
	}

	enum statement_id
	{
		stmtEnum,
		stmtEnumDesc,
		stmtDel,
		stmtDeleteOldUnlocked,
		stmtEnumLargeHistories,
		stmtAdd,
		stmtGetName,
		stmtGetNameAndType,
		stmtGetNewestName,
		stmtCount,
		stmtDelUnlocked,
		stmtGetLock,
		stmtSetLock,
		stmtGetNext,
		stmtGetPrev,
		stmtGetNewest,
		stmtSetEditorPos,
		stmtSetEditorBookmark,
		stmtGetEditorPos,
		stmtGetEditorBookmark,
		stmtSetViewerPos,
		stmtSetViewerBookmark,
		stmtGetViewerPos,
		stmtGetViewerBookmark,
		stmtDeleteOldEditor,
		stmtDeleteOldViewer,

		stmt_count
	};
};

class HistoryConfigDb: public HistoryConfigCustom
{
public:
	HistoryConfigDb()
	{
		Initialize(L"history.db", true);
	}

private:
	// TODO: log
	// TODO: implementation
	virtual void Import(const representation_source&) override {}
	virtual void Export(representation_destination&) override {}
};

class HistoryConfigMemory: public HistoryConfigCustom
{
public:
	HistoryConfigMemory()
	{
		Initialize(L":memory:", true);
	}

private:
	virtual void Import(const representation_source&) override {}
	virtual void Export(representation_destination&) override {}
};

static const std::wregex& uuid_regex()
{
	static const std::wregex re(RE_BEGIN RE_ANY_UUID RE_END, std::regex::icase | std::regex::optimize);
	return re;
}

}

void config_provider::CheckDatabase(SQLiteDb *pDb)
{
	string pname;
	const auto rc = pDb->GetInitStatus(pname, m_Mode != mode::m_default);
	if ( rc > 0 )
	{
		if (m_Mode != mode::m_default)
		{
			Console().Write(format(L"problem with {0}:\n  {1}\n", pname, rc <= 1 ? L"database file is renamed to *.bad and new one is created" : L"database is opened in memory"));
			Console().Commit();
		}
		else
		{
			m_Problems.emplace_back(pname);
		}
	}
}

void config_provider::TryImportDatabase(representable *p, const char *son, bool plugin)
{
	if (!m_TemplateSource && !Global->Opt->TemplateProfilePath.empty())
	{
		m_TemplateSource = std::make_unique<representation_source>(Global->Opt->TemplateProfilePath);
	}

	if (m_TemplateSource && m_TemplateSource->GetRoot().ToNode())
	{
		auto root = m_TemplateSource->GetRoot();

		if (!son)
		{
			p->Import(*m_TemplateSource);
		}
		else if (!plugin)
		{
			m_TemplateSource->SetRoot(root.FirstChildElement(son));
			p->Import(*m_TemplateSource);
		}
		else
		{
			for (const auto& i: xml_enum(root.FirstChildElement("pluginsconfig"), "plugin"))
			{
				const auto guid = i->Attribute("guid");
				if (guid && 0 == strcmp(guid, son))
				{
					m_TemplateSource->SetRoot(&const_cast<tinyxml::XMLElement&>(*i));
					p->Import(*m_TemplateSource);
					break;
				}
			}
		}
		m_TemplateSource->SetRoot(root);
	}
}

template<class T>
std::unique_ptr<T> config_provider::CreateDatabase(const char *son)
{
	auto cfg = std::make_unique<T>();
	CheckDatabase(cfg.get());
	if (m_Mode != mode::m_import && cfg->IsNew())
	{
		TryImportDatabase(cfg.get(), son);
	}
	return cfg;
}

template<class T>
HierarchicalConfigUniquePtr config_provider::CreateHierarchicalConfig(dbcheck DbId, const string& dbn, const char *xmln, bool Local, bool plugin)
{
	auto cfg = std::make_unique<T>(dbn, Local);
	if (!m_CheckedDb.Check(DbId))
	{
		CheckDatabase(cfg.get());
		if (m_Mode != mode::m_import && cfg->IsNew())
		{
			TryImportDatabase(cfg.get(), xmln, plugin);
		}
		m_CheckedDb.Set(DbId);
	}
	return HierarchicalConfigUniquePtr(cfg.release());
}

enum dbcheck: int
{
	CHECK_NONE       = 0,
	CHECK_FILTERS    = bit(0),
	CHECK_HIGHLIGHT  = bit(1),
	CHECK_SHORTCUTS  = bit(2),
	CHECK_PANELMODES = bit(3),
};

HierarchicalConfigUniquePtr config_provider::CreatePluginsConfig(const string& guid, bool Local)
{
	return CreateHierarchicalConfig<HierarchicalConfigDb>(CHECK_NONE, L"PluginsData\\" + guid + L".db", encoding::utf8::get_bytes(guid).data(), Local, true);
}

HierarchicalConfigUniquePtr config_provider::CreateFiltersConfig()
{
	return CreateHierarchicalConfig<HierarchicalConfigDb>(CHECK_FILTERS, L"filters.db","filters");
}

HierarchicalConfigUniquePtr config_provider::CreateHighlightConfig()
{
	return CreateHierarchicalConfig<HighlightHierarchicalConfigDb>(CHECK_HIGHLIGHT, L"highlight.db","highlight");
}

HierarchicalConfigUniquePtr config_provider::CreateShortcutsConfig()
{
	return CreateHierarchicalConfig<HierarchicalConfigDb>(CHECK_SHORTCUTS, L"shortcuts.db","shortcuts", true);
}

HierarchicalConfigUniquePtr config_provider::CreatePanelModeConfig()
{
	return CreateHierarchicalConfig<HierarchicalConfigDb>(CHECK_PANELMODES, L"panelmodes.db","panelmodes");
}

config_provider::config_provider(mode Mode):
	m_LoadResult(SQLiteDb::library_load()),
	m_Mode(Mode),
	m_GeneralCfg(CreateDatabase<GeneralConfigDb>()),
	m_LocalGeneralCfg(CreateDatabase<LocalGeneralConfigDb>()),
	m_ColorsCfg(CreateDatabase<ColorsConfigDb>()),
	m_AssocConfig(CreateDatabase<AssociationsConfigDb>()),
	m_PlCacheCfg(CreateDatabase<PluginsCacheConfigDb>()),
	m_PlHotkeyCfg(CreateDatabase<PluginsHotkeysConfigDb>()),
	m_HistoryCfg(CreateDatabase<HistoryConfigDb>()),
	m_HistoryCfgMem(CreateDatabase<HistoryConfigMemory>())
{
}

config_provider::~config_provider()
{
	MultiWaiter(ALL_CONST_RANGE(m_Threads)).Wait();
	SQLiteDb::library_free();
}

bool config_provider::Export(const string& File)
{
	representation_destination Representation;
	auto& root = Representation.GetRoot();
	root.SetAttribute("version", format("{0}.{1}.{2}", FAR_VERSION.Major, FAR_VERSION.Minor, FAR_VERSION.Build).data());

	GeneralCfg()->Export(Representation);
	LocalGeneralCfg()->Export(Representation);
	ColorsCfg()->Export(Representation);
	AssocConfig()->Export(Representation);
	PlHotkeyCfg()->Export(Representation);
	Representation.SetRoot(CreateChild(root, "filters"));
	CreateFiltersConfig()->Export(Representation);
	Representation.SetRoot(CreateChild(root, "highlight"));
	CreateHighlightConfig()->Export(Representation);
	Representation.SetRoot(CreateChild(root, "panelmodes"));
	CreatePanelModeConfig()->Export(Representation);
	Representation.SetRoot(CreateChild(root, "shortcuts"));
	CreateShortcutsConfig()->Export(Representation);

	{
		//TODO: export local plugin settings
		auto& e = CreateChild(root, "pluginsconfig");
		for(auto& i: os::fs::enum_file(Global->Opt->ProfilePath + L"\\PluginsData\\*.db"))
		{
			i.strFileName.resize(i.strFileName.size()-3);
			InplaceUpper(i.strFileName);
			if (std::regex_search(i.strFileName, uuid_regex()))
			{
				auto& PluginRoot = CreateChild(e, "plugin");
				PluginRoot.SetAttribute("guid", encoding::utf8::get_bytes(i.strFileName).data());
				Representation.SetRoot(PluginRoot);
				CreatePluginsConfig(i.strFileName)->Export(Representation);
			}
		}
	}

	return Representation.Save(File);
}

bool config_provider::ServiceMode(const string& Filename)
{
	return m_Mode == mode::m_import? Import(Filename) : m_Mode == mode::m_export? Export(Filename) : throw MAKE_FAR_EXCEPTION("unexpected service mode");
}

bool config_provider::Import(const string& Filename)
{
	representation_source Representation(Filename);

	auto root = Representation.GetRoot();

	if (!root.ToNode())
	{
		Representation.PrintError();
		return false;
	}

	GeneralCfg()->Import(Representation);
	LocalGeneralCfg()->Import(Representation);
	ColorsCfg()->Import(Representation);
	AssocConfig()->Import(Representation);
	PlHotkeyCfg()->Import(Representation);
	Representation.SetRoot(root.FirstChildElement("filters"));
	CreateFiltersConfig()->Import(Representation);
	Representation.SetRoot(root.FirstChildElement("highlight"));
	CreateHighlightConfig()->Import(Representation);
	Representation.SetRoot(root.FirstChildElement("panelmodes"));
	CreatePanelModeConfig()->Import(Representation);
	Representation.SetRoot(root.FirstChildElement("shortcuts"));
	CreateShortcutsConfig()->Import(Representation);

	//TODO: import local plugin settings
	for (const auto& plugin: xml_enum(root.FirstChildElement("pluginsconfig"), "plugin"))
	{
		const auto guid = plugin->Attribute("guid");
		if (!guid)
			continue;
		const auto Guid = Upper(encoding::utf8::get_chars(guid));

		if (std::regex_search(Guid, uuid_regex()))
		{
			Representation.SetRoot(&const_cast<tinyxml::XMLElement&>(*plugin));
			CreatePluginsConfig(Guid)->Import(Representation);
		}
	}

	return true;
}

void config_provider::ClearPluginsCache()
{
	PluginsCacheConfigDb().DiscardCache();
}

bool config_provider::ShowProblems() const
{
	if (m_Problems.empty())
		return false;
	return Message(MSG_WARNING, MSG(MProblemDb), m_Problems, { MSG(MShowConfigFolders), MSG(MIgnore) }) == Message::first_button;
}

void config_provider::AddThread(Thread&& thread)
{
	m_Threads.emplace_back(std::move(thread));
	m_Threads.erase(std::remove_if(ALL_RANGE(m_Threads), std::mem_fn(&Thread::Signaled)), m_Threads.end());
}

config_provider& ConfigProvider()
{
	return *Global->m_ConfigProvider;
}