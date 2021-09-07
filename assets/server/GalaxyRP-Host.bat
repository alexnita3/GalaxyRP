@echo off

:: Main settings
set jk_executable=openjkded.x86.exe
set jk_dedicated=2
set jk_net_port=29070
set jk_config=server.cfg

:: Executable check
if not exist "%cd%\..\%jk_executable%" (
	if exist "%cd%\..\jampDed.exe" (
		set jk_executable=jampDed.exe
	) else (
		set jk_executable=jamp.exe
	)
)

:: Launch options
cd..
%jk_executable% +set dedicated %jk_dedicated% +set net_port %jk_net_port% +set fs_game GalaxyRP +exec %jk_config%