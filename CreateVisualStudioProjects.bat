@REM Create OpenJK projects for Visual Studio using CMake
@echo off
:start
cls
echo OpenJK VS Project Generator
echo ---------------------------
echo Options available: 2013, 2015, 2017, 2019
set /p proj_option=Type your option: 

if "%proj_option%" == "2013" ( 
	set proj_ver="Visual Studio 12 2013"
) else if "%proj_option%" == "2015" ( 
	set proj_ver="Visual Studio 14 2015"
) else if "%proj_option%" == "2017" ( 
	set proj_ver="Visual Studio 15 2017"
) else if "%proj_option%" == "2019" ( 
	set proj_ver="Visual Studio 16 2019"
) else if "%proj_option%" == "2022" ( 
	set proj_ver="Visual Studio 17 2022"
) else ( 
	echo Invalid option!
	pause
	goto :start 
)
echo Visual Studio %proj_option% selected!
echo ---------------------------

for %%X in (cmake.exe) do (set FOUND=%%~$PATH:X)
if not defined FOUND (
	echo CMake was not found on your system. Please make sure you have installed CMake
	echo from http://www.cmake.org/ and cmake.exe is installed to your system's PATH
	echo environment variable.
	echo.
	pause
	exit /b 1
) else (
	echo Found CMake!
)
if not exist build\nul (mkdir build)
pushd build
cmake -G %proj_ver% -A Win32 -D CMAKE_INSTALL_PREFIX=../install ..
popd
pause