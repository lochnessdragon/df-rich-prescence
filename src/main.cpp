#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "DataDefs.h"
#include "Debug.h"
#include "cpp/discord.h"

#define xstr(s) str(s)
#define str(s) #s

// global variables
discord::Core* core{};

namespace DFHack {
    DBG_DECLARE(rich_presence, log, DebugCategory::LINFO);
}

const char* logLevelToA(discord::LogLevel level) {
    switch (level) {
        case discord::LogLevel::Error:
            return "Error";
        case discord::LogLevel::Warn:
            return "Warn";
        case discord::LogLevel::Info:
            return "Info";
        case discord::LogLevel::Debug:
            return "Debug";
        default:
            return "Unknown";
    }
}

void discordLog(discord::LogLevel level, const char* message) {
    // print debug message
    ERR(log).print("Discord [%s] - %s", logLevelToA(level), message);
}

DFHack::command_result rich_presence(DFHack::color_ostream& out, std::vector<std::string> & params) {
    out.print("Discord Rich Presence\n");
    return DFHack::CR_OK;
}

DFHACK_PLUGIN("rich_presence");

DFhackCExport DFHack::command_result plugin_init(DFHack::color_ostream& out, std::vector<DFHack::PluginCommand>& commands) {
    // create discord integration (in the future, check if discord is running to avoid a segfault)
    auto create_result = discord::Core::Create(APP_ID, (uint64_t) discord::CreateFlags::Default, &core);
    if(!core) {
        out.printerr("Failed to create a discord instance! (err: %d)\n", static_cast<int>(create_result));
        return DFHack::CR_FAILURE;
    }

    // discord logging
    core->SetLogHook(discord::LogLevel::Debug, discordLog);

    // set users activity
    discord::Activity activity{};
    activity.SetState("Playing");
    activity.SetDetails("Castle Ravenloft");
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    activity.GetTimestamps().SetStart(now);
    activity.SetType(discord::ActivityType::Playing);

    bool activityCreated = false;
    core->ActivityManager().UpdateActivity(activity, [&](discord::Result result) {
        if (result != discord::Result::Ok) {
            out.printerr("Failed to update discord activity!\n");
        } else {
            out.print("Updated discord activity successfully\n");
        }
        activityCreated = true;
    });

    // wait for activity to update
    do {
        core->RunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    } while (!activityCreated);
    
    // create df commands
    commands.push_back(DFHack::PluginCommand("rich_presence", "Configure the discord rich presence plugin.", rich_presence, false, 
        "rich_presence : configures the discord rich presence plugin\n"));
    
    out.print("Rich presence started!\n");
    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_shutdown(DFHack::color_ostream& out) {
    core->ActivityManager().ClearActivity([&](discord::Result result) {
        if(result != discord::Result::Ok) {
            out.printerr("Error clearing activity\n");
        }
    });

    delete core;
    out.print("Rich presence shutdown!\n");
    return DFHack::CR_OK;
}