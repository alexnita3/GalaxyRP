@echo off

:: galaxyrp_host.bat -- Windows dedicated server launcher

:: Main settings
set jk_dedicated=2
set jk_net_port=29070
set jk_config=galaxyrp_server.cfg
set jk_executable_64=taystjkded.x86_64.exe
set jk_executable_32=taystjkded.x86.exe

:: Optional: one of this machine's own IP addresses, to bind the server to it alone. Leave it empty
:: (the default) and the server listens on all of this machine's addresses, which is what nearly
:: every server wants. Set it only on a machine with several network interfaces. An address the
:: machine does not have -- a typo, or the public IP of a server behind a router -- leaves the server
:: unable to bind at all, so nobody can connect.
set jk_net_ip=

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
	echo Make sure a TaystJK dedicated server build is installed alongside GalaxyRP.
	pause
	exit /b 1
)

:: The optional net_ip, as arguments of their own: none at all when it is empty
set jk_net_ip_args=
if defined jk_net_ip set jk_net_ip_args=+set net_ip %jk_net_ip%

:: Launch. fs_portable 1 + fs_homepath . keep every file the server writes (config changes, the
:: accounts database, logs) inside this server folder instead of the Windows user profile.
%jk_executable% +set dedicated %jk_dedicated% +set net_port %jk_net_port% %jk_net_ip_args% +set fs_portable 1 +set fs_homepath . +set fs_game GalaxyRP +exec %jk_config%
