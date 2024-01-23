//
//  realloc.h
//  Extension
//
//  Created by Dave Hayden on 10/20/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#ifndef realloc_h
#define realloc_h

#include <stddef.h>

extern void* (*lib3d_realloc)(void* ptr, size_t size);

#define lib3d_malloc(s) lib3d_realloc(NULL, (s))
#define lib3d_free(ptr) lib3d_realloc((ptr), 0)

void lib3d_setRealloc(void* (*realloc)(void* ptr, size_t size));

#endif /* realloc_h */
