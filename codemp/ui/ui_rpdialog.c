/*
===========================================================================
GLX_U: [Lua] the ui half of the script dialogue menu (D2); ui/rp_dialog.h says how the server, cgame and the
ui work together, and ui/jamp/ingame_rpdialog.menu is the menu.

- cgame opens the menu with UIMENU_RPDIALOG (UI_SetActiveMenu) after it has set the ui_rpdlg_* cvars. It
  opens on top of a menu already open, which gets the focus back when this one closes. A dialogue that
  replaces the shown one only changes the cvars, so an open menu just stays open.
- An answer (a click on an option, or its number key 1 to 6 while the menu is open -- the voice chat
  menu's way: the key runs that option's own action) sends "cmd choose <n> <id>" and closes the menu.
- Closing the menu without an answer -- Esc, which UI_KeyEvent turns into Menus_CloseAll for a menu
  that is not full screen, or anything else that closes it -- runs its onClose: "cmd choose 0 <id>".
- ui_rpdlg_id goes to 0 first in each case, so the server hears one thing per dialogue. cgame sets it
  to 0 too when the server ends the dialogue (rpdlgclose), before UIMENU_RPDIALOG_CLOSE closes the menu.
- The answers are two numbers built here: no text from the server ever reaches the console.
===========================================================================
*/

#include "ui_local.h"
#include "rp_dialog.h"

extern void Item_RunScript( itemDef_t *item, const char *s );	// ui_shared.c

static int UI_RpDialog_Id( void ) {
	return atoi( UI_Cvar_VariableString( RPDLG_CVAR_ID ) );
}

static int UI_RpDialog_Count( void ) {
	int count = atoi( UI_Cvar_VariableString( RPDLG_CVAR_COUNT ) );

	return ( count < 0 ) ? 0 : ( count > RPDLG_MAX_OPTIONS ) ? RPDLG_MAX_OPTIONS : count;
}

// the answer, or 0 to end the dialogue; at most once per dialogue
static void UI_RpDialog_Send( int choice ) {
	int id = UI_RpDialog_Id();

	if ( id <= 0 ) {
		return;
	}
	trap->Cvar_Set( RPDLG_CVAR_ID, "0" );
	trap->Cmd_ExecuteText( EXEC_APPEND, va( "cmd choose %d %d\n", choice, id ) );
}

// closes the menu; back to the game when no other menu is open under it
static void UI_RpDialog_CloseMenu( void ) {
	menuDef_t *menu = Menus_FindByName( RPDLG_MENU );

	if ( !menu || !( menu->window.flags & WINDOW_VISIBLE ) ) {
		return;
	}
	Menus_CloseByName( RPDLG_MENU );
	if ( !Menu_GetFocused() ) {
		trap->Key_SetCatcher( trap->Key_GetCatcher() & ~KEYCATCH_UI );
		trap->Key_ClearStates();
	}
}

// the menu file is missing (an incomplete client pk3): the dialogue goes to the console, to be answered there
static void UI_RpDialog_PrintInstead( void ) {
	int i, count = UI_RpDialog_Count();

	trap->Print( "%s\n", UI_Cvar_VariableString( RPDLG_CVAR_TITLE ) );
	for ( i = 1; i <= RPDLG_LINES; i++ ) {
		const char *line = UI_Cvar_VariableString( va( RPDLG_CVAR_LINE "%d", i ) );
		if ( line[0] ) {
			trap->Print( "%s\n", line );
		}
	}
	for ( i = 1; i <= count; i++ ) {
		trap->Print( "  %s\n", UI_Cvar_VariableString( va( RPDLG_CVAR_OPTION "%d", i ) ) );
	}
	trap->Print( "^7Answer with ^3/choose <number>^7 (the dialogue menu " RPDLG_MENU " is missing from your client).\n" );
}

void UI_RpDialogOpen( void ) {
	menuDef_t *menu = Menus_FindByName( RPDLG_MENU );

	if ( !menu ) {
		UI_RpDialog_PrintInstead();
		return;
	}
	if ( menu->window.flags & WINDOW_VISIBLE ) {
		return;		// already open: it shows the new cvars
	}
	trap->Key_SetCatcher( trap->Key_GetCatcher() | KEYCATCH_UI );
	Menus_ActivateByName( RPDLG_MENU );
}

void UI_RpDialogClose( void ) {
	UI_RpDialog_CloseMenu();
}

// rpDialogChoose <n>: an option clicked or its key pressed; rpDialogClosed: the menu's onClose
qboolean UI_RpDialogScript( const char *name, char **args ) {
	if ( !Q_stricmp( name, "rpDialogChoose" ) ) {
		int n;
		if ( Int_Parse( args, &n ) && n >= 1 && n <= UI_RpDialog_Count() ) {
			UI_RpDialog_Send( n );
			UI_RpDialog_CloseMenu();
		}
		return qtrue;
	}
	if ( !Q_stricmp( name, "rpDialogClosed" ) ) {
		UI_RpDialog_Send( 0 );
		return qtrue;
	}
	return qfalse;
}

// the keys 1 to 6 (and the keypad's) while the dialogue menu has the focus: the option's own action, as a click
qboolean UI_RpDialogKey( menuDef_t *menu, int key ) {
	static const int keys[RPDLG_MAX_OPTIONS][2] = {
		{ A_1, A_KP_1 }, { A_2, A_KP_2 }, { A_3, A_KP_3 }, { A_4, A_KP_4 }, { A_5, A_KP_5 }, { A_6, A_KP_6 }
	};
	itemDef_t *item;
	int n;

	if ( !menu || Q_stricmp( menu->window.name, RPDLG_MENU ) ) {
		return qfalse;
	}
	for ( n = 0; n < RPDLG_MAX_OPTIONS; n++ ) {
		if ( key == keys[n][0] || key == keys[n][1] ) {
			break;
		}
	}
	if ( n >= RPDLG_MAX_OPTIONS ) {
		return qfalse;
	}
	if ( n < UI_RpDialog_Count() ) {
		item = Menu_FindItemByName( menu, va( "rpdlg_opt%d", n + 1 ) );
		if ( item && item->action ) {
			Item_RunScript( item, item->action );
		}
	}
	return qtrue;	// a number past the options does nothing (in this menu the keypad's 2 is not "down")
}
