#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_NODE_KEY_H__
#define __MGN_TERRAIN_MERCATOR_NODE_KEY_H__

namespace mgn {
    namespace terrain {

        // Mercator node key structure
        struct MercatorNodeKey {
            int lod;
            int x;
            int y;

            MercatorNodeKey(int lod, int x, int y);

            bool operator < (const MercatorNodeKey& other);
            friend bool operator < (const MercatorNodeKey& lhs, const MercatorNodeKey& rhs);
            bool operator ==(const MercatorNodeKey& other);
        };

    } // namespace terrain
} // namespace mgn

#endif