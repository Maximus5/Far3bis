@echo off
setlocal

for /f "tokens=1-3" %%i in ('tools\m4 -P svn_tag_build.m4') do (
	set major=%%i
	set minor=%%j
	set build=%%k
)

set tag=%major%%minor%_b%build%

echo --------------------------------------------------------------------
echo Continue only if you are sure that you have set the correct
echo build and commited the changes.
echo This command will tag the trunk under tag/unicode_far/%tag%.
echo --------------------------------------------------------------------
echo If you're not sure press CtrlC.
echo --------------------------------------------------------------------
echo --------------------------------------------------------------------
echo �த������ ⮫쪮 �᫨ �� 㢥७�, �� �� ���⠢��� �ࠢ����
echo ����� ����� � �������⨫� ���������.
echo �� ������� ������ ⥪�騩 trunk � tags/unicode_far/%tag%.
echo --------------------------------------------------------------------
echo �᫨ �� �� 㢥७�, � ������ CtrlC
echo --------------------------------------------------------------------
pause
echo.

for /f "tokens=3" %%f in ('svn info ^| find "Root:"') do set repo=%%f

set tag_path=%repo%/tags/unicode_far/%tag%

svn info %tag_path% > nul 2>&1 & (
	if not errorlevel 1 (
		echo Error: tag %tag% already exists
	) else (
		svn copy %repo%/trunk/unicode_far %tag_path% -m "tag build %build%"
	)
)
endlocal
