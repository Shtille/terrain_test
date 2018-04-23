#pragma once
#ifndef __MGN_TERRAIN_SOLID_LINE_RENDERER_H__
#define __MGN_TERRAIN_SOLID_LINE_RENDERER_H__

#include "Frustum.h"

#include "MapDrawing/Graphics/Renderer.h"

#include "mgnMdWorldPoint.h"
#include "mgnMdWorldPosition.h"
#include "mgnMdIUserDataDrawContext.h"

#include <vector>
#include <map>
#include <list>

class mgnMdTerrainView;
class mgnMdTerrainProvider;
class mgnMdWorldPosition;

namespace mgn {
    namespace terrain {

        class TerrainTile;
        class SolidLineSegment;
        class SolidLineRenderer;

        //! Class that holding all render data
        class SolidLineSegmentRenderData
        {
        public:
            SolidLineSegmentRenderData(graphics::Renderer * renderer, SolidLineSegment * owner_segment);
            ~SolidLineSegmentRenderData();

            bool prepare(const std::vector<vec2>& vertices);
            void render();

        private:
            // non-copyable
            SolidLineSegmentRenderData(const SolidLineSegmentRenderData&);
            void operator =(const SolidLineSegmentRenderData&);

            graphics::Renderer * renderer_;
            SolidLineSegment * mOwnerSegment;
            graphics::VertexBuffer * vertex_buffer_;
            graphics::IndexBuffer * index_buffer_;
        };

        class SolidLineSegment
        {
        public:
            SolidLineSegment();
            SolidLineSegment(const vec2& b, const vec2& e, const TerrainTile * tile, float width, bool modified = false);
            SolidLineSegment(const mgnMdWorldPoint& cur, const mgnMdWorldPoint& next, const TerrainTile * tile, float width);
            ~SolidLineSegment();

            vec2 begin() const; //!< begin position in local coords
            vec2 end() const; //!< end position in local coords
            vec2 center() const;
            TerrainTile const * getTile() const;
            float width() const;
            bool needToAlloc() const; //!< returns true if data should be allocated
            bool beenModified() const;
            bool hasData() const; //!< returns true if any data has been allocated

            void prepare(graphics::Renderer * renderer, const std::vector<vec2>& vertices);
            void render(const math::Frustum& frustum); //!< render segment

        protected:
            float WidthCalculation(const TerrainTile * tile, float width);

        private:
            // non-copyable
            SolidLineSegment(const SolidLineSegment&);
            void operator =(const SolidLineSegment&);

            SolidLineSegmentRenderData * mRenderData;
            TerrainTile const * mTerrainTile;

            vec2 mBegin;    //!< begin point in tile CS
            vec2 mEnd;      //!< end point in tile CS
            float mWidth;
            bool mNeedToAlloc;
            bool mBeenModified; //!< first segment modification on highlight trimming
        };

        struct SolidLineFetchData
        {
            std::vector<vec2> result_vertices;
            vec2 terrain_begin;
            float tile_size_x;
            float tile_size_y;
            SolidLineSegment * prev_segment;
        };

        //! Class for working with a chunk of solid line
        class SolidLineChunk
        {
        public:
            struct Part {
                vec3 color;                            //!< color
                std::list<SolidLineSegment*> segments; //!< segments container
            };

            SolidLineChunk(SolidLineRenderer * owner, TerrainTile const * tile);
            virtual ~SolidLineChunk();

            virtual void clear();                                   //!< clears any loaded data
            virtual void update();                                
            virtual void render(const math::Frustum& frustum);  //!< renders all segments

            void fetchTriangles();                   //!< fetches all triangles
            void recreate();                         //!< request recreation
            bool isDataReady();                      //!< have we finished highlight loading? (need for sync.)

            bool mIsFetched;                         //!< for threads sync

        protected:
            // Do not allow to copy
            SolidLineChunk(const SolidLineChunk&);
            void operator = (const SolidLineChunk&);

            void createSegments();                   //!< create render data from segments
            void generateData();                     //!< generates render data from triangles

            TerrainTile const * mTile;               //!< owner tile
            std::vector<Part> mParts;                //!< parts container

            typedef std::map<SolidLineSegment*,SolidLineFetchData> FetchMap;
            FetchMap mFetchMap;

            bool mIsDataReady;                       //!< ready for sync.
            SolidLineRenderer * mOwner;              //!< renderer that is owning this part
        };

        /*! Class for rendering solid line
        NOTE: (!!!) this class doesn't own each part itself
        */
        class SolidLineRenderer : public mgnMdIUserDataDrawContext
        {
            friend class SolidLineChunk;
        public:

            SolidLineRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos);
            virtual ~SolidLineRenderer();

            void onViewChange();

            void onGpsPositionChange(double lat, double lon);
            virtual void update();
            virtual void render(const math::Frustum& frustum);

            mgnMdTerrainView * terrain_view();
            mgnMdTerrainProvider * provider();
            const mgnMdWorldPosition * gps_pos();

            // Derived from mgnMdIUserDataDrawContext
            mgnMdWorldRect GetWorldRect() const;
            int GetMapScaleIndex() const;
            float GetMapScaleFactor() const;
            mgnMdWorldPosition GetGPSPosition() const;
            bool ShouldDrawPOIs() const;
            bool ShouldDrawLabels() const;

        protected:
            // Don't allow to copy and use default constructor
            SolidLineRenderer();
            SolidLineRenderer(const SolidLineRenderer&);
            void operator =(const SolidLineRenderer&);

            virtual void doFetch() = 0; //!< fetch data from UGDS
            void recreateAll();

            graphics::Renderer * renderer_;
            mgnMdTerrainView * terrain_view_;
            mgnMdTerrainProvider * provider_;
            graphics::Shader * shader_;
            const mgnMdWorldPosition * gps_pos_;

            float offset_;
            mgnMdWorldPosition mGpsPosition; //!< needed for search context
            unsigned short mStoredMagIndex; //!< needed for recreation

            typedef std::list<SolidLineChunk*> ChunkList;
            ChunkList mChunks;                        //!< container for chunks

            graphics::VertexFormat * vertex_format_;

        private:
            void Attach(SolidLineChunk * chunk); //!< register part into owner's list
            void Detach(SolidLineChunk * chunk); //!< unregister part from owner's list

            bool mIsUsingQuads;                      //!< using just quads with depth disabled
        };

    } // namespace terrain
} // namespace mgn

#endif