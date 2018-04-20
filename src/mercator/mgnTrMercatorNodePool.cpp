#include "mgnTrMercatorNodePool.h"

#include "mgnTrMercatorNode.h"

namespace mgn {
    namespace terrain {

        MercatorNodePool::MercatorNodePool(int capacity)
        : capacity_(capacity)
        , size_(0)
        {
            nodes_ = new MercatorNode*[capacity];
        }
        MercatorNodePool::~MercatorNodePool()
        {
            delete[] nodes_;
        }
        MercatorNode * MercatorNodePool::Pull(const MercatorNodeKey& key)
        {
            for (int i = 0; i < size_; ++i)
            {
                MercatorNode * node = nodes_[i];
                if (node->x() == key.x && node->y() == key.y && node->lod() == key.lod)
                {
                    --size_;
                    if (i < size_)
                    {
                        memmove(nodes_ + i, nodes_ + i + 1, (size_ - i) * sizeof(MercatorNode*));
                        nodes_[size_] = NULL;
                    }
                    else
                        nodes_[i] = NULL;
                    return node;
                }
            }
            return NULL;
        }
        void MercatorNodePool::Push(MercatorNode * node)
        {
            if (size_ == capacity_)
            {
                // Erase the oldest one
                --size_;
                memmove(nodes_, nodes_ + 1, size_ * sizeof(MercatorNode*));
            }
            nodes_[size_] = node;
            ++size_;
        }

    } // namespace terrain
} // namespace mgn