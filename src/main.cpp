#ifdef _WIN32
#define NOMINMAX 
#define UNICODE
#define STRICT
#endif 

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
#include "df/ui.h"
#include "df/world_site.h"
#include "df/language_name.h"

#define xstr(s) str(s)
#define str(s) #s

#define APP_ID 1041967020620648508

#define LOG_STR "[Discord RPC]: "

// global variables
// bool core_initialized = false; core should always be initialized when the plugin is enabled.
discord::Core* core{};
uint64_t pluginStartTime;

// plugin settings
DFHACK_PLUGIN("rich_presence");
DFHACK_PLUGIN_IS_ENABLED(isPluginEnabled);
REQUIRE_GLOBAL(world);
REQUIRE_GLOBAL(ui);

// create a logger
namespace DFHack {
    DBG_DECLARE(rich_presence, log, DebugCategory::LDEBUG);
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
    ERR(log).print("Discord SDK: [%s] - %s", logLevelToA(level), message);
}

bool initializeDiscord() {
    if(!isPluginEnabled) {
        pluginStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        // create discord integration
        auto create_result = discord::Core::Create(APP_ID, (uint64_t) discord::CreateFlags::NoRequireDiscord, &core);
        if(!core) {
            switch(create_result) {
                case discord::Result::InternalError:
                    ERR(log).printerr(LOG_STR "Failed to connect to the discord client, make sure it is running before your enable this plugin.\n");
                    ERR(log).printerr(LOG_STR "Run discord and then run: enable rich_presence to fix this problem.\n");
                default:
                    ERR(log).printerr(LOG_STR "Failed to create a discord instance! (err: %d)\n", static_cast<int>(create_result));
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
                ERR(log).printerr(LOG_STR "Error clearing activity err=%d\n", static_cast<int>(result));
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
    case DFHack::GameType::DWARF_TUTORIAL:
        return "Stuck in the Tutorial";
    case DFHack::GameType::DWARF_RECLAIM:
        return "Reclaiming a lost fortress";
    case DFHack::GameType::DWARF_UNRETIRE:
        return "Unretiring a fort";
    case DFHack::GameType::ADVENTURE_MAIN:
        return "Adventure Mode";
    case DFHack::GameType::ADVENTURE_DUNGEON:
        return "Adventure (Dungeon)";
    case DFHack::GameType::ADVENTURE_WORLD_DEBUG:
        return "Adventure (Debug)";
    case DFHack::GameType::ADVENTURE_ARENA:
    case DFHack::GameType::DWARF_ARENA:
        return "Object testing arena";
    case DFHack::GameType::VIEW_LEGENDS:
        return "Legends Viewer";
    default:
        return "In the Menus";
    }
}

const char * getFortressDesignation(uint16_t fortress_rank) {
    switch (fortress_rank) {
        case 0:
            return "Outpost";
        case 1:
            return "Hamlet";
        case 2:
            return "Village";
        case 3:
            return "Town";
        case 4:
            return "City";
        case 5:
            return "Metropolis";
        default:
            return "Unknown rank";
    }
}

struct DFState {
    DFHack::t_gamemodes gamemode;

    bool hasFortName;
    std::string fortName;
    std::string fortNameEnglish;
    const char* fortDesignation;

    bool hasWorldName;
    std::string worldName;
};

DFState getDFData() {
    DFState state {};
    {
        DFHack::CoreSuspender suspend;
        DFHack::World::ReadGameMode(state.gamemode);

        // try to read the world name
        if(DFHack::World::isFortressMode(state.gamemode.g_type) ||
           DFHack::World::isAdventureMode(state.gamemode.g_type) ||
           DFHack::World::isLegends(state.gamemode.g_type)
        ) {
            if(df::global::world->world_data != nullptr) {
                state.worldName = DF2UTF(DFHack::Translation::TranslateName(&df::global::world->world_data->name));
                state.hasWorldName = true;
            }
        }

        // try to read the fortress name
        if(DFHack::World::isFortressMode(state.gamemode.g_type)) {
            // df::global::ui->fortress_rank (Outpost, etc.)
            if(df::global::ui->main.fortress_site != nullptr) {
                df::language_name fortLangName = df::global::ui->main.fortress_site->name;
                state.fortName = DF2UTF(DFHack::Translation::TranslateName(&fortLangName, false));
                state.fortNameEnglish = DF2UTF(DFHack::Translation::TranslateName(&fortLangName, true));
                state.fortDesignation = getFortressDesignation(df::global::ui->fortress_rank);
                state.hasFortName = true;
            }
        }
    }

    return state;
}

void updateActivity() {
    if (isPluginEnabled) {
        // get activity stats
        DFState state = getDFData();

        const char* gameModeText = getGameModeText(state.gamemode);
        DEBUG(log).print(LOG_STR "Discord Rich Detected Gamemode: %s\n", gameModeText);

        // send update to discord client
        discord::Activity activity{};
        activity.SetType(discord::ActivityType::Playing);
        // activity detail text
        activity.SetDetails(gameModeText);

        // set activity state details
        if(state.hasFortName) {
            std::string activityStr = std::string("Working on ") + std::string(state.fortDesignation) + " " + state.fortName;
            activity.SetState(activityStr.c_str());
        } else if (DFHack::World::isAdventureMode(state.gamemode.g_type) || DFHack::World::isLegends(state.gamemode.g_type)) {            
            std::string worldDiscoveryStr = "In " + state.worldName;
            activity.SetState(worldDiscoveryStr.c_str());
        }

        // activity images
        discord::ActivityAssets& activityImgs = activity.GetAssets();
        activityImgs.SetLargeImage("df_discord_logo");
        activityImgs.SetLargeText("Dwarf Fortress");

        if(state.hasFortName) {
            activityImgs.SetSmallImage("fortress_mode_logo");
            std::string activityStr = std::string(state.fortDesignation) + " " + state.fortName;
            activityImgs.SetSmallText(activityStr.c_str());
        } else {
                activityImgs.SetSmallImage("dwarf_fortress_classic");
                activityImgs.SetSmallText(gameModeText);
        }
        
        // activity start time
        activity.GetTimestamps().SetStart(pluginStartTime);

        // update the activity
        core->ActivityManager().UpdateActivity(activity, [&](discord::Result result) {
            if (result != discord::Result::Ok) {
                ERR(log).printerr(LOG_STR "Failed to update discord activity. err=%d\n", static_cast<int>(result));
            } else {
                DEBUG(log).print(LOG_STR "Updated discord activity successfully!\n");
            }
        });
    }
}

void usage(DFHack::color_ostream& out) {
    out.print("rich_presence: tools to manipulate the Discord Rich Presence plugin");
    out.print(" - update : force a rich presence update (use this if discord has desynced with dwarf fortress)");
    out.print(" - data : shows what data this plugin is able to pull from dwarf fortress. (mostly for developers only)");
    out.print(" - help : displays this message");
}

// rich presence command callback
DFHack::command_result rich_presence(DFHack::color_ostream& out, std::vector<std::string> & params) {
    
    if(!isPluginEnabled) {
        return DFHack::CR_WRONG_USAGE;
    }

    if(params.size() > 0) {
        if (params[0] == "help") {
            // print usage
            usage(out);
        } else if (params[0] == "update") {
            out.print(LOG_STR "Updating Discord Rich Presence\n");
            updateActivity();
        } else if (params[0] == "data") {
            // print data from dwarf fortress.
            DFState state = getDFData();
            out.print("Data pulled from Dwarf Fortress:\n");
            out.print("Game mode: %s\n", getGameModeText(state.gamemode));

            if(state.hasWorldName) {
                out.print("World: %s\n", state.worldName.c_str());
            }

            if (state.hasFortName) {
                out.print("Fort: %s %s \"%s\"\n", state.fortDesignation, state.fortName.c_str(), state.fortNameEnglish.c_str());
            }
        } else {
            usage(out);
        }
    } else {
        // print usage
        usage(out);
    }

    return DFHack::CR_OK;
}

// only runs when the plugin is enabled
DFhackCExport DFHack::command_result plugin_onupdate(DFHack::color_ostream& out) {
    core->RunCallbacks(); // according to the discord sdk, this should be run once every game loop.
    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_enable(DFHack::color_ostream& out, bool enabled) {
    if(enabled) {
        out.color(DFHack::color_ostream::color_value::COLOR_GREEN);
        out.print(LOG_STR "Enabling discord rich presence\n");
        out.reset_color();

        // create the discord state
        if(!isPluginEnabled) {
            if(!initializeDiscord()) {
                out.printerr(LOG_STR "Failed to initialize discord. Are you sure it is open?\n");
                return DFHack::CR_FAILURE;
            }

            updateActivity();
        }

    } else {
        out.color(DFHack::color_ostream::color_value::COLOR_YELLOW);
        out.print(LOG_STR "Disabling discord rich presence\n");
        out.reset_color();

        // delete the discord state
        deinitDiscord();
    }

    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_onstatechange(DFHack::color_ostream& out, DFHack::state_change_event event) {
    switch (event) {
    case DFHack::SC_WORLD_LOADED:
    case DFHack::SC_WORLD_UNLOADED:
    case DFHack::SC_MAP_LOADED:
    case DFHack::SC_MAP_UNLOADED:
        updateActivity();
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
        // is plugin enabled will be true here
        updateActivity();
    }

    // create df commands
    commands.push_back(DFHack::PluginCommand("rich_presence", "Configure the discord rich presence plugin.", rich_presence, false, 
        "rich_presence : configures the discord rich presence plugin\n"));
    
    out.print(LOG_STR "Rich presence setup successfully\n");
    out.color(DFHack::color_ostream::color_value::COLOR_CYAN);
    out.print(LOG_STR "If you encounter any errors, please report them to: https://github.com/lochnessdragon/df-rich-presence/issues\n");
    out.reset_color();

    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result plugin_shutdown(DFHack::color_ostream& out) {
    if(isPluginEnabled) {
        deinitDiscord();
    }

    out.print(LOG_STR "Rich presence shutdown!\n");
    return DFHack::CR_OK;
}