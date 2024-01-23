//
//  realloc.c
//  Extension
//
//  Created by Dave Hayden on 10/20/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include "realloc.h"

void* (*lib3d_realloc)(void* ptr, size_t size);

void lib3d_setRealloc(void* (*realloc)(void* ptr, size_t size))
{
	lib3d_realloc = realloc;
}
