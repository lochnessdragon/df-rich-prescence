#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "DataDefs.h"

using namespace DFHack;

command_result rich_prescence(color_ostream& out, std::vector<std::string> & params);

DFHACK_PLUGIN("rich_prescence");

DFhackCExport command_result plugin_init(color_ostream& out, std::vector<PluginCommand>& commands) {
    out.print("Rich prescence started");
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown(color_ostream& out) {
    out.print("Rich prescence stopped");
    return CR_OK;
}