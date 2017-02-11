﻿/*
farwinapi.cpp

Враперы вокруг некоторых WinAPI функций
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

#include "headers.hpp"
#pragma hdrstop

#include "flink.hpp"
#include "imports.hpp"
#include "pathmix.hpp"
#include "ctrlobj.hpp"
#include "elevation.hpp"
#include "plugins.hpp"
#include "datetime.hpp"
#include "strmix.hpp"
#include "lasterror.hpp"
#include "local.hpp"
#include "cvtname.hpp"

namespace os
{
	template<typename T>
	bool ApiDynamicStringReceiver(string& Destination, const T& Callable)
	{
		wchar_t Buffer[MAX_PATH];
		auto Size = Callable(Buffer, std::size(Buffer));
		if (!Size)
			return false;

		if (Size < std::size(Buffer))
		{
			Destination.assign(Buffer, Size);
			return true;
		}

		for (;;)
		{
			wchar_t_ptr vBuffer(Size);
			Size = Callable(vBuffer.get(), vBuffer.size());
			if (!Size)
				return false;
			if (Size < vBuffer.size())
			{
				Destination.assign(vBuffer.get(), Size);
				return true;
			}
		}
	}

	template<typename T>
	bool ApiDynamicErrorBasedStringReceiver(DWORD ExpectedErrorCode, string& Destination, const T& Callable)
	{
		wchar_t Buffer[MAX_PATH];
		if (const auto Size = Callable(Buffer, std::size(Buffer)))
		{
			Destination.assign(Buffer, Size);
			return true;
		}

		auto BufferSize = std::size(Buffer);
		while (GetLastError() == ExpectedErrorCode)
		{
			wchar_t_ptr vBuffer(BufferSize *= 2);
			if (const auto Size = Callable(vBuffer.get(), vBuffer.size()))
			{
				Destination.assign(vBuffer.get(), Size);
				return true;
			}
		}
		return false;
	}

	namespace detail
	{
		class i_find_handle_impl
		{
		public:
			virtual ~i_find_handle_impl() = 0;
		};
		i_find_handle_impl::~i_find_handle_impl() = default;

		void handle_closer::operator()(HANDLE Handle) const { CloseHandle(Handle); }
		void find_handle_closer::operator()(HANDLE Handle) const { FindClose(Handle); }
		void find_file_handle_closer::operator()(HANDLE Handle) const { delete static_cast<i_find_handle_impl*>(Handle); }
		void find_volume_handle_closer::operator()(HANDLE Handle) const { FindVolumeClose(Handle); }
		void find_notification_handle_closer::operator()(HANDLE Handle) const { FindCloseChangeNotification(Handle); }
		void printer_handle_closer::operator()(HANDLE Handle) const { ClosePrinter(Handle); }

		class far_find_handle_impl: public i_find_handle_impl
		{
		public:
			fs::file Object;
			block_ptr<BYTE> BufferBase;
			block_ptr<BYTE> Buffer2;
			ULONG NextOffset {};
			bool Extended {};
			bool ReadDone {};
		};

		class os_find_handle_impl: public i_find_handle_impl, public handle_t<find_handle_closer>
		{
			using handle_t<find_handle_closer>::handle_t;
		};
	}

static void DirectoryInfoToFindData(const FILE_ID_BOTH_DIR_INFORMATION& DirectoryInfo, FAR_FIND_DATA& FindData, bool IsExtended)
{
	FindData.dwFileAttributes = DirectoryInfo.FileAttributes;
	FindData.ftCreationTime = UI64ToFileTime(DirectoryInfo.CreationTime);
	FindData.ftLastAccessTime = UI64ToFileTime(DirectoryInfo.LastAccessTime);
	FindData.ftLastWriteTime = UI64ToFileTime(DirectoryInfo.LastWriteTime);
	FindData.ftChangeTime = UI64ToFileTime(DirectoryInfo.ChangeTime);
	FindData.nFileSize = DirectoryInfo.EndOfFile.QuadPart;
	FindData.nAllocationSize = DirectoryInfo.AllocationSize.QuadPart;
	FindData.dwReserved0 = FindData.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT? DirectoryInfo.EaSize : 0;

	const auto& CopyNames = [&FindData](const auto& DirInfo)
	{
		FindData.strFileName.assign(DirInfo.FileName, DirInfo.FileNameLength / sizeof(wchar_t));
		FindData.strAlternateFileName.assign(DirInfo.ShortName, DirInfo.ShortNameLength / sizeof(wchar_t));
	};

	if (IsExtended)
	{
		FindData.FileId = DirectoryInfo.FileId.QuadPart;
		CopyNames(DirectoryInfo);
	}
	else
	{
		FindData.FileId = 0;
		CopyNames(reinterpret_cast<const FILE_BOTH_DIR_INFORMATION&>(DirectoryInfo));
	}

	const auto& RemoveTrailingZeros = [](string& Where)
	{
		Where.resize(Where.find_last_not_of(L'\0') + 1);
	};

	// MSDN verse 2.4.17:
	// "When working with this field, use FileNameLength to determine the length of the file name
	// rather than assuming the presence of a trailing null delimiter."

	// Some buggy implementations (e. g. ms sharepoint, rdesktop) set the length incorrectly
	// (e. g. including the terminating \0 or as ((number of bytes in the source string) * 2) when source is in UTF-8),
	// so instead of, say, "name" (4) they return "name\0\0\0\0" (8).
	// Generally speaking, it's their own problems and we shall use it as is, as per the verse above.
	// However, most of the applications use FindFirstFile API, which copies this string
	// to the fixed-size buffer WIN32_FIND_DATA.cFileName, leaving the burden of finding its length
	// to the application itself, which, by coincidence, does it correctly, effectively masking the initial error.
	// So people come to us and claim that Far isn't working properly while other programs are fine.

	RemoveTrailingZeros(FindData.strFileName);
	RemoveTrailingZeros(FindData.strAlternateFileName);
}

static auto FindFirstFileInternal(const string& Name, FAR_FIND_DATA& FindData)
{
	if (Name.empty() || IsSlash(Name.back()))
		return find_file_handle{};

	auto Handle = std::make_unique<detail::far_find_handle_impl>();

	auto strDirectory(Name);
	CutToSlash(strDirectory);

	const auto& OpenDirectory = [&] { return Handle->Object.Open(strDirectory, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING); };

	if (!OpenDirectory())
	{
		// fix error code if we are looking for FILE(S) in non-existent directory, not directory itself
		if (GetLastError() == ERROR_FILE_NOT_FOUND && *PointToName(Name))
		{
			SetLastError(ERROR_PATH_NOT_FOUND);
		}
		return find_file_handle{};
	}

	// for network paths buffer size must be <= 64k
	Handle->BufferBase.reset(0x10000);

	const auto NamePtr = PointToName(Name);
	Handle->Extended = true;

	bool QueryResult = Handle->Object.NtQueryDirectoryFile(Handle->BufferBase.get(), Handle->BufferBase.size(), FileIdBothDirectoryInformation, false, NamePtr, true);
	if (QueryResult) // try next read immediately to avoid M#2128 bug
	{
		block_ptr<BYTE> Buffer2(Handle->BufferBase.size());
		if (Handle->Object.NtQueryDirectoryFile(Buffer2.get(), Buffer2.size(), FileIdBothDirectoryInformation, false, NamePtr, false))
		{
			Handle->Buffer2 = std::move(Buffer2);
		}
		else
		{
			if (GetLastError() != ERROR_INVALID_LEVEL)
				Handle->ReadDone = true;
			else
				QueryResult = false;
		}
	}

	if (!QueryResult)
	{
		Handle->Extended = false;

		// re-create handle to avoid weird bugs with some network emulators
		Handle->Object.Close();
		if (OpenDirectory())
		{
			QueryResult = Handle->Object.NtQueryDirectoryFile(Handle->BufferBase.get(), Handle->BufferBase.size(), FileBothDirectoryInformation, false, NamePtr, true);
		}
	}

	if (!QueryResult)
		return find_file_handle{};

	const auto DirectoryInfo = reinterpret_cast<const FILE_ID_BOTH_DIR_INFORMATION*>(Handle->BufferBase.get());
	DirectoryInfoToFindData(*DirectoryInfo, FindData, Handle->Extended);
	Handle->NextOffset = DirectoryInfo->NextEntryOffset;
	return find_file_handle(Handle.release());
}

static bool FindNextFileInternal(const find_file_handle& Find, FAR_FIND_DATA& FindData)
{
	bool Result = false;
	const auto Handle = static_cast<detail::far_find_handle_impl*>(Find.native_handle());
	bool Status = true, set_errcode = true;
	auto DirectoryInfo = reinterpret_cast<const FILE_ID_BOTH_DIR_INFORMATION*>(Handle->BufferBase.get());
	if(Handle->NextOffset)
	{
		DirectoryInfo = reinterpret_cast<const FILE_ID_BOTH_DIR_INFORMATION*>(reinterpret_cast<const char*>(DirectoryInfo)+Handle->NextOffset);
	}
	else
	{
		if (Handle->ReadDone)
		{
			Status = false;
		}
		else
		{
			if (Handle->Buffer2)
			{
				Handle->BufferBase.reset();
				using std::swap;
				swap(Handle->BufferBase, Handle->Buffer2);
				DirectoryInfo = reinterpret_cast<const FILE_ID_BOTH_DIR_INFORMATION*>(Handle->BufferBase.get());
			}
			else
			{
				Status = Handle->Object.NtQueryDirectoryFile(Handle->BufferBase.get(), Handle->BufferBase.size(), Handle->Extended ? FileIdBothDirectoryInformation : FileBothDirectoryInformation, false, nullptr, false);
				set_errcode = false;
			}
		}
	}

	if(Status)
	{
		DirectoryInfoToFindData(*DirectoryInfo, FindData, Handle->Extended);
		Handle->NextOffset = DirectoryInfo->NextEntryOffset? Handle->NextOffset + DirectoryInfo->NextEntryOffset : 0;
		Result = true;
	}

	if (set_errcode)
		SetLastError(Result ? ERROR_SUCCESS : ERROR_NO_MORE_FILES);

	return Result;
}

//-------------------------------------------------------------------------
namespace fs
{
	file_status::file_status():
		m_Data(INVALID_FILE_ATTRIBUTES)
	{
	}

	file_status::file_status(const string& Object) :
		file_status(Object.data())
	{
	}

	file_status::file_status(const wchar_t* Object):
		m_Data(os::GetFileAttributes(Object))
	{
	}

	bool file_status::check(DWORD Data) const
	{
		return m_Data != INVALID_FILE_ATTRIBUTES && m_Data & Data;
	}

	bool exists(file_status Status)
	{
		return Status.check(~0);
	}

	bool is_file(file_status Status)
	{
		return exists(Status) && !is_directory(Status);
	}

	bool is_directory(file_status Status)
	{
		return Status.check(FILE_ATTRIBUTE_DIRECTORY);
	}

	bool is_not_empty_directory(const string& Object)
	{
		enum_file Find(Object + L"\\*");
		return Find.begin() != Find.end();
	}

enum_file::enum_file(const string& Object, bool ScanSymLink):
	m_Object(NTPath(Object)),
	m_ScanSymLink(ScanSymLink)
{
	bool Root = false;
	const auto Type = ParsePath(m_Object, nullptr, &Root);
	if(Root && (Type == PATH_DRIVELETTER || Type == PATH_DRIVELETTERUNC || Type == PATH_VOLUMEGUID))
	{
		AddEndSlash(m_Object);
	}
	else
	{
		DeleteEndSlash(m_Object);
	}
}

bool enum_file::get(size_t index, value_type& FindData) const
{
	bool Result = false;
	if (!index)
	{
		// temporary disable elevation to try "real" name first
		{
			SCOPED_ACTION(elevation::suppress);
			m_Handle = FindFirstFileInternal(m_Object, FindData);
		}

		if (!m_Handle && GetLastError() == ERROR_ACCESS_DENIED)
		{
			if(m_ScanSymLink)
			{
				string strReal(m_Object);
				// only links in path should be processed, not the object name itself
				CutToSlash(strReal);
				strReal = ConvertNameToReal(strReal);
				AddEndSlash(strReal);
				strReal+=PointToName(m_Object);
				strReal = NTPath(strReal);
				m_Handle = FindFirstFileInternal(strReal, FindData);
			}

			if (!m_Handle && ElevationRequired(ELEVATION_READ_REQUEST))
			{
				m_Handle = FindFirstFileInternal(m_Object, FindData);
			}
		}
		Result = m_Handle? true : false;
	}
	else
	{
		if (m_Handle)
		{
			Result = FindNextFileInternal(m_Handle, FindData);
		}
	}

	// skip ".." & "."
	if(Result && FindData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY && FindData.strFileName[0] == L'.' &&
		((FindData.strFileName.size() == 2 && FindData.strFileName[1] == L'.') || FindData.strFileName.size() == 1) &&
		// хитрый способ - у виртуальных папок не бывает SFN, в отличие от. (UPD: или бывает, но такое же)
		(FindData.strAlternateFileName.empty() || FindData.strAlternateFileName == FindData.strFileName))
	{
		// index not important here, anything but 0 is fine
		Result = get(1, FindData);
	}
	return Result;
}

//-------------------------------------------------------------------------

bool enum_name::get(size_t index, value_type& value) const
{
	if (!index)
	{
		m_Handle = FindFirstFileName(m_Object, 0, value);
		return m_Handle? true : false;
	}
	return FindNextFileName(m_Handle, value);
}

//-------------------------------------------------------------------------

bool enum_stream::get(size_t index, value_type& value) const
{
	if (!index)
	{
		m_Handle = FindFirstStream(m_Object, FindStreamInfoStandard, &value);
		return m_Handle? true : false;
	}
	return FindNextStream(m_Handle, &value);
}

//-------------------------------------------------------------------------

bool enum_volume::get(size_t index, value_type& value) const
{
	wchar_t VolumeName[50];
	if (!index)
	{
		m_Handle.reset(FindFirstVolume(VolumeName, static_cast<DWORD>(std::size(VolumeName))));
		if (!m_Handle)
		{
			return false;
		}
	}
	else
	{
		if (!FindNextVolume(m_Handle.native_handle(), VolumeName, static_cast<DWORD>(std::size(VolumeName))))
		{
			return false;
		}
	}
	value = VolumeName;
	return true;
}

//-------------------------------------------------------------------------

bool file::Open(const string& Object, DWORD DesiredAccess, DWORD ShareMode, LPSECURITY_ATTRIBUTES SecurityAttributes, DWORD CreationDistribution, DWORD FlagsAndAttributes, const file* TemplateFile, bool ForceElevation)
{
	assert(!m_Handle);

	m_Pointer = 0;
	m_NeedSyncPointer = false;

	const auto TemplateFileHandle = TemplateFile? TemplateFile->m_Handle.native_handle() : nullptr;
	m_Handle = CreateFile(Object, DesiredAccess, ShareMode, SecurityAttributes, CreationDistribution, FlagsAndAttributes, TemplateFileHandle, ForceElevation);
	if (m_Handle)
	{
		m_Name = Object;
		m_ShareMode = ShareMode;
	}
	else
	{
		m_Name.clear();
		m_ShareMode = 0;
	}
	return m_Handle? true : false;
}

void file::SyncPointer() const
{
	if(m_NeedSyncPointer)
	{
		LARGE_INTEGER Distance, NewPointer;
		Distance.QuadPart = m_Pointer;
		if (SetFilePointerEx(m_Handle.native_handle(), Distance, &NewPointer, FILE_BEGIN))
		{
			m_Pointer = NewPointer.QuadPart;
			m_NeedSyncPointer = false;
		}
	}
}


bool file::Read(LPVOID Buffer, size_t NumberOfBytesToRead, size_t& NumberOfBytesRead, LPOVERLAPPED Overlapped) const
{
	assert(NumberOfBytesToRead <= std::numeric_limits<DWORD>::max());

	SyncPointer();
	DWORD BytesRead = 0;
	bool Result = ReadFile(m_Handle.native_handle(), Buffer, static_cast<DWORD>(NumberOfBytesToRead), &BytesRead, Overlapped) != FALSE;
	NumberOfBytesRead = BytesRead;
	if(Result)
	{
		m_Pointer += NumberOfBytesRead;
	}
	return Result;
}

bool file::Write(LPCVOID Buffer, size_t NumberOfBytesToWrite, size_t& NumberOfBytesWritten, LPOVERLAPPED Overlapped) const
{
	assert(NumberOfBytesToWrite <= std::numeric_limits<DWORD>::max());

	SyncPointer();
	DWORD BytesWritten = 0;
	bool Result = WriteFile(m_Handle.native_handle(), Buffer, static_cast<DWORD>(NumberOfBytesToWrite), &BytesWritten, Overlapped) != FALSE;
	NumberOfBytesWritten = BytesWritten;
	if(Result)
	{
		m_Pointer += NumberOfBytesWritten;
	}
	return Result;
}

bool file::SetPointer(long long DistanceToMove, unsigned long long* NewFilePointer, DWORD MoveMethod) const
{
	const auto OldPointer = m_Pointer;
	switch (MoveMethod)
	{
	case FILE_BEGIN:
		m_Pointer = DistanceToMove;
		break;
	case FILE_CURRENT:
		m_Pointer+=DistanceToMove;
		break;
	case FILE_END:
		{
			unsigned long long Size = 0;
			GetSize(Size);
			m_Pointer = Size+DistanceToMove;
		}
		break;
	}
	if(OldPointer != m_Pointer)
	{
		m_NeedSyncPointer = true;
	}
	if(NewFilePointer)
	{
		*NewFilePointer = m_Pointer;
	}
	return true;
}

bool file::SetEnd()
{
	SyncPointer();
	bool ok = SetEndOfFile(m_Handle.native_handle()) != FALSE;
	if (!ok && !m_Name.empty() && GetLastError() == ERROR_INVALID_PARAMETER) // OSX buggy SMB workaround
	{
		const auto fsize = GetPointer();
		Close();
		if (Open(m_Name, GENERIC_WRITE, m_ShareMode, nullptr, OPEN_EXISTING, 0))
		{
			SetPointer(fsize, nullptr, FILE_BEGIN);
			SyncPointer();
			ok = SetEndOfFile(m_Handle.native_handle()) != FALSE;
		}
	}
	return ok;
}

bool file::GetTime(LPFILETIME CreationTime, LPFILETIME LastAccessTime, LPFILETIME LastWriteTime, LPFILETIME ChangeTime) const
{
	FILE_BASIC_INFORMATION fbi;
	if (!NtQueryInformationFile(&fbi, sizeof fbi, FileBasicInformation))
		return false;

	if (CreationTime)
	{
		*CreationTime = UI64ToFileTime(fbi.CreationTime);
	}
	if (LastAccessTime)
	{
		*LastAccessTime = UI64ToFileTime(fbi.LastAccessTime);
	}
	if (LastWriteTime)
	{
		*LastWriteTime = UI64ToFileTime(fbi.LastWriteTime);
	}
	if (ChangeTime)
	{
		*ChangeTime = UI64ToFileTime(fbi.ChangeTime);
	}

	return true;
}

bool file::SetTime(const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, const FILETIME* ChangeTime) const
{
	FILE_BASIC_INFORMATION fbi{};
	if (CreationTime)
	{
		fbi.CreationTime.QuadPart = FileTimeToUI64(*CreationTime);
	}
	if (LastAccessTime)
	{
		fbi.LastAccessTime.QuadPart = FileTimeToUI64(*LastAccessTime);
	}
	if (LastWriteTime)
	{
		fbi.LastWriteTime.QuadPart = FileTimeToUI64(*LastWriteTime);
	}
	if (ChangeTime)
	{
		fbi.ChangeTime.QuadPart = FileTimeToUI64(*ChangeTime);
	}

	IO_STATUS_BLOCK IoStatusBlock;
	const auto Status = Imports().NtSetInformationFile(m_Handle.native_handle(), &IoStatusBlock, &fbi, sizeof fbi, FileBasicInformation);
	::SetLastError(Imports().RtlNtStatusToDosError(Status));
	return Status == STATUS_SUCCESS;
}

bool file::GetSize(UINT64& Size) const
{
	return GetFileSizeEx(m_Handle.native_handle(), Size);
}

bool file::FlushBuffers() const
{
	return FlushFileBuffers(m_Handle.native_handle()) != FALSE;
}

bool file::GetInformation(BY_HANDLE_FILE_INFORMATION& info) const
{
	return GetFileInformationByHandle(m_Handle.native_handle(), &info) != FALSE;
}

bool file::IoControl(DWORD IoControlCode, LPVOID InBuffer, DWORD InBufferSize, LPVOID OutBuffer, DWORD OutBufferSize, LPDWORD BytesReturned, LPOVERLAPPED Overlapped) const
{
	return ::DeviceIoControl(m_Handle.native_handle(), IoControlCode, InBuffer, InBufferSize, OutBuffer, OutBufferSize, BytesReturned, Overlapped) != FALSE;
}

bool file::GetStorageDependencyInformation(GET_STORAGE_DEPENDENCY_FLAG Flags, ULONG StorageDependencyInfoSize, PSTORAGE_DEPENDENCY_INFO StorageDependencyInfo, PULONG SizeUsed) const
{
	DWORD Result = Imports().GetStorageDependencyInformation(m_Handle.native_handle(), Flags, StorageDependencyInfoSize, StorageDependencyInfo, SizeUsed);
	SetLastError(Result);
	return Result == ERROR_SUCCESS;
}

bool file::NtQueryDirectoryFile(PVOID FileInformation, size_t Length, FILE_INFORMATION_CLASS FileInformationClass, bool ReturnSingleEntry, LPCWSTR FileName, bool RestartScan, NTSTATUS* Status) const
{
	IO_STATUS_BLOCK IoStatusBlock;
	PUNICODE_STRING pNameString = nullptr;
	UNICODE_STRING NameString;
	if(FileName && *FileName)
	{
		NameString.Buffer = const_cast<LPWSTR>(FileName);
		NameString.Length = static_cast<USHORT>(wcslen(FileName)*sizeof(WCHAR));
		NameString.MaximumLength = NameString.Length;
		pNameString = &NameString;
	}
	const auto di = reinterpret_cast<FILE_ID_BOTH_DIR_INFORMATION*>(FileInformation);
	di->NextEntryOffset = 0xffffffffUL;

	NTSTATUS Result = Imports().NtQueryDirectoryFile(m_Handle.native_handle(), nullptr, nullptr, nullptr, &IoStatusBlock, FileInformation, static_cast<ULONG>(Length), FileInformationClass, ReturnSingleEntry, pNameString, RestartScan);
	SetLastError(Imports().RtlNtStatusToDosError(Result));
	if(Status)
	{
		*Status = Result;
	}

	return (Result == STATUS_SUCCESS) && (di->NextEntryOffset != 0xffffffffUL);
}

bool file::NtQueryInformationFile(PVOID FileInformation, size_t Length, FILE_INFORMATION_CLASS FileInformationClass, NTSTATUS* Status) const
{
	IO_STATUS_BLOCK IoStatusBlock;
	NTSTATUS Result = Imports().NtQueryInformationFile(m_Handle.native_handle(), &IoStatusBlock, FileInformation, static_cast<ULONG>(Length), FileInformationClass);
	SetLastError(Imports().RtlNtStatusToDosError(Result));
	if(Status)
	{
		*Status = Result;
	}
	return Result == STATUS_SUCCESS;
}

void file::Close()
{
	m_Handle.close();
}

bool file::Eof() const
{
	const auto Ptr = GetPointer();
	unsigned long long Size=0;
	GetSize(Size);
	return Ptr >= Size;
}
//-------------------------------------------------------------------------
struct file_walker::Chunk
{
	UINT64 Offset;
	DWORD Size;

	Chunk(UINT64 Offset, DWORD Size): Offset(Offset), Size(Size) {}
};

file_walker::file_walker():
	m_FileSize(0),
	m_AllocSize(0),
	m_ProcessedSize(0),
	m_CurrentChunk(m_ChunkList.begin()),
	m_ChunkSize(0),
	m_IsSparse(false)
{
}

file_walker::~file_walker() = default;

bool file_walker::InitWalk(size_t BlockSize)
{
	bool Result = false;
	m_ChunkSize = static_cast<DWORD>(BlockSize);
	if(GetSize(m_FileSize) && m_FileSize)
	{
		BY_HANDLE_FILE_INFORMATION bhfi;
		m_IsSparse = GetInformation(bhfi) && bhfi.dwFileAttributes&FILE_ATTRIBUTE_SPARSE_FILE;

		if(m_IsSparse)
		{
			FILE_ALLOCATED_RANGE_BUFFER QueryRange = {};
			QueryRange.Length.QuadPart = m_FileSize;
			FILE_ALLOCATED_RANGE_BUFFER Ranges[1024];
			DWORD BytesReturned;
			for(;;)
			{
				const bool QueryResult = IoControl(FSCTL_QUERY_ALLOCATED_RANGES, &QueryRange, sizeof(QueryRange), Ranges, sizeof(Ranges), &BytesReturned);
				if((QueryResult || GetLastError() == ERROR_MORE_DATA) && BytesReturned)
				{
					for (const auto& i: make_range(Ranges, BytesReturned / sizeof(*Ranges)))
					{
						m_AllocSize += i.Length.QuadPart;
						const UINT64 RangeEndOffset = i.FileOffset.QuadPart + i.Length.QuadPart;
						for(UINT64 j = i.FileOffset.QuadPart; j < RangeEndOffset; j+=m_ChunkSize)
						{
							m_ChunkList.emplace_back(j, static_cast<DWORD>(std::min(RangeEndOffset - j, static_cast<UINT64>(m_ChunkSize))));
						}
					}
					QueryRange.FileOffset.QuadPart = m_ChunkList.back().Offset+m_ChunkList.back().Size;
					QueryRange.Length.QuadPart = m_FileSize - QueryRange.FileOffset.QuadPart;
				}
				else
				{
					break;
				}
			}
			Result = !m_ChunkList.empty();
		}
		else
		{
			m_AllocSize = m_FileSize;
			m_ChunkList.emplace_back(0, static_cast<DWORD>(std::min(static_cast<UINT64>(BlockSize), m_FileSize)));
			Result = true;
		}
		m_CurrentChunk = m_ChunkList.begin();
	}
	return Result;
}

bool file_walker::Step()
{
	bool Result = false;
	if(m_IsSparse)
	{
		++m_CurrentChunk;
		if(m_CurrentChunk != m_ChunkList.end())
		{
			SetPointer(m_CurrentChunk->Offset, nullptr, FILE_BEGIN);
			m_ProcessedSize += m_CurrentChunk->Size;
			Result = true;
		}
	}
	else
	{
		UINT64 NewOffset = (!m_CurrentChunk->Size)? 0 : m_CurrentChunk->Offset + m_ChunkSize;
		if(NewOffset < m_FileSize)
		{
			m_CurrentChunk->Offset = NewOffset;
			UINT64 rest = m_FileSize - NewOffset;
			m_CurrentChunk->Size = (rest>=m_ChunkSize)?m_ChunkSize:rest;
			m_ProcessedSize += m_CurrentChunk->Size;
			Result = true;
		}
	}
	return Result;
}

UINT64 file_walker::GetChunkOffset() const
{
	return m_CurrentChunk->Offset;
}

DWORD file_walker::GetChunkSize() const
{
	return m_CurrentChunk->Size;
}

int file_walker::GetPercent() const
{
	return m_AllocSize? (m_ProcessedSize) * 100 / m_AllocSize : 0;
}

} // fs
//-------------------------------------------------------------------------

NTSTATUS GetLastNtStatus()
{
	return Imports().RtlGetLastNtStatus? Imports().RtlGetLastNtStatus() : STATUS_SUCCESS;
}

bool DeleteFile(const string& FileName)
{
	NTPath strNtName(FileName);
	bool Result = ::DeleteFile(strNtName.data()) != FALSE;
	if(!Result && ElevationRequired(ELEVATION_MODIFY_REQUEST))
	{
		Result = Global->Elevation->fDeleteFile(strNtName);
	}

	if (!Result && !os::fs::exists(strNtName))
	{
		// Someone deleted it already,
		// but job is done, no need to report error.
		Result = true;
	}

	return Result;
}

bool RemoveDirectory(const string& DirName)
{
	NTPath strNtName(DirName);
	bool Result = ::RemoveDirectory(strNtName.data()) != FALSE;
	if(!Result && ElevationRequired(ELEVATION_MODIFY_REQUEST))
	{
		Result = Global->Elevation->fRemoveDirectory(strNtName);
	}

	if (!Result && !os::fs::exists(strNtName))
	{
		// Someone deleted it already,
		// but job is done, no need to report error.
		Result = true;
	}

	return Result;
}

os::handle CreateFile(const string& Object, DWORD DesiredAccess, DWORD ShareMode, LPSECURITY_ATTRIBUTES SecurityAttributes, DWORD CreationDistribution, DWORD FlagsAndAttributes, HANDLE TemplateFile, bool ForceElevation)
{
	NTPath strObject(Object);
	FlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
	if (CreationDistribution == OPEN_EXISTING || CreationDistribution == TRUNCATE_EXISTING)
	{
		FlagsAndAttributes |= FILE_FLAG_POSIX_SEMANTICS;
	}

	os::handle Handle(::CreateFile(strObject.data(), DesiredAccess, ShareMode, SecurityAttributes, CreationDistribution, FlagsAndAttributes, TemplateFile));
	if(!Handle)
	{
		DWORD Error=::GetLastError();
		if(Error==ERROR_FILE_NOT_FOUND||Error==ERROR_PATH_NOT_FOUND)
		{
			FlagsAndAttributes&=~FILE_FLAG_POSIX_SEMANTICS;
			Handle.reset(::CreateFile(strObject.data(), DesiredAccess, ShareMode, SecurityAttributes, CreationDistribution, FlagsAndAttributes, TemplateFile));
		}
		if (STATUS_STOPPED_ON_SYMLINK == GetLastNtStatus() && ERROR_STOPPED_ON_SYMLINK != GetLastError())
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	}

	if((!Handle && ElevationRequired(DesiredAccess&(GENERIC_ALL|GENERIC_WRITE|WRITE_OWNER|WRITE_DAC|DELETE|FILE_WRITE_DATA|FILE_ADD_FILE|FILE_APPEND_DATA|FILE_ADD_SUBDIRECTORY|FILE_CREATE_PIPE_INSTANCE|FILE_WRITE_EA|FILE_DELETE_CHILD|FILE_WRITE_ATTRIBUTES)?ELEVATION_MODIFY_REQUEST:ELEVATION_READ_REQUEST)) || ForceElevation)
	{
		Handle.reset(Global->Elevation->fCreateFile(strObject, DesiredAccess, ShareMode, SecurityAttributes, CreationDistribution, FlagsAndAttributes, TemplateFile));
	}
	return Handle;
}

bool CopyFileEx(
    const string& ExistingFileName,
    const string& NewFileName,
    LPPROGRESS_ROUTINE lpProgressRoutine,
    LPVOID lpData,
    LPBOOL pbCancel,
    DWORD dwCopyFlags
)
{
	NTPath strFrom(ExistingFileName), strTo(NewFileName);
	if(IsSlash(strTo.back()))
	{
		strTo += PointToName(strFrom);
	}
	bool Result = ::CopyFileEx(strFrom.data(), strTo.data(), lpProgressRoutine, lpData, pbCancel, dwCopyFlags) != FALSE;
	if(!Result)
	{
		if (STATUS_STOPPED_ON_SYMLINK == GetLastNtStatus() && ERROR_STOPPED_ON_SYMLINK != GetLastError())
			::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

		else if (STATUS_FILE_IS_A_DIRECTORY == GetLastNtStatus())
			::SetLastError(ERROR_FILE_EXISTS);

		else if (ElevationRequired(ELEVATION_MODIFY_REQUEST)) //BUGBUG, really unknown
			Result = Global->Elevation->fCopyFileEx(strFrom, strTo, lpProgressRoutine, lpData, pbCancel, dwCopyFlags);
	}
	return Result;
}

bool MoveFile(
    const string& ExistingFileName, // address of name of the existing file
    const string& NewFileName   // address of new name for the file
)
{
	NTPath strFrom(ExistingFileName), strTo(NewFileName);
	if(IsSlash(strTo.back()))
	{
		strTo += PointToName(strFrom);
	}
	bool Result = ::MoveFile(strFrom.data(), strTo.data()) != FALSE;

	if(!Result)
	{
		if (STATUS_STOPPED_ON_SYMLINK == GetLastNtStatus() && ERROR_STOPPED_ON_SYMLINK != GetLastError())
			::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

		else if (ElevationRequired(ELEVATION_MODIFY_REQUEST)) //BUGBUG, really unknown
			Result = Global->Elevation->fMoveFileEx(strFrom, strTo, 0);
	}
	return Result;
}

bool MoveFileEx(
    const string& ExistingFileName, // address of name of the existing file
    const string& NewFileName,   // address of new name for the file
    DWORD dwFlags   // flag to determine how to move file
)
{
	NTPath strFrom(ExistingFileName), strTo(NewFileName);
	if(IsSlash(strTo.back()))
	{
		strTo += PointToName(strFrom);
	}
	bool Result = ::MoveFileEx(strFrom.data(), strTo.data(), dwFlags) != FALSE;
	if(!Result)
	{
		if (STATUS_STOPPED_ON_SYMLINK == GetLastNtStatus() && ERROR_STOPPED_ON_SYMLINK != GetLastError())
			::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

		else if (ElevationRequired(ELEVATION_MODIFY_REQUEST)) //BUGBUG, really unknown
		{
			// exclude fake elevation request for: move file over existing directory with same name
			DWORD f = GetFileAttributes(strFrom);
			DWORD t = GetFileAttributes(strTo);

			if (f!=INVALID_FILE_ATTRIBUTES && t!=INVALID_FILE_ATTRIBUTES && 0==(f & FILE_ATTRIBUTE_DIRECTORY) && 0!=(t & FILE_ATTRIBUTE_DIRECTORY))
				::SetLastError(ERROR_FILE_EXISTS); // existing directory name == moved file name
			else
				Result = Global->Elevation->fMoveFileEx(strFrom, strTo, dwFlags);
		}
	}
	return Result;
}

string& strCurrentDirectory()
{
	static string strCurrentDirectory;
	return strCurrentDirectory;
}

void InitCurrentDirectory()
{
	string InitCurDir;
	if (GetProcessRealCurrentDirectory(InitCurDir))
	{
		SetCurrentDirectory(InitCurDir);
	}
}

bool GetProcessRealCurrentDirectory(string& Directory)
{
	return ApiDynamicStringReceiver(Directory, [](wchar_t* Buffer, size_t Size)
	{
		return ::GetCurrentDirectory(static_cast<DWORD>(Size), Buffer);
	});
}

bool SetProcessRealCurrentDirectory(const string& Directory)
{
	return ::SetCurrentDirectory(Directory.data()) != FALSE;
}

string GetCurrentDirectory()
{
	//never give outside world a direct pointer to our internal string
	//who knows what they gonna do
	return strCurrentDirectory();
}

bool SetCurrentDirectory(const string& PathName, bool Validate)
{
	// correct path to our standard
	string strDir=PathName;
	ReplaceSlashToBackslash(strDir);
	bool Root = false;
	const auto Type = ParsePath(strDir, nullptr, &Root);
	if(Root && (Type == PATH_DRIVELETTER || Type == PATH_DRIVELETTERUNC || Type == PATH_VOLUMEGUID))
	{
		AddEndSlash(strDir);
	}
	else
	{
		DeleteEndSlash(strDir);
	}

	if (strDir == strCurrentDirectory())
		return true;

	if (Validate)
	{
		string TestDir=PathName;
		AddEndSlash(TestDir);
		TestDir += L"*";
		FAR_FIND_DATA fd;
		if (!GetFindDataEx(TestDir, fd))
		{
			DWORD LastError = ::GetLastError();
			if(!(LastError == ERROR_FILE_NOT_FOUND || LastError == ERROR_NO_MORE_FILES))
				return false;
		}
	}

	strCurrentDirectory()=strDir;

#ifndef NO_WRAPPER
	// try to synchronize far cur dir with process cur dir
	if(Global->CtrlObject && Global->CtrlObject->Plugins->OemPluginsPresent())
	{
		SetProcessRealCurrentDirectory(strDir);
	}
#endif // NO_WRAPPER
	return true;
}

DWORD GetTempPath(string &strBuffer)
{
	return ApiDynamicStringReceiver(strBuffer, [&](wchar_t* Buffer, size_t Size)
	{
		return ::GetTempPath(static_cast<DWORD>(Size), Buffer);
	});
};

bool GetModuleFileName(HMODULE hModule, string &strFileName)
{
	return GetModuleFileNameEx(nullptr, hModule, strFileName);
}

bool GetModuleFileNameEx(HANDLE hProcess, HMODULE hModule, string &strFileName)
{
	return ApiDynamicErrorBasedStringReceiver(ERROR_INSUFFICIENT_BUFFER, strFileName, [&](wchar_t* Buffer, size_t Size)
	{
		if (!hProcess)
		{
			auto ReturnedSize = ::GetModuleFileName(hModule, Buffer, static_cast<DWORD>(Size));
			if (ReturnedSize == Size)
			{
				// os <= XP doesn't set this
				SetLastError(ERROR_INSUFFICIENT_BUFFER);
				ReturnedSize = 0;
			}
			return ReturnedSize;
		}

		if (Imports().QueryFullProcessImageNameW && !hModule)
		{
			auto sz = static_cast<DWORD>(Size);
			return Imports().QueryFullProcessImageNameW(hProcess, 0, Buffer, &sz)? sz : 0;
		}
		else
		{
			return ::GetModuleFileNameEx(hProcess, hModule, Buffer, static_cast<DWORD>(Size));
		}
	});
}

bool WNetGetConnection(const string& LocalName, string &RemoteName)
{
	wchar_t Buffer[MAX_PATH];
	// MSDN says that call can fail with ERROR_NOT_CONNECTED or ERROR_CONNECTION_UNAVAIL if calling application
	// is running in a different logon session than the application that made the connection.
	// However, it may fail with ERROR_NOT_CONNECTED for non-network too, in this case Buffer will not be initialised.
	// Deliberately initialised with empty string to fix that.
	Buffer[0] = L'\0';
	auto Size = static_cast<DWORD>(std::size(Buffer));
	auto Result = ::WNetGetConnection(LocalName.data(), Buffer, &Size);
	
	const auto& IsReceived = [](int Code) { return Code == NO_ERROR || Code == ERROR_NOT_CONNECTED || Code == ERROR_CONNECTION_UNAVAIL; };

	if (IsReceived(Result) && *Buffer)
	{
		// Size isn't updated if the buffer is large enough
		RemoteName = Buffer;
		return true;
	}
	
	while (Result == ERROR_MORE_DATA)
	{
		wchar_t_ptr vBuffer(Size);
		Result = ::WNetGetConnection(LocalName.data(), vBuffer.get(), &Size);
		if (IsReceived(Result) && *Buffer)
		{
			// Size isn't updated if the buffer is large enough
			RemoteName = vBuffer.get();
			return true;
		}
	}

	return false;
}

bool GetVolumeInformation(
    const string& RootPathName,
    string *pVolumeName,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    string *pFileSystemName
)
{
	wchar_t_ptr VolumeNameBuffer, FileSystemNameBuffer;
	if (pVolumeName)
	{
		VolumeNameBuffer.reset(MAX_PATH + 1);
		VolumeNameBuffer[0] = L'\0';
	}
	if (pFileSystemName)
	{
		FileSystemNameBuffer.reset(MAX_PATH + 1);
		FileSystemNameBuffer[0] = L'\0';
	}
	bool bResult = ::GetVolumeInformation(RootPathName.data(), VolumeNameBuffer.get(), static_cast<DWORD>(VolumeNameBuffer.size()), lpVolumeSerialNumber,
		lpMaximumComponentLength, lpFileSystemFlags, FileSystemNameBuffer.get(), static_cast<DWORD>(FileSystemNameBuffer.size())) != FALSE;

	if (pVolumeName)
		*pVolumeName = VolumeNameBuffer.get();

	if (pFileSystemName)
		*pFileSystemName = FileSystemNameBuffer.get();

	return bResult;
}

bool GetFindDataEx(const string& FileName, FAR_FIND_DATA& FindData,bool ScanSymLink)
{
	fs::enum_file Find(FileName, ScanSymLink);
	auto ItemIterator = Find.begin();
	if (ItemIterator != Find.end())
	{
		FindData = std::move(*ItemIterator);
		return true;
	}

	FindData = {};
	FindData.dwFileAttributes = INVALID_FILE_ATTRIBUTES;

	size_t DirOffset = 0;
	ParsePath(FileName, &DirOffset);
	if (FileName.find_first_of(L"*?", DirOffset) != string::npos)
		return false;

	if ((FindData.dwFileAttributes = GetFileAttributes(FileName)) == INVALID_FILE_ATTRIBUTES)
		return false;

	// Ага, значит файл таки есть. Заполним структуру ручками.
	if (const auto File = fs::file(FileName, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING))
	{
		File.GetTime(&FindData.ftCreationTime, &FindData.ftLastAccessTime, &FindData.ftLastWriteTime, &FindData.ftChangeTime);
		File.GetSize(FindData.nFileSize);
	}

	if (FindData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
	{
		string strTmp;
		GetReparsePointInfo(FileName, strTmp, &FindData.dwReserved0); //MSDN
	}

	FindData.strFileName = PointToName(FileName);
	FindData.strAlternateFileName = ConvertNameToShort(FileName);
	return true;
}

bool GetFileSizeEx(HANDLE hFile, UINT64 &Size)
{
	if (::GetFileSizeEx(hFile,reinterpret_cast<PLARGE_INTEGER>(&Size)))
		return true;

	GET_LENGTH_INFORMATION gli;
	DWORD BytesReturned;

	if (!::DeviceIoControl(hFile, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &gli, sizeof(gli), &BytesReturned, nullptr))
		return false;

	Size=gli.Length.QuadPart;
	return true;
}

bool IsDiskInDrive(const string& Root)
{
	string strVolName;
	DWORD  MaxComSize;
	DWORD  Flags;
	string strFS;
	auto strDrive = Root;
	AddEndSlash(strDrive);
	return GetVolumeInformation(strDrive, &strVolName, nullptr, &MaxComSize, &Flags, &strFS);
}

int GetFileTypeByName(const string& Name)
{
	if (const auto File = CreateFile(Name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0))
	{
		return ::GetFileType(File.native_handle());
	}
	return FILE_TYPE_UNKNOWN;
}

bool GetDiskSize(const string& Path,unsigned long long* TotalSize, unsigned long long* TotalFree, unsigned long long* UserFree)
{
	NTPath strPath(Path);
	AddEndSlash(strPath);

	ULARGE_INTEGER FreeBytesAvailableToCaller, TotalNumberOfBytes, TotalNumberOfFreeBytes;

	auto Result = GetDiskFreeSpaceEx(strPath.data(), &FreeBytesAvailableToCaller, &TotalNumberOfBytes, &TotalNumberOfFreeBytes) != FALSE;
	if(!Result && ElevationRequired(ELEVATION_READ_REQUEST))
	{
		Result = Global->Elevation->fGetDiskFreeSpaceEx(strPath.data(), &FreeBytesAvailableToCaller, &TotalNumberOfBytes, &TotalNumberOfFreeBytes);
	}

	if (Result)
	{
		if (TotalSize)
			*TotalSize = TotalNumberOfBytes.QuadPart;

		if (TotalFree)
			*TotalFree = TotalNumberOfFreeBytes.QuadPart;

		if (UserFree)
			*UserFree = FreeBytesAvailableToCaller.QuadPart;
	}
	return Result;
}

find_handle FindFirstFileName(const string& FileName, DWORD dwFlags, string& LinkName)
{
	NTPath NtFileName(FileName);
	find_handle Handle;
	ApiDynamicStringReceiver(LinkName, [&](wchar_t* Buffer, size_t Size)
	{
		auto BufferSize = static_cast<DWORD>(Size);
		Handle.reset(Imports().FindFirstFileNameW(NtFileName.data(), 0, &BufferSize, Buffer));
		if (Handle)
			// FindFirstFileNameW always includes terminating \0 in the returned size
			return BufferSize - 1;
		return GetLastError() == ERROR_MORE_DATA? BufferSize : 0;
	});
	return Handle;
}

bool FindNextFileName(const find_handle& hFindStream, string& LinkName)
{
	return ApiDynamicStringReceiver(LinkName, [&](wchar_t* Buffer, size_t Size)
	{
		auto BufferSize = static_cast<DWORD>(Size);
		if (Imports().FindNextFileNameW(hFindStream.native_handle(), &BufferSize, Buffer))
			// FindNextFileNameW always includes terminating \0 in the returned size
			return BufferSize - 1;
		return GetLastError() == ERROR_MORE_DATA? BufferSize : 0;
	});
}

bool CreateDirectory(const string& PathName,LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	return CreateDirectoryEx(L"", PathName, lpSecurityAttributes);
}

bool CreateDirectoryEx(const string& TemplateDirectory, const string& NewDirectory, LPSECURITY_ATTRIBUTES SecurityAttributes)
{
	NTPath NtNewDirectory(NewDirectory);

	const auto& Create = [&](const string& Template)
	{
		auto Result = Template.empty()? ::CreateDirectory(NtNewDirectory.data(), SecurityAttributes) != FALSE : ::CreateDirectoryEx(Template.data(), NtNewDirectory.data(), SecurityAttributes) != FALSE;
		if (!Result && ElevationRequired(ELEVATION_MODIFY_REQUEST))
		{
			Result = Global->Elevation->fCreateDirectoryEx(Template, NtNewDirectory, SecurityAttributes);
		}
		return Result;
	};

	return Create(NTPath(TemplateDirectory)) ||
		// CreateDirectoryEx may fail on some FS, try to create anyway.
		Create({});
}

DWORD GetFileAttributes(const string& FileName)
{
	NTPath NtName(FileName);
	DWORD Result = ::GetFileAttributes(NtName.data());
	if(Result == INVALID_FILE_ATTRIBUTES && ElevationRequired(ELEVATION_READ_REQUEST))
	{
		Result = Global->Elevation->fGetFileAttributes(NtName);
	}
	return Result;
}

bool SetFileAttributes(const string& FileName, DWORD dwFileAttributes)
{
	NTPath NtName(FileName);
	bool Result = ::SetFileAttributes(NtName.data(), dwFileAttributes) != FALSE;
	if(!Result && ElevationRequired(ELEVATION_MODIFY_REQUEST))
	{
		Result = Global->Elevation->fSetFileAttributes(NtName, dwFileAttributes);
	}
	return Result;
}

bool CreateSymbolicLinkInternal(const string& Object, const string& Target, DWORD dwFlags)
{
	return Imports().CreateSymbolicLinkW?
		(Imports().CreateSymbolicLink(Object.data(), Target.data(), dwFlags) != FALSE) :
		CreateReparsePoint(Target, Object, dwFlags&SYMBOLIC_LINK_FLAG_DIRECTORY?RP_SYMLINKDIR:RP_SYMLINKFILE);
}

bool CreateSymbolicLink(const string& SymlinkFileName, const string& TargetFileName,DWORD dwFlags)
{
	NTPath NtSymlinkFileName(SymlinkFileName);
	bool Result = CreateSymbolicLinkInternal(NtSymlinkFileName, TargetFileName, dwFlags);
	if(!Result && ElevationRequired(ELEVATION_MODIFY_REQUEST))
	{
		Result=Global->Elevation->fCreateSymbolicLink(NtSymlinkFileName, TargetFileName, dwFlags);
	}
	return Result;
}

bool SetFileEncryptionInternal(const wchar_t* Name, bool Encrypt)
{
	return Encrypt? EncryptFile(Name)!=FALSE : DecryptFile(Name, 0)!=FALSE;
}

bool SetFileEncryption(const string& Name, bool Encrypt)
{
	NTPath NtName(Name);
	bool Result = SetFileEncryptionInternal(NtName.data(), Encrypt);
	if(!Result && ElevationRequired(ELEVATION_MODIFY_REQUEST, false)) // Encryption implemented in adv32, NtStatus not affected
	{
		Result=Global->Elevation->fSetFileEncryption(NtName, Encrypt);
	}
	return Result;
}

bool CreateHardLinkInternal(const string& Object, const string& Target,LPSECURITY_ATTRIBUTES SecurityAttributes)
{
	bool Result = ::CreateHardLink(Object.data(), Target.data(), SecurityAttributes) != FALSE;
	if(!Result && ElevationRequired(ELEVATION_MODIFY_REQUEST))
	{
		Result = Global->Elevation->fCreateHardLink(Object, Target, SecurityAttributes);
	}
	return Result;
}

bool CreateHardLink(const string& FileName, const string& ExistingFileName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	bool Result = CreateHardLinkInternal(NTPath(FileName),NTPath(ExistingFileName), lpSecurityAttributes) != FALSE;
	//bug in win2k: \\?\ fails
	if (!Result && !IsWindowsXPOrGreater())
	{
		Result = CreateHardLinkInternal(FileName, ExistingFileName, lpSecurityAttributes);
	}
	return Result;
}

static bool FileStreamInformationToFindStreamData(const FILE_STREAM_INFORMATION& StreamInfo, WIN32_FIND_STREAM_DATA& StreamData)
{
	const auto Length = StreamInfo.StreamNameLength / sizeof(wchar_t);
	if (!Length || Length >= std::size(StreamData.cStreamName))
		return false;

	*std::copy_n(std::cbegin(StreamInfo.StreamName), Length, StreamData.cStreamName) = L'\0';
	StreamData.StreamSize = StreamInfo.StreamSize;
	return true;
}

find_file_handle FindFirstStream(const string& FileName,STREAM_INFO_LEVELS InfoLevel,LPVOID lpFindStreamData,DWORD dwFlags)
{
	if (Imports().FindFirstStreamW)
	{
		detail::os_find_handle_impl Handle(Imports().FindFirstStreamW(NTPath(FileName).data(), InfoLevel, lpFindStreamData, dwFlags));
		return find_file_handle(Handle? std::make_unique<detail::os_find_handle_impl>(Handle.release()).release() : nullptr);
	}

	if (InfoLevel != FindStreamInfoStandard)
		return find_file_handle{};

	auto Handle = std::make_unique<detail::far_find_handle_impl>();

	if (!Handle->Object.Open(FileName, 0, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING))
		return find_file_handle{};

	// for network paths buffer size must be <= 64k
	// we double it in a first loop, so starting value is 32k
	size_t BufferSize = 0x8000;
	NTSTATUS Result = STATUS_SEVERITY_ERROR;
	do
	{
		BufferSize *= 2;
		Handle->BufferBase.reset(BufferSize);
		// sometimes for directories NtQueryInformationFile returns STATUS_SUCCESS but doesn't fill the buffer
		const auto StreamInfo = reinterpret_cast<FILE_STREAM_INFORMATION*>(Handle->BufferBase.get());
		StreamInfo->StreamNameLength = 0;
		Handle->Object.NtQueryInformationFile(Handle->BufferBase.get(), Handle->BufferBase.size(), FileStreamInformation, &Result);
	}
	while(Result == STATUS_BUFFER_OVERFLOW || Result == STATUS_BUFFER_TOO_SMALL);

	if (Result != STATUS_SUCCESS)
		return find_file_handle{};

	const auto StreamInfo = reinterpret_cast<const FILE_STREAM_INFORMATION*>(Handle->BufferBase.get());
	Handle->NextOffset = StreamInfo->NextEntryOffset;
	const auto StreamData = static_cast<WIN32_FIND_STREAM_DATA*>(lpFindStreamData);

	if (!FileStreamInformationToFindStreamData(*StreamInfo, *StreamData))
		return find_file_handle{};

	return find_file_handle(Handle.release());
}

bool FindNextStream(const find_file_handle& hFindStream,LPVOID lpFindStreamData)
{
	if (Imports().FindFirstStreamW)
	{
		return Imports().FindNextStreamW(static_cast<detail::os_find_handle_impl*>(hFindStream.native_handle())->native_handle(), lpFindStreamData) != FALSE;
	}

	const auto Handle = static_cast<detail::far_find_handle_impl*>(hFindStream.native_handle());

	if (!Handle->NextOffset)
		return false;

	const auto StreamInfo = reinterpret_cast<const FILE_STREAM_INFORMATION*>(Handle->BufferBase.get() + Handle->NextOffset);
	Handle->NextOffset = StreamInfo->NextEntryOffset? Handle->NextOffset + StreamInfo->NextEntryOffset : 0;
	const auto StreamData = static_cast<WIN32_FIND_STREAM_DATA*>(lpFindStreamData);

	return FileStreamInformationToFindStreamData(*StreamInfo, *StreamData);
}

std::vector<string> GetLogicalDriveStrings()
{
	FN_RETURN_TYPE(GetLogicalDriveStrings) Result;
	string Strings;
	if (ApiDynamicStringReceiver(Strings, [](wchar_t* Buffer, size_t Size)
	{
		return ::GetLogicalDriveStrings(static_cast<DWORD>(Size), Buffer);
	}))
	{
		for (const auto& i : enum_substrings(Strings))
		{
			Result.emplace_back(i.data(), i.size());
		}
	}
	return Result;
}

static int MatchNtPathRoot(const string &NtPath, const string& DeviceName)
{
	string TargetPath;
	if (os::QueryDosDevice(DeviceName, TargetPath))
	{
		if (PathStartsWith(NtPath, TargetPath))
			return static_cast<int>(TargetPath.size());

		// path could be an Object Manager symlink, try to resolve
		UNICODE_STRING ObjName;
		ObjName.Length = ObjName.MaximumLength = static_cast<USHORT>(TargetPath.size() * sizeof(wchar_t));
		ObjName.Buffer = UNSAFE_CSTR(TargetPath);
		OBJECT_ATTRIBUTES ObjAttrs;
		InitializeObjectAttributes(&ObjAttrs, &ObjName, 0, nullptr, nullptr);
		HANDLE hSymLink;

		if (Imports().NtOpenSymbolicLinkObject(&hSymLink, GENERIC_READ, &ObjAttrs) == STATUS_SUCCESS)
		{
			SCOPE_EXIT{ Imports().NtClose(hSymLink); };

			ULONG BufSize = 32767;
			wchar_t_ptr Buffer(BufSize);
			UNICODE_STRING LinkTarget;
			LinkTarget.MaximumLength = static_cast<USHORT>(BufSize * sizeof(wchar_t));
			LinkTarget.Buffer = Buffer.get();

			if (Imports().NtQuerySymbolicLinkObject(hSymLink, &LinkTarget, nullptr) == STATUS_SUCCESS)
			{
				TargetPath.assign(LinkTarget.Buffer, LinkTarget.Length / sizeof(wchar_t));
			}

			if (PathStartsWith(NtPath, TargetPath))
				return static_cast<int>(TargetPath.size());
		}
	}

	return 0;
}

static bool GetObjectName(HANDLE hFile, string& ObjectName)
{
	block_ptr<OBJECT_NAME_INFORMATION> oni(NT_MAX_PATH);
	ULONG ReturnLength;

	const auto& QueryObject = [&]
	{
		return Imports().NtQueryObject(hFile, ObjectNameInformation, oni.get(), static_cast<unsigned long>(oni.size()), &ReturnLength);
	};

	auto Result = QueryObject();
	if (Result == STATUS_BUFFER_OVERFLOW || Result == STATUS_BUFFER_TOO_SMALL)
	{
		oni.reset(ReturnLength);
		Result = QueryObject();
	}

	if (Result != STATUS_SUCCESS)
		return false;

	ObjectName.assign(oni->Name.Buffer, oni->Name.Length / sizeof(WCHAR));
	return true;
};

static bool internalNtQueryGetFinalPathNameByHandle(HANDLE hFile, string& FinalFilePath)
{
	string NtPath;
	if (!GetObjectName(hFile, NtPath))
		return false;

	const auto& ReplaceRoot = [&](const auto& OldRoot, const auto& NewRoot)
	{
		if (NtPath.compare(0, OldRoot.size(), OldRoot.data()))
			return false;
		FinalFilePath = NtPath.replace(0, OldRoot.size(), NewRoot.data(), NewRoot.size());
		return true;
	};

	// simple way to handle network paths
	if (ReplaceRoot(L"\\Device\\LanmanRedirector"_sl, L"\\"_sl) || ReplaceRoot(L"\\Device\\Mup"_sl, L"\\"_sl))
		return true;

	// try to convert NT path (\Device\HarddiskVolume1) to drive letter
	drives_set Drives = GetLogicalDrives();
	for (size_t i = 0; i != Drives.size(); ++i)
	{
		if (!Drives[i])
			continue;

		const string Device{ static_cast<wchar_t>(L'A' + i), L':'};
		if (const auto Len = MatchNtPathRoot(NtPath, Device))
		{
			FinalFilePath = NtPath.compare(0, 14, L"\\Device\\WinDfs")? NtPath.replace(0, Len, Device) : NtPath.replace(0, Len, 1, L'\\');
			return true;
		}
	}

	// try to convert NT path (\Device\HarddiskVolume1) to \\?\Volume{...} path
	for (auto& VolumeName: os::fs::enum_volume())
	{
		if (const auto Len = MatchNtPathRoot(NtPath, VolumeName.substr(4, VolumeName.size() - 5))) // w/o prefix and trailing slash
		{
			FinalFilePath = NtPath.replace(0, Len, VolumeName);
			return true;
		}
	}

	return false;
}

bool GetFinalPathNameByHandle(HANDLE hFile, string& FinalFilePath)
{
	const auto& GetFinalPathNameByHandleGuarded = [](HANDLE File, wchar_t* Buffer, DWORD Size, DWORD Flags)
	{
		// It seems that Microsoft has forgotten to put an exception handler around this function.
		// It causes EXCEPTION_ACCESS_VIOLATION (read from 0) in kernel32 under certain conditions,
		// e.g. badly written file system drivers or weirdly formatted volumes.
		return seh_invoke_no_ui(
		[&]
		{
			return Imports().GetFinalPathNameByHandle(File, Buffer, Size, Flags);
		},
		[]
		{
			SetLastError(ERROR_UNHANDLED_EXCEPTION);
			return 0ul;
		});
	};

	const auto& GetFinalPathNameByHandleImpl = [&]
	{
		return ApiDynamicStringReceiver(FinalFilePath, [&](wchar_t* Buffer, size_t Size)
		{
			return GetFinalPathNameByHandleGuarded(hFile, Buffer, static_cast<DWORD>(Size), VOLUME_NAME_GUID);
		});
	};

	return (Imports().GetFinalPathNameByHandleW && GetFinalPathNameByHandleImpl()) || internalNtQueryGetFinalPathNameByHandle(hFile, FinalFilePath);
}

bool SearchPath(const wchar_t *Path, const string& FileName, const wchar_t *Extension, string &strDest)
{
	return ApiDynamicStringReceiver(strDest, [&](wchar_t* Buffer, size_t Size)
	{
		return ::SearchPath(Path, FileName.data(), Extension, static_cast<DWORD>(Size), Buffer, nullptr);
	});
}

bool QueryDosDevice(const string& DeviceName, string &Path)
{
	const auto DeviceNamePtr = EmptyToNull(DeviceName.data());
	return ApiDynamicErrorBasedStringReceiver(ERROR_INSUFFICIENT_BUFFER, Path, [&](wchar_t* Buffer, size_t Size)
	{
		const auto ReturnedSize = ::QueryDosDevice(DeviceNamePtr, Buffer, static_cast<DWORD>(Size));
		// Upon success it includes two trailing '\0', we don't need them
		return ReturnedSize? ReturnedSize - 2 : 0;
	});
}

bool GetVolumeNameForVolumeMountPoint(const string& VolumeMountPoint,string& VolumeName)
{
	WCHAR VolumeNameBuffer[50];
	NTPath strVolumeMountPoint(VolumeMountPoint);
	AddEndSlash(strVolumeMountPoint);
	if (!::GetVolumeNameForVolumeMountPoint(strVolumeMountPoint.data(), VolumeNameBuffer, static_cast<DWORD>(std::size(VolumeNameBuffer))))
		return false;

	VolumeName=VolumeNameBuffer;
	return true;
}

void EnableLowFragmentationHeap()
{
	if (!Imports().HeapSetInformation)
		return;

	std::vector<HANDLE> Heaps(10);
	for (;;)
	{
		const auto NumberOfHeaps = ::GetProcessHeaps(static_cast<DWORD>(Heaps.size()), Heaps.data());
		const auto Received = NumberOfHeaps <= Heaps.size();
		Heaps.resize(NumberOfHeaps);
		if (Received)
			break;
	}

	for (auto i: Heaps)
	{
		ULONG HeapFragValue = 2;
		Imports().HeapSetInformation(i, HeapCompatibilityInformation, &HeapFragValue, sizeof(HeapFragValue));
	}
}

bool GetFileTimeSimple(const string &FileName, LPFILETIME CreationTime, LPFILETIME LastAccessTime, LPFILETIME LastWriteTime, LPFILETIME ChangeTime)
{
	fs::file File;
	return File.Open(FileName, FILE_READ_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING) && File.GetTime(CreationTime, LastAccessTime, LastWriteTime, ChangeTime);
}

bool GetFileSecurity(const string& Object, SECURITY_INFORMATION RequestedInformation, FAR_SECURITY_DESCRIPTOR& SecurityDescriptor)
{
	bool Result = false;
	NTPath NtObject(Object);
	DWORD LengthNeeded = 0;
	::GetFileSecurity(NtObject.data(), RequestedInformation, nullptr, 0, &LengthNeeded);
	if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		SecurityDescriptor.reset(LengthNeeded);
		Result = ::GetFileSecurity(NtObject.data(), RequestedInformation, SecurityDescriptor.get(), LengthNeeded, &LengthNeeded) != FALSE;
	}
	return Result;
}

bool SetFileSecurity(const string& Object, SECURITY_INFORMATION RequestedInformation, const FAR_SECURITY_DESCRIPTOR& SecurityDescriptor)
{
	return ::SetFileSecurity(NTPath(Object).data(), RequestedInformation, SecurityDescriptor.get()) != FALSE;
}

bool DetachVirtualDiskInternal(const string& Object, VIRTUAL_STORAGE_TYPE& VirtualStorageType)
{
	handle Handle;
	DWORD Result = Imports().OpenVirtualDisk(&VirtualStorageType, Object.data(), VIRTUAL_DISK_ACCESS_DETACH, OPEN_VIRTUAL_DISK_FLAG_NONE, nullptr, &ptr_setter(Handle));
	if (Result != ERROR_SUCCESS)
	{
		SetLastError(Result);
		return false;
	}

	Result = Imports().DetachVirtualDisk(Handle.native_handle(), DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
	if (Result != ERROR_SUCCESS)
	{
		SetLastError(Result);
		return false;
	}

	return true;
}

bool DetachVirtualDisk(const string& Object, VIRTUAL_STORAGE_TYPE& VirtualStorageType)
{
	NTPath NtObject(Object);
	bool Result = DetachVirtualDiskInternal(NtObject, VirtualStorageType);
	if(!Result && ElevationRequired(ELEVATION_MODIFY_REQUEST))
	{
		Result = Global->Elevation->fDetachVirtualDisk(NtObject, VirtualStorageType);
	}
	return Result;
}

string GetPrivateProfileString(const string& AppName, const string& KeyName, const string& Default, const string& FileName)
{
	wchar_t_ptr Buffer(NT_MAX_PATH);
	DWORD size = ::GetPrivateProfileString(AppName.data(), KeyName.data(), Default.data(), Buffer.get(), static_cast<DWORD>(Buffer.size()), FileName.data());
	return string(Buffer.get(), size);
}

bool GetWindowText(HWND Hwnd, string& Text)
{
	return ApiDynamicStringReceiver(Text, [&](wchar_t* Buffer, size_t Size)
	{
		size_t Length = ::GetWindowTextLength(Hwnd);

			if (!Length)
			return Length;

		if (Length + 1 > Size)
			return Length + 1;

		return static_cast<size_t>(::GetWindowText(Hwnd, Buffer, static_cast<int>(Size)));
	});
}

bool IsWow64Process()
{
#ifdef _WIN64
	return false;
#else
	const auto& GetValue = [] { BOOL Value = FALSE; return Imports().IsWow64Process(GetCurrentProcess(), &Value) && Value; };
	static const auto Wow64Process = GetValue();
	return Wow64Process;
#endif
}

DWORD GetAppPathsRedirectionFlag()
{
	const auto& GetFlag = []
	{
		// App Paths key is shared in Windows 7 and above
		if (!IsWindows7OrGreater())
		{
#ifdef _WIN64
			return KEY_WOW64_32KEY;
#else
			if (IsWow64Process())
			{
				return KEY_WOW64_64KEY;
			}
#endif
		}
		return 0;
	};

	static const auto RedirectionFlag = GetFlag();
	return RedirectionFlag;
}

bool GetDefaultPrinter(string& Printer)
{
	return ApiDynamicErrorBasedStringReceiver(ERROR_INSUFFICIENT_BUFFER, Printer, [&](wchar_t* Buffer, size_t Size)
	{
		DWORD dwSize = static_cast<DWORD>(Size);
		return ::GetDefaultPrinter(Buffer, &dwSize)? dwSize - 1 : 0;
	});
}

	namespace reg
	{
		key open_key(HKEY RootKey, const wchar_t* SubKey, DWORD SamDesired)
		{
			HKEY Result;
			return key(RegOpenKeyEx(RootKey, SubKey, 0, SamDesired, &Result) == ERROR_SUCCESS? Result : nullptr);
		}

		static bool QueryValue(const key& Key, const wchar_t* Name, DWORD* Type, void* Data, size_t* Size)
		{
			DWORD dwSize = Size? static_cast<DWORD>(*Size) : 0;
			const auto Result = RegQueryValueEx(Key.get(), Name, nullptr, Type, reinterpret_cast<LPBYTE>(Data), Size? &dwSize : nullptr);
			if (Size)
			{
				*Size = dwSize;
			}
			return Result == ERROR_SUCCESS;
		}

		static bool QueryValue(const key& Key, const wchar_t* Name, DWORD& Type, std::vector<char>& Value)
		{
			bool Result = false;
			size_t Size = 0;
			if (QueryValue(Key, Name, nullptr, nullptr, &Size))
			{
				Value.resize(Size);
				Result = QueryValue(Key, Name, &Type, Value.data(), &Size);
			}
			return Result;
		}

		bool EnumKey(const key& Key, size_t Index, string& Name)
		{
			LONG ExitCode = ERROR_MORE_DATA;

			for (DWORD Size = 512; ExitCode == ERROR_MORE_DATA; Size *= 2)
			{
				wchar_t_ptr Buffer(Size);
				DWORD RetSize = Size;
				ExitCode = RegEnumKeyEx(Key.get(), static_cast<DWORD>(Index), Buffer.get(), &RetSize, nullptr, nullptr, nullptr, nullptr);
				if (ExitCode == ERROR_SUCCESS)
				{
					Name.assign(Buffer.get(), RetSize);
				}
			}
			return ExitCode == ERROR_SUCCESS;
		}

		bool EnumValue(const key& Key, size_t Index, value& Value)
		{
			LONG ExitCode = ERROR_MORE_DATA;

			for (DWORD Size = 512; ExitCode == ERROR_MORE_DATA; Size *= 2)
			{
				wchar_t_ptr Buffer(Size);
				DWORD RetSize = Size;
				ExitCode = RegEnumValue(Key.get(), static_cast<DWORD>(Index), Buffer.get(), &RetSize, nullptr, &Value.m_Type, nullptr, nullptr);
				if (ExitCode == ERROR_SUCCESS)
				{
					Value.m_Name.assign(Buffer.get(), RetSize);
					Value.m_Key = &Key;
				}
			}

			return ExitCode == ERROR_SUCCESS;
		}

		bool GetValue(const key& Key, const wchar_t* Name)
		{
			return QueryValue(Key, Name, nullptr, nullptr, nullptr);
		}

		bool GetValue(const key& Key, const wchar_t* Name, string& Value)
		{
			bool Result = false;
			std::vector<char> Buffer;
			DWORD Type;
			if (QueryValue(Key, Name, Type, Buffer) && detail::IsStringType(Type))
			{
				Value = string(reinterpret_cast<const wchar_t*>(Buffer.data()), Buffer.size() / sizeof(wchar_t));
				if (!Value.empty() && Value.back() == L'\0')
				{
					Value.pop_back();
				}
				Result = true;
			}
			return Result;
		}

		bool GetValue(const key& Key, const wchar_t* Name, unsigned int& Value)
		{
			bool Result = false;
			std::vector<char> Buffer;
			DWORD Type;
			if (QueryValue(Key, Name, Type, Buffer) && Type == REG_DWORD)
			{
				Value = 0;
				memcpy(&Value, Buffer.data(), std::min(Buffer.size(), sizeof(Value)));
				Result = true;
			}
			return Result;
		}

		bool GetValue(const key& Key, const wchar_t* Name, unsigned long long& Value)
		{
			bool Result = false;
			std::vector<char> Buffer;
			DWORD Type;
			if (QueryValue(Key, Name, Type, Buffer) && Type == REG_QWORD)
			{
				Value = 0;
				memcpy(&Value, Buffer.data(), std::min(Buffer.size(), sizeof(Value)));
				Result = true;
			}
			return Result;
		}

		string value::GetString() const
		{
			if (!detail::IsStringType(m_Type))
				throw MAKE_FAR_EXCEPTION(L"Bad value type");

			string Result;
			GetValue(*m_Key, m_Name.data(), Result);
			return Result;
		}

		unsigned int value::GetUnsigned() const
		{
			if (m_Type != REG_DWORD)
				throw MAKE_FAR_EXCEPTION(L"Bad value type");

			unsigned int Result = 0;
			GetValue(*m_Key, m_Name.data(), Result);
			return Result;
		}

		unsigned long long value::GetUnsigned64() const
		{
			if (m_Type != REG_QWORD)
				throw MAKE_FAR_EXCEPTION(L"Bad value type");

			unsigned long long Result = 0;
			GetValue(*m_Key, m_Name.data(), Result);
			return Result;
		}
	}

	namespace env
	{
		provider::strings::strings()
		{
			m_Data = GetEnvironmentStrings();
		}

		provider::strings::~strings()
		{
			if (m_Data)
			{
				FreeEnvironmentStrings(m_Data);
			}
		}

		provider::block::block()
		{
			m_Data = nullptr;
			handle TokenHandle;
			if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &ptr_setter(TokenHandle)))
			{
				CreateEnvironmentBlock(reinterpret_cast<void**>(&m_Data), TokenHandle.native_handle(), TRUE);
			}
		}

		provider::block::~block()
		{
			if (m_Data)
			{
				DestroyEnvironmentBlock(m_Data);
			}
		}

		bool get_variable(const wchar_t* Name, string& strBuffer)
		{
			GuardLastError ErrorGuard;
			// GetEnvironmentVariable might return 0 not only in case of failure, but also when variable is empty.
			// To recognise this, we set LastError to ERROR_SUCCESS manually and check it after the call,
			// which doesn't change it upon success.
			SetLastError(ERROR_SUCCESS);
			if (ApiDynamicStringReceiver(strBuffer, [&](wchar_t* Buffer, size_t Size)
			{
				return ::GetEnvironmentVariable(Name, Buffer, static_cast<DWORD>(Size));
			}))
			{
				return true;
			}

			if (GetLastError() == ERROR_SUCCESS)
			{
				strBuffer.clear();
				return true;
			}
			// Something went wrong, it's better to leave the last error as is
			ErrorGuard.dismiss();
			return false;
		}

		bool set_variable(const wchar_t* Name, const wchar_t* Value)
		{
			return ::SetEnvironmentVariable(Name, Value) != FALSE;
		}

		bool delete_variable(const wchar_t* Name)
		{
			return ::SetEnvironmentVariable(Name, nullptr) != FALSE;
		}

		string expand_strings(const wchar_t* Str)
		{
			string Result;
			if (!ApiDynamicStringReceiver(Result, [&](wchar_t* Buffer, size_t Size)
			{
				auto ReturnedSize = ::ExpandEnvironmentStrings(Str, Buffer, static_cast<DWORD>(Size));
				// This pesky function includes the terminating null character even upon success, breaking the usual pattern
				if (ReturnedSize <= Size)
					--ReturnedSize;
				return ReturnedSize;
			}))
			{
				Result = Str;
			}
			return Result;
		}

		string get_pathext()
		{
			auto PathExt(os::env::get_variable(L"PATHEXT"));
			if (PathExt.empty())
				PathExt = L".COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC";
			return PathExt;
		}
	}

	namespace rtdl
	{
		void module::module_deleter::operator()(HMODULE Module) const
		{
			FreeLibrary(Module);
		}

		HMODULE module::get_module() const
		{
			if (!m_tried && !m_module && !m_name.empty())
			{
				m_tried = true;
				m_module.reset(LoadLibrary(m_name.data()));

				if (!m_module && m_AlternativeLoad && IsAbsolutePath(m_name))
				{
					m_module.reset(LoadLibraryEx(m_name.data(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
				}
				// TODO: log if nullptr
			}
			return m_module.get();
		}
	}

	namespace memory
	{
		bool is_pointer(const void* Address)
		{
			const auto& GetInfo = []{ SYSTEM_INFO Info; GetSystemInfo(&Info); return Info; };
			static const auto info = GetInfo();

			return InRange<const void*>(info.lpMinimumApplicationAddress, Address, info.lpMaximumApplicationAddress);
		}
	}

	namespace security
	{
		bool is_admin()
		{
			const auto& GetResult = []
			{
				SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
				const auto AdministratorsGroup = make_sid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS);
				if (!AdministratorsGroup)
					return false;

				BOOL IsMember;
				return CheckTokenMembership(nullptr, AdministratorsGroup.get(), &IsMember) && IsMember;
			};

			static const auto Result = GetResult();
			return Result;
		}
	}

	bool is_standard_drive_letter(wchar_t Letter)
	{
		return InRange(L'A', Upper(Letter), L'Z');
	}
	int get_drive_number(wchar_t Letter)
	{
		return Upper(Letter) - L'A';
	}
}

UUID CreateUuid()
{
	UUID Uuid;
	UuidCreate(&Uuid);
	return Uuid;
}

string GuidToStr(const GUID& Guid)
{
	string result;
	RPC_WSTR str;
	// declared as non-const in GCC headers :(
	if (UuidToString(const_cast<GUID*>(&Guid), &str) == RPC_S_OK)
	{
		SCOPE_EXIT{ RpcStringFree(&str); };
		result = reinterpret_cast<const wchar_t*>(str);
	}
	return Upper(result);
}

bool StrToGuid(const wchar_t* Value, GUID& Guid)
{
	return UuidFromString(reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(Value)), &Guid) == RPC_S_OK;
}
