#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TASK_H__
#define __MGN_TERRAIN_MERCATOR_TASK_H__

namespace mgn {
    namespace terrain {

        enum MercatorRequestType {
            REQUEST_STARTER,
            REQUEST_TEXTURE,
            REQUEST_HEIGHTMAP
        };

        // Forward declarations
        class MercatorNode;

        //! Base task class
        class Task {
        public:
            Task(MercatorNode * node, int type);
            virtual ~Task();

            MercatorNode * node() const;

            virtual void Execute() = 0; //!< target task, done on service thread
            virtual void Process() = 0; //!< data processing after task is completed, done on main thread

            static bool NodeMatchPredicate(Task * task);

        protected:
            MercatorNode * node_;
            int type_;
        };

        //! Functor for node matching
        class TaskNodeMatchFunctor {
        public:
            TaskNodeMatchFunctor(MercatorNode * node);
            bool operator()(Task * task) const;

        private:
            MercatorNode * node_;
        };

    } // namespace terrain
} // namespace mgn

#endif