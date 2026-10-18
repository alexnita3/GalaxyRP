# Make sure the user is not executing this script directly
if(NOT InOpenJK)
	message(FATAL_ERROR "Use the top-level cmake script!")
endif(NOT InOpenJK)

# GalaxyRP fix: [packaging] read the mod's own version out of rp_version.h so the pk3 name
# stays in sync with the actual build (matches the "GalaxyRP_v<major>.<minor>.<bugfix>.pk3"
# naming used in past releases) instead of a fixed, unversioned "GalaxyRP.pk3".
file(STRINGS "${CMAKE_SOURCE_DIR}/galaxyrp/game/rp_version.h" RPVersionDefines
	REGEX "^#define[ \t]+VERSION_(MAJOR|MINOR|BUGFIX)_RELEASE[ \t]+[0-9]+")
foreach(RPVersionDefine ${RPVersionDefines})
	if(RPVersionDefine MATCHES "VERSION_MAJOR_RELEASE[ \t]+([0-9]+)")
		set(RPVersionMajor ${CMAKE_MATCH_1})
	elseif(RPVersionDefine MATCHES "VERSION_MINOR_RELEASE[ \t]+([0-9]+)")
		set(RPVersionMinor ${CMAKE_MATCH_1})
	elseif(RPVersionDefine MATCHES "VERSION_BUGFIX_RELEASE[ \t]+([0-9]+)")
		set(RPVersionBugfix ${CMAKE_MATCH_1})
	endif()
endforeach()
set(RPVersion "${RPVersionMajor}.${RPVersionMinor}.${RPVersionBugfix}")
message(STATUS "GalaxyRP version is ${RPVersion}")
# GalaxyRP fix: [packaging] the version above is read only when CMake configures, and editing the
# header did not make it configure again -- so an existing build folder kept naming the pk3 after
# the old version until CMake was re-run by hand. Listing the header as a configure dependency makes
# the next build (Ninja, or ZERO_CHECK in Visual Studio) re-run CMake by itself whenever it changes.
# It does not delete the previous version's pk3 from the output folder.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${CMAKE_SOURCE_DIR}/galaxyrp/game/rp_version.h")

# GalaxyRP fix: [packaging] assets/server carries a play/host launcher set for every OS this mod
# supports (galaxyrp_play/host/host_autorestart.bat for Windows, .sh for Linux, .command for
# macOS), committed side-by-side so they're easy to find and maintain together -- but a given
# build should only ever ship the 3 scripts for its OWN target platform, not all 9.
# CMAKE_SYSTEM_NAME already reflects the right platform for every build that matters here (native
# on each CI runner; explicitly set by both Linux cross-toolchain files under cmake/Toolchains/)
# with no extra flag needed. Both places below that copy assets/server content (the POST_BUILD
# dev-build copy and the install() for packaging/CI) drop the other platforms' scripts using this
# same list.
set(GalaxyRPAllLauncherFiles
	galaxyrp_play.bat galaxyrp_host.bat galaxyrp_host_autorestart.bat
	galaxyrp_play.sh galaxyrp_host.sh galaxyrp_host_autorestart.sh
	galaxyrp_play.command galaxyrp_host.command galaxyrp_host_autorestart.command)

# GalaxyRP fix: [packaging] the platform detection below is the DEFAULT, not an override: it only
# runs when GalaxyRPLauncherFiles has not already been set. Without this guard the message in the
# else() branch was giving advice that could not work -- a -D on the command line creates a CACHE
# variable, and the plain set() calls here create a NORMAL variable, which shadows the cache entry
# for the rest of this directory. The branch always won and the -D was silently discarded
# (confirmed: -DGalaxyRPLauncherFiles=galaxyrp_play.bat on Linux still installed the .sh set and
# still excluded every .bat). The else() branch also overwrote it on the line right after telling
# the user to set it. Now the advice is honoured, and on any platform rather than only the
# unrecognized one -- which also makes it possible to package for a different target than the host,
# e.g. building a Linux server package from Windows. An empty -D value is falsy, so it simply means
# "use the default" rather than shipping nothing.
if(NOT GalaxyRPLauncherFiles)
	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		set(GalaxyRPLauncherFiles galaxyrp_play.bat galaxyrp_host.bat galaxyrp_host_autorestart.bat)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		set(GalaxyRPLauncherFiles galaxyrp_play.command galaxyrp_host.command galaxyrp_host_autorestart.command)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(GalaxyRPLauncherFiles galaxyrp_play.sh galaxyrp_host.sh galaxyrp_host_autorestart.sh)
	else()
		message(WARNING "GalaxyRP: unrecognized CMAKE_SYSTEM_NAME '${CMAKE_SYSTEM_NAME}' -- shipping "
			"every platform's launcher script since it's unclear which one applies. Set "
			"GalaxyRPLauncherFiles explicitly (e.g. -DGalaxyRPLauncherFiles=galaxyrp_play.sh;galaxyrp_host.sh;galaxyrp_host_autorestart.sh) "
			"to fix this.")
		set(GalaxyRPLauncherFiles ${GalaxyRPAllLauncherFiles})
	endif()
endif()

# GalaxyRP fix: [packaging] validate the names, because every use of this list is a PATTERN match
# or a file path that fails SILENTLY when the name is wrong. A typo in a -D would not error -- it
# would just never match, so the misspelled script would be excluded from the package like all the
# others and the build would look fine while shipping no launcher at all. Fail loudly instead.
foreach(GalaxyRPRequestedLauncher ${GalaxyRPLauncherFiles})
	if(NOT GalaxyRPRequestedLauncher IN_LIST GalaxyRPAllLauncherFiles)
		message(FATAL_ERROR "GalaxyRP: GalaxyRPLauncherFiles contains '${GalaxyRPRequestedLauncher}', "
			"which is not one of the launcher scripts in assets/server. Valid names are: "
			"${GalaxyRPAllLauncherFiles}")
	endif()
endforeach()

# The same silent-failure argument applies to GalaxyRPAllLauncherFiles itself: if a script is
# renamed or removed from assets/server without updating that list, its stale entry stops matching
# and the file it was meant to exclude quietly starts shipping on every platform. A warning rather
# than an error, since a deliberately removed launcher should not block anyone's build.
foreach(GalaxyRPKnownLauncher ${GalaxyRPAllLauncherFiles})
	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/assets/server/${GalaxyRPKnownLauncher}")
		message(WARNING "GalaxyRP: GalaxyRPAllLauncherFiles lists '${GalaxyRPKnownLauncher}', but "
			"assets/server/${GalaxyRPKnownLauncher} does not exist. The list is stale -- any launcher "
			"it no longer names will be shipped on every platform instead of just its own.")
	endif()
endforeach()

set(GalaxyRPExcludedLauncherFiles ${GalaxyRPAllLauncherFiles})
list(REMOVE_ITEM GalaxyRPExcludedLauncherFiles ${GalaxyRPLauncherFiles})

# Generate PK3 file
set(PK3FILE GalaxyRP_v${RPVersion}.pk3)
set(PK3PATH ${CMAKE_SOURCE_DIR}/assets/client)
set(TMPFILE ${CMAKE_CURRENT_BINARY_DIR}/${PK3FILE})
set(OUTFILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${PK3FILE})

file(GLOB PK3ASSETS "${PK3PATH}/*")

include(InstallZIP)
add_zip_command(${TMPFILE} ${OUTFILE} DIR "${PK3PATH}" FILES "${PK3ASSETS}" )

# Finish and cleanup
# GalaxyRP fix: [packaging] add ALL so this target (which builds the pk3) is included in the
# default build ("Build Solution" in VS / plain `cmake --build .`) instead of requiring a
# separate explicit `--target assets` build step before install -- previously this had to be
# built by hand, which is easy to miss and leaves `cmake --install` failing to find the pk3.
add_custom_target(assets ALL DEPENDS "${TMPFILE}")
add_custom_command(TARGET assets POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E remove -f "${TMPFILE}"
)

# GalaxyRP fix: [packaging] copy the server-side content (database/, duelarena/, the
# .cfg/.html/.bat helper files) straight into the per-config build output
# (${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>, e.g. build-x64/Release) as part of the plain
# build, the same way the pk3 above already lands there via add_zip_command -- unlike assets/client
# (zipped into the pk3), this content is just copied as loose files, not archived. jampgame/cgame/ui
# already land in that same per-config folder automatically (that's the default MSVC/Ninja output
# location), so the build output folder alone now already looks like a complete GalaxyRP folder
# without requiring a separate `cmake --install` step -- install() below still assembles the full
# JediAcademy/GalaxyRP tree for packaging/CI, this is additive, not a replacement for it.
add_custom_command(TARGET assets POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/assets/server" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
	COMMENT "Copying GalaxyRP server-side content to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
)
# GalaxyRP fix: [packaging] copy_directory above has no exclude option, so it always copies every
# platform's launcher script too -- remove the ones that don't belong to this build's target
# platform (see GalaxyRPExcludedLauncherFiles above) right after.
foreach(GalaxyRPExcludedLauncherFile ${GalaxyRPExcludedLauncherFiles})
	add_custom_command(TARGET assets POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove -f
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${GalaxyRPExcludedLauncherFile}"
	)
endforeach()

# GalaxyRP fix: [packaging] assemble the actual distributable "GalaxyRP" mod folder -- the pk3
# built above, plus the server-side content that ships alongside it (database/, duelarena/,
# the .cfg/.html/.bat helper files) from assets/server. This is what previously had
# to be put together by hand; jampgame/cgame/ui are installed here too (see their own
# CMakeLists.txt -- NOT packed inside the pk3, all three ship as loose files next to it, same as
# the old release layout). Deliberately does NOT depend on BuildMPEngine/BuildMPRdVanilla/
# BuildMPDed -- this folder never contains the engine executable or its renderer DLL, only the
# mod content and the three game-code modules.
install(FILES "${OUTFILE}"
	DESTINATION "${JKAInstallDir}/GalaxyRP"
	COMPONENT ${JKAMPCoreComponent})

# GalaxyRP fix: [packaging] exclude the other platforms' launcher scripts here too (see
# GalaxyRPExcludedLauncherFiles above), same reasoning as the POST_BUILD copy -- this is what
# actually lands in the packaged/CI-uploaded GalaxyRP folder, so it's the one that matters most for
# not shipping every OS's scripts in every release.
set(GalaxyRPServerInstallExcludes)
foreach(GalaxyRPExcludedLauncherFile ${GalaxyRPExcludedLauncherFiles})
	list(APPEND GalaxyRPServerInstallExcludes PATTERN "${GalaxyRPExcludedLauncherFile}" EXCLUDE)
endforeach()

# GalaxyRP fix: [packaging] install(DIRECTORY) does not preserve source file permissions -- it
# would otherwise install these scripts world-readable but NOT executable (confirmed: verified
# locally, installed galaxyrp_play.sh/host.sh came out 644, not the 755 they have in the repo).
# That was harmless for the old .bat-only setup (Windows has no Unix execute bit; a .bat's
# "executability" is determined by its extension), but it would silently break the whole point of
# the new .sh/.command scripts -- Linux's `./galaxyrp_play.sh` and macOS Finder's double-click both
# require the execute bit to actually be set. Force it back on explicitly for just the launcher
# files being installed (everything else under assets/server -- the .cfg/.html/.txt content,
# database/ -- has no reason to be executable).
set(GalaxyRPLauncherFilePermissions)
foreach(GalaxyRPLauncherFile ${GalaxyRPLauncherFiles})
	list(APPEND GalaxyRPLauncherFilePermissions
		PATTERN "${GalaxyRPLauncherFile}"
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
			WORLD_READ WORLD_EXECUTE)
endforeach()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/server/"
	DESTINATION "${JKAInstallDir}/GalaxyRP"
	COMPONENT ${JKAMPCoreComponent}
	${GalaxyRPServerInstallExcludes}
	${GalaxyRPLauncherFilePermissions})
