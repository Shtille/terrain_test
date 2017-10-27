#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_SERVICE_H__
#define __MGN_TERRAIN_MERCATOR_SERVICE_H__

#include "mgnTrMercatorTask.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/thread.hpp>

#include <list>

namespace mgn {
    namespace terrain {

    	class MercatorService {
		public:
			typedef std::list<Task*> TaskList;

			MercatorService();
			virtual ~MercatorService();

			void RunService();
			void StopService();

			void ClearTasks();
            void AddTask(Task * task);
            bool GetDoneTasks(TaskList& task_list);
            bool RemoveAllNodeTasks(MercatorNode * node);

		private:
			MercatorService(MercatorService&);
			MercatorService& operator=(MercatorService&);

			void ThreadFunc();

			boost::mutex mutex_;
			boost::condition_variable condition_variable_;
			boost::thread thread_;

            TaskList tasks_;
            TaskList done_tasks_;
            MercatorNode * processed_node_;

			bool finishing_;
		};

    } // namespace terrain
} // namespace mgn

#endif