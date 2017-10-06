#ifndef TERRAIN_PROVIDER_H
#define TERRAIN_PROVIDER_H 1

#include "mgnBaseType.h"
#include "mgnMdWorldPoint.h"
#include "mgnMdWorldRect.h"

#include <vector>
#include <string>

#define MAX_NODE 100000
#define MAX_ROAD_SEGMENTS (3*MAX_NODE)
#define MAX_ROAD_STYLES 24
#define MAX_LAKE_EDGES MAX_NODE

class OpenGLTexture;
class mgnMdIUserDataDrawContext;
class mgnMdBitmap;

struct GeoSquare
{
    // current implementation is orthogonal in geo space

    mgnMdWorldPoint mGeoCorner[4];

    GeoSquare(const mgnMdWorldRect &rc)
    {
        double lat0 = rc.mLowerRight.mLatitude ;
        double lon0 = rc.mLowerRight.mLongitude;
        double lon1 = rc.mUpperLeft .mLongitude;
        double lat1 = rc.mUpperLeft .mLatitude ;
        mGeoCorner[0] = mgnMdWorldPoint(lat0, lon0);
        mGeoCorner[1] = mgnMdWorldPoint(lat0, lon1);
        mGeoCorner[2] = mgnMdWorldPoint(lat1, lon0);
        mGeoCorner[3] = mgnMdWorldPoint(lat1, lon1);
    }
    GeoSquare(double lat0,double lon0,double lat1,double lon1)
    {
        mGeoCorner[0] = mgnMdWorldPoint(lat0, lon0);
        mGeoCorner[1] = mgnMdWorldPoint(lat0, lon1);
        mGeoCorner[2] = mgnMdWorldPoint(lat1, lon0);
        mGeoCorner[3] = mgnMdWorldPoint(lat1, lon1);
    }

    double latSize() const { return mGeoCorner[2].mLatitude - mGeoCorner[0].mLatitude; }
    double lonSize() const { return mGeoCorner[0].mLongitude - mGeoCorner[1].mLongitude; }
    double minLat() const { return mGeoCorner[0].mLatitude; }
    double maxLat() const { return mGeoCorner[3].mLatitude; }
    double minLon() const { return mGeoCorner[3].mLongitude; }
    double maxLon() const { return mGeoCorner[0].mLongitude; }
};

struct GeoPoint
{
    float mLat;
    float mLon;
};

struct LabelPositionInfo
{
    // Integer coordinates of the label (in pixels on main texture)
    int lat, lon;
    // Terrain height at the label point (in meters)
    double alt;
    // size of label in texture
    int w, h;

    bool centered;
    unsigned int shield_hash;
    std::wstring text;

    int bitmap_width;
    int bitmap_height;
    std::vector<unsigned char> bitmap_data;
};

struct PointUserObjectInfo
{
    // Coordinates of the object (pixels on tile)
    double lat, lon;
    // Terrain height at the object's point (in meters)
    double alt;

    int id; //!< ID of user data object (need for selection)

    bool centered;

    // bitmap
    const mgnMdBitmap *bmp;

    // texture size. It should be 2^N
    size_t tex_w, tex_h;
};

struct GeoState
{
    enum GeoResult
    {
        PENDING,
        COMPLETE,
        FAILED
    };

    GeoResult mResult;

    GeoState() : mResult(PENDING){}
    virtual ~GeoState(){}

    void setResult(GeoResult result)
    {
        mResult=result;
    }
};

struct GeoHeightMap : GeoState
{
    int mWidth;
    int mHeight;

    mgnI16_t *mHeightSamples;

    GeoHeightMap(int width, int height, mgnI16_t *samples)
        : mWidth(width)
        , mHeight(height)
        , mHeightSamples(samples)
    {
    }
};

struct GeoTerrain : GeoState
{
    int mWidth;
    int mHeight;

    float *mAltitudes;  //altitude in meters
    int mStride;        //words not bytes

    bool mErrorOccurred;// if true -- then cosmos error occurred during fetching and operation should be performed again

    GeoTerrain(int width, int height, float *buffer, int stride)
        : mWidth(width)
        , mHeight(height)
        , mAltitudes(buffer)
        , mStride(stride)
        , mErrorOccurred(false)
    {
    }
};

struct GeoPixelMap : GeoState
{
    int  mWidth ;
    int  mHeight;
    int *mPixels;
};

struct GeoTextureMap : GeoState
{
    std::vector<unsigned char> mTextureData;
    std::vector<LabelPositionInfo> mLabelsInfo;

    bool mErrorOccurred; // any errors occurred
    bool mCosmosErrors;
    bool mNeedLabels;

    GeoTextureMap() : mErrorOccurred(false), mCosmosErrors(false), mNeedLabels(true) {};
};

struct PointUserObjectsInfo : GeoState
{
    std::vector<PointUserObjectInfo> objects;
};

struct GeoRoads : GeoState
{
    GeoPoint location[MAX_NODE];
    int segments[MAX_ROAD_SEGMENTS];
    int styles[MAX_ROAD_STYLES];
    int numNodes;
};

struct GeoLakes : GeoState
{
    GeoPoint location[MAX_NODE];
    int altitudes[MAX_LAKE_EDGES];
    int numNodes;
};

struct GeoHighlight
{
    // Messages for notification
    enum Message {
        kNoMessages = 0,
        kAddRoute = 1,
        kRemoveRoute = 2
    };
    struct Part {
        std::vector<mgnMdWorldPoint> points;
        mgnU16_t color; // 565 color
    };
    // There may be different count of objects
    unsigned int points_count;
    std::vector<Part> parts;
};

struct GeoOffroad
{
    std::vector<mgnMdWorldPoint> start_line;
    std::vector<mgnMdWorldPoint> dest_line;
    char phase; //!< 0 - unknown, 1 - on start line, 2 - on road, 3 - on dest line
    // If we have highlight we always have start line and dest line
    bool exist() { return !start_line.empty() || !dest_line.empty(); }
};

/*! Service struct for obtaining maneuver data */
struct GeoGuidanceArrow
{
    float distance;                     //!< [in] distance
    mgnMdWorldPoint point;              //!< [out] point of guidance arrow
    float heading;                      //!< [out] heading, radians
};

/*! Service struct for obtaining maneuver data */
struct GeoManeuver
{
    mgnMdWorldPoint point;  //!< point of maneuver
    int source_heading;     //!< source heading from point, degrees
    int dest_heading;       //!< destination heading from point, degrees
    int startPathIndex;     
    int endPathIndex;       
};

/*! Service struct for obtaining route point data */
struct GeoRoutePoint
{
    mgnMdWorldPoint point;  //!< point of route

    GeoRoutePoint() {}

    GeoRoutePoint(const mgnMdWorldPoint& wp)
      : point(wp)
    {}
};

class mgnMdTerrainProvider
{
public:
    virtual ~mgnMdTerrainProvider(){}

    virtual void fetchHeightMap(GeoSquare& /*square*/, GeoHeightMap& /*result*/){}

    virtual void fetchTerrain(GeoSquare& /*square*/, GeoTerrain& /*result*/){}

    virtual void fetchTile(GeoSquare& /*cliprect*/, GeoPixelMap& /*result*/){}

    virtual void fetchTexture(GeoSquare& /*cliprect*/, GeoTextureMap& /*result*/){}

    virtual void fetchRoads(GeoSquare& /*clipRect*/, GeoRoads& /*result*/){}

    virtual void fetchWater(GeoSquare& /*clipRect*/, GeoLakes& /*result*/){}

    virtual void fetchHighlight(const mgnMdIUserDataDrawContext& /*contextGeo*/, GeoHighlight& /*highlight*/) {}
    virtual bool fetchNeedToTrimHighlight() { return false; }
    virtual bool isHighlightExist() { return false; }
    virtual int  dequeueHighlightMessage() { return GeoHighlight::kNoMessages; }
    virtual void reformHighlightMessages() {}
    virtual void clearHighlightMessages() { }

    virtual void fetchOffroad(const mgnMdIUserDataDrawContext& /*context*/, GeoOffroad& /*offroad*/) {}

    virtual bool fetchGuidanceArrow(const mgnMdIUserDataDrawContext& /*context*/, GeoGuidanceArrow& /*arrow*/) { return false; }

    //! Returns true if maneuver exists, false otherwise
    virtual bool fetchManeuver(GeoManeuver& /*maneuver*/) { return false; }

    virtual bool fetchManeuverAroundPath(const mgnMdIUserDataDrawContext& context, double interval, std::vector<GeoRoutePoint>& points) { return false;}

    //! Returns true if route begin exists, false otherwise
    virtual bool fetchRouteBegin(std::vector<mgnMdWorldPoint>& /*route_point*/) { return false; }

    //! Returns true if route end exists, false otherwise
    virtual bool fetchRouteEnd(std::vector<mgnMdWorldPoint>& /*route_point*/) { return false; }

    virtual bool fetchGpsMmPoint(mgnMdWorldPoint * gps_point, mgnMdWorldPoint * mm_point) { return false; }

    virtual void fetchPointUserData(const mgnMdIUserDataDrawContext& /*context*/, PointUserObjectsInfo& /*result*/) {}

    virtual double getAltitude( double lat, double lng, float dxm = -1.0f ) { return 0.0; }

    virtual void GetSelectedTracks(const mgnMdIUserDataDrawContext& context, std::vector<int>& ids) {}

    virtual void GetSelectedTrails(const mgnMdIUserDataDrawContext& context, bool onLine, std::vector<std::string>& ids) {}

    // Functions for terrain textures update
    virtual void AddTextureUpdateRequest() {}
    virtual bool IsTextureUpdateRequested() { return false; }
    virtual void ClearTextureUpdateRequests() {}

    virtual void AddUserDataUpdateRequest() {}
    virtual bool IsUserDataUpdateRequested() {return false;}
    virtual void ClearUserDataUpdateRequests() {}
};

#endif
