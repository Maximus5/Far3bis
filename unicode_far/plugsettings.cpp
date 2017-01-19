﻿/*
plugsettings.cpp

API для хранения плагинами настроек.
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

#include "plugsettings.hpp"
#include "ctrlobj.hpp"
#include "history.hpp"
#include "datetime.hpp"
#include "FarGuid.hpp"
#include "shortcuts.hpp"
#include "dizlist.hpp"
#include "config.hpp"
#include "pathmix.hpp"
#include "plugins.hpp"
#include "sqlitedb.hpp"

const wchar_t* AbstractSettings::Add(const string& String)
{
	return static_cast<const wchar_t*>(Add(String.data(), (String.size() + 1) * sizeof(wchar_t)));
}

const void* AbstractSettings::Add(const void* Data, size_t Size)
{
	return memcpy(Allocate(Size), Data, Size);
}

void* AbstractSettings::Allocate(size_t Size)
{
	m_Data.emplace_back(Size);
	return m_Data.back().get();
}

class PluginSettings: public AbstractSettings
{
public:
	PluginSettings(const GUID& Guid, bool Local);
	virtual bool IsValid() const override;
	virtual bool Set(const FarSettingsItem& Item) override;
	virtual bool Get(FarSettingsItem& Item) override;
	virtual bool Enum(FarSettingsEnum& Enum) override;
	virtual bool Delete(const FarSettingsValue& Value) override;
	virtual int SubKey(const FarSettingsValue& Value, bool bCreate) override;

	class FarSettingsNameItems;

private:
	std::vector<FarSettingsNameItems> m_Enum;
	std::vector<HierarchicalConfig::key> m_Keys;
	HierarchicalConfigUniquePtr PluginsCfg;
};


AbstractSettings* AbstractSettings::CreatePluginSettings(const GUID& Guid, bool Local)
{
	return new PluginSettings(Guid, Local);
}


PluginSettings::PluginSettings(const GUID& Guid, bool Local)
{
	const auto pPlugin = Global->CtrlObject->Plugins->FindPlugin(Guid);
	if (!pPlugin)
		return;

	const auto strGuid = GuidToStr(Guid);
	PluginsCfg = ConfigProvider().CreatePluginsConfig(strGuid, Local);
	m_Keys.emplace_back(PluginsCfg->CreateKey(HierarchicalConfig::root_key(), strGuid, &pPlugin->GetTitle()));

	if (Global->Opt->ReadOnlyConfig)
	{
		DizList Diz;
		auto DbPath = Local? Global->Opt->LocalProfilePath : Global->Opt->ProfilePath;
		AddEndSlash(DbPath);
		DbPath += L"PluginsData\\";
		Diz.Read(DbPath);
		const auto DbName = strGuid + L".db";
		const auto Description = concat(pPlugin->GetTitle(), L" (", pPlugin->GetDescription(), L')');
		if (Description != NullToEmpty(Diz.Get(DbName, L"", 0)))
		{
			Diz.Set(DbName, L"", Description);
			Diz.Flush(DbPath);
		}
	}
}

bool PluginSettings::IsValid() const
{
	return !m_Keys.empty();
}

bool PluginSettings::Set(const FarSettingsItem& Item)
{
	if (Item.Root >= m_Keys.size())
		return false;

	switch(Item.Type)
	{
	case FST_SUBKEY:
		return false;

	case FST_QWORD:
		return PluginsCfg->SetValue(m_Keys[Item.Root], Item.Name, Item.Number);

	case FST_STRING:
		return PluginsCfg->SetValue(m_Keys[Item.Root], Item.Name, Item.String);

	case FST_DATA:
		return PluginsCfg->SetValue(m_Keys[Item.Root], Item.Name, make_blob_view(Item.Data.Data, Item.Data.Size));

	default:
		return false;
	}
}

bool PluginSettings::Get(FarSettingsItem& Item)
{
	if (Item.Root >= m_Keys.size())
		return false;

	switch(Item.Type)
	{
	case FST_SUBKEY:
		return false;

	case FST_QWORD:
		{
			unsigned long long value;
			if (PluginsCfg->GetValue(m_Keys[Item.Root], Item.Name, value))
			{
				Item.Number = value;
				return true;
			}
		}
		break;

	case FST_STRING:
		{
			string data;
			if (PluginsCfg->GetValue(m_Keys[Item.Root], Item.Name, data))
			{
				Item.String = Add(data);
				return true;
			}
		}
		break;

	case FST_DATA:
		{
			writable_blob_view data;
			if (PluginsCfg->GetValue(m_Keys[Item.Root], Item.Name, data))
			{
				Item.Data.Data = Add(data.data(), data.size());
				Item.Data.Size = data.size();
				return true;
			}
		}
		break;

	default:
		return false;
	}

	return false;
}

static wchar_t* AddString(const string& String)
{
	auto result = std::make_unique<wchar_t[]>(String.size() + 1);
	*std::copy(ALL_CONST_RANGE(String), result.get()) = L'\0';
	return result.release();
}

class PluginSettings::FarSettingsNameItems
{
public:
	NONCOPYABLE(FarSettingsNameItems);
	TRIVIALLY_MOVABLE(FarSettingsNameItems);

	FarSettingsNameItems() = default;
	~FarSettingsNameItems()
	{
		std::for_each(CONST_RANGE(Items, i)
		{
			delete[] i.Name;
		});
	}

	void add(FarSettingsName& Item, const string& String);

	void get(FarSettingsEnum& e) const
	{
		e.Count = Items.size();
		e.Items = e.Count ? Items.data() : nullptr;
	}

private:
	std::vector<FarSettingsName> Items;
};

void PluginSettings::FarSettingsNameItems::add(FarSettingsName& Item, const string& String)
{
	Item.Name=AddString(String);
	Items.emplace_back(Item);
}

class FarSettings: public AbstractSettings
{
public:
	virtual bool IsValid() const override { return true; }
	virtual bool Set(const FarSettingsItem& Item) override;
	virtual bool Get(FarSettingsItem& Item) override;
	virtual bool Enum(FarSettingsEnum& Enum) override;
	virtual bool Delete(const FarSettingsValue& Value) override;
	virtual int SubKey(const FarSettingsValue& Value, bool bCreate) override;

	class FarSettingsHistoryItems;

private:
	bool FillHistory(int Type, const string& HistoryName, FarSettingsEnum& Enum, const std::function<bool(history_record_type)>& Filter);
	std::vector<FarSettingsHistoryItems> m_Enum;
	std::vector<string> m_Keys;
};


AbstractSettings* AbstractSettings::CreateFarSettings()
{
	return new FarSettings();
}


class FarSettings::FarSettingsHistoryItems
{
public:
	NONCOPYABLE(FarSettingsHistoryItems);
	TRIVIALLY_MOVABLE(FarSettingsHistoryItems);

	FarSettingsHistoryItems() = default;
	~FarSettingsHistoryItems()
	{
		std::for_each(CONST_RANGE(Items, i)
		{
			delete[] i.Name;
			delete[] i.Param;
			delete[] i.File;
		});
	}

	void add(FarSettingsHistory& Item, const string& Name, const string& Param, const GUID& Guid, const string& File);

	void get(FarSettingsEnum& e) const
	{
		e.Count = Items.size();
		e.Histories = e.Count ? Items.data() : nullptr;
	}

private:
	std::vector<FarSettingsHistory> Items;
};

void FarSettings::FarSettingsHistoryItems::add(FarSettingsHistory& Item, const string& Name, const string& Param, const GUID& Guid, const string& File)
{
	Item.Name=AddString(Name);
	Item.Param=AddString(Param);
	Item.PluginId=Guid;
	Item.File=AddString(File);
	Items.emplace_back(Item);
}

bool PluginSettings::Enum(FarSettingsEnum& Enum)
{
	if (Enum.Root >= m_Keys.size())
		return false;

	FarSettingsName item;
	DWORD Index = 0;
	string strName;

	const auto& root = m_Keys[Enum.Root];
	item.Type=FST_SUBKEY;
	FarSettingsNameItems NewEnumItem;
	while (PluginsCfg->EnumKeys(root, Index++, strName))
	{
		NewEnumItem.add(item, strName);
	}

	Index=0;
	int Type;
	while (PluginsCfg->EnumValues(root, Index++, strName, Type))
	{
		switch (static_cast<SQLiteDb::column_type>(Type))
		{
		case SQLiteDb::column_type::integer:
			item.Type = FST_QWORD;
			break;

		case SQLiteDb::column_type::string:
			item.Type = FST_STRING;
			break;

		case SQLiteDb::column_type::blob:
			item.Type = FST_DATA;
			break;

		case SQLiteDb::column_type::unknown:
		default:
			item.Type = FST_UNKNOWN;
			break;
		}
		if(item.Type!=FST_UNKNOWN)
		{
			NewEnumItem.add(item, strName);
		}
	}
	NewEnumItem.get(Enum);
	m_Enum.emplace_back(std::move(NewEnumItem));

	return true;
}

bool PluginSettings::Delete(const FarSettingsValue& Value)
{
	if (Value.Root >= m_Keys.size())
		return false;

	return Value.Value?
		PluginsCfg->DeleteValue(m_Keys[Value.Root], Value.Value) :
		PluginsCfg->DeleteKeyTree(m_Keys[Value.Root]);
}

int PluginSettings::SubKey(const FarSettingsValue& Value, bool bCreate)
{
	//Don't allow illegal key names - empty names or with backslashes
	if (Value.Root >= m_Keys.size() || !Value.Value || !*Value.Value || wcschr(Value.Value, '\\'))
		return 0;

	const auto root = bCreate? PluginsCfg->CreateKey(m_Keys[Value.Root], Value.Value) : PluginsCfg->FindByName(m_Keys[Value.Root], Value.Value);
	if (!root)
		return 0;

	m_Keys.emplace_back(root);
	return static_cast<int>(m_Keys.size() - 1);
}

bool FarSettings::Set(const FarSettingsItem& Item)
{
	return false;
}

bool FarSettings::Get(FarSettingsItem& Item)
{
	const auto Data = Global->Opt->GetConfigValue(Item.Root, Item.Name);
	if (!Data)
		return false;

	Data->Export(Item);

	if (Item.Type == FST_STRING)
		Item.String = Add(Item.String);

	return true;
}

bool FarSettings::Enum(FarSettingsEnum& Enum)
{
	const auto& FilterNone = [](history_record_type) { return true; };

	switch(Enum.Root)
	{
	case FSSF_HISTORY_CMD:
		return FillHistory(HISTORYTYPE_CMD,L"", Enum, FilterNone);
	
	case FSSF_HISTORY_FOLDER:
		return FillHistory(HISTORYTYPE_FOLDER, L"", Enum, FilterNone);
	
	case FSSF_HISTORY_VIEW:
		return FillHistory(HISTORYTYPE_VIEW, L"", Enum, [](history_record_type Type) { return Type == HR_VIEWER; });
	
	case FSSF_HISTORY_EDIT:
		return FillHistory(HISTORYTYPE_VIEW, L"", Enum, [](history_record_type Type) { return Type == HR_EDITOR || Type == HR_EDITOR_RO; });
	
	case FSSF_HISTORY_EXTERNAL:
		return FillHistory(HISTORYTYPE_VIEW, L"", Enum, [](history_record_type Type) { return Type == HR_EXTERNAL || Type == HR_EXTERNAL_WAIT; });
	
	case FSSF_FOLDERSHORTCUT_0:
	case FSSF_FOLDERSHORTCUT_1:
	case FSSF_FOLDERSHORTCUT_2:
	case FSSF_FOLDERSHORTCUT_3:
	case FSSF_FOLDERSHORTCUT_4:
	case FSSF_FOLDERSHORTCUT_5:
	case FSSF_FOLDERSHORTCUT_6:
	case FSSF_FOLDERSHORTCUT_7:
	case FSSF_FOLDERSHORTCUT_8:
	case FSSF_FOLDERSHORTCUT_9:
		{
			FarSettingsHistory item{};
			string strName,strFile,strData;
			GUID plugin; size_t index=0;
			FarSettingsHistoryItems NewEnumItem;
			while (Shortcuts().Get(Enum.Root - FSSF_FOLDERSHORTCUT_0, index++, &strName, &plugin, &strFile, &strData))
			{
				NewEnumItem.add(item, strName, strData, plugin, strFile);
			}
			NewEnumItem.get(Enum);
			m_Enum.emplace_back(std::move(NewEnumItem));
			return true;
		}

	default:
		if(Enum.Root >= FSSF_COUNT)
		{
			size_t root = Enum.Root - FSSF_COUNT;
			if(root < m_Keys.size())
			{
				return FillHistory(HISTORYTYPE_DIALOG, m_Keys[root], Enum, FilterNone);
			}
		}
		return false;
	}
}

bool FarSettings::Delete(const FarSettingsValue& Value)
{
	return false;
}

int FarSettings::SubKey(const FarSettingsValue& Value, bool bCreate)
{
	if (bCreate || Value.Root != FSSF_ROOT)
		return 0;

	m_Keys.emplace_back(Value.Value);
	return static_cast<int>(m_Keys.size() - 1 + FSSF_COUNT);
}

static const auto& HistoryRef(int Type)
{
	const auto& IsSave = [](int Type) -> bool
	{
		switch (Type)
		{
		case HISTORYTYPE_CMD: return Global->Opt->SaveHistory;
		case HISTORYTYPE_FOLDER: return Global->Opt->SaveFoldersHistory;
		case HISTORYTYPE_VIEW: return Global->Opt->SaveViewHistory;
		case HISTORYTYPE_DIALOG: return Global->Opt->Dialogs.EditHistory;
		default: return true;
		}
	};

	return IsSave(Type)? ConfigProvider().HistoryCfg() : ConfigProvider().HistoryCfgMem();
}

bool FarSettings::FillHistory(int Type,const string& HistoryName,FarSettingsEnum& Enum, const std::function<bool(history_record_type)>& Filter)
{
	FarSettingsHistory item = {};
	DWORD Index=0;
	string strName,strGuid,strFile,strData;

	unsigned long long id;
	history_record_type HType;
	bool HLock;
	unsigned long long Time;
	FarSettingsHistoryItems NewEnumItem;
	while (HistoryRef(Type)->Enum(Index++, Type, HistoryName, &id, strName, &HType, &HLock, &Time, strGuid, strFile, strData))
	{
		if(Filter(HType))
		{
			item.Time = UI64ToFileTime(Time);
			item.Lock=HLock;
			GUID Guid;
			if(strGuid.empty()||!StrToGuid(strGuid,Guid)) Guid=FarGuid;
			NewEnumItem.add(item, strName, strData, Guid, strFile);
		}
	}
	NewEnumItem.get(Enum);
	m_Enum.emplace_back(std::move(NewEnumItem));
	return true;
}
