#!/bin/sh

# galaxyrp_host_autorestart.sh -- Linux dedicated server launcher with auto-restart
#
# Same as galaxyrp_host.sh, but if the server dies it is brought straight back up. A clean
# shutdown (/quit, or any exit with status 0) ends the loop instead, so the server stays
# stoppable without reaching for kill. Every restart is appended to galaxyrp_restart.log
# next to this script.

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

# Auto-restart settings
jk_restart_delay=5			# seconds to wait before bringing the server back up
jk_restart_min_uptime=10	# a run shorter than this counts as a failed start, not a crash
jk_restart_max_failures=5	# consecutive failed starts before giving up
jk_restart_log=galaxyrp_restart.log
jk_restart_log_max=1048576	# rotate the log once it passes 1 MB

# GalaxyRP: [TaystJK] the restart log belongs next to THIS script, in the GalaxyRP folder --
# which is also where the server's own logs land, since fs_homepath below points at the engine
# folder and fs_game puts everything the server writes under GalaxyRP/. Resolve it before the
# cd on the next line, because after that the working directory is the engine folder.
jk_script_dir="$(cd "$(dirname "$0")" && pwd)" || exit 1
jk_restart_log_path="$jk_script_dir/$jk_restart_log"

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

# Append one line to the restart log, rotating it first if it has grown past the cap. A server
# that crash-loops for a week should not be able to fill the disk with its own restart log.
jk_log() {
	if [ -f "$jk_restart_log_path" ]; then
		jk_log_size="$(wc -c < "$jk_restart_log_path" 2>/dev/null)" || jk_log_size=0
		[ -n "$jk_log_size" ] || jk_log_size=0
		if [ "$jk_log_size" -gt "$jk_restart_log_max" ]; then
			mv -f "$jk_restart_log_path" "$jk_restart_log_path.old" 2>/dev/null
		fi
	fi
	printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$1" >> "$jk_restart_log_path" 2>/dev/null
}

# Seconds as a short human figure: 4h12m, 3m07s, 8s. Uptime is the single most useful number in
# the log -- it separates "one bad map" from "this server never came up at all".
jk_format_uptime() {
	jk_fu_t="$1"
	jk_fu_h=$((jk_fu_t / 3600))
	jk_fu_m=$(((jk_fu_t % 3600) / 60))
	jk_fu_s=$((jk_fu_t % 60))
	if [ "$jk_fu_h" -gt 0 ]; then
		printf '%dh%02dm' "$jk_fu_h" "$jk_fu_m"
	elif [ "$jk_fu_m" -gt 0 ]; then
		printf '%dm%02ds' "$jk_fu_m" "$jk_fu_s"
	else
		printf '%ds' "$jk_fu_s"
	fi
}

# Ctrl-C reaches the server and this wrapper alike, because they share the terminal's foreground
# process group. Without this trap the shell would carry on to the restart logic and bring the
# server straight back up -- the opposite of what the operator just asked for.
trap 'echo; echo "Stopped from the console. Not restarting."; jk_log "wrapper stopped from the console"; exit 0' INT TERM

# The optional net_ip, as arguments of its own: none at all when it is empty
if [ -n "$jk_net_ip" ]; then
	set -- +set net_ip "$jk_net_ip"
else
	set --
fi

jk_restart_count=0
jk_fast_failures=0

jk_log "wrapper started -- executable $jk_executable, port $jk_net_port"
echo "GalaxyRP auto-restart wrapper. Restart log: $jk_restart_log_path"
echo "A clean shutdown (/quit) stops the wrapper; a crash restarts it after ${jk_restart_delay}s."

while : ; do
	jk_started="$(date +%s 2>/dev/null)" || jk_started=0
	[ -n "$jk_started" ] || jk_started=0

	# Launch. fs_portable 1 + an absolute fs_homepath keep every file the server writes (config
	# changes, the accounts database, logs) inside this server folder instead of the user's home
	# directory.
	"./$jk_executable" +set dedicated "$jk_dedicated" +set net_port "$jk_net_port" "$@" +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +exec "$jk_config"
	jk_status=$?

	jk_ended="$(date +%s 2>/dev/null)" || jk_ended="$jk_started"
	[ -n "$jk_ended" ] || jk_ended="$jk_started"
	jk_run_time=$((jk_ended - jk_started))
	[ "$jk_run_time" -ge 0 ] || jk_run_time=0
	jk_uptime_text="$(jk_format_uptime "$jk_run_time")"

	if [ "$jk_status" -eq 0 ]; then
		jk_log "clean exit after $jk_uptime_text -- not restarting"
		echo "Server exited cleanly after $jk_uptime_text. Not restarting."
		exit 0
	fi

	jk_restart_count=$((jk_restart_count + 1))

	# A server that dies within seconds is not crashing under load -- it is failing to start at
	# all: a bad cvar in the .cfg, a missing pk3, a port already taken. Restarting that as fast
	# as the loop can go achieves nothing except a pinned CPU and a log that eats the disk.
	if [ "$jk_run_time" -lt "$jk_restart_min_uptime" ]; then
		jk_fast_failures=$((jk_fast_failures + 1))
	else
		jk_fast_failures=0
	fi

	if [ "$jk_fast_failures" -ge "$jk_restart_max_failures" ]; then
		jk_log "giving up after $jk_fast_failures starts that each lasted under ${jk_restart_min_uptime}s -- last exit=$jk_status"
		echo
		echo "ERROR: the server has failed to stay up $jk_fast_failures times in a row,"
		echo "each time for less than ${jk_restart_min_uptime} seconds. The last exit status was $jk_status."
		echo "That is a startup problem, not a crash -- check $jk_config, the port, and the"
		echo "server's own console output above. Not restarting again."
		printf "Press Enter to exit..."
		read -r _
		exit 1
	fi

	jk_log "restart #$jk_restart_count exit=$jk_status after $jk_uptime_text"
	echo "Server exited with status $jk_status after $jk_uptime_text -- restarting in ${jk_restart_delay}s..."
	sleep "$jk_restart_delay"
done
