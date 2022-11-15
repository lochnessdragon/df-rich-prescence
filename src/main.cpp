#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "DataDefs.h"
#include "Debug.h"
#include "cpp/discord.h"

#define xstr(s) str(s)
#define str(s) #s

#define APP_ID 1041967020620648508

// global variables
// bool core_initialized = false; core should always be initialized when the plugin is enabled.
discord::Core* core{};

// plugin settings
DFHACK_PLUGIN("rich_presence");
DFHACK_PLUGIN_IS_ENABLED(isPluginEnabled);

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

bool initializeDiscord() {
    // create discord integration
    auto create_result = discord::Core::Create(APP_ID, (uint64_t) discord::CreateFlags::NoRequireDiscord, &core);
    if(!core) {
        ERR(log).printerr("Failed to create a discord instance! (err: %d)\n", static_cast<int>(create_result));
        return false;
    }

    // discord logging
    core->SetLogHook(discord::LogLevel::Debug, discordLog);

    isPluginEnabled = true;

    return true;
}

void deinitDiscord() {
    if (isPluginEnabled) {
        core->ActivityManager().ClearActivity([&](discord::Result result) {
            if(result != discord::Result::Ok) {
                ERR(log).printerr("Error clearing activity\n");
            }
        });
        delete core;

        isPluginEnabled = false;
    }
}

void updateActivity() {
    if (isPluginEnabled) {
        // get activity stats

        // send update to discord client
        discord::Activity activity{};
        activity.SetType(discord::ActivityType::Playing);
        activity.SetDetails("in Fortress mode");
        activity.SetState("Working on Miitopia");
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        activity.GetTimestamps().SetStart(now);

        core->ActivityManager().UpdateActivity(activity, [&](discord::Result result) {
            if (result != discord::Result::Ok) {
                ERR(log).printerr("Failed to update discord activity!\n");
            } else {
                DEBUG(log).print("Updated discord activity successfully\n");
            }
        });
    }
}

DFHack::command_result rich_presence(DFHack::color_ostream& out, std::vector<std::string> & params) {
    out.print("Discord Rich Presence\n");
    return DFHack::CR_OK;
}

// only runs when the plugin is enabled
DFhackCExport DFHack::command_result plugin_onupdate(DFHack::color_ostream& out) {
    core->RunCallbacks();
    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_enable(DFHack::color_ostream& out, bool enabled) {
    if(enabled) {
        out.print("Enabling discord rich presence\n");

        // create the discord state
        if(!isPluginEnabled) {
            if(!initializeDiscord()) {
                out.printerr("Failed to initialize discord. Are you sure it is open?");
                return DFHack::CR_FAILURE;
            }

            updateActivity();
        }

    } else {
        out.print("Disabling discord rich presence\n");

        // delete the discord state
        deinitDiscord();
    }

    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_init(DFHack::color_ostream& out, std::vector<DFHack::PluginCommand>& commands) {
    // attempt to create the discord core
    if(!initializeDiscord()) {
        isPluginEnabled = false;
    } else {
        // plugin enabled is automatically set
        // set users activity
        updateActivity();
    }

    // create df commands
    commands.push_back(DFHack::PluginCommand("rich_presence", "Configure the discord rich presence plugin.", rich_presence, false, 
        "rich_presence : configures the discord rich presence plugin\n"));
    
    out.print("Rich presence started!\n");

    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_shutdown(DFHack::color_ostream& out) {
    if(isPluginEnabled) {
        deinitDiscord();
    }

    out.print("Rich presence shutdown!\n");
    return DFHack::CR_OK;
}