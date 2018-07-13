#pragma once
#ifndef __MGN_TERRAIN_DOTTED_LINE_RENDERER_H__
#define __MGN_TERRAIN_DOTTED_LINE_RENDERER_H__

#include "Frustum.h"

#include "MapDrawing/Graphics/Renderer.h"

#include "mgnMdWorldPoint.h"
#include "mgnMdWorldPosition.h"
#include "mgnMdIUserDataDrawContext.h"

#include <vector>
#include <list>

class mgnMdTerrainView;
class mgnMdWorldPosition;

namespace mgn {
    namespace terrain {

        class MercatorProvider;

        struct DottedLinePointInfo
        {
            // Rotation data for using in local CS
            struct Rotation {
                /** 
                * Here we must decide: what solution do we prefer?
                * Maximum memory usage and maximum performance (matrix)
                * Minimum memory usage and minimum performance (euler angles)
                * I'd decided to use first case.
                */
                math::Matrix4 matrix; //!< 4x4 column-major matrix
            } rotation;

            // Data need to be rendered:
            struct Local {
                vec3 position;          //!< position relatively to segment begin point (in meters)
            } local;

            // Data need to be converted to local CS
            struct World {
                mgnMdWorldPoint point;  //!< position in world CS
                // altitude stores in local.position.y
            } world;

            void set_begin(mgnMdTerrainView * terrain_view, MercatorProvider * provider, float& minh, float& maxh);
            void obtain_position(mgnMdTerrainView * terrain_view, MercatorProvider * provider, float offset_x, float offset_y, float& minh, float& maxh);
            void fill_rotation(const vec3& vx, const vec3& vy, const vec3& vz);
        };

        class DottedLineSegment
        {
        public:
            DottedLineSegment(const mgnMdWorldPoint& begin, const mgnMdWorldPoint& end)
                : mOffset(0.0), mSkipPoint(0)
                , mMinHeight(0.0f), mMaxHeight(0.0f), mPoints(0), mNumPoints(0)
                , mCalculated(false), mNeedToCalculate(false)
            {
                mBegin.world.point = begin;
                mEnd.world.point = end;
            }
            virtual ~DottedLineSegment() 
            {
                if (mPoints)
                    delete[] mPoints;
            }

            DottedLinePointInfo mBegin;
            DottedLinePointInfo mEnd;
            double mOffset;
            int mSkipPoint;
            float mMinHeight;
            float mMaxHeight;
            DottedLinePointInfo * mPoints;  //!< points array
            int mNumPoints;                 //!< number of points
            bool mCalculated;
            bool mNeedToCalculate;
        };

        //! Class for rendering dotted line
        class DottedLineRenderer : public mgnMdIUserDataDrawContext
        {
        public:

            DottedLineRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, MercatorProvider * provider,
                graphics::Shader * shader, const mgnMdWorldPosition * gps_pos);
            virtual ~DottedLineRenderer();

            virtual void onGpsPositionChange();                         //!< should be called when gps pos changes
            virtual void onViewChange();                        //!< should be called when camera changes
            virtual void update(const math::Frustum& frustum);
            virtual void render(const math::Frustum& frustum);

            // Derived from mgnMdIUserDataDrawContext
            mgnMdWorldRect GetWorldRect() const;
            int GetMapScaleIndex() const;
            float GetMapScaleFactor() const;
            mgnMdWorldPosition GetGPSPosition() const;
            bool ShouldDrawPOIs() const;
            bool ShouldDrawLabels() const;

        protected:

            DottedLineRenderer();
            DottedLineRenderer(const DottedLineRenderer&);
            void operator=(const DottedLineRenderer&);

            void onAddSegment(DottedLineSegment * segment);
            void onAddSegment(DottedLineSegment * segment, DottedLineSegment * prev_segment);
            void fillSegment(DottedLineSegment * prev_segment, DottedLineSegment * segment); //!< fill segment with points

            // Some class tuning options
            virtual const bool isUsingLongSegmentFix() { return false; }
            virtual const bool isUsingSeparateSegments() { return true; }
            virtual const bool isUsingPointsCache() { return true; }
            virtual const bool isUsingFastNormalComputation() { return false; }
            virtual const int maxRenderedPointsPerSegment() { return 1000; } //!< works only if without cache
            virtual const float elementSize() { return 0.5f; }
            virtual const float elementStep() { return 2.0f; }

            virtual void createTexture() = 0;
            virtual void doFetch() = 0; //!< fetch data from UGDS

            void cropSegment(DottedLineSegment *& segment);
            void subdivideSegment(const mgnMdWorldPoint& p1, const mgnMdWorldPoint& p2, std::vector<mgnMdWorldPoint>* points);
            void calcNormal(DottedLinePointInfo& point, float circle_size);
            void calcNormal(DottedLinePointInfo& point, const vec3& to_target); //!< to_target should be in RHS
            void calcFlatNormal(DottedLinePointInfo& point, const vec3& to_target); //!< to_target should be in RHS

            graphics::Renderer * renderer_;
            mgnMdTerrainView * terrain_view_;
            MercatorProvider * provider_;
            graphics::Shader * shader_;
            const mgnMdWorldPosition * gps_pos_;

            mgnMdWorldPosition mGpsPosition; //!< needed for search context
            mgnMdWorldPosition mOldPosition; //!< position at last update

            bool mHighAltitudes; //!< are we too high above terrain?
            bool mAnySegmentToFill; //!< is there anything to fill

            std::list<DottedLineSegment*> mSegments;

            graphics::Texture * texture_;
            graphics::VertexFormat * vertex_format_;
            graphics::VertexBuffer * vertex_buffer_;
            graphics::IndexBuffer * index_buffer_;

        private:

            void create();                                    //!< generate quad geometry
        };

    } // namespace terrain
} // namespace mgn

#endif