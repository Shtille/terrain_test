#include "mgnMdTerrainView.h"
#include "mgnTrConstants.h"
#include "mgnTrMercatorUtils.h"

#include <math.h>
#include <algorithm>

#include "mgnMath.h"
#include "mgnLog.h"
#include "../src/mgnMdUtils/mgnMdMapScale.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

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
  mIs3D(false),
  mPixelScale(1.0f),
  mZNear(1.0f),
  mZFar(1000.0f),
  mFovX(PI*0.4f),
  mLod(5),
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
int mgnMdTerrainView::GetMapScaleIndex(int level_of_detail) const
{
    return MdMapScale(GetMapScaleFactor(level_of_detail)).GetScale();
}
float mgnMdTerrainView::GetMapScaleFactor(int level_of_detail) const
{
    return static_cast<float>(Mercator::GetNativeScale(level_of_detail));
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
void mgnMdTerrainView::updateLod()
{
    const int kTileResolution = mgn::terrain::GetTileResolution();
    const int kMaxLod = mgn::terrain::GetMaxLod();
    const double kScreenWidth = 500.0;
    const double kInvLog2 = 1.442695040888963; // 1/ln(2)
    double ex = (20037508.34 / mCamDistance) * (kScreenWidth / static_cast<double>(kTileResolution));
    double log_value = log(ex) * kInvLog2;
    mLod = static_cast<int>(log_value) + 1; // ~ ceil(log_value)
    if (mLod < 0) mLod = 0;
    if (mLod > kMaxLod) mLod = kMaxLod;
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
int mgnMdTerrainView::GetLod() const
{
    return mLod;
}
void mgnMdTerrainView::Set3D(bool is3D)
{
    mIs3D = is3D;
}
bool mgnMdTerrainView::Is3D() const
{
    return mIs3D;
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
    updateLod();
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
void mgnMdTerrainView::LocalToPixelDistance(float local_x, float& pixel_x, float map_size_max) const
{
    double longitude = (double)local_x / 111111.0;
    pixel_x = static_cast<float>(longitude / 360.0 * (double)map_size_max);
}
void mgnMdTerrainView::PixelToLocalDistance(float pixel_x, float& local_x, float map_size_max) const
{
    float longitude = (pixel_x / map_size_max) * 360.0f;
    local_x = longitude * 111111.0f;
}
void mgnMdTerrainView::LocalToPixel(const vec3& local, vec3& pixel, float map_size_max) const
{
    double longitude = (double)local.x / 111111.0 + mLocation.mLongitude;
    double x = (longitude + 180.0) / 360.0;

    double latitude = (double)local.z / 111111.0 + mLocation.mLatitude;
    double sin_latitude = sin(latitude * (double)math::kPi / 180.0);
    double y = 0.5 - log((1.0 + sin_latitude) / (1.0 - sin_latitude)) / (4.0 * (double)math::kPi);

    pixel.x = x * (double)map_size_max; // we want East to be in positive direction of X axis
    pixel.y = local.y / 360.0 * (double)map_size_max / 111111.0;
    pixel.z = (1.0 - y) * (double)map_size_max; // we want North to be in positive direction of Z axis
}
void mgnMdTerrainView::PixelLocation(vec3& pixel, float map_size_max) const
{
    float x = ((float)mLocation.mLongitude + 180.0f) / 360.0f;

    float sin_latitude = sin((float)mLocation.mLatitude * math::kPi / 180.0f);
    float y = 0.5f - log((1.0f + sin_latitude) / (1.0f - sin_latitude)) / (4.0f * math::kPi);

    pixel.x = x * map_size_max; // we want East to be in positive direction of X axis
    pixel.y = mCenterHeight / 360.0f * map_size_max / 111111.0f;
    pixel.z = (1.0f - y) * map_size_max; // we want North to be in positive direction of Z axis
}
void mgnMdTerrainView::WorldToPixel(double latitude, double longitude, double altitude, vec3& pixel, float map_size_max) const
{
    /*
    float x = ((float)longitude + 180.0f) / 360.0f;

    float sin_latitude = sin((float)latitude * (math::kPi / 180.0f));
    float y = 0.5f - log((1.0f + sin_latitude) / (1.0f - sin_latitude)) / (4.0f * math::kPi);

    pixel.x = x * map_size_max; // we want East to be in positive direction of X axis
    pixel.y = ((float)altitude) / 360.0f * map_size_max / 111111.0f;
    pixel.z = (1.0f - y) * map_size_max; // we want North to be in positive direction of Z axis
    */
    double x = (longitude + 180.0) / 360.0;

    double sin_latitude = sin(latitude * ((double)math::kPi / 180.0));
    double y = 0.5 - log((1.0 + sin_latitude) / (1.0 - sin_latitude)) / (4.0 * (double)math::kPi);

    pixel.x = (double)(x * (double)map_size_max); // we want East to be in positive direction of X axis
    pixel.y = (double)(altitude / 360.0 * (double)map_size_max / 111111.0);
    pixel.z = (double)((1.0 - y) * (double)map_size_max); // we want North to be in positive direction of Z axis
}
void mgnMdTerrainView::PixelToWorld(float pixel_x, float pixel_y, mgnMdWorldPoint& world_point, float map_size_max) const
{
    /*
    float x = pixel_x / map_size_max;
    float y = pixel_y / map_size_max - 0.5f; // 0.5 - y_original

    world_point.mLongitude = static_cast<double>(x * 360.0f - 180.0f);
    world_point.mLatitude = static_cast<double>(90.0f - 360.0f * atan(exp(-y * 2.0f * math::kPi)) / math::kPi);
    */
    double x = (double)pixel_x / (double)map_size_max;
    double y = (double)pixel_y / (double)map_size_max - 0.5; // 0.5 - y_original

    world_point.mLongitude = x * 360.0 - 180.0;
    world_point.mLatitude = 90.0 - 360.0 * atan(exp(-y * 2.0 * (double)math::kPi)) / (double)math::kPi;
}
void mgnMdTerrainView::PixelToLocal(const vec3& pixel, vec3& local, float map_size_max) const
{
    double x = pixel.x / map_size_max;
    double y = (double)pixel.z / (double)map_size_max - 0.5; // 0.5 - y_original

    double longitude = x * 360.0 - 180.0;
    double latitude = 90.0 - 360.0 * atan(exp(-y * 2.0 * (double)math::kPi)) / (double)math::kPi;
    local.x = (longitude - mLocation.mLongitude) * 111111.0;
    local.y = pixel.y / map_size_max * 360.0f * 111111.f;
    local.z = (latitude - mLocation.mLatitude) * 111111.0;
}
void mgnMdTerrainView::IntersectionWithRay(const vec3& ray, vec3& intersection) const
{
    const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
    if (!mTerrainProvider ||
        getCamTiltRad() > 1.0 && // High altitudes, use approximate approach
        ray.y < 0.0f)
    {
        vec3 origin = getCamPosition();
        float d = -(float)getCenterHeight();
        float t = -(origin.y + d)/ray.y;
        intersection = origin + t * ray;
        return;
    }
    mgnMdWorldPoint world_point;
    float distance = getLargestCamDistance();
    vec3 left_point = getCamPosition();
    vec3 right_point = left_point + distance * ray;
    LocalToWorld(left_point.x, left_point.z, world_point);
    float left_height = (float)mTerrainProvider->getAltitude(world_point.mLatitude, world_point.mLongitude);
    LocalToWorld(right_point.x, right_point.z, world_point);
    float right_height = (float)mTerrainProvider->getAltitude(world_point.mLatitude, world_point.mLongitude);
    if ((left_point.y - left_height)*(right_point.y - right_height) < 0.0f)
    {
        const float kMinimumDistance = kMSM / 111111.0f / 360.0f; // = LocalToPixelDistance(1.0f,...)
        do
        {
            vec3 center_point = 0.5f*(left_point + right_point);
            LocalToWorld(center_point.x, center_point.z, world_point);
            float center_height = (float)mTerrainProvider->getAltitude(world_point.mLatitude, world_point.mLongitude);
            if ((left_point.y - left_height)*(center_point.y - center_height) < 0.0f)
            {
                right_point = center_point;
                right_height = center_height;
            }
            else
            {
                left_point = center_point;
                left_height = center_height;
            }
            distance *= 0.5f; // equal to / 2
        }
        while (distance > 1.0f);

        // We don't need an accurate value, so just take middle
        intersection = 0.5f*(left_point + right_point);
    }
    else // no possible intersection
    {
        distance = (float)getCamDistance();
        intersection = left_point + distance * ray;
    }
}
void mgnMdTerrainView::IntersectionWithRayPixel(const vec3& ray, vec3& intersection) const
{
    const float kMSM = static_cast<float>(mgn::terrain::GetMapSizeMax());
    //if (!mTerrainProvider ||
    //    getCamTiltRad() > 1.0 && // High altitudes, use approximate approach
    //    ray.y < 0.0f)
    {
        /*
        vec3 origin = getCamPosition();
        LocalToPixel(origin, origin, kMSM);
        float d = -(float)getCenterHeight();
        LocalToPixelDistance(d, d, kMSM);
        float t = -(origin.y + d)/ray.y;
        intersection = origin + t * ray;
        */
        vec3 origin = getCamPosition();
        LocalToPixel(origin, origin, kMSM);
        float d = -(float)getCenterHeight();
        LocalToPixelDistance(d, d, kMSM);
        double t = -((double)origin.y + (double)d)/(double)ray.y;
        intersection.x = origin.x + static_cast<float>(t * ray.x);
        intersection.y = origin.y + static_cast<float>(t * ray.y);
        intersection.z = origin.z + static_cast<float>(t * ray.z);
        return;
    }
    mgnMdWorldPoint world_point;
    float distance = getLargestCamDistance();
    LocalToPixelDistance(distance, distance, kMSM);
    vec3 left_point = getCamPosition();
    LocalToPixel(left_point, left_point, kMSM);
    vec3 right_point = left_point + distance * ray;
    PixelToWorld(left_point.x, left_point.z, world_point, kMSM);
    float left_height = (float)mTerrainProvider->getAltitude(world_point.mLatitude, world_point.mLongitude);
    LocalToPixelDistance(left_height, left_height, kMSM);
    PixelToWorld(right_point.x, right_point.z, world_point, kMSM);
    float right_height = (float)mTerrainProvider->getAltitude(world_point.mLatitude, world_point.mLongitude);
    LocalToPixelDistance(right_height, right_height, kMSM);
    if ((left_point.y - left_height)*(right_point.y - right_height) < 0.0f)
    {
        const float kMinimumDistance = kMSM / 111111.0f / 360.0f; // = LocalToPixelDistance(1.0f,...)
        do
        {
            vec3 center_point = 0.5f*(left_point + right_point);
            PixelToWorld(center_point.x, center_point.z, world_point, kMSM);
            float center_height = (float)mTerrainProvider->getAltitude(world_point.mLatitude, world_point.mLongitude);
            LocalToPixelDistance(center_height, center_height, kMSM);
            if ((left_point.y - left_height)*(center_point.y - center_height) < 0.0f)
            {
                right_point = center_point;
                right_height = center_height;
            }
            else
            {
                left_point = center_point;
                left_height = center_height;
            }
            distance *= 0.5f; // equal to / 2
        }
        while (distance > kMinimumDistance);

        // We don't need an accurate value, so just take middle
        intersection = 0.5f*(left_point + right_point);
    }
    else // no possible intersection
    {
        distance = (float)getCamDistance();
        LocalToPixelDistance(distance, distance, kMSM);
        intersection = left_point + distance * ray;
    }
}