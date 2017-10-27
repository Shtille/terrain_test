#include "mgnTrMercatorService.h"

#include <boost/functional.hpp>

namespace mgn {
    namespace terrain {

    	MercatorService::MercatorService()
		: processed_node_(NULL)
		, finishing_(false)
		{
		}
		MercatorService::~MercatorService()
		{
			volatile bool finishing;
			mutex_.lock();
			finishing = finishing_;
			mutex_.unlock();
			if (!finishing)
				StopService();
		}
		void MercatorService::RunService()
		{
			finishing_ = false;
			thread_ = boost::thread(boost::bind(&MercatorService::ThreadFunc, this));
		}
		void MercatorService::StopService()
		{
			{//---
				boost::unique_lock<boost::mutex> guard(mutex_);
				finishing_ = true;
				condition_variable_.notify_one();
			}//---
			thread_.join();

			Task * task = 0;
            while (!tasks_.empty()) // there are some tasks
            {
                task = tasks_.front();
                tasks_.pop_front();
                delete task;
            }
            while (!done_tasks_.empty())
            {
                task = done_tasks_.front();
                done_tasks_.pop_front();
                delete task;
            }
		}
		void MercatorService::ClearTasks()
		{
			{//---
				boost::unique_lock<boost::mutex> guard(mutex_);
				Task * task = 0;
	            while (!tasks_.empty()) // there are some tasks
	            {
	                task = tasks_.front();
	                tasks_.pop_front();
	                delete task;
	            }
			}//---
		}
        void MercatorService::AddTask(Task * task)
        {
            {//---
				boost::unique_lock<boost::mutex> guard(mutex_);
				tasks_.push_back(task);
				condition_variable_.notify_one();
			}//---
        }
        bool MercatorService::GetDoneTasks(TaskList& task_list)
        {
        	boost::unique_lock<boost::mutex> guard(mutex_);
        	if (done_tasks_.empty())
        		return false;
        	else
        	{
        		done_tasks_.swap(task_list);
        		return true;
        	}
        }
        bool MercatorService::RemoveAllNodeTasks(MercatorNode * node)
        {
        	boost::unique_lock<boost::mutex> guard(mutex_);
        	if (processed_node_ == node)
        		return false;
        	TaskNodeMatchFunctor task_match_functor(node);
        	tasks_.remove_if(task_match_functor);
        	done_tasks_.remove_if(task_match_functor);
        	return true;
        }
		void MercatorService::ThreadFunc()
		{
			Task * task = NULL;
			bool finishing = false;
			for (;;)
			{
				{//---
					boost::lock_guard<boost::mutex> guard(mutex_);
					finishing = finishing_;
					if (task)
					{
						done_tasks_.push_back(task);
					}
                    if (!tasks_.empty())
                    {
                        task = tasks_.front();
                        tasks_.pop_front();
                        processed_node_ = task->node();
                    }
                    else
                    {
                    	task = NULL;
                    	processed_node_ = NULL;
                    }
				}//---

				if (finishing)
					break;

				if (!task)
				{
					boost::unique_lock<boost::mutex> guard(mutex_);
					while (!finishing_ && tasks_.empty())
						condition_variable_.wait(guard);
					continue;
				}

				task->Execute();
			}
		}

    } // namespace terrain
} // namespace mgn