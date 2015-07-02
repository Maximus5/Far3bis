call %~dp0base_64.bat
echo on
set PATH=%PATH%;c:\perl\perl\site\bin;c:\perl\perl\bin;c:\perl\c\bin;C:\unxutils\usr\local\wbin
set TERM=dumb

cd libs\openssl
rm -rf out32dll tmp32dll tmp32 inc32 out32
rm -rf x64
perl Configure VC-WIN64A no-unit-test no-cast no-err no-bf no-sctp no-rsax no-asm enable-static-engine no-shared no-hw no-camellia no-seed no-rc4 no-rc5 no-krb5 no-whirlpool no-srp no-gost no-idea no-ripemd -Ox -Ob1 -Oi -Os -Oy -GF -GS- -Gy  -DNDEBUG;OPENSSL_NO_CAPIENG;NO_CHMOD;OPENSSL_NO_DGRAM;OPENSSL_NO_RIJNDAEL;DSO_WIN32
call ms\do_win64a
nmake -f ms\nt.mak
mkdir x64
cp out32/ssleay32.lib out32/libeay32.lib x64
cp -R inc32 x64

cd ..\..\build\Release\x64

set FAR_VERSION=Far3
set PROJECT_ROOT=c:\src\Far-NetBox

set PROJECT_CONFIG=Release
set PROJECT_BUILD=Build

set PROJECT_CONF=x64
set PROJECT_PLATFORM=x64

c:\cmake\bin\cmake.exe -D PROJECT_ROOT=%PROJECT_ROOT% -D CMAKE_BUILD_TYPE=%PROJECT_CONFIG% -D CONF=%PROJECT_CONF% -D FAR_VERSION=%FAR_VERSION% %PROJECT_ROOT%\src\NetBox
nmake
