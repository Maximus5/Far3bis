﻿/*
cache.cpp

Кеширование записи в файл/чтения из файла
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

#include "headers.hpp"
#pragma hdrstop

#include "cache.hpp"

CachedRead::CachedRead(os::fs::file& file, size_t buffer_size):
	file(file),
	ReadSize(0),
	BytesLeft(0),
	LastPtr(0),
	Alignment(512),
	Buffer(buffer_size? ALIGNAS(buffer_size, 512) : 65536)
{
}

CachedRead::~CachedRead()
{
}

void CachedRead::AdjustAlignment()
{
	if (!file)
		return;

	auto buff_size = Buffer.size();

	STORAGE_PROPERTY_QUERY q;
	DWORD ret = 0;
	STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR a = {0};

	q.QueryType  = PropertyStandardQuery;
	q.PropertyId = StorageAccessAlignmentProperty;

	if (file.IoControl(IOCTL_STORAGE_QUERY_PROPERTY, &q,sizeof(q), &a,sizeof(a), &ret, nullptr))
	{
		if (a.BytesPerPhysicalSector > 512 && a.BytesPerPhysicalSector <= 256*1024)
		{
			Alignment = (int)a.BytesPerPhysicalSector;
			buff_size = 16 * a.BytesPerPhysicalSector;
		}
		file.IoControl(FSCTL_ALLOW_EXTENDED_DASD_IO, nullptr,0, nullptr,0, &ret,nullptr);
	}

	if (buff_size > Buffer.size())
	{
		Buffer.resize(buff_size);
	}

	Clear();
}

void CachedRead::Clear()
{
	ReadSize=0;
	BytesLeft=0;
	LastPtr=0;
}

bool CachedRead::Read(void* Data, size_t DataSize, size_t* BytesRead)
{
	const auto Ptr = file.GetPointer();

	if(Ptr!=LastPtr)
	{
		const auto MaxValidPtr=LastPtr+BytesLeft, MinValidPtr=MaxValidPtr-ReadSize;
		if(Ptr>=MinValidPtr && Ptr<MaxValidPtr)
		{
			BytesLeft-=static_cast<int>(Ptr-LastPtr);
		}
		else
		{
			BytesLeft=0;
		}
		LastPtr=Ptr;
	}
	bool Result=false;
	*BytesRead=0;
	if(DataSize<=Buffer.size())
	{
		while (DataSize)
		{
			if (!BytesLeft)
			{
				FillBuffer();

				if (!BytesLeft)
					break;
			}

			Result=true;

			const auto Actual = std::min(BytesLeft, DataSize);
			memcpy(Data, &Buffer[ReadSize-BytesLeft], Actual);
			Data=((LPBYTE)Data)+Actual;
			BytesLeft-=Actual;
			file.SetPointer(Actual, &LastPtr, FILE_CURRENT);
			*BytesRead+=Actual;
			DataSize-=Actual;
		}
	}
	else
	{
		Result = file.Read(Data, DataSize, *BytesRead);
	}
	return Result;
}

bool CachedRead::Unread(size_t BytesUnread)
{
	if (BytesUnread + BytesLeft <= ReadSize)
	{
		BytesLeft += BytesUnread;
		const __int64 off = BytesUnread;
		file.SetPointer(-off, &LastPtr, FILE_CURRENT);
		return true;
	}
	return false;
}

bool CachedRead::FillBuffer()
{
	bool Result=false;
	if (!file.Eof())
	{
		const auto Pointer = file.GetPointer();

		int shift = (int)(Pointer % Alignment);
		if (Pointer-shift > Buffer.size()/2)
			shift += static_cast<int>(Buffer.size() / 2);

		if (shift)
			file.SetPointer(-shift, nullptr, FILE_CURRENT);

		size_t read_size = Buffer.size();
		UINT64 FileSize = 0;
		if (file.GetSize(FileSize) && Pointer - shift + Buffer.size() > FileSize)
			read_size = FileSize - Pointer + shift;

		Result = file.Read(Buffer.data(), read_size, ReadSize);
		if (Result)
		{
			if (ReadSize > (DWORD)shift)
			{
				BytesLeft = ReadSize - shift;
				file.SetPointer(Pointer, nullptr, FILE_BEGIN);
			}
			else
			{
				BytesLeft = 0;
			}
		}
		else
		{
			if (shift)
				file.SetPointer(Pointer, nullptr, FILE_BEGIN);
			ReadSize=0;
			BytesLeft=0;
		}
	}

	return Result;
}

#if 1
//Maximus: Implement CachedWrite::WriteStr considering requested codepage
CachedWrite::CachedWrite(os::fs::file& file, uintptr_t codepage):
#else
CachedWrite::CachedWrite(os::fs::file& file):
#endif
	file(file),
	Buffer(0x10000),
	FreeSize(Buffer.size()),
	Flushed(false),
	codepage(codepage)
{
}

CachedWrite::~CachedWrite()
{
	Flush();
}

bool CachedWrite::Write(const void* Data, size_t DataSize)
{
	bool Result=false;

	bool SuccessFlush=true;
	if (DataSize>FreeSize)
	{
		SuccessFlush=Flush();
	}

	if(SuccessFlush)
	{
		if (DataSize>FreeSize)
		{
			size_t WrittenSize=0;

			if (file.Write(Data, DataSize,WrittenSize) && DataSize==WrittenSize)
			{
				Result=true;
			}
		}
		else
		{
			memcpy(&Buffer[Buffer.size() - FreeSize], Data, DataSize);
			FreeSize -= DataSize;
			Flushed=false;
			Result=true;
		}
	}
	return Result;
}

bool CachedWrite::WriteStr(const wchar_t* SaveStr, size_t Length)
{
	if (!Length)
		return true;

	if (codepage == CP_UNICODE)
	{
		return Write(SaveStr, Length*sizeof(wchar_t));
	}
	else
	{
		auto SwapBytes = [](const wchar_t* src, char* dst, size_t count)
		{
			return _swab(reinterpret_cast<char*>(const_cast<wchar_t*>(src)), dst, static_cast<int>(count));
		};

		DWORD length = (codepage == CP_REVERSEBOM?static_cast<DWORD>(Length*sizeof(wchar_t)):WideCharToMultiByte(codepage, 0, SaveStr, Length, nullptr, 0, nullptr, nullptr));
		char_ptr SaveStrCopy(length);

		if (SaveStrCopy)
		{
			if (codepage == CP_REVERSEBOM)
				SwapBytes(SaveStr, SaveStrCopy.get(), length);
			else
				WideCharToMultiByte(codepage, 0, SaveStr, Length, SaveStrCopy.get(), length, nullptr, nullptr);

			return Write(SaveStrCopy.get(),length);
		}
	}
	return false;
}

bool CachedWrite::Flush()
{
	if (!Flushed)
	{
		size_t WrittenSize = 0;

		if (file.Write(Buffer.data(), Buffer.size() - FreeSize, WrittenSize, nullptr) && Buffer.size() - FreeSize == WrittenSize)
		{
			Flushed=true;
			FreeSize = Buffer.size();
		}
	}

	return Flushed;
}
