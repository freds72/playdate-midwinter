#ifndef _ground_limits_h
#define _ground_limits_h

// must be 32 as visibility is packed into a uint32
#define GROUND_WIDTH 32
// 48: high cpu usage on rev A
#define GROUND_HEIGHT 40
#define GROUND_CELL_SIZE 4
#define MAX_TILE_DIST (GROUND_HEIGHT/2)
#define Z_NEAR 0.5f
#define Z_FAR ((0.707f * MAX_TILE_DIST * GROUND_CELL_SIZE) - 1)

#endif