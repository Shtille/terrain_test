#include "mgnTrMercatorRenderable.h"

#include "mgnTrMercatorTree.h"
#include "mgnTrMercatorNode.h"
#include "mgnTrMercatorMapTile.h"

#include "MapDrawing/Graphics/mgnCommonMath.h"

#include <cmath>
#include <algorithm>
#include <assert.h>

namespace {
    void QuadPointToPixelPoint(float msm, float qpx, float qpy, float * ppx, float * ppy)
    {
        if (ppx)
            *ppx = (qpx + 1.0f) * 0.5f * msm; // to range [0,msm]
        if (ppy)
            *ppy = (1.0f - (qpy + 1.0f) * 0.5f) * msm; // to range [0,msm]
    }
}

namespace mgn {
    namespace terrain {

    	MercatorRenderable::MercatorRenderable()
		: node_(NULL)
		, map_tile_(NULL)
		, lod_priority_(0.0f)
		, child_distance_(0.0f)
		, lod_difference_(0.0f)
		, distance_(0.0f)
		, is_in_lod_range_(false)
		, is_in_mip_range_(false)
		, is_clipped_(false)
		, is_far_away_(false)
		{
		}
		MercatorRenderable::~MercatorRenderable()
		{
			Destroy();
		}
		void MercatorRenderable::Create(MercatorNode * node, MercatorMapTile * map_tile)
		{
			node_ = node;
			map_tile_ = map_tile;
			child_distance_ = 0.0f;

			AnalyzeTerrain();
			InitDisplacementMapping();
		}
		void MercatorRenderable::Destroy()
		{
		}
		void MercatorRenderable::SetFrameOfReference()
		{
			const MercatorLodParams& params = node_->owner_->lod_params_;
			const math::Frustum * frustum = node_->owner_->frustum_;
            const float planet_radius = node_->owner_->earth_radius_;

			// Bounding box clipping.
			is_clipped_ = !frustum->IsBoxIn(bounding_box_);
			is_far_away_ = false;

			// Get vector from center to camera and normalize it.
			math::Vector3 position_offset = params.camera_position - center_;
			math::Vector3 view_direction = position_offset;

			// Find the offset between the center of the grid and the grid point closest to the camera (rough approx).
            const math::Vector3 surface_normal(UNIT_Y);
			const float reference_length = 1.41f * math::kPi * planet_radius / (float)(1 << node_->lod_);
			math::Vector3 reference_offset = view_direction - ((view_direction & surface_normal) * surface_normal);
			if (reference_offset.Sqr() > reference_length * reference_length)
			{
				reference_offset.Normalize();
				reference_offset *= reference_length;
			}

			// Find the position offset to the nearest point to the camera (approx).
			math::Vector3 near_position_offset = position_offset - reference_offset;
			float near_position_distance = near_position_offset.Length();
			math::Vector3 to_camera = near_position_offset / near_position_distance;

			// Determine LOD priority.
			lod_priority_ = -(to_camera & params.camera_front);

			is_in_lod_range_ = GetLodDistance() * params.geo_factor < near_position_distance;

			// Calculate texel resolution relative to near grid-point (approx).
			float cos_angle = to_camera.y; // tile incination angle
			float face_size = cos_angle * planet_radius * math::kPi * 2.0f; // Curved width/height of texture cube face on the sphere
			float cube_side_pixels = static_cast<float>(256 << map_tile_->GetNode()->lod_);
			float texel_size = face_size / cube_side_pixels; // Size of a single texel in world units

			is_in_mip_range_ = texel_size * params.tex_factor < near_position_distance;
		}
		const bool MercatorRenderable::IsInLODRange() const
		{
			return is_in_lod_range_;
		}
		const bool MercatorRenderable::IsClipped() const
		{
			return is_clipped_;
		}
		const bool MercatorRenderable::IsInMIPRange() const
		{
			return is_in_mip_range_;
		}
		const bool MercatorRenderable::IsFarAway() const
		{
			return is_far_away_;
		}
		float MercatorRenderable::GetLodPriority() const
		{
			return lod_priority_;
		}
		float MercatorRenderable::GetLodDistance()
		{
			if (distance_ > child_distance_)
				return distance_;
			else
				return child_distance_;
		}
		void MercatorRenderable::SetChildLodDistance(float lod_distance)
		{
			child_distance_ = lod_distance;
		}
		MercatorMapTile * MercatorRenderable::GetMapTile()
		{
			return map_tile_;
		}
		void MercatorRenderable::AnalyzeTerrain()
		{
			const int grid_size = node_->owner_->grid_size();
            const float fMapSizeMax = node_->owner_->GetMapSizeMax();

			// Calculate scales, offsets for tile position on cube face.
			const float inv_scale = 2.0f / (float)(1 << node_->lod_);
			const float position_x = -1.f + inv_scale * node_->x_;
			const float position_y = -1.f + inv_scale * node_->y_;

			// Keep track of extents.
			math::Vector3 min = math::Vector3(fMapSizeMax), max = math::Vector3(-fMapSizeMax);
			center_.Set(0.0f, 0.0f, 0.0f);

			// Lossy representation of heightmap
			//const int offsetX2 = offsetX * 2;
		 //   const int offsetY2 = offsetY * 2;
		 //   float diff = 0;
		 //   for (int j = 0; j < (grid_size - 1); j += 2) {
		 //       HeightMapPixel* pMapRow = &pMap[(pixelY + j * stepY) * mMap->getWidth() + pixelX];
		 //       for (int i = 0; i < (grid_size - 1); i += 2) {
		 //           // dx
		 //           diff = maxf(diff, fabs((getOffsetPixel(0) + getOffsetPixel(offsetX2)) / 2.0f - getOffsetPixel(offsetX)));
		 //           diff = maxf(diff, fabs((getOffsetPixel(offsetY2) + getOffsetPixel(offsetY2 + offsetX2)) / 2.0f - getOffsetPixel(offsetY + offsetX)));
		 //           // dy
		 //           diff = maxf(diff, fabs((getOffsetPixel(0) + getOffsetPixel(offsetY2)) / 2.0f - getOffsetPixel(offsetY)));
		 //           diff = maxf(diff, fabs((getOffsetPixel(offsetX2) + getOffsetPixel(offsetX2 + offsetY2)) / 2.0f - getOffsetPixel(offsetX + offsetY)));
		 //           // diag
		 //           diff = maxf(diff, fabs((getOffsetPixel(offsetX2) + getOffsetPixel(offsetY2)) / 2.0f - getOffsetPixel(offsetY + offsetX)));
		 //           
		 //           pMapRow += offsetX2;
		 //       }
		 //   }
		    lod_difference_ = 0.0f;//diff / PlanetMapBuffer::LEVEL_RANGE;
		    distance_ = 0.0f;//lod_difference_ * mPlanetHeight;

			// Process vertex data for regular grid.
			for (int j = 0; j < grid_size; ++j)
			{
				for (int i = 0; i < grid_size; ++i)
				{
					float x = (float)i / (float)(grid_size - 1);
					float y = (float)j / (float)(grid_size - 1);

					math::Vector2 quad_point(x * inv_scale + position_x, y * inv_scale + position_y);
                    math::Vector3 vertex;
                    QuadPointToPixelPoint(fMapSizeMax, quad_point.x, quad_point.y,
                        &vertex.x, &vertex.z);
                    vertex.y = 0.0f; // height

					center_ += vertex;

					min.MakeFloor(vertex);
					max.MakeCeil(vertex);
				}
			}

			// Calculate center.
			center_ /= (float)(grid_size * grid_size);

			// Set bounding box
			bounding_box_.center = 0.5f * (max + min);
			bounding_box_.extent = 0.5f * (max - min);
		}
		void MercatorRenderable::InitDisplacementMapping()
		{
			// Calculate scales, offsets for tile position on cube face.
			const float inv_scale = 2.0f / (1 << node_->lod_);
			const float position_x = -1.f + inv_scale * node_->x_;
			const float position_y = -1.f + inv_scale * node_->y_;

			// Correct for GL texture mapping at borders.
			const float uv_correction = 0.0f; //.05f / (mMap->getWidth() + 1);

			// Calculate scales, offset for tile position in map tile.
			int relative_lod = node_->lod_ - map_tile_->GetNode()->lod_;
			const float inv_tex_scale = 1.0f / (1 << relative_lod) * (1.0f - uv_correction);
			const float texture_x = inv_tex_scale * (node_->x_ - (map_tile_->GetNode()->x_ << relative_lod)) + uv_correction;
			const float texture_y = inv_tex_scale * (node_->y_ - (map_tile_->GetNode()->y_ << relative_lod)) + uv_correction;

			// Set shader parameters
			stuv_scale_.x = inv_scale;
			stuv_scale_.y = inv_scale;
			stuv_scale_.z = inv_tex_scale;
			stuv_scale_.w = inv_tex_scale;
			stuv_position_.x = position_x;
			stuv_position_.y = position_y;
			stuv_position_.z = texture_x;
			stuv_position_.w = texture_y;

#ifdef DEBUG
			// Set color/tint
			color_.x = cosf(map_tile_->GetNode()->lod_ * 0.70f) * .35f + .85f;
			color_.y = cosf(map_tile_->GetNode()->lod_ * 1.71f) * .35f + .85f;
			color_.z = cosf(map_tile_->GetNode()->lod_ * 2.64f) * .35f + .85f;
			color_.w = 1.0f;
#endif
		}

    } // namespace terrain
} // namespace mgn