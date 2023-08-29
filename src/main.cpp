#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "pch.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/GameEntities/Agent.h>

#include <imgui_renderer.h>
#include <GWCA/Managers/StoCMgr.h>


#define RGBA_COL(r, g, b, a) ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f)
#define RGB_COL(r, g, b) RGBA_COL(r, g, b, 255.f)


using BattleId = uint32_t;
enum class BattleType : uint32_t
{
    New_Scrimmage,
    New_Unrated_Challenge = 3,
    Join_Scrimmage,
    Rated_AutomatedMatch,
    GvG_Daily_B_Registration = 7,
};

namespace GW::Packet::StoC
{

    struct PvP_AddBattleEntry : Packet<PvP_AddBattleEntry> {
        /* 
        This packet gets sent when you have the 'Guild Battle' - Window already open
        and someone creates a new battle entry.
        */

        /* 0x0004 */ BattleId   m_Id;
        /* 0x0008 */ BattleType m_Type;
        /* 0x000C */ wchar_t    m_PlayerName[32];
        /* 0x004C */ uint32_t   unk004C;
        /* 0x0050 */ uint32_t   unk0050;
        /* 0x0054 */ uint32_t   unk0054;
    };
    constexpr uint32_t Packet<PvP_AddBattleEntry>::STATIC_HEADER = 434;
    static_assert(sizeof(PvP_AddBattleEntry) == 0x58);


    struct PvP_StreamBattleEntry : Packet<PvP_StreamBattleEntry> {
        /*
        This packet gets sent when you start to open the 'Guild Battle' - Window.
        It indicates a stream and sends you all existing battles. The stream ends
        with either P437 or P440.
        */

        /* 0x0004 */ BattleId   m_Id;
        /* 0x0008 */ BattleType m_Type;
        /* 0x000C */ wchar_t    m_PlayerName[32];
        /* 0x004C */ uint32_t   unk004C;
        /* 0x0050 */ uint32_t   unk0050;
        /* 0x0054 */ uint32_t   unk0054;
    };
    constexpr uint32_t Packet<PvP_StreamBattleEntry>::STATIC_HEADER = 436;
    static_assert(sizeof(PvP_StreamBattleEntry) == 0x58);


    struct PvP_GuildBattleClosed : Packet<PvP_GuildBattleClosed> 
    {
        /*
        This packet gets sent when you close the 'Guild Battle' - Window.
        */
    };
    constexpr uint32_t Packet<PvP_GuildBattleClosed>::STATIC_HEADER = 441;
    static_assert(sizeof(PvP_GuildBattleClosed) == 0x4);

}


std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty())
        return std::string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(static_cast<size_t>(size_needed), 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

    return strTo;
}

namespace
{
    static GW::HookEntry* g_GwPacketEntries;
    static std::vector<std::wstring> g_BattleEntries {};
    static std::vector<std::wstring> g_DuplicatorNames {};
    static unsigned int g_AnetEntryCounter = 0;
    static unsigned int g_BattleEntryCounter = 0;
    static unsigned int g_BlockedEntryCounter = 0;

}

bool Draw(IDirect3DDevice9* device)
{
    UNREFERENCED_PARAMETER(device);

    static bool keep_alive = true;
    static bool init = false;
    static bool show_window = true;

    if (!init)
    {
        GW::Chat::WriteChat(GW::Chat::CHANNEL_MODERATOR, APP_NAMEW L": Initialized");
        init = true;
    }    

    ImGui::SetNextWindowSize({ 150, -1 });
    if (ImGui::Begin(APP_NAME, &show_window, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImVec4 ColorText = RGB_COL(147, 105, 255);
        ImVec4 ColorNumber = RGB_COL(66, 227, 245);

        ImGui::TextColored(ColorText, "A-Net Entries: ");
        ImGui::SameLine(ImGui::GetWindowWidth() - 20);
        ImGui::TextColored(ColorNumber, "%d", g_AnetEntryCounter);

        ImGui::TextColored(ColorText, "Blocked scrims: ");
        ImGui::SameLine(ImGui::GetWindowWidth() - 20);
        ImGui::TextColored(ColorNumber, "%d", g_BlockedEntryCounter);

        ImGui::TextColored(ColorText, "Total Entries: ");
        ImGui::SameLine(ImGui::GetWindowWidth() - 20);
        ImGui::TextColored(ColorNumber, "%d", g_BattleEntryCounter);

        ImGui::NewLine();
        ImGui::TextColored(ColorText, "Tried to crash:");
        for (const auto& entry : g_DuplicatorNames)
            ImGui::Text(" - %s",  WStringToString(entry).c_str());
        
    }
    ImGui::End();


    if (!show_window)
    {
        GW::Chat::WriteChat(GW::Chat::CHANNEL_MODERATOR, APP_NAMEW L": Bye!");
        keep_alive = false;
    }        

    return keep_alive;
}



void PvP_StreamBattleEntry_Callback(GW::HookStatus* status, GW::Packet::StoC::PvP_StreamBattleEntry* packet)
{
    g_BattleEntryCounter++;

    //First Battle entries are always with empty names, can just use that as a reset.
    if (packet->m_PlayerName[0] == '\0')
    {
        g_AnetEntryCounter++;
        return;
    }

    //GW can only store up to 255 (1 Byte) Battle entries or else it will crash.
    if (packet->m_Id > 255)
    {
        status->blocked = true;
        return;
    }


    std::wstring name = { packet->m_PlayerName };
    if (std::find(g_BattleEntries.begin(), g_BattleEntries.end(), name) == g_BattleEntries.end())
    {
        g_BattleEntries.push_back(name);
        return;
    }

    if (std::find(g_DuplicatorNames.begin(), g_DuplicatorNames.end(), name) == g_DuplicatorNames.end())
        g_DuplicatorNames.push_back(name);

    g_BlockedEntryCounter++;
    status->blocked = true;
}


void PvP_AddBattleEntry_Callback(GW::HookStatus* status, GW::Packet::StoC::PvP_AddBattleEntry* packet)
{
    PvP_StreamBattleEntry_Callback(status, (GW::Packet::StoC::PvP_StreamBattleEntry*)packet);
}


void PvP_GuildBattleClosed_Callback(GW::HookStatus*, GW::Packet::StoC::PvP_GuildBattleClosed*)
{
    g_DuplicatorNames.clear();
    g_BattleEntries.clear();
    g_AnetEntryCounter = 0;
    g_BattleEntryCounter = 0;
}

static DWORD WINAPI ThreadProc(LPVOID lpModule)
{
    HMODULE hModule = static_cast<HMODULE>(lpModule);

    if (GW::Initialize())
    {
        InitializeImGui(Draw);

        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PvP_StreamBattleEntry>(g_GwPacketEntries, PvP_StreamBattleEntry_Callback);
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PvP_AddBattleEntry>(g_GwPacketEntries, PvP_AddBattleEntry_Callback);
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PvP_GuildBattleClosed>(g_GwPacketEntries, PvP_GuildBattleClosed_Callback);

        while (IsRunning()) {
            Sleep(100);
        }

        while (GW::HookBase::GetInHookCount())
            Sleep(16);

        Sleep(16);
        GW::Terminate();
    }

    FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);
    DisableThreadLibraryCalls(hModule);

    if (dwReason == DLL_PROCESS_ATTACH) {
        HANDLE handle = CreateThread(0, 0, ThreadProc, hModule, 0, 0);
        if (handle)
            CloseHandle(handle);
    }

    return TRUE;
}
