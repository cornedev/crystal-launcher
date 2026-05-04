#include "api.hpp"

namespace crystal
{

// - helper defines.
static DiscordRichPresence presence;
static std::string smallimagekey;
static std::string smallimagetext;
static std::string largeimagekey;
static std::string largeimagetext;
static std::string rpcdetails;
static std::string buttonlabel;
static std::string buttonurl;
static int64_t time = 0;
// - end helper defines.

namespace discord
{
    bool StartRichPresence()
    {
        DiscordEventHandlers handlers{};
        std::string appid = "1500192183431467058";
        Discord_Initialize(appid.c_str(), &handlers, 1, nullptr);

        std::memset(&presence, 0, sizeof(presence));

        time = std::time(nullptr);
        presence.startTimestamp = time;

        largeimagekey = "crystal";
        largeimagetext = "crystal launcher";
        presence.largeImageKey = largeimagekey.c_str();
        presence.largeImageText = largeimagetext.c_str();

        buttonlabel = "view project";
        buttonurl = "https://github.com/cornedev/crystal-launcher";
        presence.button1_label = buttonlabel.c_str();
        presence.button1_url = buttonurl.c_str();
        
        SetRichPresenceDetails("idling...");

        Discord_UpdatePresence(&presence);
        return true;
    }

    bool SetRichPresenceImage(const std::string& image)
    {
        largeimagekey = image;
        presence.largeImageKey = largeimagekey.c_str();

        Discord_UpdatePresence(&presence);
        return true;
    }

    bool SetRichPresenceSmall(const std::string& username, const std::string& uuid)
    {
        smallimagekey = "https://api.mcheads.org/head/" + uuid + "/32";
        smallimagetext = username;
        presence.smallImageKey = smallimagekey.c_str();
        presence.smallImageText = smallimagetext.c_str();

        Discord_UpdatePresence(&presence);
        return true;
    }

    bool RemoveRichPresenceSmall()
    {
        presence.smallImageKey = nullptr;
        presence.smallImageText = nullptr;

        Discord_UpdatePresence(&presence);
        return true;
    }

    bool SetRichPresenceDetails(const std::string& details)
    {
        rpcdetails = details;
        presence.details = rpcdetails.c_str();

        Discord_UpdatePresence(&presence);
        return true;
    }
}

}
