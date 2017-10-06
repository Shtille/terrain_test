#include "mgnTrActiveTrackRenderer.h"

#include "mgnMdTerrainView.h"
#include "mgnMdTerrainProvider.h"

#include "mgnUgdsTrackUserData.h" // for tracks and active track
#include "UGDSInterface.hpp"
#include "UGDSTypesAndConsts.hpp"
#include "mgnUgdsManager.h"

#include <assert.h>
#include <cmath>

namespace mgn {
    namespace terrain {

        ActiveTrackRenderer::ActiveTrackRenderer(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * provider,
                        graphics::Shader * shader, const mgnMdWorldPosition * gps_pos)
        : DottedLineRenderer(renderer, terrain_view, provider, shader, gps_pos)
        , mNumPoints(0)
        {
        }
        ActiveTrackRenderer::~ActiveTrackRenderer()
        {
        }
        void ActiveTrackRenderer::createTexture()
        {
            int w = 32;
            int h = 32;

            int *pixels = new int[w*h];

            for(int y = 0; y < h; ++y)
            {
                for(int x = 0; x < w; ++x)
                {
                    float r = (float)w/2;
                    float dx = (float)(x-w/2);
                    float dy = (float)(y-h/2);
                    float len = sqrt(dx*dx + dy*dy);
                    if (len <= r)
                        pixels[y*w+x] = 0xffff0000;
                    else
                        pixels[y*w+x] = 0;
                }
            }

            unsigned char* data = reinterpret_cast<unsigned char*>(pixels);
            renderer_->CreateTextureFromData(texture_, w, h,
                graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, data);

            delete[] pixels;
        }
        void ActiveTrackRenderer::clearSegments()
        {
            for (std::list<DottedLineSegment*>::iterator it = mSegments.begin(); it != mSegments.end(); ++it)
            {
                DottedLineSegment * segment = *it;
                if (segment)
                    delete segment;
            }
            mSegments.clear();
            mNumPoints = 0;
        }
        void ActiveTrackRenderer::doFetch()
        {
            mgnUgdsManager * manager = mgnUgdsManager::GetInstance();
            CUGDSInterface * ugds = manager->GetUGDS();
            int num_points = ugds->TRK_PointsExport(UGDS_ActiveTrackID);
            if (num_points > mNumPoints)
            {
                UGDS_TrackPoint_t *points = new UGDS_TrackPoint_t[num_points];
                if (ugds->TRK_PointsExport(UGDS_ActiveTrackID, 
                                          0,                            // start index
                                          num_points-1,                 // end index
                                          num_points,                   // buffer size
                                          points) > 0)
                {
                    vec2 cur, next;
                    if (mNumPoints > 0 && !(dynamic_cast<ActiveTrackSegment*>(mSegments.back()))->mHasBreak)
                    {
                        // Correct last segment
                        delete mSegments.back();
                        mSegments.pop_back();

                        mgnMdWorldPoint curp, nextp;
                        curp.mLatitude = points[mNumPoints-1].GeoPt.GetDoubleLat();
                        curp.mLongitude = points[mNumPoints-1].GeoPt.GetDoubleLon();
                        nextp.mLatitude = points[mNumPoints].GeoPt.GetDoubleLat();
                        nextp.mLongitude = points[mNumPoints].GeoPt.GetDoubleLon();
                        bool has_break = (points[mNumPoints].AltNFlags.GetFlags() & UGDS_TKPTS_BREAK) == UGDS_TKPTS_BREAK;
                        DottedLineSegment * prev_segment = (mSegments.empty()) ? NULL : mSegments.back();
                        mSegments.push_back(new ActiveTrackSegment(curp, nextp, has_break));
                        onAddSegment(mSegments.back(), prev_segment);
                    }
                    for (int i = mNumPoints; i < num_points-1; ++i)
                    {
                        mgnMdWorldPoint curp, nextp;
                        curp.mLatitude = points[i].GeoPt.GetDoubleLat();
                        curp.mLongitude = points[i].GeoPt.GetDoubleLon();
                        nextp.mLatitude = points[i+1].GeoPt.GetDoubleLat();
                        nextp.mLongitude = points[i+1].GeoPt.GetDoubleLon();
                        bool has_break = (points[i+1].AltNFlags.GetFlags() & UGDS_TKPTS_BREAK) == UGDS_TKPTS_BREAK;
                        DottedLineSegment * prev_segment = (mSegments.empty()) ? NULL : mSegments.back();
                        mSegments.push_back(new ActiveTrackSegment(curp, nextp, has_break));
                        onAddSegment(mSegments.back(), prev_segment);
                    }
                    // to the current location - we will use it only in simulation mode
                    if ((points[num_points-1].AltNFlags.GetFlags() & UGDS_TKPTS_BREAK) != UGDS_TKPTS_BREAK)
                    {
                        mgnMdWorldPoint curp, nextp;
                        curp.mLatitude = points[num_points-1].GeoPt.GetDoubleLat();
                        curp.mLongitude = points[num_points-1].GeoPt.GetDoubleLon();
                        nextp.mLatitude = mGpsPosition.mLatitude;
                        nextp.mLongitude = mGpsPosition.mLongitude;
                        DottedLineSegment * prev_segment = (mSegments.empty()) ? NULL : mSegments.back();
                        mSegments.push_back(new ActiveTrackSegment(curp, nextp));
                        onAddSegment(mSegments.back(), prev_segment);
                    }
                    mNumPoints = num_points;
                }
                delete[] points;
            }
            else if (num_points < mNumPoints)
            {
                clearSegments();
            }

            // We should also update the last segment to current vehicle position on change
            if (!mSegments.empty() && !(dynamic_cast<ActiveTrackSegment*>(mSegments.back()))->mHasBreak) 
            {
                const bool position_has_changed = fabs(mGpsPosition.mLatitude  - mOldPosition.mLatitude ) > 1e-9 ||
                                                  fabs(mGpsPosition.mLongitude - mOldPosition.mLongitude) > 1e-9;
                if (position_has_changed)
                {
                    mgnMdWorldPoint curp, nextp;
                    curp.mLatitude = mSegments.back()->mBegin.world.point.mLatitude;
                    curp.mLongitude = mSegments.back()->mBegin.world.point.mLongitude;

                    delete mSegments.back();
                    mSegments.pop_back();
                    
                    nextp.mLatitude = mGpsPosition.mLatitude;
                    nextp.mLongitude = mGpsPosition.mLongitude;
                    DottedLineSegment * prev_segment = (mSegments.empty()) ? NULL : mSegments.back();
                    mSegments.push_back(new ActiveTrackSegment(curp, nextp));
                    onAddSegment(mSegments.back(), prev_segment);
                }
            }
        }

    } // namespace terrain
} // namespace mgn