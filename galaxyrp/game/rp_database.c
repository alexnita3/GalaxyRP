#include "../../codemp/game/g_local.h"
#include "sqlite/sqlite3.h"

void run_db_query(const char *query, sqlite3 *db)
{
	char *zErrMsg = NULL;
	int rc = sqlite3_exec(db, query, NULL, NULL, &zErrMsg);

	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
}
