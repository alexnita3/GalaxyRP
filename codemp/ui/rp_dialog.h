/*
===========================================================================
GLX_U: [Lua] the script dialogue menu (D2): what cgame (cg_rpdialog.c) and the ui (ui_rpdialog.c) share,
and what ui/jamp/ingame_rpdialog.menu must match. The server side is galaxyrp/game/rp_lua_dialog.c.

- The server sends a player whose client has the menu (userinfo "rpdlg" RPDLG_PROTOCOL)
  "rpdlg <id> <seconds left> "<title>" "<text>" "<option 1>" ... "<option n>"", and
  "rpdlgclose <id>" when that dialogue ends on the server.
- cgame breaks the text into lines with the menu's font and size (the menu's own wrapping would let a
  line run out of its box by a word, and would not carry a colour to the next line), puts the lines,
  the title and the options into the cvars below, and opens the menu (UIMENU_RPDIALOG); the text
  never goes through the console. It closes it on rpdlgclose (UIMENU_RPDIALOG_CLOSE).
- The menu draws those cvars. A click on an option, or its number key while the menu is open, sends
  "cmd choose <n> <id>"; closing the menu any other way (Esc) sends "cmd choose 0 <id>".
===========================================================================
*/

#ifndef RP_DIALOG_H
#define RP_DIALOG_H

#define RPDLG_PROTOCOL			"1"					// the userinfo key's value our cgame sends
#define RPDLG_MENU				"ingame_rpdialog"	// the menuDef's name

#define RPDLG_MAX_OPTIONS		6
#define RPDLG_LINES				7					// rpdlg_line1..7: enough for the 255 characters the server allows
#define RPDLG_LINE_BYTES		250					// a line's cvar: under the 255 a cvar keeps

// the menu items' sizes (ingame_rpdialog.menu, font 2 = "ergoec"): the text lines and the options are drawn
// at RPDLG_TEXT_SCALE in a column RPDLG_TEXT_WIDTH wide, the title at RPDLG_TITLE_SCALE. The widest option
// the server allows, "1. " and 64 times "W", is 570 wide at 0.58.
#define RPDLG_FONT				"ergoec"
#define RPDLG_TEXT_SCALE		0.58f
#define RPDLG_TITLE_SCALE		0.7f
#define RPDLG_TEXT_WIDTH		572

// the cvars cgame sets and the menu shows; ui_rpdlg_id is 0 while no dialogue is waiting for an answer
#define RPDLG_CVAR_TITLE		"ui_rpdlg_title"
#define RPDLG_CVAR_LINE			"ui_rpdlg_line"		// + 1..RPDLG_LINES
#define RPDLG_CVAR_OPTION		"ui_rpdlg_opt"		// + 1..RPDLG_MAX_OPTIONS
#define RPDLG_CVAR_COUNT		"ui_rpdlg_count"
#define RPDLG_CVAR_ID			"ui_rpdlg_id"
#define RPDLG_CVAR_LEFT			"ui_rpdlg_left"

#endif // RP_DIALOG_H
