#pragma once

// zayr: small pure helpers shared by the app and the host test (tests/test_data.cpp).

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Zayr
{
    // PlayerStatus.SavePos (Vector3): the value in the flag table is the file offset of x, y, z (floats).
    constexpr uint32_t PlayerSavePosHash = 0xc884818d;

    inline bool ReadVector3(const unsigned char* buffer, size_t size, uint32_t offset, float out[3])
    {
        if (offset < 0x28 || (size_t)offset + 12 > size)
            return false;
        std::memcpy(out, buffer + offset, 12); // little endian, same as the console
        return true;
    }

    // Layer by height, the app's rule: sky >= 750, depths < -50 (0 depths, 1 surface, 2 sky)
    inline int LayerForHeight(float y)
    {
        if (y >= 750.0f) return 2;
        if (y < -50.0f) return 0;
        return 1;
    }

    // Fixed layer from map_data.json (-1 = none) or the height rule
    inline int ObjectLayer(int fixedLayer, float y)
    {
        return fixedLayer >= 0 ? fixedLayer : LayerForHeight(y);
    }
}
