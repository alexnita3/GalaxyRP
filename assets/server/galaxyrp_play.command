#!/bin/sh

# galaxyrp_play.command -- macOS client launcher

# Main settings
jk_executable_x64="taystjk.x86_64.app"
jk_executable_arm64="taystjk.arm64.app"
jk_executable_universal="taystjk.app"

# GalaxyRP: [TaystJK] this script lives inside the GalaxyRP folder, one level below the engine
# executables -- switch to that parent folder FIRST, using the script's own location (not whatever
# directory Finder/Terminal happened to start it from). This makes both the executable check below
# and the absolute homepath computed further down resolve against the engine's own folder, not
# wherever the script's working directory started out.
cd "$(dirname "$0")/.." || exit 1
ENGINE_DIR="$(pwd)"

# GalaxyRP: [TaystJK] prefer whichever arch-specific build matches this Mac's actual CPU (fastest,
# runs native, no Rosetta) before falling back to the universal build. Deliberately NOT falling
# back to the *other* arch-specific build: on an arm64 Mac that would mean running the x86_64
# build under Rosetta instead of the native slice already inside the universal build, and on an
# x86_64 Mac the arm64 build wouldn't run at all -- so universal is always the better second choice.
# GalaxyRP: [TaystJK] the CPU is asked through sysctl hw.optional.arm64 first: it is 1 on an Apple
# Silicon Mac even when this script runs in a Terminal under Rosetta, where uname -m answers x86_64 --
# which picked the Intel build there and ran the whole game translated. Intel Macs answer 0, or nothing
# on older macOS, and uname -m decides as before.
jk_mac_arch="$(uname -m 2>/dev/null)"
if [ "$(sysctl -in hw.optional.arm64 2>/dev/null)" = "1" ]; then
	jk_mac_arch=arm64
fi
case "$jk_mac_arch" in
	arm64)
		jk_executable_native="$jk_executable_arm64"
		;;
	*)
		jk_executable_native="$jk_executable_x64"
		;;
esac

# Executable check -- prefer the native-arch build, fall back to the universal build, and bail out
# with an error instead of silently trying to launch something that isn't there.
if [ -d "$jk_executable_native" ]; then
	jk_executable="$jk_executable_native"
elif [ -d "$jk_executable_universal" ]; then
	jk_executable="$jk_executable_universal"
else
	echo "ERROR: Could not find $jk_executable_native or $jk_executable_universal next to the GalaxyRP folder."
	echo "Make sure a TaystJK client build is installed alongside GalaxyRP."
	printf "Press Enter to exit..."
	read -r _
	exit 1
fi

# Clear the quarantine flag TaystJK's own installer script strips too, so macOS doesn't block a
# freshly-downloaded (not App-Store, ad-hoc signed) build with a Gatekeeper prompt.
xattr -dr com.apple.quarantine "$jk_executable" 2>/dev/null
chmod +x "$jk_executable/Contents/MacOS/"* 2>/dev/null

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

# Launch. fs_portable 1 + an absolute fs_homepath keep every file the client writes (settings,
# saved profiles, screenshots) inside this folder instead of ~/Library/Application Support -- an
# absolute path is used here (rather than ".") because "open" does not guarantee the launched app
# inherits this script's working directory.
# GalaxyRP fix: [TaystJK] the TaystJK client (unlike the dedicated server) defaults fs_forcegame
# to "taystjk", which unconditionally overwrites fs_gamedir back to "taystjk" at the end of
# FS_Startup -- after fs_game GalaxyRP has already added GalaxyRP to the search path, but before
# the engine decides where to actually WRITE files. Game content still loads fine from GalaxyRP
# (it's on the search path), but screenshots, saved configs, and demos would silently land in
# <fs_homepath>/taystjk/ instead of GalaxyRP/. Setting fs_forcegame explicitly to GalaxyRP here
# neutralizes that override (fs_forcegame ends up equal to fs_gamedir, so the override no-ops)
# and makes GalaxyRP the actual write target too.
if [ -z "$option" ]; then
	open "$jk_executable" --args +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +set fs_forcegame GalaxyRP
else
	open "$jk_executable" --args +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +set fs_forcegame GalaxyRP +connect "$option"
fi
