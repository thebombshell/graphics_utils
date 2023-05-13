
/**
 * fbx_import.h
 */

#ifndef GRAPHICS_UTILS_FBX_IMPORT_H
#define GRAPHICS_UTILS_FBX_IMPORT_H

#include "data_structures.h"

typedef struct
{
	char magic_string[21];
	char magic_number[2];
	unsigned int version_number;
} fbx_header;

typedef struct
{
	unsigned int end_offset;
	unsigned int num_properties;
	unsigned int property_list_length;
	unsigned char name_length;
} fbx_node_record_header;

typedef struct
{
	char typecode;
	buffer data;
} fbx_property;

typedef struct
{
	int length;
	buffer data;
} fbx_array_property;

typedef struct
{
	fbx_node_record_header header;
	buffer name;
	vector properties;
	vector children;
	fbx_node_record_header null_record;
} fbx_node_record;

typedef struct
{
	int version;
	vector nodes;
	vector root_nodes;
} fbx;

int fbx_load(fbx* t_fbx, const char* t_string);

int fbx_stringify_property(fbx_property* t_property, vector* t_string);

int fbx_stringify_node(fbx_node_record* t_node, vector* t_nodes, vector* t_string, unsigned int t_should_stringify_properties);

int fbx_stringify(fbx* t_fbx, buffer* t_out_buffer);

int fbx_stringify_without_properties(fbx* t_fbx, buffer* t_out_buffer);

void fbx_final(fbx* t_fbx);

#endif