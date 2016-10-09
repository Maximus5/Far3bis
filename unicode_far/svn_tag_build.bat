@echo off
setlocal
set command=tools\m4 -P svn_tag_build_ssl.m4

if "%~1"=="" set mode=info && %command% && goto pause

set tag=%1%2_b%3
goto %mode%

:info
echo --------------------------------------------------------------------
echo Continue only if you are sure that you have set the correct
echo build and commited the changes.
echo This command will tag the trunk under tag/%tag%.
echo --------------------------------------------------------------------
echo If you're not sure press CtrlC.
echo --------------------------------------------------------------------
echo --------------------------------------------------------------------
echo �த������ ⮫쪮 �᫨ �� 㢥७�, �� �� ���⠢��� �ࠢ����
echo ����� ����� � �������⨫� ���������.
echo �� ������� ������ ⥪�騩 trunk � tags/%tag%.
echo --------------------------------------------------------------------
echo �᫨ �� �� 㢥७�, � ������ CtrlC
echo --------------------------------------------------------------------
goto :eof

:pause
pause
echo.

set mode=tag && %command% && goto :eof

:tag
for /f "tokens=3" %%f in ('svn info ^| find "Root:"') do set repo=%%f
set tag_path=%repo%/tags/unicode_far/%tag%

svn info %tag_path% > nul 2>&1 & (
	if not errorlevel 1 (
		echo Error: tag %tag% already exists
	) else (
		svn copy %repo%/trunk/unicode_far %tag_path% -m "tag build %3"
	)
)
endlocal
