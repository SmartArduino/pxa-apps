#ifndef VOXEL_CRAFT_SKY_H
#define VOXEL_CRAFT_SKY_H

#define VOXEL_SKY_SUN_X 0.25F
#define VOXEL_SKY_SUN_Y 0.36F
#define VOXEL_SKY_SUN_Z 0.90F
#define VOXEL_SKY_CLOUD_COUNT 8

/* Horizontal directions stay fixed in the world as the camera turns. */
static const float kVoxelSkyCloudDirections[VOXEL_SKY_CLOUD_COUNT][2] = {
    {0.0F, 1.0F}, {0.7F, 0.7F}, {1.0F, 0.0F}, {0.7F, -0.7F},
    {0.0F, -1.0F}, {-0.7F, -0.7F}, {-1.0F, 0.0F}, {-0.7F, 0.7F},
};

#endif
