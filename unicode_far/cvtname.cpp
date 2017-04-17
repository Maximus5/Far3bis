﻿/*
cvtname.cpp

Функций для преобразования имен файлов/путей.
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

#include "cvtname.hpp"
#include "flink.hpp"
#include "pathmix.hpp"
#include "network.hpp"
#include "imports.hpp"
#include "strmix.hpp"
#include "elevation.hpp"
#include "local.hpp"
#include "drivemix.hpp"

static void MixToFullPath(string& strPath)
{
	//Skip all path to root (with slash if exists)
	size_t DirOffset = 0;
	ParsePath(strPath, &DirOffset);

	//Process "." and ".." if exists
	for (size_t Pos = DirOffset; Pos < strPath.size();)
	{
		//fragment "."
		if (strPath[Pos] == L'.' && (!Pos || IsSlash(strPath[Pos - 1])))
		{
			//fragment "." at the end
			if(strPath.size() == Pos + 1)
			{
				strPath.resize(Pos);
				// don't change x:\ to x:
				if (strPath[Pos - 2] != L':')
				{
					strPath.pop_back();
				}
				continue;
			}

			switch (strPath[Pos + 1])
			{
					//fragment ".\"
				case L'\\':
					//fragment "./"
				case L'/':
					strPath.erase(Pos, 2);
					continue;

				//fragment "..\" or "../" or ".." at the end
				case L'.':
				{
					if (Pos + 2 == strPath.size() || IsSlash(strPath[Pos + 2]))
					{
						//Calculate subdir name offset
						size_t n = strPath.find_last_of(L"\\/", Pos-2);
						n = (n == string::npos || n < DirOffset) ? DirOffset : n+1;

						//fragment "..\" or "../"
						if (Pos + 2 < strPath.size())
						{
							strPath.erase(n, Pos + 3 - n);
						}
						//fragment ".." at the end
						else
						{
							strPath.resize(n);
						}

						Pos = n;
						continue;
					}
				}
				break;
			}
		}

		++Pos;
	}
}

static void MixToFullPath(const string& stPath, string& Dest, const string& stCurrentDir)
{
		string strDest;
		const string* pstCurrentDir = nullptr;
		bool blIgnore = false;
		size_t PathDirOffset = 0;
		const auto PathType = ParsePath(stPath, &PathDirOffset);
		size_t PathOffset = PathDirOffset;
		switch (PathType)
		{
			case PATH_UNKNOWN:
			{
				if (HasPathPrefix(stPath)) // \\?\<ANY_UNKNOWN_FORMAT>
				{
					blIgnore = true;
				}
				else if (!stPath.empty() && IsSlash(stPath.front())) //"\" or "\abc"
				{
					++PathOffset;
					if (!stCurrentDir.empty())
					{
						size_t CurDirDirOffset = 0;
						if (ParsePath(stCurrentDir, &CurDirDirOffset) != PATH_UNKNOWN)
						{
							strDest = string(stCurrentDir.data(), CurDirDirOffset);
						}
					}
				}
				else //"abc" or whatever
				{
					pstCurrentDir = &stCurrentDir;
				}
			}
			break;
			case PATH_DRIVELETTER: //"C:" or "C:abc"
			{
				if(stPath.size() > 2 && IsSlash(stPath[2]))
				{
					PathOffset = 0;
				}
				else
				{
					const auto DriveVar = L'=' + os::fs::get_drive(stPath[0]);
					const auto strValue(os::env::get_variable(DriveVar));

					if (!strValue.empty())
					{
						strDest=strValue;
					}
					else
					{
						if (Upper(stPath[0])==Upper(stCurrentDir[0]))
						{
							strDest=stCurrentDir;
						}
						else
						{
							strDest=DriveVar.substr(1);
						}
					}
					AddEndSlash(strDest);
				}
			}
			break;
			case PATH_REMOTE: //"\\abc"
			{
				PathOffset = 0;
			}
			break;
			case PATH_DRIVELETTERUNC: //"\\?\whatever"
			case PATH_REMOTEUNC:
			case PATH_VOLUMEGUID:
			case PATH_PIPE:
			{
				blIgnore=true;
				PathOffset = 0;
			}
			break;
		}

		if (pstCurrentDir)
		{
			strDest += *pstCurrentDir;
			AddEndSlash(strDest);
		}

		strDest.append(stPath, PathOffset, string::npos);

		if (!blIgnore && !HasPathPrefix(strDest))
			MixToFullPath(strDest);

		Dest = std::move(strDest);
}

string ConvertNameToFull(const string& Object)
{
	string strDest;
	MixToFullPath(Object, strDest, os::GetCurrentDirectory());
	return strDest;
}

// try to replace volume GUID (if present) with drive letter
// used by ConvertNameToReal() only
static string TryConvertVolumeGuidToDrivePath(const string& Path, const wchar_t *path=nullptr, size_t path_len=0)
{
	string Result = Path;
	size_t DirectoryOffset;
	if (ParsePath(Path, &DirectoryOffset) == PATH_VOLUMEGUID)
	{
		if (Imports().GetVolumePathNamesForVolumeNameW)
		{
			wchar_t_ptr Buffer(os::NT_MAX_PATH);
			DWORD RetSize;
			bool Res = Imports().GetVolumePathNamesForVolumeName(ExtractPathRoot(Path).data(), Buffer.get(), static_cast<DWORD>(Buffer.size()), &RetSize) != FALSE;

			if (!Res && RetSize > Buffer.size())
			{
				Buffer.reset(RetSize);
				Res = Imports().GetVolumePathNamesForVolumeName(ExtractPathRoot(Path).data(), Buffer.get(), static_cast<DWORD>(Buffer.size()), &RetSize) != FALSE;
			}

			if (Res)
			{
				const wchar_t* PathName = Buffer.get();

				while (*PathName)
				{
					string strPath(PathName);

					if (path && strPath.size() <= path_len && 0 == StrCmpNI(path, PathName, strPath.size()))
						return strPath;

					if (IsRootPath(strPath))
					{
						Result.replace(0, DirectoryOffset, strPath);
						break;
					}

					PathName += strPath.size() + 1;
				}
			}

			if (path)
				Result.clear();
		}

		else if (path)
			Result.clear();

		else
		{
			string strVolumeGuid;
			const auto Enumerator = os::fs::enum_drives(os::fs::get_logical_drives());
			const auto ItemIterator = std::find_if(ALL_CONST_RANGE(Enumerator), [&](const auto& i)
			{
				return os::GetVolumeNameForVolumeMountPoint(os::fs::get_drive(i), strVolumeGuid) && Path.compare(0, DirectoryOffset, strVolumeGuid.data(), DirectoryOffset) == 0;
			});
			if (ItemIterator != Enumerator.cend())
			{
				Result.replace(0, DirectoryOffset, os::fs::get_drive(*ItemIterator));
			}
		}
	}

	else if (path)
		Result.clear();

	return Result;
}

size_t GetMountPointLen(const string& abs_path, const string& drive_root)
{
	size_t n = drive_root.size();
	if (abs_path.size() >= n && 0 == StrCmpNI(abs_path.data(), drive_root.data(), n))
		return n;

	size_t dir_offset = 0;
	if (ParsePath(abs_path, &dir_offset) == PATH_VOLUMEGUID)
		return dir_offset;

	string vol_guid(drive_root);
	switch (ParsePath(drive_root))
	{
	case PATH_VOLUMEGUID:
		break;
	case PATH_DRIVELETTER:
		if (os::GetVolumeNameForVolumeMountPoint(drive_root, vol_guid))
			break;
		// else fall down to default:
	default:
		return 0;
	}

	string mount_point = TryConvertVolumeGuidToDrivePath(vol_guid, abs_path.data(), abs_path.size());
	return mount_point.size();
}

/*
  Преобразует Src в полный РЕАЛЬНЫЙ путь с учетом reparse point.
  Note that Src can be partially non-existent.
*/
string ConvertNameToReal(const string& Object)
{
	SCOPED_ACTION(elevation::suppress);

	// Получим сначала полный путь до объекта обычным способом
	const auto FullPath = ConvertNameToFull(Object);
	auto strDest = FullPath;

	string Path = FullPath;
	os::handle File;

	for (;;)
	{
		if ((File = os::CreateFile(Path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0)))
			break;

		if (IsRootPath(Path))
			break;

		Path = ExtractFilePath(Path);
	}

	if (File)
	{
		string FinalFilePath;
		os::GetFinalPathNameByHandle(File.native_handle(), FinalFilePath);
		File.close();

		//assert(!FinalFilePath.empty());

		if (!FinalFilePath.empty())
		{
			// append non-existent path part (if present)
			DeleteEndSlash(Path);

			if (FullPath.size() > Path.size() + 1)
			{
				AddEndSlash(FinalFilePath);
				FinalFilePath.append(FullPath, Path.size() + 1, string::npos);
			}

			FinalFilePath = TryConvertVolumeGuidToDrivePath(FinalFilePath);
			strDest = FinalFilePath;
		}
	}
	return strDest;
}

string ConvertNameToShort(const string& Object)
{
	string strDest;
	const auto& GetShortName = [&strDest](const string& str)
	{
		wchar_t Buffer[MAX_PATH];
		if (auto Size = GetShortPathName(str.data(), Buffer, static_cast<DWORD>(std::size(Buffer))))
		{
			if (Size < std::size(Buffer))
			{
				strDest.assign(Buffer, Size);
			}
			else
			{
				wchar_t_ptr vBuffer(Size);
				Size = GetShortPathName(str.data(), vBuffer.get(), Size);
				strDest.assign(vBuffer.get(), Size);
			}
			return true;
		}
		return false;
	};

	if(!GetShortName(Object))
	{
		strDest = Object;

		bool Prefixed = HasPathPrefix(Object);
		if (!Prefixed)
		{
			if (GetShortName(NTPath(Object)))
			{
				switch (ParsePath(strDest))
				{
				case PATH_DRIVELETTERUNC:
					strDest = strDest.substr(4); // \\?\X:\path -> X:\path
					break;

				case PATH_REMOTEUNC:
					strDest.erase(2, 6); // \\?\UNC\server -> \\server
					break;

				default:
					// should never happen
					break;
				}
			}
		}
	}
	return strDest;
}

string ConvertNameToLong(const string& Object)
{
	wchar_t Buffer[MAX_PATH];
	if (auto Size = GetLongPathName(Object.data(), Buffer, static_cast<DWORD>(std::size(Buffer))))
	{
		if (Size < std::size(Buffer))
		{
			return { Buffer, Size };
		}
		else
		{
			wchar_t_ptr vBuffer(Size);
			Size = GetLongPathName(Object.data(), vBuffer.get(), Size);
			return { vBuffer.get(), Size };
		}
	}
	return Object;
}

string ConvertNameToUNC(const string& Object)
{
	auto strFileName = ConvertNameToFull(Object);
	// Посмотрим на тип файловой системы
	string strFileSystemName;
	os::GetVolumeInformation(GetPathRoot(strFileName), nullptr, nullptr, nullptr, nullptr, &strFileSystemName);

	DWORD uniSize = 1024;
	block_ptr<UNIVERSAL_NAME_INFO> uni(uniSize);

	// применяем WNetGetUniversalName для чего угодно, только не для Novell`а
	if (StrCmpI(strFileSystemName.data(),L"NWFS"))
	{
		DWORD dwRet=WNetGetUniversalName(strFileName.data(),UNIVERSAL_NAME_INFO_LEVEL,uni.get(),&uniSize);

		switch (dwRet)
		{
			case NO_ERROR:
				strFileName = uni->lpUniversalName;
				break;

			case ERROR_MORE_DATA:
				uni.reset(uniSize);
				if (WNetGetUniversalName(strFileName.data(),UNIVERSAL_NAME_INFO_LEVEL,uni.get(),&uniSize)==NO_ERROR)
					strFileName = uni->lpUniversalName;
				break;
		}
	}
	else if (strFileName.size() > 1 && strFileName[1] == L':')
	{
		// BugZ#449 - Неверная работа CtrlAltF с ресурсами Novell DS
		// Здесь, если не получилось получить UniversalName и если это
		// мапленный диск - получаем как для меню выбора дисков
		string strTemp;
		if (DriveLocalToRemoteName(DRIVE_UNKNOWN,strFileName[0],strTemp))
		{
			const auto SlashPos = FindSlash(strFileName);
			if (SlashPos != string::npos)
			{
				AddEndSlash(strTemp);
				strTemp += strFileName.substr(SlashPos + 1);
			}

			strFileName = strTemp;
		}
	}

	return ConvertNameToReal(strFileName);
}

// Косметические преобразования строки пути.
// CheckFullPath используется в FCTL_SET[ANOTHER]PANELDIR
void PrepareDiskPath(string &strPath, bool CheckFullPath)
{
	// elevation not required during cosmetic operation
	SCOPED_ACTION(elevation::suppress);

	if (!strPath.empty())
	{
		if (strPath.size() > 1 && (strPath[1]==L':' || (IsSlash(strPath[0]) && IsSlash(strPath[1]))))
		{
			ReplaceSlashToBackslash(strPath);
			bool DoubleSlash = strPath[1]==L'\\';
			while(ReplaceStrings(strPath,L"\\\\",L"\\"))
				;
			if(DoubleSlash)
			{
				strPath = L"\\" + strPath;
			}

			if (CheckFullPath)
			{
				strPath = ConvertNameToFull(strPath);

				size_t DirOffset = 0;
				const auto Type = ParsePath(strPath, &DirOffset);
				if (Type == PATH_UNKNOWN && HasPathPrefix(strPath))
				{
					DirOffset = 4;
				}

				size_t StartPos = DirOffset;

				if (StartPos < strPath.size())
				{
					string TmpStr;
					TmpStr.reserve(strPath.size());
					size_t LastPos = StartPos;
					bool EndsWithSlash = IsSlash(strPath.back());

					for (size_t i = StartPos; i <= strPath.size(); ++i)
					{
						if ((i < strPath.size() && IsSlash(strPath[i])) || (i == strPath.size() && !EndsWithSlash))
						{
							TmpStr = strPath.substr(0, i);
							os::FAR_FIND_DATA fd;

							if (os::GetFindDataEx(TmpStr, fd))
							{
								strPath.replace(LastPos, i - LastPos, fd.strFileName);
								i += fd.strFileName.size() - (i - LastPos);
							}

							if (i != strPath.size())
							{
								LastPos = i + 1;
							}
						}
					}
				}
			}

			if (ParsePath(strPath) == PATH_DRIVELETTER)
			{
				strPath[0] = Upper(strPath[0]);
			}
		}
	}
}
