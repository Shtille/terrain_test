#include "mgnTrTerrainTile.h"
#include "mgnTrTerrainMap.h"
#include "mgnTrConstants.h"
#include "mgnTrIcon.h"
#include "mgnTrLabel.h"
#include "mgnTrAtlasLabel.h"
#include "mgnTrHeightmap.h"
#include "mgnTrHighlightTrackRenderer.h"

#include "mgnMdBitmap.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

template <typename T>
static T roundToPowerOfTwo(T x)
{
    T n = 1;
    while (n < x)
        n <<= 1;
    return n;
}

static int interpolateColor(int c1, int c2, float alpha)
{
    int a1 = (c1 & 0xff000000) >> 24;
    int b1 = (c1 & 0xff0000) >> 16;
    int g1 = (c1 & 0x00ff00) >> 8;
    int r1 = (c1 & 0x0000ff);
    int a2 = (c2 & 0xff000000) >> 24;
    int b2 = (c2 & 0xff0000) >> 16;
    int g2 = (c2 & 0x00ff00) >> 8;
    int r2 = (c2 & 0x0000ff);
    int r = r1 + (int)((r2-r1)*alpha);
    int g = g1 + (int)((g2-g1)*alpha);
    int b = b1 + (int)((b2-b1)*alpha);
    int a = a1 + (int)((a2-a1)*alpha);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

static void MakePotTextureFromNpot(int& width, int& height, std::vector<unsigned char>& vdata)
{
    if (((width & (width-1)) != 0) || ((height & (height-1)) != 0))
    {
        // Need to make POT
        unsigned int * data = reinterpret_cast<unsigned int*>(&vdata[0]);
        int w2 = roundToPowerOfTwo(width);
        int h2 = roundToPowerOfTwo(height);

        // Rescale image to power of 2
        int new_size = w2 * h2;
        std::vector<unsigned char> new_vdata;
        new_vdata.resize(new_size*4);
        unsigned int * new_data = reinterpret_cast<unsigned int*>(&new_vdata[0]);
        for (int dh2 = 0; dh2 < h2; ++dh2)
        {
            float rh = (float)dh2 / (float)h2;
            float y = rh * (float)height;
            int dh = (int)y;
            int dh1 = std::min<int>(dh+1, height-1);
            float ry = y - (float)dh; // fract part of y
            for (int dw2 = 0; dw2 < w2; ++dw2)
            {
                float rw = (float)dw2 / (float)w2;
                float x = rw * (float)width;
                int dw = (int)x;
                int dw1 = std::min<int>(dw+1, width-1);
                float rx = x - (float)dw; // fract part of x

                // We will use bilinear interpolation
                int sample1 = (int) data[dw +width*dh ];
                int sample2 = (int) data[dw1+width*dh ];
                int sample3 = (int) data[dw +width*dh1];
                int sample4 = (int) data[dw1+width*dh1];

                int color1 = interpolateColor(sample1, sample2, rx);
                int color2 = interpolateColor(sample3, sample4, rx);;
                int color3 = interpolateColor(color1, color2, ry);
                new_data[dw2+w2*dh2] = (unsigned int)color3;
            }
        }
        // Finally
        width = w2;
        height = h2;
        vdata.swap(new_vdata);
    }
}

namespace mgn {
    namespace terrain {

        TerrainTile::TerrainTile(const TerrainMap *owner, const mgnTileKey &key, GeoSquare geoSquare, float gx, float gy)
        : mOwner(owner)
        , mKey(key)
        , mGeoSquare(geoSquare)
        , mHeightSamples(NULL)
        , mFetchedHeightSamples(NULL)
        , mTerrainMesh(NULL)
        , mFetchedTerrainMesh(NULL)
        , mDrawFrame(-1)
        , mTexture(NULL)
        , mPosition(gx, 0.0f, gy)
        , mIsFetched(false)
        , mIsFetchedTerrain(false)
        , mIsFetchedTexture(false)
        , mIsFetchedLabels(false)
        , mNeedToGenerateLabels(true)
        , mRefetchTerrain(false)
        , mRefetchTexture(false)
        , mUpdateTexture(false)
        , mUpdateTextureRequested(false)
        , mUpdateUserData(false)
        , mUpdateUserDataRequested(false)
        , mHighlightMessage(0)
        {
            mgnCriticalSectionInitialize(&mCriticalSection);
            mHighlightTrackChunk = new HighlightTrackChunk(owner->pHighlightTrackRenderer, this);
            calcBoundingBox();
        }

        TerrainTile::~TerrainTile()
        {
            delete mHighlightTrackChunk;

            if (mHeightSamples)
            {
                delete[] mHeightSamples;
                mHeightSamples = NULL;
            }

            if (mTerrainMesh)
            {
                delete mTerrainMesh;
                mTerrainMesh = NULL;
            }

            for (size_t i=0; i<mLabelMeshes.size(); ++i)
                delete mLabelMeshes[i];
            mLabelMeshes.clear();
            for (size_t i=0; i<mAtlasLabelMeshes.size(); ++i)
                delete mAtlasLabelMeshes[i];
            mAtlasLabelMeshes.clear();

            freeUserObjects();

            if (mTexture)
            {
                mOwner->renderer_->DeleteTexture(mTexture);
                mTexture = NULL;
            }
            mgnCriticalSectionDelete(&mCriticalSection);
        }
        void TerrainTile::updateTracks() // this function is called every frame
        {
            mHighlightTrackChunk->update();
        }
        void TerrainTile::fetchTracks() // called in other thread
        {
            mHighlightTrackChunk->fetchTriangles();
        }

        int   TerrainTile::GetMapScaleIndex () const
        {
            return mOwner->getTerrainView()->getMapScaleIndex (mKey.magIndex);
        }
        float TerrainTile::GetMapScaleFactor() const
        {
            return mOwner->getTerrainView()->getMapScaleFactor(mKey.magIndex);
        }
        const vec3& TerrainTile::position() const
        {
            return mPosition;
        }
        void TerrainTile::Lock() const
        {
            mgnCriticalSectionEnter(const_cast<mgnCSHandle*>(&mCriticalSection));
        }
        void TerrainTile::Unlock() const
        {
            mgnCriticalSectionRelease(const_cast<mgnCSHandle*>(&mCriticalSection));
        }
        void TerrainTile::calcBoundingBox()
        {
            mBoundingBox.center.x = mPosition.x;
            mBoundingBox.center.z = mPosition.z;
            mBoundingBox.extent.x = sizeMetersLon * 0.5f;
            mBoundingBox.extent.z = sizeMetersLat * 0.5f;
            // Heights is unknown until it get fetched
        }

        // Derived from mgnMdIUserDataDrawContext :
        mgnMdWorldRect TerrainTile::GetWorldRect() const
        {
            return mgnMdWorldRect(mGeoSquare.minLat(), mGeoSquare.minLon(), 
                                  mGeoSquare.maxLat(), mGeoSquare.maxLon());
        }

        float TerrainTile::pureHeight(int ix, int iy) const
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            return mHeightSamples[iy*kTileHeightSamples + ix];
        }
        void TerrainTile::fetchTerrain()
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            sizeMetersLat = (float)mOwner->getTerrainView()->getCellSizeLat(mKey.magIndex);
            sizeMetersLon = (float)mOwner->getTerrainView()->getCellSizeLon(mKey.magIndex);

            float * height_samples = new float[kTileHeightSamples * kTileHeightSamples];
            GeoTerrain geoTerrain(kTileHeightSamples, kTileHeightSamples, height_samples, 1);
            mOwner->mTerrainProvider.fetchTerrain(mGeoSquare, geoTerrain);

            Heightmap * terrain_mesh = new Heightmap(mOwner->renderer_,
                kTileHeightSamples, kTileHeightSamples, height_samples,
                sizeMetersLon, sizeMetersLat);

            Lock();
            mRefetchTerrain = geoTerrain.mErrorOccurred;
            mFetchedTerrainMesh = terrain_mesh;
            mFetchedHeightSamples = height_samples;
            Unlock();
        }

        void TerrainTile::fetchTexture()
        {
            GeoTextureMap geoTextureMap;

            Lock();
            geoTextureMap.mNeedLabels = !mIsFetchedLabels;
            Unlock();

            mOwner->mTerrainProvider.fetchTexture(mGeoSquare, geoTextureMap);

            Lock();
            mRefetchTexture = geoTextureMap.mErrorOccurred;
            if (geoTextureMap.mResult == GeoState::COMPLETE)
            {
                std::swap(mTextureData, geoTextureMap.mTextureData);
                // Labels will be fetched only in case of no cosmos errors
                if (geoTextureMap.mNeedLabels && !geoTextureMap.mCosmosErrors)
                {
                    std::swap(mLabelsInfo, geoTextureMap.mLabelsInfo);
                    mIsFetchedLabels = true;
                }
            }
            Unlock();
        }

        void TerrainTile::fetchUserObjects()
        {
            PointUserObjectsInfo puo;
            mOwner->mTerrainProvider.fetchPointUserData(*this, puo);
            if (puo.mResult == GeoState::COMPLETE)
            {
                Lock();
                std::swap(mPointUserObjects, puo.objects);
                Unlock();
            }
        }

        bool TerrainTile::isRefetchTerrain() const
        {
            volatile bool refetch;
            Lock();
            refetch = mRefetchTerrain;
            Unlock();
            return refetch;
        }
        bool TerrainTile::isRefetchTexture() const
        {
            volatile bool refetch;
            Lock();
            refetch = mRefetchTexture;
            Unlock();
            return refetch;
        }
        bool TerrainTile::isFetchedTerrain() const { return mIsFetchedTerrain; }
        bool TerrainTile::isFetchedTexture() const { return mIsFetchedTexture; }
        bool TerrainTile::isFetched       () const { return mIsFetched; }

        bool TerrainTile::isFetchedLabels() const
        {
            volatile bool fetched;
            Lock();
            fetched = mIsFetchedLabels;
            Unlock();
            return fetched;
        }

        void TerrainTile::generateMesh()
        {
            if (mTerrainMesh)
                delete mTerrainMesh;
            Lock();
            if (mFetchedTerrainMesh)
            {
                mTerrainMesh = mFetchedTerrainMesh;
                mFetchedTerrainMesh = NULL;
            }
            if (mFetchedHeightSamples)
            {
                mHeightSamples = mFetchedHeightSamples;
                mFetchedHeightSamples = NULL;
            }
            Unlock();
            assert(mTerrainMesh != NULL);
            // Adjust bounding box
            mBoundingBox.center.y = 0.5f * (mTerrainMesh->maxHeight() + mTerrainMesh->minHeight());
            mBoundingBox.extent.y = 0.5f * (mTerrainMesh->maxHeight() - mTerrainMesh->minHeight());

            mTerrainMesh->MakeRenderable();
        }

        void TerrainTile::generateTextures()
        {
            // Swap fetched data with temporary one (we don't need it further)
            std::vector<unsigned char> texture_data;

            Lock();
            texture_data.swap(mTextureData);
            Unlock();

            // Texture data has been obtained
            if (!texture_data.empty())
            {
                graphics::Renderer * renderer = mOwner->renderer_;
                const int kTileResolution = GetTileResolution();
                if (mTexture)
                {
                    // Texture already generated
                    mTexture->SetData(0, 0, kTileResolution, kTileResolution, &texture_data[0]);
                }
                else
                {
                    // A new texture
                    renderer->CreateTextureFromData(mTexture, kTileResolution, kTileResolution,
                        graphics::Image::Format::kRGB565, graphics::Texture::Filter::kTrilinear, &texture_data[0]);
                }
            }
        }

        void TerrainTile::generateLabels()
        {
            mNeedToGenerateLabels = false;

            std::vector<LabelPositionInfo> labels_info;

            Lock();
            labels_info.swap(mLabelsInfo);
            Unlock();

            // Build label meshes
            for (std::vector<LabelPositionInfo>::iterator it = labels_info.begin();
                it != labels_info.end(); ++it)
            {
                // Recognize type of billboard (label or shield)
                LabelPositionInfo& info = *it;
                if (info.centered) // shield
                {
                    graphics::Renderer * renderer = mOwner->renderer_;
                    graphics::Texture * texture;
                    TerrainMap::ShieldTextureCache::iterator it_shield =
                        mOwner->mShieldTextureCache.find(info.shield_hash);
                    if (it_shield == mOwner->mShieldTextureCache.end())
                    {
                        MakePotTextureFromNpot(info.bitmap_width, info.bitmap_height, info.bitmap_data);
                        // This shield doesn't exist in texture cache, make a new texture from bitmap
                        renderer->CreateTextureFromData(texture, info.bitmap_width, info.bitmap_height,
                            graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, &info.bitmap_data[0]);
                        // Also insert texture into cache
                        mOwner->mShieldTextureCache.insert(std::make_pair(info.shield_hash, texture));
                    }
                    else
                    {
                        // This shield exists in texture cache, so obtain it texture from cache
                        texture = it_shield->second;
                    }

                    Label *label = new Label(renderer, mOwner->mTerrainView, mOwner->mBillboardShader,
                        &mPosition, info, mKey.magIndex);
                    label->setTexture(texture, false); // shield texture cache owns the texture
                    mLabelMeshes.push_back(label);
                }
                else // atlas-based label
                {
                    const Font * font = mOwner->chooseCompatibleFont();
                    AtlasLabel *label = new AtlasLabel(mOwner->renderer_, mOwner->mTerrainView,
                        mOwner->mBillboardShader, &mPosition, font, info, mKey.magIndex);
                    mAtlasLabelMeshes.push_back(label);
                }
            }
        }

        void TerrainTile::generateUserObjects()
        {
            std::vector<PointUserObjectInfo> point_user_objects;

            Lock();
            point_user_objects.swap(mPointUserObjects);
            Unlock();

            if (point_user_objects.empty()) return;

            for (size_t i = 0; i < point_user_objects.size(); ++i)
            {
                PointUserObjectInfo &pui = point_user_objects[i];

                TerrainMap::IconTextureCache::iterator it_cache = mOwner->mIconTextureCache.find((void*)pui.bmp);
                if (it_cache != mOwner->mIconTextureCache.end()) // texture with this ID exists in cache
                {
                    graphics::Texture * texture = it_cache->second;
                    Icon *icon = new Icon(mOwner->renderer_, mOwner->mTerrainView,
                        mOwner->mBillboardShader, &mPosition, pui, mKey.magIndex);
                    icon->setTexture(texture, false);
                    mPointUserMeshes.push_back(icon);
                    continue;
                }

                const mgnMdBitmap *bmp = pui.bmp;

                unsigned short w = bmp->mWidth;
                unsigned short h = bmp->mHeight;

                const unsigned int *src = (const unsigned int*)bmp->mpData;

                size_t N = w*h;
                std::vector<unsigned int> data; data.reserve(N);
                for (size_t i=0; i<N; ++i, ++src)
                {
                    unsigned int pix = 0;
        //                 if (*src != bmp->mTransColor)
                    pix = ((*src & 0xf800) >> 0x08) | ((*src & 0x7e0) << 0x05) | ((*src & 0x1f) << 0x13) | (*src & 0xff000000);
                    data.push_back(pix);
                }

                // Rescale image if it's not power of two 
                // (thus generateMipmaps isn't working on iOS devices with NPOT textures)
                graphics::Texture *tex = 0;
                if (((w & (w-1)) != 0) || ((h & (h-1)) != 0))
                {
                    unsigned short w2 = roundToPowerOfTwo(w);
                    unsigned short h2 = roundToPowerOfTwo(h);

                    // Rescale image to power of 2
                    unsigned int new_size = w2 * h2;
                    unsigned int * new_data = new unsigned int[new_size];
                    for (unsigned short dh2 = 0; dh2 < h2; ++dh2)
                    {
                        float rh = (float)dh2 / (float)h2;
                        float y = rh * (float)h;
                        unsigned short dh = (unsigned short)y;
                        unsigned short dh1 = std::min<unsigned short>(dh+1, h-1);
                        float ry = y - (float)dh; // fract part of y
                        for (unsigned short dw2 = 0; dw2 < w2; ++dw2)
                        {
                            float rw = (float)dw2 / (float)w2;
                            float x = rw * (float)w;
                            unsigned short dw = (unsigned short)x;
                            unsigned short dw1 = std::min<unsigned short>(dw+1, w-1);
                            float rx = x - (float)dw; // fract part of x
                            
                            // We will use bilinear interpolation
                            int sample1 = (int) data[dw +w*dh ];
                            int sample2 = (int) data[dw1+w*dh ];
                            int sample3 = (int) data[dw +w*dh1];
                            int sample4 = (int) data[dw1+w*dh1];

                            int color1 = interpolateColor(sample1, sample2, rx);
                            int color2 = interpolateColor(sample3, sample4, rx);;
                            int color3 = interpolateColor(color1, color2, ry);
                            new_data[dw2+w2*dh2] = (unsigned int)color3;
                        }
                    }
                    unsigned char * udata = reinterpret_cast<unsigned char*>(new_data);
                    mOwner->renderer_->CreateTextureFromData(tex, w2, h2,
                        graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, udata);
                    delete[] new_data;
                }
                else
                {
                    unsigned char * udata = reinterpret_cast<unsigned char*>(&data[0]);
                    mOwner->renderer_->CreateTextureFromData(tex, w, h,
                        graphics::Image::Format::kRGBA8, graphics::Texture::Filter::kTrilinear, udata);
                }
                
                mOwner->mIconTextureCache.insert(std::make_pair<void*, graphics::Texture*>((void*)pui.bmp, tex));
                
                Icon *icon = new Icon(mOwner->renderer_, mOwner->mTerrainView,
                    mOwner->mBillboardShader, &mPosition, pui, mKey.magIndex);
                icon->setTexture(tex, false); // cache owns texture
                mPointUserMeshes.push_back(icon);
            }
        }

        void TerrainTile::freeUserObjects()
        {
            for (size_t i=0; i<mPointUserMeshes.size(); ++i)
                delete mPointUserMeshes[i];
            mPointUserMeshes.clear();
        }

        float TerrainTile::getCellSizeY() const // size of an elementary cell of height map
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            const int number_of_tiles_per_side = kTileHeightSamples - 1;
            return sizeMetersLat / (float)number_of_tiles_per_side;
        }
        float TerrainTile::getCellSizeX() const // size of an elementary cell of height map
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            const int number_of_tiles_per_side = kTileHeightSamples - 1;
            return sizeMetersLon / (float)number_of_tiles_per_side;
        }
        void TerrainTile::origin(float& x, float& y) const // returns origin of the tile in local cs
        {
            // TODO: its wont give the right value as i think
            const int kTileHeightSamples = GetTileHeightSamples();
            float half = (float)(kTileHeightSamples - 1) * 0.5f;
            x = half * getCellSizeX();
            y = half * getCellSizeY();
        }
        bool TerrainTile::isPointInside(float x, float y) const
        {
            return mPosition.x - sizeMetersLon*0.5f <= x && x < mPosition.x + sizeMetersLon*0.5f &&
                   mPosition.z - sizeMetersLat*0.5f <= y && y < mPosition.z + sizeMetersLat*0.5f;
        }
        float TerrainTile::getHeightFromTilePos(float x, float y) const // returns height from position in tile coords
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            float half = (float)(kTileHeightSamples - 1) * 0.5f;
            float size_x = getCellSizeX();
            float size_y = getCellSizeY();
            int ix = clampX((int)floor(half - x/size_x));
            int iy = clampY((int)floor(half - y/size_y));
            int ix1 = clampX(ix+1);
            int iy1 = clampY(iy+1);
            // heights interpolation

            float cx = (half-(float)ix)*size_x;
            float cy = (half-(float)iy)*size_y;

            vec3 p1(cy, pureHeight(ix, iy), cx), p2, p3;
            if (x-cx < y-cy) // lower right triangle (0-1-2)
            {
                p2.Set(cy         , pureHeight(ix1, iy ), cx - size_x);
                p3.Set(cy - size_y, pureHeight(ix1, iy1), cx - size_x);
            }
            else // upper left triangle (0-2-3)
            {
                p2.Set(cy - size_y, pureHeight(ix1, iy1), cx - size_x);
                p3.Set(cy - size_y, pureHeight(ix , iy1), cx         );
            }
            vec3 n = ((p2 - p1) ^ (p3 - p1)).GetNormalized();
            float D = - n.x*p1.x - n.y*p1.y - n.z*p1.z; // use equation of plane to calculate height
            return -(n.x*y + n.z*x + D)/n.y;
        }
        void TerrainTile::worldToTile(const mgnMdWorldPoint& pt, float &x, float &y) const
        {
            double lx, ly;
            mOwner->mTerrainView->WorldToLocal(pt, lx, ly);
            x = (float)lx - mPosition.x;
            y = (float)ly - mPosition.z;
        }
        void TerrainTile::worldToTile(double lat, double lon, float &x, float &y) const
        {
            double lx, ly;
            mOwner->mTerrainView->WorldToLocal(mgnMdWorldPoint(lat, lon), lx, ly);
            x = (float)lx - mPosition.x;
            y = (float)ly - mPosition.z;
        }
        void TerrainTile::localToTile(float lx, float ly, float &x, float &y) const
        {
            x = lx - mPosition.x;
            y = ly - mPosition.z;
        }
        void TerrainTile::tileToLocal(float tx, float ty, float &lx, float &ly) const
        {
            lx = tx + mPosition.x;
            ly = ty + mPosition.z;
        }
        void TerrainTile::tileToWorld(float tx, float ty, mgnMdWorldPoint& pt) const
        {
            float lx = tx + mPosition.x;
            float ly = ty + mPosition.z;
            mOwner->mTerrainView->LocalToWorld((double)lx, (double)ly, pt);
        }
        int TerrainTile::clampX(int x) // clamps index to be in range
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            const int min_x = 0;
            const int max_x = kTileHeightSamples - 1;
            return (x < min_x) ? min_x : (x > max_x) ? max_x : x;
        }
        int TerrainTile::clampY(int y) // clamps index to be in range
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            const int min_y = 0;
            const int max_y = kTileHeightSamples - 1;
            return (y < min_y) ? min_y : (y > max_y) ? max_y : y;
        }
        int TerrainTile::clampIndX(int x) // clamps index to be in range
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            const int min_x = 0;
            const int max_x = kTileHeightSamples - 2;
            return (x < min_x) ? min_x : (x > max_x) ? max_x : x;
        }
        int TerrainTile::clampIndY(int y) // clamps index to be in range
        {
            const int kTileHeightSamples = GetTileHeightSamples();
            const int min_y = 0;
            const int max_y = kTileHeightSamples - 2;
            return (y < min_y) ? min_y : (y > max_y) ? max_y : y;
        }
        void TerrainTile::drawSegments(const math::Frustum& frustum)
        {
            mHighlightTrackChunk->render(frustum);
        }
        void TerrainTile::drawTileMesh(const math::Frustum& frustum, graphics::Shader * shader)
        {
            if (mTerrainMesh && mTexture)
            {
                calcBoundingBox();
                if (frustum.IsBoxIn(mBoundingBox))
                {
                    graphics::Renderer * renderer = mOwner->renderer_;
                    shader->Bind();
                    shader->UniformMatrix4fv("u_model", renderer->model_matrix());
                    renderer->ChangeTexture(mTexture);
                    mTerrainMesh->Render();
                    renderer->ChangeTexture(NULL);
                }
            }
        }

        void TerrainTile::drawLabelsMesh(boost::unordered_set<std::wstring>& used_labels, std::vector<math::Rect>& bboxes)
        {
            for (size_t i=0; i<mLabelMeshes.size(); ++i)
            {
                Label * label = mLabelMeshes[i];
                if(used_labels.find(label->text()) == used_labels.end())
                {
                    label->render();
                    used_labels.insert(label->text());
                }
            }

            const math::Matrix4& view = mOwner->renderer_->view_matrix();
            for (size_t i = 0; i < mAtlasLabelMeshes.size(); ++i)
            {
                AtlasLabel * label = mAtlasLabelMeshes[i];
                math::Rect bbox;
                label->GetBoundingBox(view, bbox);
                bool intersection_found = false;
                for (size_t j = 0; j < bboxes.size(); ++j) //
                {
                    const math::Rect& checked_bbox = bboxes[j];
                    if (math::RectRectIntersection2D(bbox, checked_bbox))
                    {
                        intersection_found = true;
                        break;
                    }
                }
                if(!intersection_found && used_labels.find(label->text()) == used_labels.end())
                {
                    label->render();
                    used_labels.insert(label->text());
                    bboxes.push_back(bbox);
                }
            }
        }

        void TerrainTile::drawUserObjects()
        {
            if (!mIsFetchedUserObjects)
                return;

            for (size_t i=0; i<mPointUserMeshes.size(); ++i)
            {
                mPointUserMeshes[i]->render();
            }
        }

        mgnMdWorldPosition TerrainTile::GetGPSPosition() const
        {
            return mOwner->GetGpsPosition();
        }

        void TileSetParams::clear(TerrainMap *terrainMap)
        {
            for (TileMap::iterator it=tileMap.begin(); it!=tileMap.end();)
            {
                terrainMap->recycleTile(it, this);
            }
            tileMap.clear();
        }

    } // namespace terrain
} // namespace mgn
