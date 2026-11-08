// Host test for romfs/map_data.json (zayr).
// Build and run from the repository root:
//   g++ -std=c++17 -O2 -Iinclude tests/test_data.cpp -o test_data && ./test_data
// Checks that every flag hash matches the name it comes from (MurmurHash3, seed 0) and
// evaluates every object against romfs/100.sav with the same rules as SavefileIO::ParseFile.
#include <json.hpp>

#include "../source/Zayr.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using nlohmann::json;

static uint32_t Murmur3(const std::string& s)
{
    const uint8_t* data = (const uint8_t*)s.data();
    const size_t len = s.size();
    const uint32_t c1 = 0xcc9e2d51, c2 = 0x1b873593;
    uint32_t h = 0;
    auto rotl = [](uint32_t x, int r) { return (x << r) | (x >> (32 - r)); };
    size_t i = 0;
    for (; i + 4 <= len; i += 4)
    {
        uint32_t k;
        std::memcpy(&k, data + i, 4);
        k *= c1; k = rotl(k, 15); k *= c2;
        h ^= k; h = rotl(h, 13); h = h * 5 + 0xe6546b64;
    }
    uint32_t k = 0;
    switch (len & 3)
    {
        case 3: k ^= data[i + 2] << 16; // fall through
        case 2: k ^= data[i + 1] << 8;  // fall through
        case 1: k ^= data[i]; k *= c1; k = rotl(k, 15); k *= c2; h ^= k;
    }
    h ^= (uint32_t)len;
    h ^= h >> 16; h *= 0x85ebca6b; h ^= h >> 13; h *= 0xc2b2ae35; h ^= h >> 16;
    return h;
}

static int failures = 0;

static void Check(bool ok, const std::string& message)
{
    if (!ok)
    {
        failures++;
        std::cerr << "FAIL: " << message << "\n";
    }
}

static uint32_t U32(const std::vector<uint8_t>& b, size_t o)
{
    return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | ((uint32_t)b[o + 3] << 24);
}

int main()
{
    std::ifstream jsonFile("romfs/map_data.json");
    if (!jsonFile) { std::cerr << "run from the repository root\n"; return 2; }
    json data = json::parse(jsonFile);

    // Flag names behind each hash
    Check(Murmur3("DungeonState.Dungeon000") == 0x3eb899d5, "murmur3 self test");
    for (auto& e : data["shrines"])
        Check(uint32_t(e["completion_hash"]) == Murmur3("DungeonState." + std::string(e["internal_name"])), "shrine " + std::string(e["internal_name"]));
    for (auto& e : data["lightroots"])
        Check(uint32_t(e["completion_hash"]) == Murmur3("ArrivalPointState." + std::string(e["internal_name"])), "lightroot " + std::string(e["internal_name"]));
    for (auto& e : data["hidden_koroks"])
        Check(uint32_t(e["completion_hash"]) == Murmur3("IsAppearKorok." + std::to_string(std::stoull(std::string(e["internalHash"]), nullptr, 16))), "korok " + std::string(e["internalHash"]));
    for (auto& e : data["carry_koroks"])
        Check(uint32_t(e["completion_hash"]) == Murmur3("KorokCarryProgress." + std::to_string(std::stoull(std::string(e["internalHash"]), nullptr, 16))), "carry korok " + std::string(e["internalHash"]));

    // Caves, wells and chasms: the cave flag and the flag of that entrance
    int entrances = 0;
    for (const char* cat : { "caves", "wells", "chasms" })
    {
        for (auto& e : data[cat])
        {
            std::string name = e["internal_name"];
            Check(uint32_t(e["completion_hash"]) == Murmur3("IsVisitLocation." + name), std::string(cat) + " " + name);
            Check(e.contains("entrance_id"), std::string(cat) + " " + name + " without entrance_id");
            if (name == "ZoraZonauTerminal")
                Check(!e.contains("completion_hash2"), "ZoraZonauTerminal keeps one flag");
            else if (e.contains("entrance_id"))
            {
                Check(e.contains("completion_hash2") && uint32_t(e["completion_hash2"]) == Murmur3("IsVisitLocationArea_CaveEntrance." + std::string(e["entrance_id"])),
                      std::string(cat) + " " + name + " entrance flag");
                entrances++;
            }
        }
    }
    Check(entrances == 290, "290 entrances with a second flag, got " + std::to_string(entrances));

    // Issue #13: the four caves have their second entrance flag
    int issue13 = 0;
    for (auto& e : data["caves"])
    {
        std::string id = e.value("entrance_id", "");
        if (id == "11762902907749850765" || id == "11860991231750302286" || id == "6101576164770829020" || id == "6248504940692771516")
            issue13++;
    }
    Check(issue13 == 4, "issue #13 entrances");

    // New locations
    int towers = 0, tablets = 0, temples = 0;
    for (auto& e : data["locations"])
    {
        std::string n = e["display_name"];
        if (n.find("Skyview Tower") != std::string::npos) { towers++; Check(e.contains("completion_hash2"), n + " needs IsOpenCannon"); }
        if (n.rfind("Ancient Tablet", 0) == 0) { tablets++; Check(e.value("layer", "") == "Sky", n + " layer"); }
        if (n == "The Legendary Stormwind Ark" || n == "Lost Gorondia Rediscovered" || n == "The Mural's Myth, Risen from the Sands" ||
            n == "The Vessel of Spirit's Resting Place" || n == "The Great Wellspring of Hyrule") temples++;
    }
    Check(towers == 15, "15 towers, got " + std::to_string(towers));
    Check(tablets == 12, "12 tablets, got " + std::to_string(tablets));
    Check(temples == 5, "5 temples, got " + std::to_string(temples));

    // Evaluate everything against the 100% save, same rules as SavefileIO::ParseFile
    std::ifstream saveFile("romfs/100.sav", std::ios::binary);
    std::vector<uint8_t> save((std::istreambuf_iterator<char>(saveFile)), std::istreambuf_iterator<char>());
    Check(save.size() > 0x28, "romfs/100.sav");
    std::unordered_map<uint32_t, uint32_t> flags;
    bool hasPlayer = false;
    float player[3] = { 0, 0, 0 };
    for (size_t o = 0x28; o + 8 <= save.size(); o += 8)
    {
        uint32_t h = U32(save, o);
        if (h == 0xa3db7114) break;
        flags.emplace(h, U32(save, o + 4));
        if (h == Zayr::PlayerSavePosHash)
            hasPlayer = Zayr::ReadVector3(save.data(), save.size(), U32(save, o + 4), player);
    }

    // Link's position (PlayerStatus.SavePos) in the 100% save: in the sky, near the Great Sky Island
    Check(Murmur3("PlayerStatus.SavePos") == Zayr::PlayerSavePosHash, "PlayerStatus.SavePos hash");
    Check(hasPlayer && std::abs(player[0] - 1074.545f) < 0.01f && std::abs(player[1] - 891.802f) < 0.01f && std::abs(player[2] - 3347.016f) < 0.01f,
          "Link position in the 100% save");
    Check(Zayr::LayerForHeight(player[1]) == 2, "Link is on the sky layer");
    Check(!Zayr::ReadVector3(save.data(), save.size(), (uint32_t)save.size() - 8, player), "vector out of the file is rejected");
    Check(Zayr::LayerForHeight(749.9f) == 1 && Zayr::LayerForHeight(750.0f) == 2 && Zayr::LayerForHeight(-50.1f) == 0, "layer by height");
    Check(Zayr::ObjectLayer(2, 100.0f) == 2 && Zayr::ObjectLayer(-1, 100.0f) == 1, "fixed layer wins");

    // The three sky shrines keep their real height and a fixed sky layer
    int skyShrines = 0;
    for (auto& e : data["shrines"])
        if (e.value("layer", "") == "Sky") { skyShrines++; Check(float(e["position"]["y"]) < 750.0f, "sky shrine at its real height"); }
    Check(skyShrines == 3, "3 sky shrines with a fixed layer, got " + std::to_string(skyShrines));
    const uint32_t Clear = 1654019904, Open = 404286466;
    int total = 0, missing = 0;
    for (auto& [cat, list] : data.items())
    {
        if (cat == "sages_wills" || cat == "addison_signposts") continue;
        for (auto& e : list)
        {
            total++;
            auto it = flags.find(uint32_t(e["completion_hash"]));
            bool found = false;
            if (it != flags.end())
            {
                uint32_t s = it->second;
                if (cat == "shrines" || cat == "carry_koroks") found = s == Clear;
                else if (cat == "lightroots") found = s == Open;
                else found = s != 0;
                if (found && e.contains("completion_hash2"))
                {
                    auto it2 = flags.find(uint32_t(e["completion_hash2"]));
                    found = it2 != flags.end() && it2->second != 0;
                }
            }
            if (!found)
            {
                missing++;
                std::string what = e.contains("display_name") ? std::string(e["display_name"]) : std::to_string(uint32_t(e["completion_hash"]));
                // Known: one old map (IsFindTreasureMap.Armor_215_Head) is not in the 100% save; old maps do not count.
                if (cat != "old_maps")
                    Check(false, "not found in the 100% save: " + cat + " " + what);
            }
        }
    }
    Check(missing == 1, "exactly one object missing in the 100% save (an old map), got " + std::to_string(missing));

    std::cout << total << " objects checked, " << entrances << " entrances, " << failures << " failures\n";
    return failures ? 1 : 0;
}
