#!/bin/sh

# galaxyrp_host.sh -- Linux dedicated server launcher

# Main settings
jk_dedicated=2
jk_net_port=29070
jk_config=galaxyrp_server.cfg
jk_executable_64="taystjkded.x86_64"
jk_executable_32="taystjkded.i386"
jk_executable_arm64="taystjkded.arm64"

# Optional: one of this machine's own IP addresses, to bind the server to it alone. Leave it empty
# (the default) and the server listens on all of this machine's addresses, which is what nearly every
# server wants. Set it only on a machine with several network interfaces. An address the machine does
# not have -- a typo, or the public IP of a server behind a router -- leaves the server unable to bind
# at all, so nobody can connect.
jk_net_ip=""

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
	echo "Make sure a TaystJK dedicated server build for this machine is installed alongside GalaxyRP."
	printf "Press Enter to exit..."
	read -r _
	exit 1
fi

chmod +x "$jk_executable" 2>/dev/null

# The optional net_ip, as arguments of its own: none at all when it is empty
if [ -n "$jk_net_ip" ]; then
	set -- +set net_ip "$jk_net_ip"
else
	set --
fi

# Launch. fs_portable 1 + an absolute fs_homepath keep every file the server writes (config
# changes, the accounts database, logs) inside this server folder instead of the user's home
# directory.
"./$jk_executable" +set dedicated "$jk_dedicated" +set net_port "$jk_net_port" "$@" +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +exec "$jk_config"
