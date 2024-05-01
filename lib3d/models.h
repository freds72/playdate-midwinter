
#ifndef _models_h
#define _models_h

// DO NOT EDIT - GENERATED CODE 

#include "3dmath.h"

// models ID
#define PROP_TREE0	1
#define PROP_TREE1	2
#define PROP_TREE_SNOW	3
#define PROP_CHECKPOINT_LEFT	4
#define PROP_CHECKPOINT_RIGHT	5

#define NEXT_PROP_ID 6

#define FACE_FLAG_TRANSPARENT 0x01
#define FACE_FLAG_EDGES       0x02
#define FACE_FLAG_QUAD        0x04
#define FACE_FLAG_DUALSIDED   0x08

typedef struct {
    // face properties (see bits above)
    int flags;
    // visible edges mask
    int edges;
    // 0: black - 15: white
    int material;
    // pre-computed normal.point
    float cp;
    // normal (useless?)
    Point3d n;
    // vertices (direct points)
    Point3d vertices[4];
} ThreeDFace;

typedef struct {
    int face_count;
    ThreeDFace* faces;
} ThreeDModel;

// faces

// tree0 face properties & coordinates
static ThreeDFace tree0_faces[8] = { { 
    .n = { .v = {-0.6754f,0.0787f,-0.7333f} },
    .cp = 0.5172f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {0.0048f,0.0010f,-0.7097f} },{ .v = {-0.0039f,1.9010f,-0.4978f} },{ .v = {-0.5188f,1.9010f,-0.0236f} },{ .v = {-0.7307f,0.0010f,-0.0323f} } }
    },{ 
    .n = { .v = {-0.7333f,0.0787f,0.6754f} },
    .cp = 0.5141f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {-0.7307f,0.0010f,-0.0323f} },{ .v = {-0.5188f,1.9010f,-0.0236f} },{ .v = {-0.0445f,1.9010f,0.4913f} },{ .v = {-0.0532f,0.0010f,0.7033f} } }
    },{ 
    .n = { .v = {0.7333f,0.0787f,-0.6754f} },
    .cp = 0.4830f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {0.6823f,0.0010f,0.0258f} },{ .v = {0.4703f,1.9010f,0.0171f} },{ .v = {-0.0039f,1.9010f,-0.4978f} },{ .v = {0.0048f,0.0010f,-0.7097f} } }
    },{ 
    .n = { .v = {0.6754f,0.0787f,0.7333f} },
    .cp = 0.4798f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {-0.0532f,0.0010f,0.7033f} },{ .v = {-0.0445f,1.9010f,0.4913f} },{ .v = {0.4703f,1.9010f,0.0171f} },{ .v = {0.6823f,0.0010f,0.0258f} } }
    },{ 
    .n = { .v = {0.9820f,0.1614f,0.0984f} },
    .cp = 1.8611f, 
    .flags = 0,
    .edges = 0x03,
    .material=12,
    .vertices={ { .v = {1.2966f,1.9010f,2.8565f} },{ .v = {0.0082f,11.4811f,0.0002f} },{ .v = {1.8377f,1.9010f,-2.5438f} } }
    },{ 
    .n = { .v = {0.0000f,-1.0000f,0.0000f} },
    .cp = -1.9010f, 
    .flags = 0,
    .edges = 0x00,
    .material=15,
    .vertices={ { .v = {1.2966f,1.9010f,2.8565f} },{ .v = {1.8377f,1.9010f,-2.5438f} },{ .v = {-3.1096f,1.9010f,-0.3123f} } }
    },{ 
    .n = { .v = {-0.4058f,0.1614f,-0.8996f} },
    .cp = 1.8495f, 
    .flags = 0,
    .edges = 0x03,
    .material=12,
    .vertices={ { .v = {1.8377f,1.9010f,-2.5438f} },{ .v = {0.0082f,11.4811f,0.0002f} },{ .v = {-3.1096f,1.9010f,-0.3123f} } }
    },{ 
    .n = { .v = {-0.5762f,0.1614f,0.8012f} },
    .cp = 1.8484f, 
    .flags = 0,
    .edges = 0x03,
    .material=12,
    .vertices={ { .v = {-3.1096f,1.9010f,-0.3123f} },{ .v = {0.0082f,11.4811f,0.0002f} },{ .v = {1.2966f,1.9010f,2.8565f} } }
    } };

// tree1 face properties & coordinates
static ThreeDFace tree1_faces[8] = { { 
    .n = { .v = {0.0000f,0.0609f,-0.9981f} },
    .cp = 0.4689f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {0.4833f,-0.2222f,-0.4833f} },{ .v = {0.3500f,1.9645f,-0.3500f} },{ .v = {-0.3500f,1.9645f,-0.3500f} },{ .v = {-0.4529f,0.2761f,-0.4529f} } }
    },{ 
    .n = { .v = {-0.9981f,0.0609f,0.0000f} },
    .cp = 0.4689f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {-0.4529f,0.2761f,-0.4529f} },{ .v = {-0.3500f,1.9645f,-0.3500f} },{ .v = {-0.3500f,1.9645f,0.3500f} },{ .v = {-0.4626f,0.1177f,0.4626f} } }
    },{ 
    .n = { .v = {0.9981f,0.0609f,-0.0000f} },
    .cp = 0.4689f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {0.4943f,-0.4028f,0.4943f} },{ .v = {0.3500f,1.9645f,0.3500f} },{ .v = {0.3500f,1.9645f,-0.3500f} },{ .v = {0.4833f,-0.2222f,-0.4833f} } }
    },{ 
    .n = { .v = {-0.0000f,0.0609f,0.9981f} },
    .cp = 0.4689f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {-0.4626f,0.1177f,0.4626f} },{ .v = {-0.3500f,1.9645f,0.3500f} },{ .v = {0.3500f,1.9645f,0.3500f} },{ .v = {0.4943f,-0.4028f,0.4943f} } }
    },{ 
    .n = { .v = {0.6263f,0.2392f,0.7420f} },
    .cp = 1.9550f, 
    .flags = 0,
    .edges = 0x03,
    .material=12,
    .vertices={ { .v = {-0.9453f,1.5399f,2.9363f} },{ .v = {0.0215f,8.0400f,0.0245f} },{ .v = {3.0907f,1.9645f,-0.6073f} } }
    },{ 
    .n = { .v = {0.0287f,-0.9958f,-0.0866f} },
    .cp = -1.8149f, 
    .flags = 0,
    .edges = 0x00,
    .material=15,
    .vertices={ { .v = {-0.9453f,1.5399f,2.9363f} },{ .v = {3.0907f,1.9645f,-0.6073f} },{ .v = {-2.0601f,1.9645f,-2.3176f} } }
    },{ 
    .n = { .v = {0.3052f,0.2497f,-0.9190f} },
    .cp = 1.9917f, 
    .flags = 0,
    .edges = 0x03,
    .material=12,
    .vertices={ { .v = {3.0907f,1.9645f,-0.6073f} },{ .v = {0.0215f,8.0400f,0.0245f} },{ .v = {-2.0601f,1.9645f,-2.3176f} } }
    },{ 
    .n = { .v = {-0.9457f,0.2392f,0.2200f} },
    .cp = 1.9084f, 
    .flags = 0,
    .edges = 0x03,
    .material=12,
    .vertices={ { .v = {-2.0601f,1.9645f,-2.3176f} },{ .v = {0.0215f,8.0400f,0.0245f} },{ .v = {-0.9453f,1.5399f,2.9363f} } }
    } };

// tree_snow face properties & coordinates
static ThreeDFace tree_snow_faces[11] = { { 
    .n = { .v = {0.7123f,0.0787f,-0.6974f} },
    .cp = 0.4964f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {0.7040f,-0.0032f,0.0070f} },{ .v = {0.4919f,1.8968f,0.0047f} },{ .v = {0.0022f,1.8968f,-0.4954f} },{ .v = {0.0044f,-0.0032f,-0.7076f} } }
    },{ 
    .n = { .v = {-0.6974f,0.0787f,-0.7123f} },
    .cp = 0.5007f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {0.0044f,-0.0032f,-0.7076f} },{ .v = {0.0022f,1.8968f,-0.4954f} },{ .v = {-0.4980f,1.8968f,-0.0057f} },{ .v = {-0.7101f,-0.0032f,-0.0080f} } }
    },{ 
    .n = { .v = {0.6974f,0.0787f,0.7123f} },
    .cp = 0.4957f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {-0.0105f,-0.0032f,0.7066f} },{ .v = {-0.0083f,1.8968f,0.4944f} },{ .v = {0.4919f,1.8968f,0.0047f} },{ .v = {0.7040f,-0.0032f,0.0070f} } }
    },{ 
    .n = { .v = {-0.7123f,0.0787f,0.6974f} },
    .cp = 0.5000f, 
    .flags = 4,
    .edges = 0x00,
    .material=14,
    .vertices={ { .v = {-0.7101f,-0.0032f,-0.0080f} },{ .v = {-0.4980f,1.8968f,-0.0057f} },{ .v = {-0.0083f,1.8968f,0.4944f} },{ .v = {-0.0105f,-0.0032f,0.7066f} } }
    },{ 
    .n = { .v = {-0.0679f,0.2031f,0.9768f} },
    .cp = 1.9509f, 
    .flags = 4,
    .edges = 0x05,
    .material=12,
    .vertices={ { .v = {-2.8212f,1.8968f,1.4068f} },{ .v = {-1.0539f,6.6386f,0.5440f} },{ .v = {1.4377f,5.2558f,1.0046f} },{ .v = {2.5931f,1.8968f,1.7831f} } }
    },{ 
    .n = { .v = {-0.0118f,-0.9855f,0.1695f} },
    .cp = -1.5975f, 
    .flags = 0,
    .edges = 0x00,
    .material=15,
    .vertices={ { .v = {-2.8212f,1.8968f,1.4068f} },{ .v = {2.5931f,1.8968f,1.7831f} },{ .v = {0.1233f,1.0563f,-3.2752f} } }
    },{ 
    .n = { .v = {0.8732f,0.1858f,-0.4505f} },
    .cp = 1.8134f, 
    .flags = 4,
    .edges = 0x05,
    .material=12,
    .vertices={ { .v = {2.5931f,1.8968f,1.7831f} },{ .v = {1.4377f,5.2558f,1.0046f} },{ .v = {0.0797f,6.4914f,-1.1929f} },{ .v = {0.1233f,1.0563f,-3.2752f} } }
    },{ 
    .n = { .v = {-0.8120f,0.2031f,-0.5472f} },
    .cp = 1.9064f, 
    .flags = 4,
    .edges = 0x05,
    .material=12,
    .vertices={ { .v = {0.1233f,1.0563f,-3.2752f} },{ .v = {0.0797f,6.4914f,-1.1929f} },{ .v = {-1.0539f,6.6386f,0.5440f} },{ .v = {-2.8212f,1.8968f,1.4068f} } }
    },{ 
    .n = { .v = {0.8589f,0.0355f,-0.5109f} },
    .cp = 0.9081f, 
    .flags = 0,
    .edges = 0x00,
    .material=3,
    .vertices={ { .v = {0.0797f,6.4914f,-1.1929f} },{ .v = {1.4377f,5.2558f,1.0046f} },{ .v = {0.7750f,9.3510f,0.1747f} } }
    },{ 
    .n = { .v = {-0.0779f,0.1859f,0.9795f} },
    .cp = 1.8489f, 
    .flags = 0,
    .edges = 0x00,
    .material=3,
    .vertices={ { .v = {1.4377f,5.2558f,1.0046f} },{ .v = {-1.0539f,6.6386f,0.5440f} },{ .v = {0.7750f,9.3510f,0.1747f} } }
    },{ 
    .n = { .v = {-0.7397f,0.4281f,-0.5191f} },
    .cp = 3.3395f, 
    .flags = 0,
    .edges = 0x00,
    .material=3,
    .vertices={ { .v = {-1.0539f,6.6386f,0.5440f} },{ .v = {0.0797f,6.4914f,-1.1929f} },{ .v = {0.7750f,9.3510f,0.1747f} } }
    } };

// checkpoint_left face properties & coordinates
static ThreeDFace checkpoint_left_faces[7] = { { 
    .n = { .v = {-0.7071f,0.0000f,-0.7071f} },
    .cp = 0.1000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {0.0000f,0.0000f,-0.1414f} },{ .v = {0.0000f,3.0000f,-0.1414f} },{ .v = {-0.1414f,3.0000f,0.0000f} },{ .v = {-0.1414f,0.0000f,0.0000f} } }
    },{ 
    .n = { .v = {-0.7071f,0.0000f,0.7071f} },
    .cp = 0.1000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {-0.1414f,0.0000f,0.0000f} },{ .v = {-0.1414f,3.0000f,0.0000f} },{ .v = {0.0000f,3.0000f,0.1414f} },{ .v = {0.0000f,0.0000f,0.1414f} } }
    },{ 
    .n = { .v = {0.7071f,0.0000f,0.7071f} },
    .cp = 0.1000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {0.0000f,0.0000f,0.1414f} },{ .v = {0.0000f,3.0000f,0.1414f} },{ .v = {0.1414f,3.0000f,0.0000f} },{ .v = {0.1414f,0.0000f,0.0000f} } }
    },{ 
    .n = { .v = {0.7071f,0.0000f,-0.7071f} },
    .cp = 0.1000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {0.1414f,0.0000f,0.0000f} },{ .v = {0.1414f,3.0000f,0.0000f} },{ .v = {0.0000f,3.0000f,-0.1414f} },{ .v = {0.0000f,0.0000f,-0.1414f} } }
    },{ 
    .n = { .v = {0.0000f,1.0000f,-0.0000f} },
    .cp = 3.0000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {0.0000f,3.0000f,0.1414f} },{ .v = {-0.1414f,3.0000f,0.0000f} },{ .v = {0.0000f,3.0000f,-0.1414f} },{ .v = {0.1414f,3.0000f,0.0000f} } }
    },{ 
    .n = { .v = {-0.0000f,0.0000f,1.0000f} },
    .cp = 0.0000f, 
    .flags = 0,
    .edges = 0x07,
    .material=15,
    .vertices={ { .v = {0.1417f,2.8046f,-0.0000f} },{ .v = {0.8748f,2.4644f,-0.0000f} },{ .v = {0.1417f,2.1084f,-0.0000f} } }
    },{ 
        .n = { .v = {0.0000f,-0.0000f,-1.0000f} },
        .cp = -0.0000f, 
        .flags = 0,
        .edges = 0x07,
        .material=15,
        .vertices={ { .v = {0.1417f,2.1084f,-0.0000f} },{ .v = {0.8748f,2.4644f,-0.0000f} },{ .v = {0.1417f,2.8046f,-0.0000f} } }
        } };

// checkpoint_right face properties & coordinates
static ThreeDFace checkpoint_right_faces[7] = { { 
    .n = { .v = {0.7071f,0.0000f,0.7071f} },
    .cp = 0.1000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {0.0000f,0.0000f,0.1414f} },{ .v = {0.0000f,3.0000f,0.1414f} },{ .v = {0.1414f,3.0000f,-0.0000f} },{ .v = {0.1414f,0.0000f,-0.0000f} } }
    },{ 
    .n = { .v = {0.7071f,0.0000f,-0.7071f} },
    .cp = 0.1000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {0.1414f,0.0000f,-0.0000f} },{ .v = {0.1414f,3.0000f,-0.0000f} },{ .v = {0.0000f,3.0000f,-0.1414f} },{ .v = {0.0000f,0.0000f,-0.1414f} } }
    },{ 
    .n = { .v = {-0.7071f,0.0000f,-0.7071f} },
    .cp = 0.1000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {0.0000f,0.0000f,-0.1414f} },{ .v = {0.0000f,3.0000f,-0.1414f} },{ .v = {-0.1414f,3.0000f,0.0000f} },{ .v = {-0.1414f,0.0000f,0.0000f} } }
    },{ 
    .n = { .v = {-0.7071f,0.0000f,0.7071f} },
    .cp = 0.1000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {-0.1414f,0.0000f,0.0000f} },{ .v = {-0.1414f,3.0000f,0.0000f} },{ .v = {0.0000f,3.0000f,0.1414f} },{ .v = {0.0000f,0.0000f,0.1414f} } }
    },{ 
    .n = { .v = {0.0000f,1.0000f,0.0000f} },
    .cp = 3.0000f, 
    .flags = 4,
    .edges = 0x00,
    .material=13,
    .vertices={ { .v = {0.0000f,3.0000f,-0.1414f} },{ .v = {0.1414f,3.0000f,-0.0000f} },{ .v = {0.0000f,3.0000f,0.1414f} },{ .v = {-0.1414f,3.0000f,0.0000f} } }
    },{ 
    .n = { .v = {-0.0000f,0.0000f,-1.0000f} },
    .cp = 0.0000f, 
    .flags = 0,
    .edges = 0x07,
    .material=15,
    .vertices={ { .v = {-0.1417f,2.8046f,0.0000f} },{ .v = {-0.8748f,2.4644f,0.0000f} },{ .v = {-0.1417f,2.1084f,0.0000f} } }
    },{ 
        .n = { .v = {0.0000f,-0.0000f,1.0000f} },
        .cp = -0.0000f, 
        .flags = 0,
        .edges = 0x07,
        .material=15,
        .vertices={ { .v = {-0.1417f,2.1084f,0.0000f} },{ .v = {-0.8748f,2.4644f,0.0000f} },{ .v = {-0.1417f,2.8046f,0.0000f} } }
        } };


// models
static ThreeDModel three_d_models[5]={
    { .face_count = 8, .faces = &tree0_faces },{ .face_count = 8, .faces = &tree1_faces },{ .face_count = 11, .faces = &tree_snow_faces },{ .face_count = 7, .faces = &checkpoint_left_faces },{ .face_count = 7, .faces = &checkpoint_right_faces }  
};

#endif // _models_h
