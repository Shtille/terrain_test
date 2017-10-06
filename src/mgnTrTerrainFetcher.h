#pragma once  

#include "mgnTrTileKey.h"

#include "mgnCriticalSections.h"
#include "mgnThread.h"

#include <list>

namespace mgn {
    namespace terrain {

        class TerrainTile;

        class mgnTerrainFetcher
        {
        public:

            enum CommandType
            {
                FETCH_TERRAIN,
                FETCH_TEXTURE,
                FETCH_USER_DATA,
                UPDATE_TRACKS,
                REFETCH_TEXTURE, // has lowest priority
                REFETCH_USER_DATA, // has lowest priority

                UNSPECIFIED, // used for comparing only
            };

            struct CommandData
            {
                CommandData(TerrainTile *tile_=0, CommandType cmd_=UNSPECIFIED)
                    : tile(tile_), cmd(cmd_) { MakeWeight(); }
                TerrainTile     *tile;
                CommandType      cmd;
                int weight;

                int getWeight() const; // for sorting

                bool operator<(const CommandData &o) const;

                void MakeWeight();
            };
            typedef std::list<CommandData> CommandList;

            mgnTerrainFetcher();
            ~mgnTerrainFetcher();

            void addCommand(CommandData cmd);
            void addCommandLow(CommandData cmd); //!< low priority, like refetch

            // Update incoming queue sorting for quick fetching the most important tiles
            void resort();

            // Remove specified command or any command with specified tile from fetching queue
            // If the method returned false -- this tile cannot be unqueued now (do it later).
            bool removeCommand(const CommandData &cmd);
            bool removeTile(TerrainTile *tile) { return removeCommand(CommandData(tile, UNSPECIFIED)); }

            // Remove all tiles from fetcher
            void clear(TileMap *protectedTiles);

            // Give back fetched tiles
            // Note: The fetcher forget about tiles which have been returned
            void getResults(CommandList & commands);

            // Invocation support. ICallable -- interface for external code
            class ICallable
            {
            public:
                virtual void call() = 0;
            };
            // Invoke external code in fetching thread
            // This method waits until external code has been finished
            void invoke(ICallable *o);

        private:
        #if defined(WIN32) || defined(_WIN32_WCE)
            static unsigned long __stdcall routineStarter(void * fetcher) { ((mgnTerrainFetcher*)(fetcher))->threadRoutine(); return 0; }
        #else
            static void*                   routineStarter(void * fetcher) { ((mgnTerrainFetcher*)(fetcher))->threadRoutine(); return 0; }
        #endif

            void threadRoutine();

            mgnHandle       mThread;

            // Currently all shared objects will be protected by single mutex
            // TODO: optimisation may be required
            mgnCSHandle mCriticalSection;

            CommandList mCommandsList;
            CommandList mDoneList;
            CommandData mCmdInProcess;

            ICallable * mCallMe;

            bool mFinishing;

            // non-copyable
            mgnTerrainFetcher(const mgnTerrainFetcher&); // = delete
            void operator=(const mgnTerrainFetcher&); // = delete
        };

    } // namespace terrain
} // namespace mgn