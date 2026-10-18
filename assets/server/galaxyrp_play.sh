#!/bin/sh

# galaxyrp_play.sh -- Linux client launcher

# Main settings
jk_executable_64="taystjk.x86_64"
jk_executable_32="taystjk.i386"
jk_executable_arm64="taystjk.arm64"

# GalaxyRP: [TaystJK] this script lives inside the GalaxyRP folder, one level below the engine
# executables -- switch to that parent folder FIRST, using the script's own location (not whatever
# directory the script happened to be launched from). This makes both the executable check below
# and the absolute homepath computed further down resolve against the engine's own folder, not
# wherever the script's working directory started out.
cd "$(dirname "$0")/.." || exit 1
ENGINE_DIR="$(pwd)"

# Executable check -- by this machine's architecture first (uname -m), so a build it cannot run is
# never picked. A 64-bit x86 system prefers the 64-bit build and falls back to the 32-bit one (it runs
# 32-bit programs too, given the 32-bit libraries); a 32-bit system looks for the 32-bit build only, as
# a 64-bit one fails there with "Exec format error"; an ARM system looks for an arm64 build, which
# TaystJK does not publish but makes when built from source there (GalaxyRP's own Linux arm64 game
# modules run with it); any other machine is tried as before, 64-bit then 32-bit. Bail out with an
# error naming what was looked for instead of trying to launch something that isn't there.
jk_arch="$(uname -m 2>/dev/null)"
case "$jk_arch" in
	x86_64|amd64)
		jk_candidates="$jk_executable_64 $jk_executable_32"
		;;
	i386|i486|i586|i686)
		jk_candidates="$jk_executable_32"
		;;
	aarch64|arm64)
		jk_candidates="$jk_executable_arm64"
		;;
	*)
		jk_candidates="$jk_executable_64 $jk_executable_32"
		;;
esac

jk_executable=""
jk_looked_for=""
for jk_candidate in $jk_candidates; do
	if [ -z "$jk_executable" ] && [ -f "$jk_candidate" ]; then
		jk_executable="$jk_candidate"
	fi
	jk_looked_for="${jk_looked_for:+$jk_looked_for or }$jk_candidate"
done

if [ -z "$jk_executable" ]; then
	echo "ERROR: Could not find $jk_looked_for next to the GalaxyRP folder (this machine: ${jk_arch:-unknown})."
	case "$jk_arch" in
		i386|i486|i586|i686)
			if [ -f "$jk_executable_64" ]; then
				echo "$jk_executable_64 is there, but a 64-bit build cannot run on this 32-bit system: install the i386 build."
			fi
			;;
		aarch64|arm64)
			echo "TaystJK publishes no Linux ARM builds: build TaystJK from source on this machine, which makes $jk_executable_arm64."
			;;
	esac
	echo "Make sure a TaystJK client build for this machine is installed alongside GalaxyRP."
	printf "Press Enter to exit..."
	read -r _
	exit 1
fi

chmod +x "$jk_executable" 2>/dev/null

# Welcome message
printf '%s\n' '  _____________________________________________________'
printf '%s\n' ' | ___________________________________________________ |'
printf '%s\n' ' ||   _____       _                    _____  _____   ||'
printf '%s\n' ' ||  / ____|     | |                  |  __ \|  __ \  ||'
printf '%s\n' ' || | |  __  __ _| | __ ___  ___   _  | |__) | |__) | ||'
printf '%s\n' ' || | | |_ |/ _` | |/ _` \ \/ / | | | |  _  /|  ___/  ||'
printf '%s\n' ' || | |__| | (_| | | (_| |>  <| |_| | | | \ \| |      ||'
printf '%s\n' ' ||  \_____|\__,_|_|\__,_/_/\_\\__, | |_|  \_\_|      ||'
printf '%s\n' ' ||                             __/ |                 ||'
printf '%s\n' ' ||        ___________________ |___/ _______          ||'
printf '%s\n' ' ||                                                   ||'
printf '%s\n' ' ||               A JKA ROLEPLAYING MOD               ||'
printf '%s\n' ' ||___________________________________________________||'
printf '%s\n' ' |_____________________________________________________|'
printf '\n'

# Show options
echo "[1] Press ENTER to play locally"
printf "[2] Type or paste an IP to connect: "
read -r option

# Launch (detached, so this script/terminal returns immediately -- matches the Windows client
# script's "start ""). fs_portable 1 + an absolute fs_homepath keep every file the client writes
# (settings, saved profiles, screenshots) inside this folder instead of the user's home directory.
# GalaxyRP fix: [TaystJK] the TaystJK client (unlike the dedicated server) defaults fs_forcegame
# to "taystjk", which unconditionally overwrites fs_gamedir back to "taystjk" at the end of
# FS_Startup -- after fs_game GalaxyRP has already added GalaxyRP to the search path, but before
# the engine decides where to actually WRITE files. Game content still loads fine from GalaxyRP
# (it's on the search path), but screenshots, saved configs, and demos would silently land in
# <fs_homepath>/taystjk/ instead of GalaxyRP/. Setting fs_forcegame explicitly to GalaxyRP here
# neutralizes that override (fs_forcegame ends up equal to fs_gamedir, so the override no-ops)
# and makes GalaxyRP the actual write target too.
if [ -z "$option" ]; then
	nohup "./$jk_executable" +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +set fs_forcegame GalaxyRP >/dev/null 2>&1 &
else
	nohup "./$jk_executable" +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +set fs_forcegame GalaxyRP +connect "$option" >/dev/null 2>&1 &
fi
