#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_RENDERABLE_H__
#define __MGN_TERRAIN_MERCATOR_RENDERABLE_H__

#include "Frustum.h"

namespace mgn {
    namespace terrain {

    	// Forward declarations
    	class MercatorNode;
		class MercatorMapTile;

		//! Mercator renderable class
		class MercatorRenderable {
		public:
			MercatorRenderable(MercatorNode * node, MercatorMapTile * map_tile);
			~MercatorRenderable();

			void SetFrameOfReference();

			const bool IsInLODRange() const;
			const bool IsClipped() const;
			const bool IsInMIPRange() const;
			const bool IsFarAway() const;
			float GetLodPriority() const;
			float GetLodDistance();
			void SetChildLodDistance(float lod_distance);

			MercatorMapTile * GetMapTile();

			// Additional params to map to shader
			math::Vector4 stuv_scale_;
			math::Vector4 stuv_position_;
			math::Vector4 color_;
			float distance_;

		private:
			void AnalyzeTerrain();
			void InitDisplacementMapping();

			const MercatorNode * node_;
			MercatorMapTile* map_tile_;

			math::BoundingBox bounding_box_; //!< bounding box in world coordinates

			math::Vector3 center_;

			float lod_priority_; //!< priority for nodes queue processing
			float child_distance_;
			float lod_difference_;

			bool is_in_lod_range_;
			bool is_in_mip_range_;
			bool is_clipped_;
			bool is_far_away_;
		};

    } // namespace terrain
} // namespace mgn

#endif