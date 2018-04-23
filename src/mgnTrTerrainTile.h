#pragma once

#include "MapDrawing/Graphics/Renderer.h"

#include <string>

#include "mgnMdWorldPoint.h"
#include "mgnMdWorldRect.h"
#include "mgnMdWorldPosition.h"

#include "mgnCriticalSections.h"

#include "mgnMdIUserDataDrawContext.h"

#include "mgnTrTileKey.h"
#include "mgnMdTerrainProvider.h"
#include "mgnMdTerrainView.h"
#include "Frustum.h"

#include "boost/unordered_set.hpp"

namespace math {
    struct Rect;
}

namespace mgn {
    namespace terrain {

        class TerrainMap;
        class SolidLineChunk;
        class Heightmap;
        class Label;
        class AtlasLabel;
        class Icon;

        class TerrainTile : public mgnMdIUserDataDrawContext
        {
        public:
            const TerrainMap *mOwner;

            mgnCSHandle mCriticalSection;

            GeoSquare mGeoSquare;

            mgnTileKey mKey;
            math::Vector3 mPosition; //!< position of tile in world space
            size_t priority; // updated by render. Used for tile sorting in fetcher
            // Tile size in meters
            float sizeMetersLat;
            float sizeMetersLon;
            mutable int mHighlightMessage;  //!< highlight messages

            Heightmap * mTerrainMesh;
            Heightmap * mFetchedTerrainMesh; // should be in sync
            std::vector<Label*> mLabelMeshes;
            std::vector<AtlasLabel*> mAtlasLabelMeshes;
            std::vector<Icon*> mPointUserMeshes;

            float *mHeightSamples;
            float *mFetchedHeightSamples;

            graphics::Texture * mTexture;

            // These flags should be used only in render thread
            bool mIsFetchedTerrain;
            bool mIsFetchedTexture;
            bool mIsFetchedLabels;
            bool mNeedToGenerateLabels;
            bool mIsFetchedUserObjects;
            bool mIsFetched;

            // Flags for refetching tile in case incomplete fetching
            bool mRefetchTerrain;
            bool mRefetchTexture;
            bool mUpdateTexture; // update texture flag
            bool mUpdateTextureRequested;
            bool mUpdateUserData;
            bool mUpdateUserDataRequested;
            bool mUpdatePassiveHighlight;
            bool mUpdatePassiveHighlightRequested;

            // Data which will be shared between several threads (fetching and displaying)
            std::vector<unsigned char>          mTextureData;
            std::vector<LabelPositionInfo>      mLabelsInfo;
            std::vector<PointUserObjectInfo>    mPointUserObjects;

            SolidLineChunk * mHighlightTrackChunk;
            SolidLineChunk * mPassiveHighlightTrackChunk;

            math::BoundingBox mBoundingBox;

        public:

            int mDrawFrame;

            void updateTracks(); // this function is called every frame
            void fetchTracks(); // called in other thread
            void fetchPassiveHighlight();
            void calcBoundingBox();

            TerrainTile(const TerrainMap *owner, const mgnTileKey &key, GeoSquare geoSquare, float gx, float gy);
            virtual ~TerrainTile();

            // Derived from mgnMdIUserDataDrawContext :
            mgnMdWorldRect GetWorldRect() const;
            mgnMdWorldPosition GetGPSPosition() const;
            int GetMapScaleIndex() const;
            float GetMapScaleFactor() const;
            bool ShouldDrawPOIs() const { return false; }
            bool ShouldDrawLabels() const { return false; }

            float pureHeight(int ix, int iy) const;

            const vec3& position() const;

            void Lock() const;
            void Unlock() const;

            void fetchTerrain();
            void fetchTexture();
            void fetchUserObjects();

            bool isRefetchTerrain() const;
            bool isRefetchTexture() const;
            bool isFetchedTerrain() const;
            bool isFetchedTexture() const;
            bool isFetchedLabels() const;
            bool isFetched       () const;

            void generateMesh();
            void generateTextures();
            void generateLabels();
            void generateUserObjects();
            void freeUserObjects();
            float getCellSizeY() const; // size of an elementary cell of height map
            float getCellSizeX() const; // size of an elementary cell of height map
            void origin(float& x, float& y) const; // returns origin of the tile in local cs
            bool isPointInside(float x, float y) const;
            float getHeightFromTilePos(float x, float y) const;
            void worldToTile(const mgnMdWorldPoint& pt, float &x, float &y) const;
            void worldToTile(double lat, double lon, float &x, float &y) const;
            void localToTile(float lx, float ly, float &x, float &y) const;
            void tileToLocal(float tx, float ty, float &lx, float &ly) const;
            void tileToWorld(float tx, float ty, mgnMdWorldPoint& pt) const;
            static int clampX(int x); // clamps index to be in range
            static int clampY(int y); // clamps index to be in range
            static int clampIndX(int x); // clamps index to be in range
            static int clampIndY(int y); // clamps index to be in range

            void drawSegments(const math::Frustum& frustum);
            void drawTileMesh(const math::Frustum& frustum, graphics::Shader * shader);
            void drawLabelsMesh(
                boost::unordered_set<std::wstring>& used_labels, std::vector<math::Rect>& bboxes);
            void drawUserObjects();
        };

        // Set of supplementary information for rendering tiles
        struct TileSetParams
        {
            TileSetParams()
                : tx(0), ty(0), magIndex(-1), heading(-100) {}
            ~TileSetParams() { assert(tileMap.empty()); clear(0); }

            // indices of central tile
            int tx, ty;
            // Magnitude index for the tile set
            unsigned short magIndex;
            // scaled and rounded heading (used for performance increasing)
            int heading;

            TerrainTile * findTile(const mgnTileKey &key)
            {
                TileMap::iterator it = tileMap.find(key);
                if (it != tileMap.end()) return it->second;
                else return 0;
            }

            TileMap tileMap;
            std::vector<mgnTileKey> tileKeys;

            void clear(TerrainMap *terrainMap);
        };

    } // namespace terrain
} // namespace mgn