#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "DataDefs.h"
#include "Debug.h"
#include "cpp/discord.h"
#include "modules/Maps.h"
#include "modules/World.h"
#include "modules/Translation.h"
#include "df/world.h"
#include "df/world_data.h"

#define xstr(s) str(s)
#define str(s) #s

#define APP_ID 1041967020620648508

// global variables
// bool core_initialized = false; core should always be initialized when the plugin is enabled.
discord::Core* core{};

// plugin settings
DFHACK_PLUGIN("rich_presence");
DFHACK_PLUGIN_IS_ENABLED(isPluginEnabled);

// create a logger
namespace DFHack {
    DBG_DECLARE(rich_presence, log, DebugCategory::LINFO);
}

// converts discord log levels to a string
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

// discord log level printer
void discordLog(discord::LogLevel level, const char* message) {
    // print debug message
    ERR(log).print("Discord [%s] - %s", logLevelToA(level), message);
}

bool initializeDiscord() {
    if(!isPluginEnabled) {
        // create discord integration
        auto create_result = discord::Core::Create(APP_ID, (uint64_t) discord::CreateFlags::NoRequireDiscord, &core);
        if(!core) {
            switch(create_result) {
                case discord::Result::InternalError:
                    ERR(log).printerr("Failed to connect to the discord client, make sure it is running before your enable this plugin.\n");
                    ERR(log).printerr("Run discord and then run: enable rich_presence to fix this problem.\n");
                default:
                    ERR(log).printerr("Failed to create a discord instance! (err: %d)\n", static_cast<int>(create_result));
                    break;
            }
            
            return false;
        }

        // discord logging
        core->SetLogHook(discord::LogLevel::Debug, discordLog);

        isPluginEnabled = true;
    }

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

const char* getGameModeText(DFHack::t_gamemodes gamemode) {
    switch (gamemode.g_type) {
    case DFHack::GameType::DWARF_MAIN:
        return "Fortress Mode";
    case DFHack::GameType::DWARF_ARENA:
        return "Arena Mode";
    case DFHack::GameType::DWARF_TUTORIAL:
        return "Stuck in the Tutorial";
    case DFHack::GameType::DWARF_RECLAIM:
        return "Reclaiming lost lands";
    case DFHack::GameType::DWARF_UNRETIRE:
        return "Coming back from a quick break";
    case DFHack::GameType::ADVENTURE_MAIN:
        return "Adventure Mode";
    case DFHack::GameType::ADVENTURE_ARENA:
        return "Adventure (Arena)";
    case DFHack::GameType::ADVENTURE_DUNGEON:
        return "Adventure (Dungeon)";
    case DFHack::GameType::ADVENTURE_WORLD_DEBUG:
        return "Adventure (Debug)";
    case DFHack::GameType::VIEW_LEGENDS:
        return "Legends Viewer";
    default:
        return "In the Menus";
    }
}

void updateActivity() {
    if (isPluginEnabled) {
        // get activity stats
        DFHack::t_gamemodes gamemode;
        {
            DFHack::CoreSuspender suspend;
            DFHack::World::ReadGameMode(gamemode);
        }

        DEBUG(log).print("Got gamemode sucessfully\n");
        const char* gameModeText = getGameModeText(gamemode);
        DEBUG(log).print("Discord Rich Detected Gamemode: %s\n", gameModeText);

        // send update to discord client
        discord::Activity activity{};
        activity.SetType(discord::ActivityType::Playing);
        std::string worldName = DF2UTF(DFHack::Translation::TranslateName(&df::global::world->world_data->name));
        DEBUG(log).print("Discord Rich Detected World: %s\n", worldName.c_str());

        std::string fortressName = "Test Fort";
        // activity detail text
        activity.SetDetails(gameModeText);

        std::string fortStr = "Working on " + fortressName;

        if(DFHack::World::isFortressMode(gamemode.g_type)) {
            activity.SetState(fortStr.c_str());
        } else {
            std::string worldDiscoveryStr = "In" + worldName;
            activity.SetState(worldDiscoveryStr.c_str());
        }

        // activity images
        discord::ActivityAssets& activityImgs = activity.GetAssets();
        activityImgs.SetLargeImage("df_discord_logo");
        activityImgs.SetLargeText("Dwarf Fortress");
        if(DFHack::World::isFortressMode(gamemode.g_type)) {
            activityImgs.SetSmallImage("fortress_mode_logo");
                activityImgs.SetSmallText(fortressName.c_str());
        } else {
                activityImgs.SetSmallImage("dwarf_fortress_classic");
                activityImgs.SetSmallText(gameModeText);
        }
        
        // activity start time
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        activity.GetTimestamps().SetStart(now);

        // update the activity
        core->ActivityManager().UpdateActivity(activity, [&](discord::Result result) {
            if (result != discord::Result::Ok) {
                ERR(log).printerr("Failed to update discord activity!\n");
            } else {
                DEBUG(log).print("Updated discord activity successfully!\n");
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
        out.color(DFHack::color_ostream::color_value::COLOR_GREEN);
        out.print("Enabling discord rich presence\n");
        out.reset_color();

        // create the discord state
        if(!isPluginEnabled) {
            if(!initializeDiscord()) {
                out.printerr("Failed to initialize discord. Are you sure it is open?");
                return DFHack::CR_FAILURE;
            }

            updateActivity();
        }

    } else {
        out.color(DFHack::color_ostream::color_value::COLOR_YELLOW);
        out.print("Disabling discord rich presence\n");
        out.reset_color();

        // delete the discord state
        deinitDiscord();
    }

    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_onstatechange(DFHack::color_ostream& out, DFHack::state_change_event event) {
    switch (event) {
    case DFHack::SC_WORLD_LOADED:
        out.print("Setting new discord activity\n");
        break;
    default:
        break;
    }

    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_init(DFHack::color_ostream& out, std::vector<DFHack::PluginCommand>& commands) {
    // attempt to create the discord core
    if(!initializeDiscord()) {
        isPluginEnabled = false;
    } else {
        // plugin enabled is automatically set
        out.print("[DF Rich Presence] Updating the user's activity\n");
        // set users activity
        updateActivity();
    }

    // create df commands
    commands.push_back(DFHack::PluginCommand("rich_presence", "Configure the discord rich presence plugin.", rich_presence, false, 
        "rich_presence : configures the discord rich presence plugin\n"));
    
    out.print("Rich presence setup successfully\n");
    out.color(DFHack::color_ostream::color_value::COLOR_CYAN);
    out.print("If you encounter any errors, please report them to: https://github.com/lochnessdragon/df-rich-presence/issues\n");
    out.reset_color();

    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_shutdown(DFHack::color_ostream& out) {
    if(isPluginEnabled) {
        deinitDiscord();
    }

    out.print("Rich presence shutdown!\n");
    return DFHack::CR_OK;
}