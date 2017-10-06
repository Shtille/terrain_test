#pragma once  

#include "mgnTrTerrainTile.h"

namespace mgn {
    namespace terrain {

        class Font;
        class HighlightTrackRenderer;
        class mgnTerrainFetcher;
        class TileCache;

        class TerrainMap
        {
            friend class TerrainTile;

            mgnMdTerrainView * mTerrainView;

            graphics::Shader * mTerrainShader;
            graphics::Shader * mBillboardShader;

            TileSetParams mTileSetParams[1];
            mutable TileSetParams *mActiveTSParams;     // tile set which is currently visible

            std::vector<TerrainTile*> renderedTiles;
            std::list<Icon*> mIconList; //!< list of icons (any billboard objects) for rendering
            mgnCSHandle mIconListCriticalSection;

            std::vector<math::Rect> mLabelBoundingBoxes;
            boost::unordered_set<std::wstring> mUsedLabels;

            mutable int mFrameCount;

            void clearTiles();
            void flushTiles();

            mgnTerrainFetcher  *mFetcher;
            TileCache          *mTileCache;

            typedef std::map<void*, graphics::Texture*> IconTextureCache;
            mutable IconTextureCache mIconTextureCache;

            typedef std::map<unsigned int, graphics::Texture*> ShieldTextureCache;
            mutable ShieldTextureCache mShieldTextureCache;

            Font * mOutlinedFont;
            
            const mgnMdWorldPosition * pGpsPosition;

            HighlightTrackRenderer * pHighlightTrackRenderer;

            bool mOnlineRasterMapsEnabled;

            TerrainMap &operator=(const TerrainMap &other);    //no assignment

        public:
            graphics::Renderer * renderer_;
            mgnMdTerrainProvider &mTerrainProvider;

            TerrainMap(graphics::Renderer * renderer, mgnMdTerrainView * terrain_view, mgnMdTerrainProvider * terrain_provider,
                graphics::Shader * terrain_shader, graphics::Shader * billboard_shader,
                const mgnMdWorldPosition * gps_pos,
                const char * font_path, const float pixel_scale);
            ~TerrainMap();

            const mgnMdWorldPosition& GetGpsPosition() const
            {
                return *pGpsPosition;
            }
            mgnMdTerrainView * getTerrainView()
            {
                return mTerrainView;
            }
            const mgnMdTerrainView * getTerrainView() const
            {
                return mTerrainView;
            }
            void setHighlightRenderer(HighlightTrackRenderer * highlight_renderer)
            {
                pHighlightTrackRenderer = highlight_renderer;
            }
            mgnTerrainFetcher& getFetcher()
            {
                return *mFetcher;
            }
            mgnTerrainFetcher * getFetcherPtr()
            {
                return mFetcher;
            }
            bool shouldRenderFakeTerrain() const;

            //! TODO: call it only when view changes or some new data is loaded
            // this enhancement will improve performance
            void updateIconList();
            void Update();
            void render(const math::Frustum& frustum);

            void renderTiles(mgnMdWorldPoint &location, TileSetParams &ts,
                const math::Frustum& frustum);
            void renderLabels();

            const Font * chooseCompatibleFont() const;

            void updateTiles(mgnMdWorldPoint &location, TileSetParams &ts);

            TerrainTile * findTile( mgnTileKey tilekey);
            TerrainTile * createNewTile( TileMap &tileMap, mgnTileKey tilekey, bool &created );

            // recycle tile from specified tile set
            void recycleTile(TileMap::iterator &it, TileSetParams *ts);

            void GetSelectedIcons(int x, int y, int radius, std::vector<int>& ids) const;
            void GetSelectedTracks(int x, int y, int radius, std::vector<int>& ids) const;
            void GetSelectedTrails(int x, int y, int radius, bool onLine, std::vector<std::string>& ids) const;

            void IntersectionWithRay(const vec3& ray, vec3& intersection) const;
        };

    } // namespace terrain
} // namespace mgn
