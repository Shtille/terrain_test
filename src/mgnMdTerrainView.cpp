#include "mgnMdTerrainView.h"
#include <math.h>
#include <algorithm>

#include "mgnMath.h"
#include "mgnLog.h"
#include "../src/mgnMdUtils/mgnMdMapScale.h"

// terrain maps are based on projection of uniform grid centered on a specific geographic long lat

#define EarthCircumference 40075160.0
#define EarthRadius 6371000.0f  // Radius in Kilometers default

class WGS84
{
public:

    static double haversineDistance(double lat1,double lon1,double lat2,double lon2)
    {
        double Radius = EarthRadius;
        double dLat = toRadians(lat2-lat1);
        double dLon = toRadians(lon2-lon1);            
        double a = sin(dLat/2) * sin(dLat/2) + cos(toRadians(lat1)) *   cos(toRadians(lat2)) * sin(dLon/2) * sin(dLon/2);
        double c = 2 * asin(sqrt(a));
        return Radius * c;
    }

    static double toRadians(double degrees)
    {
        return degrees*PI/180;
    }
};

static double calcTilt(double distance, double height)
{
    double s = height/distance;
    if (s >= 1) s = 1;
    if (s < 0.4) s = 0.4;
    return asin(s);
}

namespace
{;
const unsigned short MAG_COUNT = 16;
const double MAGNITUDES[MAG_COUNT] =
{               // distances
      64        // [0..64)
    , 128       // [64..128)
    , 256       // .........
    , 512
    , 1024
    , 2048
    , 4096
    , 8192
    , 16384
    , 32768
    , 65536     // [10]
    , 131072
    , 262144
    , 524288
    , 1048576
    , 2097152   // [15]
};

} // namespace

mgnMdTerrainView::mgnMdTerrainView(mgnMdWorldPoint location,int altitude):
  mLocation(location),
  mHeading(0),
  mGroundHeight(0),
  mCenterHeight(0),
  mCamDistance(400),
  mTilt(0.4),
  mMagIndex(0),
  mMagnitude(-1),
//   mCellSizeLat(100),
//   mCellSizeLon(100),
  mMetersPerLatitude(-1),
  mMetersPerLongitude(-1),
  mAutoMag( true ),
  mAnchorRatioX(0.5f),
  mAnchorRatioY(0.75f),
  mPanMap(true),
  mPixelScale(1.0f),
  mZNear(1.0f),
  mZFar(1000.0f),
  mFovX(PI*0.4f),
  mTerrainProvider(NULL)
{
}

mgnMdTerrainView::~mgnMdTerrainView()
{
}

double mgnMdTerrainView::getMagnitudeByIndex(unsigned short magindex)
{
    if (magindex >= MAG_COUNT) magindex = MAG_COUNT-1;
    return MAGNITUDES[magindex];
}

unsigned short mgnMdTerrainView::getMaxMagnitudeIndex()
{
    return MAG_COUNT;
}
const int mgnMdTerrainView::getGridRadius()
{
    return 3;
}

unsigned short mgnMdTerrainView::calcMagnitudeIndexByDistance(double distance, unsigned short hint/*=0*/)
{
    double mag = distance;
    unsigned short ind = hint;

    while ((ind < MAG_COUNT-1) && (mag >  MAGNITUDES[ind])) ++ind;
    while ((ind >           0) && (mag <= MAGNITUDES[ind])) --ind;

    if (ind > 0) --ind;

    return ind;
}

bool mgnMdTerrainView::isVisible(const GeoSquare &/*square*/, float /*x*/, float /*y*/, float /*z*/) const
{
    return true;
}

bool mgnMdTerrainView::checkViewChange() const
{
    const bool changed = fabs(mLocation.mLatitude  - mOldLocation.mLatitude) > 1e-9
                      || fabs(mLocation.mLongitude - mOldLocation.mLongitude) > 1e-9
                      || fabs(mCamDistance - mOldCamDistance) > 1e-3
                      || fabs(mHeading     - mOldHeading    ) > 1e-3
                      || fabs(mTilt        - mOldTilt       ) > 1e-3;
    if (changed)
    {
        mOldLocation = mLocation;
        mOldCamDistance = mCamDistance;
        mOldHeading = mHeading;
        mOldTilt = mTilt;
    }
    return changed;
}

bool mgnMdTerrainView::useFog() const
{
    return mTilt < 1.0;
}

bool mgnMdTerrainView::isPanMap() const
{
    return mPanMap;
}

double mgnMdTerrainView::getMagnitude      () const
{
    static const int   tmp_offset = -1;
    static const float tmp_hdens = 1;

    // TODO: refactoring required
    if (mMagnitude < 0)
    {
        double d = getCamDistance();
        mMagIndex = calcMagnitudeIndexByDistance(d);
        mMagnitude = getMagnitudeByIndex(mMagIndex);
    }

    return mMagnitude;
}

unsigned short mgnMdTerrainView::getMagIndex() const
{
    return mMagIndex;
}
int    mgnMdTerrainView::getMapScaleIndex() const
{
    return getMapScaleIndex(mMagIndex);
}
float  mgnMdTerrainView::getMapScaleFactor() const
{
    return getMapScaleFactor(mMagIndex);
}
int    mgnMdTerrainView::getMapScaleIndex (unsigned short magIndex) const
{
    return MdMapScale(getMapScaleFactor(magIndex)).GetScale();
}
float  mgnMdTerrainView::getMapScaleFactor (unsigned short magIndex) const
{
    return getMagnitudeByIndex(magIndex)/3;
}
bool mgnMdTerrainView::isViewPathClear(double horz_dist, double tilt, double dxm) const
{
    // This function should be called once per view change
    const double kCellsPerTileEdge = 30.0;
    const double kDeltaHeight = 1.0;
    const double trace_step = 0.25*getCellSizeLat()/kCellsPerTileEdge;
    const double height_step = trace_step * tan(tilt);
    mgnMdWorldPoint point = mLocation;
    double cos_a = cos(mHeading)/getMetersPerLatitude();
    double sin_a = sin(mHeading)/getMetersPerLongitude();
    double distance = 0.0;
    double path_height = mCenterHeight;
    do
    {
        distance += trace_step;
        path_height += height_step;
        point.mLatitude  -= trace_step * cos_a;
        point.mLongitude -= trace_step * sin_a;
        double terrain_height = mTerrainProvider->getAltitude(point.mLatitude, point.mLongitude, (float)dxm);
        if (path_height + kDeltaHeight < terrain_height)
            return false;
    }
    while (distance < horz_dist);

    return true;
}
void mgnMdTerrainView::update() const
{
    const double dxm = getMagnitude() / 111111.0;
    const bool low_altitudes = dxm < 0.01;
    mCamPoint = mLocation;
    float dh = 3.125f * mZNear;
    double h = getCamHeight();
    double desired_tilt = calcTilt(mCamDistance, h);
    double horz_dist = mCamDistance * cos(desired_tilt);
    double cos_heading = cos(mHeading)/getMetersPerLatitude();
    double sin_heading = sin(mHeading)/getMetersPerLongitude();
    mCamPoint.mLatitude  -= horz_dist * cos_heading;
    mCamPoint.mLongitude -= horz_dist * sin_heading;
    mCenterHeight = (float)mTerrainProvider->getAltitude(mLocation.mLatitude, mLocation.mLongitude);
    mGroundHeight = (float)mTerrainProvider->getAltitude(mCamPoint.mLatitude, mCamPoint.mLongitude);
    if (low_altitudes) // using ray tracing to find terrain intersection with view ray
    {
        //while (mGroundHeight - mCenterHeight > mCamDistance * sin(desired_tilt) - dh)
        while (!isViewPathClear(horz_dist, desired_tilt, dxm))
        {
            desired_tilt += 0.1;
            horz_dist = mCamDistance * cos(desired_tilt);
            mCamPoint = mLocation;
            mCamPoint.mLatitude  -= horz_dist * cos_heading;
            mCamPoint.mLongitude -= horz_dist * sin_heading;
            mGroundHeight = (float)mTerrainProvider->getAltitude(mCamPoint.mLatitude, mCamPoint.mLongitude);
        }
    }
    else // high altitudes
    {
        // do nothing
    }
    mTilt = desired_tilt;

    // Calculate camera position
    {
        double local_x = (mCamPoint.mLongitude - mLocation.mLongitude) * getMetersPerLongitude();
        double local_y = (mCamPoint.mLatitude - mLocation.mLatitude) * getMetersPerLatitude();
        mCameraPosition.x = static_cast<float>(local_x);
        mCameraPosition.z = static_cast<float>(local_y);
        mCameraPosition.y = mCenterHeight + mCamDistance * sin(mTilt);
    }
}
mgnMdWorldPoint mgnMdTerrainView::getViewPoint() const
{
    return mLocation;
}
mgnMdWorldPoint mgnMdTerrainView::getCamPoint () const
{
    update();
    return mCamPoint;
}

double mgnMdTerrainView::getCellSizeLat() const
{
    return getCellSizeLat(mMagIndex);
}

double mgnMdTerrainView::getCellSizeLon() const
{
    return getCellSizeLon(mMagIndex);
}

double mgnMdTerrainView::getCellSizeLat(unsigned short magIndex) const
{
    return getMagnitudeByIndex(magIndex);
}
double mgnMdTerrainView::getCellSizeLon(unsigned short magIndex) const
{
    return getMagnitudeByIndex(magIndex);
}
TPointInt mgnMdTerrainView::getCenterCell() const
{
    return getCenterCell(mMagIndex);
}
TPointInt mgnMdTerrainView::getCenterCell(unsigned short magIndex) const
{
    return getCell(magIndex, mLocation.mLatitude, mLocation.mLongitude);
}

mgnMdWorldPoint mgnMdTerrainView::getCenter() const
{
    return getCenter(mMagIndex);
}
mgnMdWorldPoint mgnMdTerrainView::getCenter(unsigned short magIndex) const
{
    TPointInt cc = getCenterCell(magIndex);
    mgnMdWorldRect rc = getTileRect(magIndex, cc.x, cc.y);

    return mgnMdWorldPoint
    (
        (rc.mLowerRight.mLatitude + rc.mUpperLeft.mLatitude)/2,
        (rc.mLowerRight.mLongitude + rc.mUpperLeft.mLongitude)/2
    );
}

TPointInt mgnMdTerrainView::getCell (double lat, double lon) const
{
    return getCell(mMagIndex, lat, lon);
}
TPointInt mgnMdTerrainView::getCell (unsigned short magIndex, double lat, double lon) const
{
    TPointInt cell;
    cell.x = static_cast<int>(floor(lon * getMetersPerLongitude() / getCellSizeLon(magIndex)));
    cell.y = static_cast<int>(floor(lat * getMetersPerLatitude () / getCellSizeLat(magIndex)));
    return cell;
}

mgnMdWorldRect mgnMdTerrainView::getTileRect  (int ix, int iy) const
{
    // TODO: refactor required
    return mgnMdWorldRect
    (
         iy    * getCellSizeLat() / getMetersPerLatitude (),
         ix    * getCellSizeLon() / getMetersPerLongitude(),
        (iy+1) * getCellSizeLat() / getMetersPerLatitude (),
        (ix+1) * getCellSizeLon() / getMetersPerLongitude()
    );
}

mgnMdWorldRect mgnMdTerrainView::getTileRect  (unsigned short magIndex, int ix, int iy) const
{
    // TODO: refactor required
    return mgnMdWorldRect
    (
         iy    * getCellSizeLat(magIndex) / getMetersPerLatitude (),
         ix    * getCellSizeLon(magIndex) / getMetersPerLongitude(),
        (iy+1) * getCellSizeLat(magIndex) / getMetersPerLatitude (),
        (ix+1) * getCellSizeLon(magIndex) / getMetersPerLongitude()
    );
}

mgnMdWorldRect mgnMdTerrainView::getWorldRect() const
{
    mgnMdWorldPoint center = getCenter();
    double half = (double)mgnMdTerrainView::getGridRadius() + 0.5;
    double size_lat = getCellSizeLat() * half / getMetersPerLatitude();
    double size_lon = getCellSizeLon() * half / getMetersPerLongitude();
    mgnMdWorldPoint upper_left  = mgnMdWorldPoint(center.mLatitude  + size_lat,
        center.mLongitude - size_lon);
    mgnMdWorldPoint lower_right = mgnMdWorldPoint(center.mLatitude  - size_lat,
        center.mLongitude + size_lon);
    return mgnMdWorldRect(upper_left, lower_right);
}

mgnMdWorldRect mgnMdTerrainView::getLowDetailWorldRect() const
{
    mgnMdWorldPoint center = getCenter();
    double half = 0.5;
    double size_lat = getCellSizeLat(getMagIndex()+5) * half / getMetersPerLatitude();
    double size_lon = getCellSizeLon(getMagIndex()+5) * half / getMetersPerLongitude();
    mgnMdWorldPoint upper_left  = mgnMdWorldPoint(center.mLatitude + size_lat, center.mLongitude - size_lon);
    mgnMdWorldPoint lower_right = mgnMdWorldPoint(center.mLatitude - size_lat, center.mLongitude + size_lon);
    return mgnMdWorldRect(upper_left, lower_right);
}

double mgnMdTerrainView::getMetersPerLatitude () const
{
    return 111111;
//     if (mMetersPerLatitude < 0)
//     {
//         double lat = mLocation.mLatitude;
//         double lon = mLocation.mLongitude;
//         mMetersPerLatitude = 111111;//WGS84::haversineDistance(lat, lon, lat+1, lon);
//     }
// 
//     return mMetersPerLatitude;
}

double mgnMdTerrainView::getMetersPerLongitude() const
{
    return 111111;
//     if (mMetersPerLongitude < 0)
//     {
//         double lat = mLocation.mLatitude;
//         double lon = mLocation.mLongitude;
//         mMetersPerLongitude = 111111;//WGS84::haversineDistance(lat, lon, lat, lon+1);
//     }
// 
//     return mMetersPerLongitude;
}

void   mgnMdTerrainView::resetLatLonScale ()
{
    mMetersPerLatitude = mMetersPerLongitude = -1;
}
vec3 mgnMdTerrainView::getCamPosition() const
{
    return mCameraPosition;
}
double mgnMdTerrainView::getCamHeight     () const
{
    static volatile double q = 20, p = 20000;
    double d = getCamDistance();
    double h = d*(1 - p/(q*d+p));
    return h;
}
double mgnMdTerrainView::getCamDistance   () const
{
    return mCamDistance;
}
double mgnMdTerrainView::getCamHeadingRad () const
{
    return mHeading;
}
double mgnMdTerrainView::getCamTiltRad    () const
{
    return mTilt;
}
double mgnMdTerrainView::getCamTiltStableRad() const
{
    return calcTilt(getCamDistance(), getCamHeight());
}
double mgnMdTerrainView::getCenterHeight() const
{
    return mCenterHeight;
}
float mgnMdTerrainView::getAnchorRatioX() const
{
    return mAnchorRatioX;
}
float mgnMdTerrainView::getAnchorRatioY() const
{
    return mAnchorRatioY;
}
float mgnMdTerrainView::getZNear() const
{
    return mZNear;
}
float mgnMdTerrainView::getZFar() const
{
    return mZFar;
}
float mgnMdTerrainView::getFovX() const
{
    return mFovX;
}
float mgnMdTerrainView::getLargestCamDistance() const
{
    return mZFar;
}
void mgnMdTerrainView::setTerrainProvider(mgnMdTerrainProvider * provider)
{
    mTerrainProvider = provider;
}
void mgnMdTerrainView::setPixelScale(float pixel_scale)
{
    mPixelScale = pixel_scale;
}
float mgnMdTerrainView::getPixelScale() const
{
    return mPixelScale;
}
void mgnMdTerrainView::setViewPoint(const mgnMdWorldPoint &pt)
{
    mLocation = pt;
}

void   mgnMdTerrainView::setGroundHeight      (float v)
{
    // now we are not distinguish ground and center height (TODO)
    mGroundHeight = v;
}

void mgnMdTerrainView::setCenterHeight (float v)
{
    // now we are not distinguish ground and center height (TODO)
    mCenterHeight = v;
}

void  mgnMdTerrainView::setCamDistance    (float v)
{
    float delta = fabs(mCamDistance - v);
    mCamDistance = v;
    if (mAutoMag && delta > 0.1) mMagnitude = -1;
    getMagnitude();
    //---
    //mZFar = 10.0f * mCamDistance;
    mZFar = (2.5f + cos((float)mTilt)) * (float)getMagnitudeByIndex(mMagIndex + 3);
    mZNear = 0.001f * mZFar;
    //---
    update();
}
void  mgnMdTerrainView::setCamHeadingRad  (float v)
{
    mHeading = v;
}
void mgnMdTerrainView::setAnchorPosition(float x, float y, bool panMap)
{
    mAnchorRatioX = x;
    mAnchorRatioY = y;
    mPanMap = panMap;
}
void mgnMdTerrainView::WorldToLocal (const mgnMdWorldPoint& wrld_pt, double &local_x, double &local_y) const
{
    // terrainView->getLocation() in local coordinates is always (0, 0)

    const mgnMdWorldPoint &location = getViewPoint();

    // Shift world point to currently requested center

    // The point in meters
    local_y = (wrld_pt.mLatitude - location.mLatitude)  * getMetersPerLatitude ();
    local_x = (wrld_pt.mLongitude - location.mLongitude) * getMetersPerLongitude();
}

void mgnMdTerrainView::LocalToWorld (double local_x, double local_y, mgnMdWorldPoint &wrld_pt) const
{
    // terrainView->getLocation() in local coordinates is always (0, 0)

    const mgnMdWorldPoint &location = getViewPoint();

    // The point in degrees (shifted)
    wrld_pt.mLatitude  = local_y / getMetersPerLatitude ();
    wrld_pt.mLongitude = local_x / getMetersPerLongitude();

    // Shift world point to currently requested center
    wrld_pt.mLatitude  += location.mLatitude ;
    wrld_pt.mLongitude += location.mLongitude;
}