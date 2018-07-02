#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_NODE_POOL_H__
#define __MGN_TERRAIN_MERCATOR_NODE_POOL_H__

#include "mgnTrMercatorNodeKey.h"

namespace mgn {
    namespace terrain {

        // Forward declarations
        class MercatorNode;

        /*! Mercator node pool class.
        ** The main proposal of such a pool class is not to destroy nodes on detach.
        ** This class has been made as LRU cache analog without memory allocations.
        */
        class MercatorNodePool {
        public:
            MercatorNodePool(int capacity);
            ~MercatorNodePool();

            //! Pulls node from pool.
            // @return Returns node if key exists and NULL otherwise.
            MercatorNode * Pull(const MercatorNodeKey& key);

            //! Pushes node to pull
            // @return Returns NULL if cache isn't full and NOT NULL if
            //         storage is full and one node has been moved out.
            MercatorNode * Push(MercatorNode * node);

        private:
            int capacity_;
            int size_;
            MercatorNode* *nodes_;
        };

    } // namespace terrain
} // namespace mgn

#endif