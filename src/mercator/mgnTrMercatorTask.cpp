#include "mgnTrMercatorTask.h"

namespace mgn {
    namespace terrain {

        Task::Task(MercatorNode * node, int type)
        : node_(node)
        , type_(type)
        {
        }
        Task::~Task()
        {
        }
        MercatorNode * Task::node() const
        {
            return node_;
        }
        TaskNodeMatchFunctor::TaskNodeMatchFunctor(MercatorNode * node)
        : node_(node)
        {

        }
        bool TaskNodeMatchFunctor::operator()(Task * task) const
        {
            return task->node() == node_;
        }

    } // namespace terrain
} // namespace mgn