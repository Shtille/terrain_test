#include "mgnTrTerrainFetcher.h"

#include "mgnTrTerrainTile.h"

namespace mgn {
    namespace terrain {

        class mgnMutexGuard
        {
        public:
            mgnMutexGuard(mgnCSHandle* m) : mMutexHandle(m), mIsGuarded(false)
            {
                //mgnU32_t time_out = (mgnU32_t)(-1);
                if(MGN_STATUS_SUCCESS == mgnCriticalSectionEnter(mMutexHandle))
                    mIsGuarded = true;
            }

            ~mgnMutexGuard()
            {
                if (mIsGuarded)
                    mgnCriticalSectionRelease(mMutexHandle);
            }

        private:
            mgnCSHandle* mMutexHandle;
            bool mIsGuarded;
        };

        static bool operator==(const mgnTerrainFetcher::CommandData &a, const mgnTerrainFetcher::CommandData &b)
        {
            mgnTerrainFetcher::CommandType a_cmd = a.cmd;
            mgnTerrainFetcher::CommandType b_cmd = b.cmd;
            if (a_cmd == mgnTerrainFetcher::UNSPECIFIED) b_cmd = mgnTerrainFetcher::UNSPECIFIED;
            if (b_cmd == mgnTerrainFetcher::UNSPECIFIED) a_cmd = mgnTerrainFetcher::UNSPECIFIED;

            return (a.tile == b.tile) && (a_cmd == b_cmd);
        }

        int mgnTerrainFetcher::CommandData::getWeight() const
        {
            return weight;
        }
        bool mgnTerrainFetcher::CommandData::operator<(const CommandData &o) const
        {
            return getWeight() < o.getWeight();
        }
        void mgnTerrainFetcher::CommandData::MakeWeight()
        {
            if (!tile)
            {
                weight = 0;
                return;
            }

            int w = 0;
            //if      (cmd == FETCH_TERRAIN)   w = 1 << 5;
            //else if (cmd == FETCH_TEXTURE)   w = 1 << 5;

            //int magind = tile->mKey.magIndex + (tile->priority >> 5);
            //w += magind;
            //w <<= 8;
            //w += tile->priority;
            if      (cmd == FETCH_TERRAIN)   w = 1 << 5;
            else if (cmd == FETCH_TEXTURE)   w = 1 << 5;
            if (cmd != REFETCH_TEXTURE && cmd != REFETCH_USER_DATA)
            {
                if (cmd != UPDATE_TRACKS)
                w += tile->mKey.magIndex;
                w <<= 8;
            }
            if (cmd == UPDATE_TRACKS)
            {
                // The more distant parts of highlight should be loaded after nearest ones
                float x,y;
                mgnMdWorldPosition pos = tile->GetGPSPosition();
                tile->worldToTile(pos.mLatitude, pos.mLongitude, x, y);
                int dx = static_cast<int>(x / (tile->sizeMetersLon * 0.5f));
                int dy = static_cast<int>(y / (tile->sizeMetersLat * 0.5f));
                int dist_sqr = std::min<int>(dx*dx + dy*dy, 255);
                w += 255 - dist_sqr;
            }
            w += tile->priority;

            weight = w;
        }

        mgnTerrainFetcher::mgnTerrainFetcher()
          : mFinishing(false), mCallMe(0)
        {
            mgnCriticalSectionInitialize(&mCriticalSection);

            FUNCPTR starter = mgnTerrainFetcher::routineStarter;
            mThread = mgnThreadCreate(1024*1024, starter, this, mgnTHREAD_PRIORITY_NORMAL, true);
        }

        mgnTerrainFetcher::~mgnTerrainFetcher()
        {
            // Request thread to stop
            {
                mgnMutexGuard guard(&mCriticalSection);
                mFinishing = true;
            }

            mgnWaitForThreadExit(mThread, 10*1000); // wait until thread stopped
            mgnThreadClose(mThread);
            mgnCriticalSectionDelete(&mCriticalSection);

            mCommandsList.clear();
            mDoneList.clear();
        }

        void mgnTerrainFetcher::addCommand(CommandData cmd)
        {
            mgnMutexGuard guard(&mCriticalSection);
            if ((!mCommandsList.empty()) && (mCommandsList.back().getWeight() > cmd.getWeight()))
            {
                // smart insert
                CommandData old = mCommandsList.back();
                mCommandsList.back() = cmd;
                mCommandsList.push_back(old);
            }
            else
            {
                mCommandsList.push_back(cmd);
            }
            //mCommandsList.push_back(cmd);
            //mCommandsList.sort();
        }

        void mgnTerrainFetcher::addCommandLow(CommandData cmd)
        {
            mgnMutexGuard guard(&mCriticalSection);
            mCommandsList.push_front(cmd); // low priority are placed in front
        }

        void mgnTerrainFetcher::resort()
        {
            mgnMutexGuard guard(&mCriticalSection);
            //std::stable_sort(mCommandsList.begin(), mCommandsList.end());
            mCommandsList.sort();
        }

        bool mgnTerrainFetcher::removeCommand(const CommandData &cmd)
        {
            mgnMutexGuard guard(&mCriticalSection);
            if (mCmdInProcess.tile && cmd == mCmdInProcess)
                return false;

            CommandList *lst;

            // Remove element from incoming and outcoming arrays
            lst = &mCommandsList;   lst->erase(std::remove(lst->begin(), lst->end(), cmd), lst->end());
            lst = &mDoneList;       lst->erase(std::remove(lst->begin(), lst->end(), cmd), lst->end());

            return true;
        }

        void mgnTerrainFetcher::clear(TileMap *protectedTiles)
        {
            // firstly clear incoming queue
            {
                mgnMutexGuard guard(&mCriticalSection);
                if (!protectedTiles)
                    mCommandsList.clear();
                else if (!mCommandsList.empty())
                {
                    //for (int i=mCommandsList.size()-1; i>=0; --i)
                    //{
                    //    if (protectedTiles->find(mCommandsList[i].tile->mKey) == protectedTiles->end())
                    //        mCommandsList.erase(mCommandsList.begin()+i);
                    //}
                    CommandList::iterator it = mCommandsList.begin();
                    while (it != mCommandsList.end())
                    {
                        if (protectedTiles->find(it->tile->mKey) == protectedTiles->end())
                            it = mCommandsList.erase(it);
                        else
                            ++it;
                    }
                }
            }

            // now wait until current fetching procedure finished
            // When this will be happened the pointer mCmdInProcess.tile will be null_ptr
            while (true)
            {
                bool wait;
                {
                    mgnMutexGuard guard(&mCriticalSection);
                    wait = mCmdInProcess.tile ? true : false;
                }
                if (!wait) break;

                // if we are here -- fetching procedure is still performing... wait a little
                mgnSleep(1);
            }

            // and finally clear outcoming storage
            {
                mgnMutexGuard guard(&mCriticalSection);
                if (!protectedTiles)
                    mDoneList.clear();
                else if (!mDoneList.empty())
                {
                    //for (int i=mDoneList.size()-1; i>=0; --i)
                    //{
                    //    if (protectedTiles->find(mDoneList[i].tile->mKey) == protectedTiles->end())
                    //        mDoneList.erase(mDoneList.begin()+i);
                    //}
                    CommandList::iterator it = mDoneList.begin();
                    while (it != mDoneList.end())
                    {
                        if (protectedTiles->find(it->tile->mKey) == protectedTiles->end())
                            it = mDoneList.erase(it);
                        else
                            ++it;
                    }
                }
            }
        }

        void mgnTerrainFetcher::getResults(mgnTerrainFetcher::CommandList & commands)
        {
            mgnMutexGuard guard(&mCriticalSection);
            commands.clear();
            std::swap(mDoneList, commands);
        }

        void mgnTerrainFetcher::invoke(ICallable *o)
        {
            // init invocation
            {
                mgnMutexGuard guard(&mCriticalSection);
                assert(!mCallMe);
                mCallMe = o;
            }
            bool done = false;

            while (!done)
            {
                // Check if tje task is finished
                {
                    mgnMutexGuard guard(&mCriticalSection);
                    if (!mCallMe) done = true;
                }
                mgnSleep(1);
            }
        }

        void mgnTerrainFetcher::threadRoutine()
        {
            bool finishing = false;

            while (!finishing)
            {
                TerrainTile *tile = 0;
                mgnTerrainFetcher::CommandType cmd;

                // Get tile from queue
                {
                    mgnMutexGuard guard(&mCriticalSection);

                    // check if we should execute some external code
                    if (mCallMe)
                    {
                        mCallMe->call();
                        mCallMe = 0;
                    }

                    // ... now get tile for fetching
                    finishing = mFinishing;
                    if (!mCommandsList.empty())
                    {
                        mCmdInProcess = mCommandsList.back();
                        mCommandsList.pop_back();
                        tile = mCmdInProcess.tile;
                        cmd = mCmdInProcess.cmd;
                    }
                }

                // queue is empty -- wait a little
                if (!tile)
                {
                    mgnSleep(1);
                    continue;
                }

                switch (cmd)
                {
                case mgnTerrainFetcher::FETCH_TERRAIN:
                    assert(tile);
                    tile->fetchTerrain();
                    break;
                case mgnTerrainFetcher::FETCH_TEXTURE:
                    assert(tile);
                    tile->fetchTexture();
                    break;
                case mgnTerrainFetcher::FETCH_USER_DATA:
                    assert(tile);
                    tile->fetchUserObjects();
                    break;
                case mgnTerrainFetcher::UPDATE_TRACKS:
                    assert(tile);
                    tile->fetchTracks();
                    break;
                case mgnTerrainFetcher::REFETCH_TEXTURE:
                    assert(tile);
                    tile->fetchTexture();
                    break;
                case mgnTerrainFetcher::REFETCH_USER_DATA:
                    assert(tile);
                    tile->fetchUserObjects();
                    break;
                }

                // put fetched tile to resulting queue
                {
                    mgnMutexGuard guard(&mCriticalSection);

                    //mDoneList.push_back(mCmdInProcess);
                    //mDoneList.sort();
                    //if ((!mDoneList.empty()) && (mDoneList.back().getWeight() > mCmdInProcess.getWeight()))
                    //{
                    //    // smart insert
                    //    CommandData old = mDoneList.back();
                    //    mDoneList.back() = mCmdInProcess;
                    //    mDoneList.push_back(old);
                    //}
                    //else
                    {
                        mDoneList.push_front(mCmdInProcess);
                        //mDoneList.sort();
                    }

                    mCmdInProcess = CommandData();
                }
                //resort();
            }
        }

    } // namespace terrain
} // namespace mgn