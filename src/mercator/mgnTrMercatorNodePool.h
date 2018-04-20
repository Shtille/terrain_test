#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_NODE_POOL_H__
#define __MGN_TERRAIN_MERCATOR_NODE_POOL_H__

#include "mgnTrMercatorNodeKey.h"

namespace mgn {
    namespace terrain {

        // Forward declarations
        class MercatorNode;

        //! Mercator node pool class
        /*! The main proposal of such a pool class is not to destroy nodes on detach.
        */
        class MercatorNodePool {
        public:
            MercatorNodePool(int capacity);
            ~MercatorNodePool();

            MercatorNode * Pull(const MercatorNodeKey& key);
            void Push(MercatorNode * node);

        private:
            int capacity_;
            int size_;
            MercatorNode* *nodes_;
        };

    } // namespace terrain
} // namespace mgn

#endif