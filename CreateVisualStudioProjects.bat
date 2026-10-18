@REM Create OpenJK projects for Visual Studio using CMake
@echo off
:start
cls
echo OpenJK VS Project Generator
echo ---------------------------
echo Options available: 2013, 2015, 2017, 2019, 2022, 2026
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
) else if "%proj_option%" == "2026" ( 
	set proj_ver="Visual Studio 18 2026"
) else ( 
	echo Invalid option!
	pause
	goto :start 
)
echo Visual Studio %proj_option% selected!
echo ---------------------------

@REM GalaxyRP fix: [multi-arch] prompt for target architecture instead of always hardcoding
@REM 32-bit (-A Win32) -- pick x86 for a 32-bit build, x64 for 64-bit.
:arch
echo Options available: x86, x64
set /p arch_option=Type your architecture:

if "%arch_option%" == "x86" (
	set proj_arch=Win32
) else if "%arch_option%" == "x64" (
	set proj_arch=x64
) else (
	echo Invalid option!
	pause
	goto :arch
)
echo %arch_option% selected!
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
@REM GalaxyRP fix: [multi-arch] build into a per-architecture directory -- CMake won't let you
@REM reconfigure an existing cache with a different -A platform, so x86 and x64 need separate
@REM build folders (and each gets its own install\ output, keeping GalaxyRP\ folders separate
@REM until you copy the per-arch DLLs together -- see README/deploy notes).
if not exist build-%arch_option%\nul (mkdir build-%arch_option%)
pushd build-%arch_option%
@REM GalaxyRP fix: [packaging] force BuildMPEngine/BuildMPRdVanilla/BuildMPDed OFF explicitly --
@REM we only want the GalaxyRP mod (jampgame/cgame/ui + assets), never the engine executable or
@REM its renderer DLL. -D flags override anything already cached in an existing build folder,
@REM so this also fixes a build directory where these were previously left ON.
cmake -G %proj_ver% -A %proj_arch% -D CMAKE_INSTALL_PREFIX=../install-%arch_option% -D BuildMPEngine=OFF -D BuildMPRdVanilla=OFF -D BuildMPDed=OFF -D BuildMPGame=ON -D BuildMPCGame=ON -D BuildMPUI=ON ..
popd
pause