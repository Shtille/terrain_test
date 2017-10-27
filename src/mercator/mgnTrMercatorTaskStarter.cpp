#include "mgnTrMercatorTaskStarter.h"

#include "mgnTrMercatorNode.h"

namespace mgn {
    namespace terrain {

        StarterTask::StarterTask(MercatorNode * node)
        : Task(node, REQUEST_STARTER)
        {
        }
        StarterTask::~StarterTask()
        {
        }
        void StarterTask::Execute()
        {
        }
        void StarterTask::Process()
        {
            node_->OnStarterTaskCompleted();
        }

    } // namespace terrain
} // namespace mgn