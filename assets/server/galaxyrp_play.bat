@echo off

:: galaxyrp_play.bat -- Windows client launcher

:: Main settings
set jk_executable_64=taystjk.x86_64.exe
set jk_executable_32=taystjk.x86.exe

:: GalaxyRP fix: [TaystJK] this script lives inside the GalaxyRP folder, one level below the
:: engine executables -- switch to that parent folder FIRST, using %~dp0 (this batch file's own
:: location) rather than relying on whatever folder it happened to be double-clicked or launched
:: from. This makes both the executable check below and "fs_homepath ." further down resolve
:: against the engine's own folder, not wherever the script's working directory started out.
cd /d "%~dp0.."

:: Executable check -- by this machine's architecture first, so a build it cannot run is never
:: picked. PROCESSOR_ARCHITECTURE is what this command window runs as; PROCESSOR_ARCHITEW6432 is set
:: only when that is a 32-bit window on 64-bit Windows, and then holds the real one. 32-bit Windows
:: (x86) looks for the 32-bit build only -- a 64-bit one fails there with "This app can't run on your
:: PC". Everything else prefers the 64-bit build and falls back to the 32-bit one: 64-bit Windows
:: (AMD64), and Windows on ARM (ARM64), for which TaystJK has no build of its own -- Windows 11 on ARM
:: runs the 64-bit one, Windows 10 on ARM only the 32-bit one. Bail out with an error instead of
:: trying to launch something that isn't there.
set jk_arch=%PROCESSOR_ARCHITECTURE%
if defined PROCESSOR_ARCHITEW6432 set jk_arch=%PROCESSOR_ARCHITEW6432%
set jk_executable=
if /i "%jk_arch%"=="x86" (
	if exist "%jk_executable_32%" set jk_executable=%jk_executable_32%
) else if exist "%jk_executable_64%" (
	set jk_executable=%jk_executable_64%
) else if exist "%jk_executable_32%" (
	set jk_executable=%jk_executable_32%
)
if not defined jk_executable (
	if /i "%jk_arch%"=="x86" (
		echo ERROR: Could not find %jk_executable_32% next to the GalaxyRP folder ^(this is 32-bit Windows^).
		if exist "%jk_executable_64%" echo %jk_executable_64% is there, but a 64-bit build cannot run on 32-bit Windows: install the x86 build.
	) else (
		echo ERROR: Could not find %jk_executable_64% or %jk_executable_32% next to the GalaxyRP folder.
	)
	echo Make sure a TaystJK client build is installed alongside GalaxyRP.
	pause
	exit /b 1
)

:: Welcome message
echo   _____________________________________________________
echo "| ___________________________________________________ |"
echo "||   _____       _                    _____  _____   ||"
echo "||  / ____|     | |                  |  __ \|  __ \  ||"
echo "|| | |  __  __ _| | __ ___  ___   _  | |__) | |__) | ||"
echo "|| | | |_ |/ _` | |/ _` \ \/ / | | | |  _  /|  ___/  ||"
echo "|| | |__| | (_| | | (_| |>  <| |_| | | | \ \| |      ||"
echo "||  \_____|\__,_|_|\__,_/_/\_\\__, | |_|  \_\_|      ||"
echo "||                             __/ |                 ||"
echo "||        ___________________ |___/ _______          ||"
echo "||                                                   ||"
echo "||               A JKA ROLEPLAYING MOD               ||"
echo "||___________________________________________________||"
echo "|_____________________________________________________|"
echo.

:: Show options
echo [1] Press ENTER to play locally
set /p option=[2] Type or paste an IP to connect:

:: Check options. fs_portable 1 + fs_homepath . keep every file the client writes (settings,
:: saved profiles, screenshots) inside this folder instead of the Windows user profile.
:: GalaxyRP fix: [TaystJK] dropped "+set net_port 29070 +set dedicated 0 +exec server.cfg" from
:: the "play locally" branch -- those are dedicated-server-only settings (server.cfg isn't even
:: this mod's config filename, that's galaxyrp_server.cfg) that don't belong on a client launch
:: and appear to have been copy-pasted here by mistake.
:: GalaxyRP fix: [TaystJK] the TaystJK client (unlike the dedicated server) defaults fs_forcegame
:: to "taystjk", which unconditionally overwrites fs_gamedir back to "taystjk" at the end of
:: FS_Startup -- after fs_game GalaxyRP has already added GalaxyRP to the search path, but before
:: the engine decides where to actually WRITE files. Game content still loads fine from GalaxyRP
:: (it's on the search path), but screenshots, saved configs, and demos silently land in
:: <fs_homepath>/taystjk/ instead of GalaxyRP/. Setting fs_forcegame explicitly to GalaxyRP here
:: neutralizes that override (fs_forcegame ends up equal to fs_gamedir, so the override no-ops)
:: and makes GalaxyRP the actual write target too.
if "%option%" == "" (
	start "" %jk_executable% +set fs_portable 1 +set fs_homepath . +set fs_game GalaxyRP +set fs_forcegame GalaxyRP
) else (
	start "" %jk_executable% +set fs_portable 1 +set fs_homepath . +set fs_game GalaxyRP +set fs_forcegame GalaxyRP +connect %option%
)
