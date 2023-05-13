
/**
 * gltf_import.h
 */

#ifndef GRAPHICS_UTILS_GLTF_IMPORT_H
#define GRAPHICS_UTILS_GLTF_IMPORT_H

#include "data_structures.h"

typedef struct {
	
	int placeholder;
} gltf;

int gltf_load(gltf* t_fbx, const char* t_string);

void gltf_final(gltf* t_fbx);

#endif