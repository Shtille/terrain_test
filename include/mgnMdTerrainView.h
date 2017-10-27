#ifndef TERRAIN_VIEW_H
#define TERRAIN_VIEW_H 1

#include "mgnBaseType.h"
#include "mgnMdWorldPoint.h"
#include "mgnMdPoint.hpp"
#include "mgnMdWorldRect.h"
#include "mgnMdTerrainProvider.h"
#include "Frustum.h"
#include "MapDrawing/Graphics/mgnMatrix.h"

class mgnMdBitmap;
class mgnMdTerrainView;

class mgnMdTerrainView
{
private:

    // Local coordinates are the "coordinates in meters" relatively to the point mLocation
    // mLocation in local coordinates is (0, 0, 0)

    // Reference point of rendering region
    mgnMdWorldPoint mLocation;  
    mutable mgnMdWorldPoint mCamPoint;

    // Magnitude is a parameter which is similar to camera height
    // but reduced to some discrete values.
    // Actually this is a kind of scales where map scale and tile configuration
    // will be changed
    mutable double mMagnitude;
    mutable unsigned short mMagIndex;
    // If this parameter is true -- magnitude will be automatically calculated
    // accordingly to the altitude
    bool mAutoMag;

    //------------------------------------
    // World coordinates (meters, degrees)

    // Camera position and orientation
    mutable float mGroundHeight;    // height of the ground over the sea level, meters
    mutable float mCenterHeight;    // height of the center over the sea level, meters
    float mCamDistance;             // distance of the camera from the viewpoint, meters
    float mHeading;                 // heading (=0 -- north direction), radians
    mutable double mTilt;           // tilt of the camera, radians
    mutable vec3 mCameraPosition;   //!< position of camera

    // Old camera position and orientation
    mutable mgnMdWorldPoint mOldLocation;
    mutable float mOldCamDistance;
    mutable float mOldHeading;
    mutable double mOldTilt;

//     // Size of single cell (meters)
//     mutable float mCellSizeLat;
//     mutable float mCellSizeLon;

    //------------------------------------
    // Local coordinates.
    // During conversion world-->local, mLocation should always be translated to (0, 0, 0)

    // Local <--> World conversion
    mutable double mMetersPerLatitude;
    mutable double mMetersPerLongitude;

    float mAnchorRatioX; //!< Anchor position ratio X in [0; 1], from top to bottom
    float mAnchorRatioY; //!< Anchor position ratio Y in [0; 1], from left to right
    bool mPanMap;        //!< are we panning map or not?

    float mPixelScale;  //!< Pixel scale

    float mZNear;       //!< Distance to the near clipping plane
    float mZFar;        //!< Distance to the far clipping plane
    float mFovX;        //!< Field of view in x direction

    mgnMdTerrainProvider * mTerrainProvider;

    bool isViewPathClear(double horz_dist, double tilt, double dxm) const;

public:
    mgnMdTerrainView(mgnMdWorldPoint location,int altitude);
    virtual ~mgnMdTerrainView();

    bool isVisible(const GeoSquare &square, float x, float y, float z) const;
    bool checkViewChange() const;

    // Unified fog usage condition
    bool useFog() const;

    bool isPanMap() const;

    // Update tilt and other parameters
    void update() const;

    // Get camera parameters
    mgnMdWorldPoint getViewPoint  () const;
    mgnMdWorldPoint getCamPoint   () const;

    //////////////////////////////////////////////////////////////////////////
    // TODO: should be removed ///////////////////////////////////////////////
    double          getCellSizeLat() const; // size of single tile in meters (latititude direction)
    double          getCellSizeLon() const; // size of single tile in meters (latititude direction)
    TPointInt       getCenterCell () const; // integer (lon, lat) indices of the center cell
    mgnMdWorldPoint getCenter     () const; // center of the central cell
    mgnMdWorldRect  getTileRect   (int ix, int iy) const;
    TPointInt       getCell       (double lat, double lon) const;

    // overloads for different mag indices
    double          getCellSizeLat(unsigned short magIndex) const;
    double          getCellSizeLon(unsigned short magIndex) const;
    TPointInt       getCenterCell (unsigned short magIndex) const;
    mgnMdWorldPoint getCenter     (unsigned short magIndex) const;
    mgnMdWorldRect  getTileRect   (unsigned short magIndex, int ix, int iy) const;
    mgnMdWorldRect  getWorldRect() const;
    mgnMdWorldRect  getLowDetailWorldRect() const;
    TPointInt       getCell       (unsigned short magIndex, double lat, double lon) const;
    //////////////////////////////////////////////////////////////////////////

    vec3   getCamPosition       () const; //!< returns current camera 3D position
    double getCamHeight         () const; // height over the ground level
    double getCamDistance       () const;
    double getCenterHeight      () const;
    double getCamHeadingRad     () const;
    double getCamTiltRad        () const;
    double getCamTiltStableRad  () const; // This is "approximate" tilt which is depend only on cam distance
    float  getAnchorRatioX      () const;
    float  getAnchorRatioY      () const;
    float  getZNear             () const;
    float  getZFar              () const;
    float  getFovX              () const;
    float  getLargestCamDistance() const;

    // Attendant parameters
    unsigned short getMagIndex  () const;
    double getMagnitude         () const;
    double getMetersPerLatitude () const;
    double getMetersPerLongitude() const;
    void   resetLatLonScale     ();
    // Related MapDraw parameters
    int    getMapScaleIndex     () const;
    float  getMapScaleFactor    () const;
    int    getMapScaleIndex     (unsigned short magIndex) const;
    float  getMapScaleFactor    (unsigned short magIndex) const;

    void setTerrainProvider(mgnMdTerrainProvider * provider);
    void setPixelScale(float pixel_scale);
    float getPixelScale() const;

    void   setViewPoint         (const mgnMdWorldPoint &pt);
    void   setGroundHeight      (float v); // height of the ground level over the sea level
    void   setCenterHeight      (float v); // height of the center over the sea level
    void   setCamDistance       (float v);
    void   setCamHeadingRad     (float v);
    void   setAnchorPosition    (float x, float y, bool panMap);

    void WorldToLocal (const mgnMdWorldPoint& wrld_pt, double &local_x, double &local_y) const;
    void LocalToWorld (double local_x, double local_y, mgnMdWorldPoint &wrld_pt) const;

    void LocalToPixelDistance(float local_x, float& pixel_x, float map_size_max) const;
    void PixelToLocalDistance(float pixel_x, float& local_x, float map_size_max) const;
    void LocalToPixel(const vec3& local, vec3& pixel, float map_size_max) const;
    void PixelLocation(vec3& pixel, float map_size_max) const;

    void IntersectionWithRay(const vec3& ray, vec3& intersection) const;

    //####################################################################################
    //      UTILITES
    
    static unsigned short   getMaxMagnitudeIndex();
    static unsigned short   calcMagnitudeIndexByDistance(double distance, unsigned short hint=0);
    static double           getMagnitudeByIndex(unsigned short magindex);
    static const int getGridRadius();
};

#endif