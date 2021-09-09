/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#define _STR(x) #x
#define STR(x) _STR(x)

// Current version of the multi player game
#define VERSION_MAJOR_RELEASE		3
#define VERSION_MINOR_RELEASE		4
#define VERSION_EXTERNAL_BUILD		0
#define VERSION_INTERNAL_BUILD		0

#define VERSION_STRING STR(VERSION_MAJOR_RELEASE) ", " STR(VERSION_MINOR_RELEASE) // "a, b"
#define VERSION_STRING_DOTTED STR(VERSION_MAJOR_RELEASE) "." STR(VERSION_MINOR_RELEASE) // "a.b"

#if defined(_DEBUG)
	#define	JK_VERSION		"(debug)Galaxy RP Mod: v" VERSION_STRING_DOTTED
	#define JK_VERSION_OLD	"(debug)JAmp: v" VERSION_STRING_DOTTED
#else
	#define	JK_VERSION		"Galaxy RP Mod: v" VERSION_STRING_DOTTED
	#define JK_VERSION_OLD	"JAmp: v" VERSION_STRING_DOTTED
#endif
