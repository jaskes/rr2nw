#define LAST_H__VIEW
#include "game.h"
#include "storage/h/savefile.h"
#include "kernel/h/context.h"
#include "console.h"

bool SaveGame(char *fileName, SimulationContext *context)
{
	PIN_SaveFile file;

	if (!file.OpenWrite(fileName))
		return false;

	VERIFY(context->dump(file));
	file.Close();
	return true;
}

bool LoadGame(char *fileName, SimulationContext *context)
{
	g_GameConsole.DeleteUrgentMessge();
	PIN_SaveFile file;

	if (!file.OpenRead(fileName))
		return false;

	VERIFY(context->load(file));
	file.Close();
	return true;
}
