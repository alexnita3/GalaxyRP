/*
===========================================================================
GLX_U: [Lua] the cgame half of the script dialogue menu (D2); ui/rp_dialog.h says how the server, cgame and
the ui work together.

- "rpdlg": the dialogue goes into the ui_rpdlg_* cvars -- the text broken into lines here, with the menu's
  own font and size, so no line leaves its box and a colour carries on to the next line -- and the menu
  opens. It waits while the console or the chat line is open, so the player's typing is never cut off;
  the server's timeout keeps running meanwhile. A dialogue that replaces the shown one just changes the
  cvars: the menu stays open and shows the new one.
- "rpdlgclose <id>": the menu closes, when that is the dialogue shown.
- Every frame the time left is updated (ui_rpdlg_left), once a second.
- Demo playback ignores both commands: a recorded dialogue cannot be answered.
- GLX_U: the menu also closes, without answering, on a map_restart (the server's Lua state starts again; it
  sends rpdlgclose as well) and RPDLG_LOST_MS after the dialogue's own end, in case its rpdlgclose never
  came. On its first frame the cgame asks for the open dialogue again ("choose resend"): after a vid_restart
  the menu is gone but the server's dialogue is not.
===========================================================================
*/

#include "cg_local.h"
#include "ui/ui_public.h"
#include "ui/rp_dialog.h"

#define RPDLG_MAX_WRAPPED		64		// lines kept while breaking, before cutting to RPDLG_LINES
#define RPDLG_ELLIPSIS			"..."
#define RPDLG_LOST_MS			5000	// GLX_U: a dialogue still shown this long after its end is closed here

static struct {
	int			id;				// the dialogue shown or waiting to be shown; 0 for none
	int			ends;			// cg.time
	int			shownLeft;		// the seconds in ui_rpdlg_left
	qboolean	pending;		// the menu is still to be opened
	qboolean	askResend;		// GLX_U: "choose resend" is still to be sent (the first frame)
	qhandle_t	font;
} rpdlg;

static int CG_RpDialog_Width( const char *s, float scale ) {
	return trap->R_Font_StrLenPixels( s, rpdlg.font, scale );
}

static qboolean CG_RpDialog_IsColour( const char *s ) {
	return ( s[0] == '^' && s[1] >= '0' && s[1] <= '9' ) ? qtrue : qfalse;
}

// the colour in effect after s, starting from colour
static char CG_RpDialog_ColourAfter( const char *s, int len, char colour ) {
	int i;

	for ( i = 0; i + 1 < len; i++ ) {
		if ( CG_RpDialog_IsColour( s + i ) ) {
			colour = s[i + 1];
		}
	}
	return colour;
}

// drops the end of s, a character at a time, until s plus "..." fits; then adds the "..."
static void CG_RpDialog_Ellipsis( char *s, int size, float scale, int width ) {
	char test[MAX_STRING_CHARS];
	int len = (int)strlen( s );

	while ( len > 0 ) {
		Com_sprintf( test, sizeof( test ), "%s" RPDLG_ELLIPSIS, s );
		if ( len + (int)strlen( RPDLG_ELLIPSIS ) < size && CG_RpDialog_Width( test, scale ) <= width ) {
			break;
		}
		s[--len] = '\0';
		if ( len > 0 && s[len - 1] == '^' ) {	// never leave half a colour code
			s[--len] = '\0';
		}
	}
	Q_strcat( s, size, RPDLG_ELLIPSIS );
}

// s as it is when it fits, else cut with "..."
static void CG_RpDialog_Fit( char *s, int size, float scale, int width ) {
	if ( CG_RpDialog_Width( s, scale ) > width || (int)strlen( s ) >= size ) {
		CG_RpDialog_Ellipsis( s, size, scale, width );
	}
}

/*
Breaks text into lines of at most width at scale, at spaces. Each line starts with the colour in effect
("^7" when none was given), so a colour carries on and no line can start with '@' (which a menu item
would take for a string-table name). A word too wide for a line on its own is cut where it no longer
fits; the server breaks any run of more than 40 characters, so that only happens with a text from
elsewhere. Returns the number of lines, at most max; *more is set when the text did not fit in them.
*/
int CG_RpDialog_Wrap( const char *text, float scale, int width, char lines[][RPDLG_LINE_BYTES + 1], int max, qboolean *more ) {
	char cur[RPDLG_LINE_BYTES + 1], cand[RPDLG_LINE_BYTES * 2 + 2];
	char colour = '7';
	int n = 0, words = 0;
	const char *p = text;

	*more = qfalse;
	Com_sprintf( cur, sizeof( cur ), "^%c", colour );

	while ( *p ) {
		const char *q;
		int len;

		while ( *p && (unsigned char)*p <= ' ' ) {	// a space, or a line break or tab the server should have taken out
			p++;
		}
		if ( !*p ) {
			break;
		}
		for ( q = p; *q && (unsigned char)*q > ' '; q++ ) {
		}
		len = (int)( q - p );
		if ( len > RPDLG_LINE_BYTES ) {
			len = RPDLG_LINE_BYTES;
		}

		Com_sprintf( cand, sizeof( cand ), "%s%s%.*s", cur, words ? " " : "", len, p );
		if ( (int)strlen( cand ) <= RPDLG_LINE_BYTES && CG_RpDialog_Width( cand, scale ) <= width ) {
			Q_strncpyz( cur, cand, sizeof( cur ) );
			words++;
			colour = CG_RpDialog_ColourAfter( p, len, colour );
			p += len;
			continue;
		}

		if ( !words ) {	// alone on its line and still too wide: as much of it as fits
			int base = (int)strlen( cur ), k = 0;

			while ( k < len ) {
				int step = CG_RpDialog_IsColour( p + k ) && k + 1 < len ? 2 : 1;
				Com_sprintf( cand, sizeof( cand ), "%s%.*s", cur, k + step, p );
				if ( base + k + step > RPDLG_LINE_BYTES || CG_RpDialog_Width( cand, scale ) > width ) {
					break;
				}
				k += step;
			}
			if ( !k ) {		// not even one letter fits: put it there anyway, so the text moves on
				k = 1;
			}
			Com_sprintf( cand, sizeof( cand ), "%s%.*s", cur, k, p );
			Q_strncpyz( cur, cand, sizeof( cur ) );
			colour = CG_RpDialog_ColourAfter( p, k, colour );
			p += k;
			words = 1;
		}

		// the line is full: keep it and start the next with the colour in effect
		if ( n >= max ) {
			*more = qtrue;
			return n;
		}
		Q_strncpyz( lines[n++], cur, RPDLG_LINE_BYTES + 1 );
		Com_sprintf( cur, sizeof( cur ), "^%c", colour );
		words = 0;
	}

	if ( words ) {
		if ( n >= max ) {
			*more = qtrue;
			return n;
		}
		Q_strncpyz( lines[n++], cur, RPDLG_LINE_BYTES + 1 );
	}
	return n;
}

static void CG_RpDialog_ClearCvars( void ) {
	int i;

	trap->Cvar_Set( RPDLG_CVAR_TITLE, "" );
	for ( i = 1; i <= RPDLG_LINES; i++ ) {
		trap->Cvar_Set( va( RPDLG_CVAR_LINE "%d", i ), "" );
	}
	for ( i = 1; i <= RPDLG_MAX_OPTIONS; i++ ) {
		trap->Cvar_Set( va( RPDLG_CVAR_OPTION "%d", i ), "" );
	}
	trap->Cvar_Set( RPDLG_CVAR_COUNT, "0" );
	trap->Cvar_Set( RPDLG_CVAR_ID, "0" );
	trap->Cvar_Set( RPDLG_CVAR_LEFT, "" );
}

static void CG_RpDialog_SetLeft( void ) {
	int left = ( rpdlg.ends - cg.time + 999 ) / 1000;

	if ( left < 0 ) {
		left = 0;
	}
	if ( left != rpdlg.shownLeft ) {
		rpdlg.shownLeft = left;
		trap->Cvar_Set( RPDLG_CVAR_LEFT, va( "%d:%02d left", left / 60, left % 60 ) );
	}
}

// the menu opens unless the player is typing in the console or the chat line; then it waits for them
static void CG_RpDialog_TryOpen( void ) {
	if ( !rpdlg.pending || ( trap->Key_GetCatcher() & ( KEYCATCH_CONSOLE | KEYCATCH_MESSAGE ) ) ) {
		return;
	}
	rpdlg.pending = qfalse;
	trap->OpenUIMenu( UIMENU_RPDIALOG );
}

void CG_RpDialogInit( void ) {
	memset( &rpdlg, 0, sizeof( rpdlg ) );
	rpdlg.font = trap->R_RegisterFont( RPDLG_FONT );
	CG_RpDialog_ClearCvars();
	rpdlg.askResend = qtrue;
}

// GLX_U: the menu closes here and answers nothing: the id goes to 0 first, so closing it sends no choice
static void CG_RpDialog_CloseShown( void ) {
	rpdlg.id = 0;
	rpdlg.pending = qfalse;
	trap->Cvar_Set( RPDLG_CVAR_ID, "0" );
	trap->OpenUIMenu( UIMENU_RPDIALOG_CLOSE );
}

// GLX_U: map_restart (cg_servercmds.c): the server's dialogues went with its Lua state
void CG_RpDialogReset( void ) {
	if ( rpdlg.id ) {
		CG_RpDialog_CloseShown();
	}
}

// rpdlg <id> <seconds left> "<title>" "<text>" "<option 1>" ... "<option n>"
void CG_RpDialog_f( void ) {
	char title[MAX_STRING_CHARS], text[MAX_STRING_CHARS], buf[MAX_STRING_CHARS];
	char lines[RPDLG_MAX_WRAPPED][RPDLG_LINE_BYTES + 1];
	int argc = trap->Cmd_Argc(), id, seconds, count, numLines, i;
	qboolean more;

	if ( cg.demoPlayback ) {
		return;
	}
	count = argc - 5;
	id = atoi( CG_Argv( 1 ) );
	seconds = atoi( CG_Argv( 2 ) );
	if ( count < 1 || count > RPDLG_MAX_OPTIONS || id <= 0 || seconds < 1 || seconds > 600 ) {
		trap->Print( S_COLOR_YELLOW "rpdlg: a dialogue the server sent could not be read (%d arguments)\n", argc - 1 );
		return;
	}

	// the title: a leading '@' would be taken for a string-table name
	Q_strncpyz( buf, CG_Argv( 3 ), sizeof( buf ) );
	Com_sprintf( title, sizeof( title ), "%s%s", buf[0] == '@' ? "^7" : "", buf );
	CG_RpDialog_Fit( title, RPDLG_LINE_BYTES + 1, RPDLG_TITLE_SCALE, RPDLG_TEXT_WIDTH );
	trap->Cvar_Set( RPDLG_CVAR_TITLE, title );

	// the text, in lines
	Q_strncpyz( text, CG_Argv( 4 ), sizeof( text ) );
	numLines = CG_RpDialog_Wrap( text, RPDLG_TEXT_SCALE, RPDLG_TEXT_WIDTH, lines, RPDLG_LINES, &more );
	if ( more && numLines > 0 ) {	// longer than the server allows: the last line says so
		CG_RpDialog_Ellipsis( lines[numLines - 1], RPDLG_LINE_BYTES + 1, RPDLG_TEXT_SCALE, RPDLG_TEXT_WIDTH );
	}
	for ( i = 0; i < RPDLG_LINES; i++ ) {
		trap->Cvar_Set( va( RPDLG_CVAR_LINE "%d", i + 1 ), i < numLines ? lines[i] : "" );
	}

	// the options, numbered
	for ( i = 0; i < RPDLG_MAX_OPTIONS; i++ ) {
		if ( i < count ) {
			char opt[RPDLG_LINE_BYTES + 1];
			Q_strncpyz( buf, CG_Argv( 5 + i ), sizeof( buf ) );
			Com_sprintf( opt, sizeof( opt ), "^3%d.^7 %s", i + 1, buf );
			CG_RpDialog_Fit( opt, sizeof( opt ), RPDLG_TEXT_SCALE, RPDLG_TEXT_WIDTH );
			trap->Cvar_Set( va( RPDLG_CVAR_OPTION "%d", i + 1 ), opt );
		} else {
			trap->Cvar_Set( va( RPDLG_CVAR_OPTION "%d", i + 1 ), "" );
		}
	}
	trap->Cvar_Set( RPDLG_CVAR_COUNT, va( "%d", count ) );
	trap->Cvar_Set( RPDLG_CVAR_ID, va( "%d", id ) );

	rpdlg.id = id;
	rpdlg.ends = cg.time + seconds * 1000;
	rpdlg.shownLeft = -1;
	CG_RpDialog_SetLeft();
	rpdlg.pending = qtrue;
	CG_RpDialog_TryOpen();
}

// rpdlgclose <id>
void CG_RpDialogClose_f( void ) {
	int id = atoi( CG_Argv( 1 ) );

	if ( cg.demoPlayback || id <= 0 || id != rpdlg.id ) {
		return;		// an older dialogue's, or one the menu has answered and the next has replaced
	}
	CG_RpDialog_CloseShown();
}

void CG_RpDialogFrame( void ) {
	if ( rpdlg.askResend ) {
		rpdlg.askResend = qfalse;
		if ( !cg.demoPlayback ) {
			trap->SendClientCommand( "choose resend" );
		}
	}
	if ( !rpdlg.id ) {
		return;
	}
	if ( cg.time - rpdlg.ends > RPDLG_LOST_MS ) {
		CG_RpDialog_CloseShown();	// GLX_U: its end never came
		return;
	}
	CG_RpDialog_TryOpen();
	CG_RpDialog_SetLeft();
}
