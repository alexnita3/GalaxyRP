@echo off

:: Main executable
set jk_executable=openjk.x86.exe

:: Executable check
if not exist "%cd%\..\%jk_executable%" (
	set jk_executable=jamp.exe
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

:: Check options
if "%option%" == "" (
	cd..
	start "" %jk_executable% +set net_port 29070 +set dedicated 0 +set fs_game GalaxyRP +exec server.cfg
) else (
	cd..
	start "" %jk_executable% +set fs_game GalaxyRP +connect %option%
)