﻿/*
drivemix.cpp

Misc functions for drive/disk info
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

#include "drivemix.hpp"
#include "config.hpp"
#include "flink.hpp"
#include "cddrv.hpp"
#include "strmix.hpp"
#include "pathmix.hpp"

/*
  FarGetLogicalDrives
  оболочка вокруг GetLogicalDrives, с учетом скрытых логических дисков
  <HKCU|HKLM>\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer
  NoDrives:DWORD
    Последние 26 бит определяют буквы дисков от A до Z (отсчет справа налево).
    Диск виден при установленном 0 и скрыт при значении 1.
    Диск A представлен правой последней цифрой при двоичном представлении.
    Например, значение 00000000000000000000010101(0x7h)
    скрывает диски A, C, и E
*/
os::drives_set FarGetLogicalDrives()
{
	static unsigned int LogicalDrivesMask = 0;
	unsigned int NoDrives=0;

	if ((!Global->Opt->RememberLogicalDrives) || !LogicalDrivesMask)
		LogicalDrivesMask=GetLogicalDrives();

	if (!Global->Opt->Policies.ShowHiddenDrives)
	{
		static const HKEY Roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
		std::any_of(CONST_RANGE(Roots, i)
		{
			return os::reg::GetValue(i, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", L"NoDrives", NoDrives);
		});
	}

	return static_cast<int>(LogicalDrivesMask&(~NoDrives));
}

int CheckDisksProps(const string& SrcPath,const string& DestPath,int CheckedType)
{
	DWORD SrcVolumeNumber=0, DestVolumeNumber=0;
	string strSrcVolumeName, strDestVolumeName;
	DWORD SrcFileSystemFlags, DestFileSystemFlags;
	const auto strSrcRoot = GetPathRoot(ConvertNameToUNC(SrcPath));
	const auto strDestRoot = GetPathRoot(ConvertNameToUNC(DestPath));
	int DestDriveType=FAR_GetDriveType(strDestRoot, TRUE);

	if (!os::GetVolumeInformation(strSrcRoot, &strSrcVolumeName, &SrcVolumeNumber, nullptr, &SrcFileSystemFlags, nullptr))
		return FALSE;

	if (!os::GetVolumeInformation(strDestRoot, &strDestVolumeName, &DestVolumeNumber, nullptr, &DestFileSystemFlags, nullptr))
		return FALSE;

	if (CheckedType == CHECKEDPROPS_ISSAMEDISK)
	{
		if (DestPath.find_first_of(L"\\:") == string::npos)
			return TRUE;

		if (((IsSlash(strSrcRoot[0]) && IsSlash(strSrcRoot[1])) || (IsSlash(strDestRoot[0]) && IsSlash(strDestRoot[1]))) &&
		        StrCmpI(strSrcRoot, strDestRoot))
			return FALSE;

		if (SrcPath.empty() || DestPath.empty() || (SrcPath[1]!=L':' && DestPath[1]!=L':'))  //????
			return TRUE;

		if (Upper(strDestRoot[0])==Upper(strSrcRoot[0]))
			return TRUE;

		unsigned __int64 SrcTotalSize, DestTotalSize;

		if (!os::GetDiskSize(SrcPath, &SrcTotalSize, nullptr, nullptr))
			return FALSE;

		if (!os::GetDiskSize(DestPath, &DestTotalSize, nullptr, nullptr))
			return FALSE;

		if (!SrcVolumeNumber
			|| SrcVolumeNumber != DestVolumeNumber
			|| StrCmpI(strSrcVolumeName, strDestVolumeName)
			|| SrcTotalSize != DestTotalSize)
			return FALSE;
	}
	else if (CheckedType == CHECKEDPROPS_ISDST_ENCRYPTION)
	{
		if (!(DestFileSystemFlags&FILE_SUPPORTS_ENCRYPTION))
			return FALSE;

		if (!(DestDriveType==DRIVE_REMOVABLE || DestDriveType==DRIVE_FIXED || DestDriveType==DRIVE_REMOTE))
			return FALSE;
	}

	return TRUE;
}

bool IsDriveTypeRemote(UINT DriveType)
{
	return DriveType == DRIVE_REMOTE || DriveType == DRIVE_REMOTE_NOT_CONNECTED;
}
