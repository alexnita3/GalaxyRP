#!/bin/sh

# galaxyrp_host.command -- macOS dedicated server launcher

# Main settings
jk_dedicated=2
jk_net_port=29070
jk_config=galaxyrp_server.cfg
jk_executable_x64="taystjkded.x86_64"
jk_executable_arm64="taystjkded.arm64"
jk_executable_universal="taystjkded"

# Optional: one of this machine's own IP addresses, to bind the server to it alone. Leave it empty
# (the default) and the server listens on all of this machine's addresses, which is what nearly every
# server wants. Set it only on a machine with several network interfaces. An address the machine does
# not have -- a typo, or the public IP of a server behind a router -- leaves the server unable to bind
# at all, so nobody can connect.
jk_net_ip=""

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
if [ -f "$jk_executable_native" ]; then
	jk_executable="$jk_executable_native"
elif [ -f "$jk_executable_universal" ]; then
	jk_executable="$jk_executable_universal"
else
	echo "ERROR: Could not find $jk_executable_native or $jk_executable_universal next to the GalaxyRP folder."
	echo "Make sure a TaystJK dedicated server build is installed alongside GalaxyRP."
	printf "Press Enter to exit..."
	read -r _
	exit 1
fi

# Clear the quarantine flag TaystJK's own installer script strips too, so macOS doesn't block a
# freshly-downloaded (not App-Store, ad-hoc signed) build from running.
xattr -d com.apple.quarantine "$jk_executable" 2>/dev/null
chmod +x "$jk_executable" 2>/dev/null

# The optional net_ip, as arguments of its own: none at all when it is empty
if [ -n "$jk_net_ip" ]; then
	set -- +set net_ip "$jk_net_ip"
else
	set --
fi

# Launch. fs_portable 1 + an absolute fs_homepath keep every file the server writes (config
# changes, the accounts database, logs) inside this server folder instead of
# ~/Library/Application Support.
"./$jk_executable" +set dedicated "$jk_dedicated" +set net_port "$jk_net_port" "$@" +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +exec "$jk_config"
