#============================================================================
# Copyright (C) 2013 - 2018, OpenJK contributors
# 
# This file is part of the OpenJK source code.
# 
# OpenJK is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#============================================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")

set(InOpenJK ON)

set(ProjectName "GalaxyRP" CACHE STRING "OpenJK")
project(${ProjectName})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

#=============================================================================
#
# Customizable options
#
#=============================================================================
option(BuildPortableVersion "Build portable version (does not read or write files from your user/home directory" OFF)

option(BuildMPEngine "Whether to create projects for the MP client (openjk.exe)" OFF)
option(BuildMPRdVanilla "Whether to create projects for the MP default renderer (rd-vanilla_x86.dll)" OFF)
option(BuildMPDed "Whether to create projects for the MP dedicated server (openjkded.exe)" OFF)
option(BuildMPGame "Whether to create projects for the MP server-side gamecode (jampgamex86.dll)" ON)
option(BuildMPCGame "Whether to create projects for the MP clientside gamecode (cgamex86.dll)" ON)
option(BuildMPUI "Whether to create projects for the MP UI code (uix86.dll)" ON)
option(BuildSPEngine "Whether to create projects for the SP engine (openjk_sp.exe)" OFF)
option(BuildSPGame "Whether to create projects for the SP gamecode (jagamex86.dll)" OFF)
option(BuildSPRdVanilla "Whether to create projects for the SP default renderer (rdsp-vanilla_x86.dll)" OFF)

# Configure the use of bundled libraries.  By default, we assume the user is on
# a platform that does not require any bundling.
#
# Note that there is no UseInternalMinizip option: when minizip is built at all it is always
# our bundled copy, since it is modified to use Z_Malloc.
#
# These options are only acted on when the engine, dedicated server or a renderer is being
# built -- see the gated block further down.

set(UseInternalOpenALDefault OFF)
set(UseInternalZlibDefault   OFF)
set(UseInternalPNGDefault    OFF)
set(UseInternalJPEGDefault   OFF)
set(UseInternalSDL2Default   OFF)

if(WIN32)
  set(UseInternalOpenALDefault ON)
  set(UseInternalZlibDefault   ON)
  set(UseInternalPNGDefault    ON)
  set(UseInternalJPEGDefault   ON)
  set(UseInternalSDL2Default   ON)
endif()

if(APPLE)
  set(UseInternalJPEGDefault ON)
endif()

option(UseInternalOpenAL "If set, use bundled OpenAL."  ${UseInternalOpenALDefault})
option(UseInternalZlib   "If set, use bundled zlib."    ${UseInternalZlibDefault})
option(UseInternalPNG    "If set, use bundled libpng."  ${UseInternalPNGDefault})
option(UseInternalJPEG   "If set, use bundled libjpeg." ${UseInternalJPEGDefault})
option(UseInternalSDL2   "If set, use bundled SDL2."    ${UseInternalSDL2Default})

# This option won't appear on non-Apple platforms.
if(APPLE)
  option(MakeApplicationBundles "Whether to build .app application bundles for engines built" ON)
endif()

#=============================================================================
#
# Custom CMake Modules needed
#
#=============================================================================
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/cmake/Modules")

#=============================================================================
#
# Architecture/OS defines
#
#=============================================================================
# ${Architecture} must match ARCH_STRING in q_platform.h,
# and is used in DLL names (jagamex86.dll, jagamex86.dylib, jagamei386.so).
if(WIN32)
	set(X86 ON)
	if(CMAKE_SIZEOF_VOID_P MATCHES "8")
		set(Architecture "x86_64")
		set(WIN64 TRUE)
	else()
		set(Architecture "x86")
		set(WIN64 FALSE)
	endif()
else()
	set(X86 OFF)
	# GalaxyRP fix: [TaystJK multi-arch] arm64/aarch64 must be checked BEFORE the generic "^arm"
	# match below, or a 64-bit ARM host (e.g. Apple Silicon, aarch64 Linux) falls into the "arm"
	# (32-bit-implying) branch instead. Matches TaystJK's own detection logic.
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
		set(Architecture "arm64")
		add_definitions(-DPNG_ARM_NEON_OPT=0)
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
		set(Architecture "arm")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
		set(X86 ON)
		if(APPLE)
			set(Architecture "x86")
		else()
			# e.g. Linux
			set(Architecture "i386")
		endif()
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^x86.64$")
		set(X86 ON)
		set(Architecture "x86_64")
	elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "powerpc")
		set(Architecture "ppc")
	elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "powerpc64")
		set(Architecture "ppc64")
	else()
		set(Architecture "${CMAKE_SYSTEM_PROCESSOR}")
	endif()
endif()

message(STATUS "Architecture is ${Architecture}")

if(WIN32 AND CMAKE_VERSION VERSION_LESS "3.4")
	message(WARNING "Building on Windows platform with CMake version less than 3.4 is deprecated. Manifest file will fail to be included.")
endif()

#=============================================================================
#
# Binary names
#
#=============================================================================
set(SPEngine "openjk_sp.${Architecture}")
set(SPGame "jagame${Architecture}")
set(SPRDVanillaRenderer "rdsp-vanilla_${Architecture}")
set(MPEngine "openjk.${Architecture}")
set(MPVanillaRenderer "rd-vanilla_${Architecture}")
set(MPDed "openjkded.${Architecture}")
set(MPGame "jampgame${Architecture}")
set(MPCGame "cgame${Architecture}")
set(MPUI "ui${Architecture}")
set(AssetsPk3 "openjk-${Architecture}.pk3")
# Library names
set(MPBotLib "botlib")
set(SharedLib "shared")

#=============================================================================
#
# Paths
#
#=============================================================================
set(SPDir "${CMAKE_SOURCE_DIR}/code")
set(MPDir "${CMAKE_SOURCE_DIR}/codemp")
set(RPDir "${CMAKE_SOURCE_DIR}/galaxyrp") # GalaxyRP source files
set(SharedDir ${CMAKE_SOURCE_DIR}/shared)
set(OpenJKLibDir "${CMAKE_SOURCE_DIR}/lib")
set(GSLIncludeDirectory "${OpenJKLibDir}/gsl-lite/include")

include(InstallConfig)

#=============================================================================
#
# Compiler definitions/flags
#
#=============================================================================

# Operating system settings
if(WIN64)
	set(SharedDefines ${SharedDefines} "WIN64")
endif()

if (APPLE)
	# GalaxyRP fix: [TaystJK multi-arch] this used to be set(SharedDefines "MACOS_X") -- an
	# unconditional overwrite (no ${SharedDefines} on the right-hand side) that silently discarded
	# whatever WIN64 (or any earlier) define had already been appended above. Append instead.
	set(SharedDefines ${SharedDefines} "MACOS_X")
endif()

if (NOT WIN32 AND NOT APPLE)
	# GalaxyRP fix: [TaystJK multi-arch] same overwrite-vs-append bug as the APPLE branch above.
	set(SharedDefines ${SharedDefines} "ARCH_STRING=\"${Architecture}\"")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "BSD")
  add_definitions(-DIOAPI_NO_64)
endif()

# Compiler settings
if(MSVC)
	set(SharedDefines ${SharedDefines} "NOMINMAX")
	set(SharedDefines ${SharedDefines} "_CRT_SECURE_NO_WARNINGS")
	set(SharedDefines ${SharedDefines} "_SCL_SECURE_NO_WARNINGS")
	set(SharedDefines ${SharedDefines} "_CRT_NONSTDC_NO_DEPRECATE")

	# GalaxyRP fix: [TaystJK parity] /EHsc enables standard C++ exception unwind semantics --
	# without it, MSVC emits C4530 ("C++ exception handler used, but unwind semantics are not
	# enabled") for any code using try/catch or STL operations that can throw. TaystJK's own
	# CMakeLists.txt sets this; DAJ_RP's did not.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /EHsc")

	# GalaxyRP fix: [TaystJK multi-arch] /arch:SSE2 is a no-op-with-warning on x64 (SSE2 is the x64
	# baseline already) and is invalid on an ARM64 MSVC target -- scope it to 32-bit x86 only,
	# matching TaystJK.
	if (Architecture STREQUAL "x86")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:SSE2")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
	endif()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

	# GalaxyRP fix: [TaystJK parity] Suppress warnings TaystJK's own build already suppresses:
	# 4244 = 'conversion' conversion from 'type1' to 'type2', possible loss of data
	# 4996 = 'function': was declared deprecated
	# 4267 = 'var' : conversion from 'size_t' to 'type', possible loss of data
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4244 /wd4996 /wd4267")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4996 /wd4267")

	# We don't try to control symbol visibility under MSVC.
	set(OPENJK_VISIBILITY_FLAGS "")
elseif (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_C_COMPILER_ID}" MATCHES "Clang"))
	# I hope this doesn't come back to bite me in the butt later on.
	# Realistically though, can the C and CXX compilers be different?

	# Visibility can't be set project-wide -- it needs to be specified on a
	# per-target basis.  This is primarily due to the bundled copy of ZLib.
	# ZLib explicitly declares symbols hidden, rather than defaulting to hidden.
	#
	# Note that -fvisibility=hidden is stronger than -fvisibility-inlines-hidden.
	set(OPENJK_VISIBILITY_FLAGS "-fvisibility=hidden")

	# removes the -rdynamic flag at linking (which causes crashes for some reason)
	set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
	set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

	# additional flags for debug configuration
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")

	if (X86)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
	endif()

	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

	# enable somewhat modern C++
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	
	if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-comment")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsigned-char")
		if (X86)
			# "x86 vm will crash without -mstackrealign since MMX
			# instructions will be used no matter what and they
			# corrupt the frame pointer in VM calls"
			# -ioquake3 Makefile
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mstackrealign")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse")
		endif()

		if(WIN32)
			# Link libgcc and libstdc++ statically
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
		endif()
	elseif("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-comment")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsigned-char")
	endif()

	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-offsetof")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-comment")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsigned-char")
		if (X86)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mstackrealign")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpmath=sse")
		endif()
	elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings")
		#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-writable-strings")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-comment")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-offsetof")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsigned-char")
	endif()
else()
	message(ERROR "Unsupported compiler")
endif()

set(SharedDefines ${SharedDefines} "$<$<CONFIG:Debug>:_DEBUG>")
set(SharedDefines ${SharedDefines} "$<$<NOT:$<CONFIG:Debug>>:FINAL_BUILD>")

if(BuildPortableVersion)
	set(BUILD_PORTABLE ON)
endif()

#=============================================================================
#
# Generate version file
#
#=============================================================================

# Reproducible builds
# https://reproducible-builds.org/specs/source-date-epoch/
if ("$ENV{SOURCE_DATE_EPOCH}" STREQUAL "")
	message(STATUS "SOURCE_DATE_EPOCH is not set: SOURCE_DATE will be set to the compile-time date")
else()
	execute_process(
		COMMAND "date" "--date=@$ENV{SOURCE_DATE_EPOCH}" "+%b %_d %Y"
		OUTPUT_VARIABLE SOURCE_DATE
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	message(STATUS "SOURCE_DATE_EPOCH is set ($ENV{SOURCE_DATE_EPOCH}): SOURCE_DATE set to \"${SOURCE_DATE}\"")
endif()

# Current Git SHA1 hash
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_HASH)
message(STATUS "Git revision is ${GIT_HASH}")

configure_file(shared/qcommon/q_version.h.in shared/qcommon/q_version.h @ONLY)

#=============================================================================

# Files shared across all projects
#
#=============================================================================
set(SharedCommonFiles
	"${SharedDir}/qcommon/q_color.h"
	"${SharedDir}/qcommon/q_color.c"
	"${SharedDir}/qcommon/q_math.h"
	"${SharedDir}/qcommon/q_math.c"
	"${SharedDir}/qcommon/q_string.h"
	"${SharedDir}/qcommon/q_string.c"
	"${SharedDir}/qcommon/q_platform.h"
	"${CMAKE_BINARY_DIR}/shared/qcommon/q_version.h"
	"${SharedDir}/sqlite/sqlite3.h"
	"${SharedDir}/sqlite/sqlite3.c"
	)
set(SharedCommonSafeFiles
	"${SharedDir}/qcommon/safe/gsl.h"
	"${SharedDir}/qcommon/safe/string.cpp"
	"${SharedDir}/qcommon/safe/string.h"
	"${SharedDir}/qcommon/safe/sscanf.h"
	"${SharedDir}/qcommon/safe/limited_vector.h"
	)

#=============================================================================
#
# Adding all of the projects
#
#=============================================================================
# GalaxyRP fix: [build] these four libraries exist for the engine, the dedicated server and the
# renderer -- nothing else consumes JPEG_LIBRARIES, ZLIB_LIBRARIES, PNG_LIBRARIES or
# MINIZIP_LIBRARIES (BuildSPGame deliberately is not in the list below; it links none of them).
# The block used to run unconditionally, which had two consequences for the gamecode-only build
# this repo actually ships:
#
#   1. find_package(... REQUIRED) hard-failed at configure time on any Linux or macOS box
#      without the matching -dev packages, for libraries the mod never links. That is the
#      failure our own CI hit (run 33105699123) and works around to this day by forcing
#      -DUseInternalZlib/PNG/JPEG=ON; a contributor cloning the repo and running plain
#      `cmake ..` got the same error with no such hint.
#   2. With the internal copies selected, bundled_zlib/libpng/libjpeg were added to ALL and
#      fully compiled -- and bundled_minizip on every platform, internal or not -- then linked
#      by no target at all.
#
# Gating on the five options that actually consume them fixes both: a mod-only configure no
# longer looks for, or builds, any of the four. The UseInternal* options themselves stay
# declared above so they remain visible in cmake-gui/ccmake for engine builds.
if(BuildMPEngine OR BuildMPDed OR BuildMPRdVanilla OR BuildSPEngine OR BuildSPRdVanilla)
	if(UseInternalJPEG)
	  # GalaxyRP fix: [libs] bumped bundled libjpeg 9a (2014) -> 9c (2018), matching TaystJK, for
	  # renderer-side JPEG texture/screenshot loading. Folder renamed accordingly.
	  add_subdirectory(lib/jpeg-9c)
	else()
	  find_package(JPEG REQUIRED)
	endif()

	if(UseInternalZlib)
	  # GalaxyRP fix: [libs] bumped bundled zlib 1.2.8 -> 1.3.1, matching TaystJK. Our old 1.2.8
	  # predates the fix for CVE-2018-25032 (a deflate() memory-corruption bug under Z_FIXED).
	  add_subdirectory(lib/zlib)
	else()
	  find_package(ZLIB REQUIRED)
	endif()

	if(UseInternalPNG)
	  # GalaxyRP fix: [libs] bumped bundled libpng 1.6.16 -> 1.6.53, matching TaystJK. Same 1.6
	  # branch (API/ABI-compatible), just ~37 point releases of bug and security fixes we were
	  # missing.
	  add_subdirectory(lib/libpng)
	else()
	  find_package(PNG REQUIRED)
	endif()

	# minizip is always our bundled copy -- there is no system-copy branch, because ours is
	# patched to route allocations through Z_Malloc (sets MINIZIP_{LIBRARIES,INCLUDE_DIR}).
	# Bundled alongside zlib's release above; see lib/minizip/CMakeLists.txt for the details.
	add_subdirectory(lib/minizip)
endif()

# Add projects
add_subdirectory(${SPDir})

add_subdirectory(${MPDir})

# Add PK3 projects
add_subdirectory("assets")
set_property(TARGET assets PROPERTY PROJECT_LABEL "Build PK3")
