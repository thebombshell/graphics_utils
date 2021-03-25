
#ifndef SMOL_GRAPHICS_H
#define SMOL_GRAPHICS_H

#include <window.h>
#include "glad.h"
#include <data_structures.h>

/** @struct		graphics
 *	@brief		a struct containign graphical context information
 *	@member		graphics::surface - the window used to create the graphical context
 *	@member		graphics::device - the device the graphical context is bound to
 *	@member		graphics::context - the graphical context
 */
typedef struct 
{	
	window* surface;
	HDC device;
	HGLRC context;
} graphics;


/** returns the graphics error string
 */
const char* graphics_get_error_string();

/** returns nonzero if the given graphics is valid
 *	@memberof	graphics
 *	@param		t_graphics - the graphics to check for validity
 *	@returns	nonzero if the graphics is valid
 */
int graphics_is_valid(graphics* t_graphics);

/** initialize the given graphics
 *	@memberof	graphics
 *	@param		t_graphics - an invalid graphics to be initialized
 *	@param		t_window - a valid window to get the graphical device from
 *	@returns	nonzero if initialized successfully
 */
int init_graphics(graphics* t_graphics, window* t_window);

/**	finalize the given graphics
 *	@memberof	graphics
 *	@param		t_graphics - a valid graphics to be finalized
 */
void final_graphics(graphics* t_graphics);

/**	swaps the backbuffer and forebuffer
 *	@memberof	graphics
 *	@param		t_graphics - a valid graphics to swap the buffers of
 */
void graphics_swap_buffers(graphics* t_graphics);

/**	@struct		shader 
 *	@brief		a struct containing information about a shader
 *	@member		shader::id - the opengl shader id
 */
typedef struct {
	
	unsigned int id;
} shader;

/** init_shader - initializes a shader
 *	@memberof	shader
 *	@param		t_shader - an invalid shader to be initialized
 *	@param		t_shader_type - the shader type
 *	@param		t_string_count - the number of strings in t_shader_stings
 *	@param		t_shader_strings - a t_string_count length array of char arrays representing the body of the shader
 *	@param		t_shader_lengths - a t_string_count length array of lengths of the char arrays within t_shader_strings
 *	@returns	nonzero if the shader is valid
 */
int init_shader(shader* t_shader, unsigned int t_shader_type, unsigned int t_string_count, const char** t_shader_strings, const int* t_shader_lengths);

/**	finalizes a shader
 *	@memberof	shader
 *	@param		t_shader - a valid shader to be finalized
 */
void final_shader(shader* t_shader);

/**	@struct		uniform
 *	@brief		a struct containing information about an active uniform
 *	@member		uniform::length - the number of characters written to name
 *	@member		uniform::size - the number of members to an array uniform
 *	@member		uniform::type - the type of the uniform
 *	@member		uniform::name - the name of the uniform
 */
typedef struct {
	
	int length;
	int size;
	unsigned int type;
	char name[64];
} uniform;

/**	@struct		program 
 *	@brief		a struct containing information about a shader program
 *	@member		program::id - the internal id of the program
 *	@member		program::shader_count - the number of shaders in shaders
 *	@member		program::shaders - an array of pointers to shaders linked to the program
 *	@member		program::uniform_count - the number of active unifroms in the program
 *	@member		program::uniforms - an array of active uniforms in the program
 */
typedef struct {
	
	unsigned int id;
	unsigned int shader_count;
	shader** shaders;
	int uniform_count;
	uniform* uniforms;
} program;

/** returns whether a given program pointer is valid
 *	@memberof	program
 *	@param		t_program - the pointer to check for validity
 *	@returns	nonzero if the program is valid
 */
int program_is_valid(program* t_program);

/** initializes a program
 *	@memberof	program
 *	@param		t_program - an invalid program to be initialized
 *	@param		t_shader_count - the number of shaders to link the program to
 *	@param		t_shaders - pointers to the valid shaders
 *	@returns	nonzero if the program is valid
 */
int init_program(program* t_program, unsigned int t_shader_count, shader** t_shaders);

/** finalizes a program
 *	@memberof	program
 *	@param		t_program - a valid program to be finalized
 */
void final_program(program* t_program);

/**	@struct		vertex_attribute
 *	@brief		a struct containign vertex attribute data
 *	@member		vertex_attribute::buffer - the base address of the vertex data
 *	@member		vertex_attribute::buffer_size - the amount of vertex data following from buffer
 *	@member		vertex_attribute::size - the size of a single element in components
 *	@member		vertex_attribute::type - the type of components in an element
 *	@member		vertex_attribute::stride - the stride of a single element
 */
typedef struct {
	
	void* buffer;
	size_t buffer_size;
	unsigned int size;
	unsigned int type;
	unsigned int stride;
} vertex_attribute;

/**	@struct		index_buffer
 *	@brief		a struct containing indexing data
 *	@member		index_buffer::buffer - the base address of the index data
 *	@member		index_buffer::buffer_size - the amount of index data following from buffer
 *	@member		index_buffer::type - the type of index data in buffer
 */
typedef struct {
	
	void* buffer;
	size_t buffer_size;
	unsigned int type;
} index_buffer;

/** gets the total element count within an index buffer specification
 *	@memberof 	index_buffer
 *	@param		t_buffer - the index buffer to query for element count
 *	@returns	the total element count within the index buffer specification
 */
unsigned int index_buffer_get_element_count(index_buffer* t_buffer);

/**	@struct		mesh
 *	@brief		a struct for containing a renderable mesh
 *	@member		mesh::id - the internal id of the mesh
 *	@member		mesh::vertex_id - the internal id of the vertex buffer
 *	@member		mesh::vertex_usage - the usage type of the vertex buffer
 *	@member		mesh::index_id - the internal id of the index buffer
 *	@member		mesh::index_usage - the usage type of the index buffer
 *	@member		mesh::vertex_attribute_count - the number of vertex attributes in the mesh
 *	@member		mesh::vertex_attributes - an array of the vertex attributes in the mesh
 *	@member		mesh::index_buffer - the index buffer data in the mesh
 */
typedef struct {
	
	unsigned int id;
	unsigned int vertex_id;
	unsigned int vertex_usage;
	unsigned int index_id;
	unsigned int index_usage;
	unsigned int vertex_attribute_count;
	vertex_attribute* vertex_attributes;
	index_buffer index_buffer;
} mesh;

/**	initializes a renderable mesh
 *	@memberof	mesh
 *	@param		t_mesh - the mesh to initialize
 *	@param		t_vertex_attribute_count - the number of vertex attributes pointed to by t_vertex_attributes
 *	@param		t_vertex_attributes - an array of vertex attribute data
 *	@param		t_vertex_usage - the usage of the vertex buffer
 *	@param 		t_index_buffer - index buffer data
 *	@param		t_index_usage - the usage of the index buffer
 */
int init_mesh
	( mesh* t_mesh
	, unsigned int t_vertex_attribute_count
	, const vertex_attribute* t_vertex_attributes
	, unsigned int t_vertex_usage
	, const index_buffer* t_index_buffer
	, unsigned int t_index_usage);

/** finalizes a renderable mesh
 *	@memberof	mesh
 *	@param		t_mesh - the mesh to finalize
 *	@param		t_free_buffers - if non-zero, the cpu side buffers will be free'd
 */
void final_mesh(mesh* t_mesh, int t_free_buffers);

/** draws a renderable mesh
 *	@memberof	mesh
 *	@param		t_mesh - the mesh to render
 */
void mesh_render(mesh* t_mesh);

/** draws part of a renderable mesh
 *	@memberof	mesh
 *	@param		t_mesh - the mesh to render
 *	@param		t_offset - the offset of the first index to render
 *	@param		t_count - the count of indices to render
 */
void mesh_render_part(mesh* t_mesh, unsigned int t_offset, unsigned int t_count);

/**
 *
 */
typedef struct {
	
	unsigned int id;
	char* filepath;
	unsigned int component;
	unsigned int format;
	unsigned int type;
	void* buffer;
	unsigned int width;
	unsigned int height;
	
	unsigned int wrap_s;
	unsigned int wrap_t;
	unsigned int min;
	unsigned int mag;
} texture;

/**
 */
int init_texture
	( texture* t_texture
	, void* t_data
	, unsigned int t_width
	, unsigned int t_height
	, unsigned int t_component
	, unsigned int t_format
	, unsigned int t_type );

/**
 */
int load_texture(texture* t_texture, const char* t_filepath);

/**
 */
void final_texture(texture* t_texture);

/**
 */
void texture_set_parameters
	( texture* t_texture
	, unsigned int t_wrap_s
	, unsigned int t_wrap_t
	, unsigned int t_min
	, unsigned int t_mag );

/**
 */
void texture_bind(texture* t_texture, unsigned int t_index);

/**
 */
int texture_save(texture* t_texture, const char* t_filepath);

#endif