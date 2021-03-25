
#include "graphics.h"

#include "lode_png.h"

#include <assert.h>
#include <stdio.h>
#include <GL/wglext.h>

const PIXELFORMATDESCRIPTOR g_pixel_format_descriptor = {
	sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
	PFD_TYPE_RGBA, 32, 8, 0, 8, 0, 8, 0, 8, 0, 0, 0, 0, 0, 0, 24, 8, 0, 0, 0, 0, 0, 0
};

const int g_graphics_attributes[7] =
	{ WGL_CONTEXT_MAJOR_VERSION_ARB, 4
	, WGL_CONTEXT_MINOR_VERSION_ARB, 6
    , WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB
	, 0
	};

static char g_graphics_error[1024];

const char* graphics_get_error_string()
{
	return g_graphics_error;
}

#define GRAPHICS_ERROR(...) { sprintf(g_graphics_error, __VA_ARGS__); }

int graphics_is_valid(graphics* t_graphics) {
	
	return t_graphics && window_is_valid(t_graphics->surface);
}

int init_graphics(graphics* t_graphics, window* t_window) {
	
	assert(t_graphics);
	
	HGLRC temp;
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
	int pixel_format;
	
	t_graphics->surface = t_window;
	t_graphics->device = GetDC(t_window->handle);
	
	pixel_format = ChoosePixelFormat(t_graphics->device, &g_pixel_format_descriptor);
	if (!SetPixelFormat(t_graphics->device, pixel_format, &g_pixel_format_descriptor)) {
		
		GRAPHICS_ERROR("failed to set pixel format");
		return 0;
	}
	
	temp = wglCreateContext(t_graphics->device);
	wglMakeCurrent(t_graphics->device, temp);
	
	wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
	if (!wglCreateContextAttribsARB) {
		
		GRAPHICS_ERROR("failed to load wglCreateContextAttribsARB: %ld", GetLastError());
		return 0;
	}
	
	wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	if (!wglChoosePixelFormatARB) {
		
		GRAPHICS_ERROR("failed to load wglChoosePixelFormatARB: %ld", GetLastError());
		return 0;
	}
	
	if (!gladLoadGL()) {
		
		GRAPHICS_ERROR("failed to load gl via glad");
		return 0;
	}
	wglMakeCurrent(0, 0);
	wglDeleteContext(temp);
	
	t_graphics->context = wglCreateContextAttribsARB(t_graphics->device, 0, &g_graphics_attributes[0]);
	if (!t_graphics->context) {
		
		GRAPHICS_ERROR("failed to create graphics context");
		return 0;
	}
	
	wglMakeCurrent(t_graphics->device, t_graphics->context);
	GRAPHICS_ERROR("OpenGL %s - glsl %s - %s - %s"
		, glGetString(GL_VERSION)
		, glGetString(GL_SHADING_LANGUAGE_VERSION)
		, glGetString(GL_VENDOR)
		, glGetString(GL_RENDERER));
	return 1;
}

void final_graphics(graphics* t_graphics) {
	
	assert(graphics_is_valid(t_graphics));
	
	wglMakeCurrent(0, 0);
	wglDeleteContext(t_graphics->context);
}

void graphics_swap_buffers(graphics* t_graphics) {
	
	SwapBuffers(t_graphics->device);
}

int init_shader
	( shader* t_shader
	, unsigned int t_shader_type
	, unsigned int t_string_count
	, const char** t_shader_strings
	, const int* t_shader_lengths) {
	
	assert(t_shader);
	
	int is_compiled;
	int err_length;
	
	t_shader->id = glCreateShader(t_shader_type);
	
	glShaderSource(t_shader->id, t_string_count, t_shader_strings, t_shader_lengths);
	glCompileShader(t_shader->id);
	
	glGetShaderiv(t_shader->id, GL_COMPILE_STATUS, &is_compiled);
	if (!is_compiled) {
		
		char err_log[1024];
		
		glGetShaderInfoLog(t_shader->id, 1024, &err_length, err_log);
		
		GRAPHICS_ERROR("failed to compile %s shader: %s\n", t_shader_type == GL_FRAGMENT_SHADER ? "fragment" : "vertex", err_log);
		
		glDeleteShader(t_shader->id);
		
		return 0;
	}
	
	return 1;
}

void final_shader(shader* t_shader) {
	
	assert(t_shader);
	
	glDeleteShader(t_shader->id);
}

int program_is_valid(program* t_program) {
	
	return t_program && t_program->shader_count && t_program->shaders && t_program->uniforms;
}

int init_program(program* t_program, unsigned int t_shader_count, shader** t_shaders) {
	
	assert(t_program && !program_is_valid(t_program));
	
	int is_linked;
	int err_length;
	unsigned int i;
	
	t_program->shader_count = t_shader_count;
	t_program->shaders = t_shaders;
	
	t_program->id = glCreateProgram();
	
	for (i = 0; i < t_shader_count; ++i) {
		
		glAttachShader(t_program->id, t_shaders[i]->id);
	}
	
	glLinkProgram(t_program->id);
	
	glGetProgramiv(t_program->id, GL_LINK_STATUS, &is_linked);
	if (!is_linked) {
		
		char err_log[1024];
		
		glGetProgramInfoLog(t_program->id, 1024, &err_length, err_log);
		
		GRAPHICS_ERROR("failed to link program: %s\n", err_log);
		
		glDeleteProgram(t_program->id);
		
		return 0;
	}
		
	glGetProgramiv(t_program->id, GL_ACTIVE_UNIFORMS, &t_program->uniform_count);
	
	t_program->uniforms = malloc(sizeof(uniform) * t_program->uniform_count);
	
	for (i = 0; i < t_program->uniform_count; ++i) {
		
		glGetActiveUniform(t_program->id, i, 64
			, &t_program->uniforms[i].length
			, &t_program->uniforms[i].size
			, &t_program->uniforms[i].type
			, t_program->uniforms[i].name);
	}
	
	return 1;
}

void final_program(program* t_program) {
	
	assert(program_is_valid(t_program));
	
	glDeleteProgram(t_program->id);
	t_program->id = 0;
	
	free(t_program->uniforms);
	t_program->uniform_count = 0;
	t_program->uniforms = 0;
	
	t_program->shader_count = 0;
	t_program->shaders = 0;
}

unsigned int index_buffer_get_element_count(index_buffer* t_buffer) {
	
	assert(t_buffer);
	
	if (t_buffer->type == GL_UNSIGNED_BYTE) {
		
		return t_buffer->buffer_size / sizeof(char);
	}
	if (t_buffer->type == GL_UNSIGNED_SHORT) {
		
		return t_buffer->buffer_size / sizeof(unsigned short);
	}
	if (t_buffer->type == GL_UNSIGNED_INT) {
		
		return t_buffer->buffer_size / sizeof(unsigned int);
	}
		
	return 0;
}

int init_mesh
	( mesh* t_mesh
	, unsigned int t_vertex_attribute_count
	, const vertex_attribute* t_vertex_attributes
	, unsigned int t_vertex_usage
	, const index_buffer* t_index_buffer
	, unsigned int t_index_usage) {
	
	assert(t_mesh);
	
	size_t i;
	
	/* Fill Variables */
	
	unsigned int buffers[2] = {0, 0};
	glGenBuffers(2, buffers);
	t_mesh->vertex_id = buffers[0];
	t_mesh->index_id = buffers[1];
	glGenVertexArrays(1, &t_mesh->id);
	t_mesh->vertex_attribute_count = t_vertex_attribute_count;
	t_mesh->vertex_attributes = malloc(sizeof(vertex_attribute) * t_vertex_attribute_count);
	t_mesh->vertex_usage = t_vertex_usage;
	t_mesh->index_buffer = *t_index_buffer;
	t_mesh->index_usage = t_index_usage;
	
	size_t total_size = 0;
	for (i = 0; i < t_vertex_attribute_count; ++i) {
		
		t_mesh->vertex_attributes[i] = t_vertex_attributes[i];
		total_size += t_mesh->vertex_attributes[i].buffer_size;
	}
	
	/* Fill Vertex Array */
	
	glBindVertexArray(t_mesh->id);
	
	glBindBuffer(GL_ARRAY_BUFFER, t_mesh->vertex_id);
	glBufferData(GL_ARRAY_BUFFER, total_size, 0, t_vertex_usage);
	
	size_t offset_size = 0;
	for (i = 0; i < t_vertex_attribute_count; ++i) {
		
		const vertex_attribute* attribute = &t_mesh->vertex_attributes[i];
		glBufferSubData(GL_ARRAY_BUFFER, offset_size, attribute->buffer_size, attribute->buffer);
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(i, attribute->size, attribute->type, 0, attribute->stride, (void*)(offset_size));
		offset_size += t_mesh->vertex_attributes[i].buffer_size;
	}
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t_mesh->index_id);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, t_mesh->index_buffer.buffer_size, t_mesh->index_buffer.buffer, t_index_usage);
	
	glBindVertexArray(0);
	
	return 1;
}

void final_mesh(mesh* t_mesh, int t_free_buffers) {
	
	assert(t_mesh);
	
	glDeleteVertexArrays(1, &t_mesh->id);
	glDeleteBuffers(1, &t_mesh->vertex_id);
	glDeleteBuffers(1, &t_mesh->index_id);
	
	if (t_free_buffers) {
		
		size_t i;
		for (i = 0; i < t_mesh->vertex_attribute_count; ++i) {
			
			free(t_mesh->vertex_attributes[i].buffer);
		}
		free(t_mesh->index_buffer.buffer);
	}
	
	free(t_mesh->vertex_attributes);
}

void mesh_render(mesh* t_mesh) {
	
	assert(t_mesh);
	
	glBindVertexArray(t_mesh->id);
	glDrawElements(GL_TRIANGLES, index_buffer_get_element_count(&t_mesh->index_buffer), t_mesh->index_buffer.type, 0);
	glBindVertexArray(0);
}

void mesh_render_part(mesh* t_mesh, unsigned int t_offset, unsigned int t_count) {
	
	assert(t_mesh && (t_offset + t_count) <= index_buffer_get_element_count(&t_mesh->index_buffer));
	
	glBindVertexArray(t_mesh->id);
	glDrawElements(GL_TRIANGLES, t_count, t_mesh->index_buffer.type, (void*)t_offset);
	glBindVertexArray(0);
}

int init_texture_impl
	( texture* t_texture
	, char* t_filepath
	, void* t_data
	, unsigned int t_width
	, unsigned int t_height
	, unsigned int t_component
	, unsigned int t_format
	, unsigned int t_type) {
	
	t_texture->filepath = t_filepath;
	t_texture->buffer = t_data;
	t_texture->width = t_width;
	t_texture->height = t_height;
	t_texture->component = t_component;
	t_texture->format = t_format;
	t_texture->type = t_type;
	
	glGenTextures(1, &t_texture->id);
	glActiveTexture(GL_TEXTURE0 + 7);
	glBindTexture(GL_TEXTURE_2D, t_texture->id);
	glTexImage2D(GL_TEXTURE_2D, 0, t_component, t_width, t_height, 0, t_format, t_type, t_data);
	glGenerateMipmap(GL_TEXTURE_2D);
	texture_set_parameters(t_texture, GL_REPEAT, GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	return 1;
}

int init_texture
	( texture* t_texture
	, void* t_data
	, unsigned int t_width
	, unsigned int t_height
	, unsigned int t_component
	, unsigned int t_format
	, unsigned int t_type) {
	
	assert(t_texture);
	
	return init_texture_impl(t_texture, 0, t_data, t_width, t_height, t_component, t_format, t_type);
}

int load_texture(texture* t_texture, const char* t_filepath) {
	
	assert(t_texture);
	
	char* filepath = malloc(strlen(t_filepath));
	strcpy(filepath, t_filepath);
	
	unsigned int error = lodepng_decode32_file((unsigned char**)&t_texture->buffer, &t_texture->width, &t_texture->height, filepath);
	if (error) {
		
		GRAPHICS_ERROR("failed to load texture: %s", lodepng_error_text(error));
		
		return 0;
	}
	
	return init_texture_impl(t_texture, t_texture->filepath, t_texture->buffer, t_texture->width, t_texture->height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
}

void final_texture(texture* t_texture) {
	
	assert(t_texture);
	
	free(t_texture->filepath);
	free(t_texture->buffer);
}

void texture_set_parameters
	( texture* t_texture
	, unsigned int t_wrap_s
	, unsigned int t_wrap_t
	, unsigned int t_min
	, unsigned int t_mag ) {
	
	assert(t_texture);
	
	glActiveTexture(GL_TEXTURE0 + 7);
	glBindTexture(GL_TEXTURE_2D, t_texture->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, t_wrap_s);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, t_wrap_t);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, t_min);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, t_mag);
	t_texture->wrap_s = t_wrap_s;
	t_texture->wrap_t = t_wrap_t;
	t_texture->min = t_min;
	t_texture->mag = t_mag;
	glBindTexture(GL_TEXTURE_2D, 0);
}

void texture_bind(texture* t_texture, unsigned int t_index) {
	
	assert(t_texture);
	
	glActiveTexture(GL_TEXTURE0 + t_index);
	glBindTexture(GL_TEXTURE_2D, t_texture->id);
}

int texture_save(texture* t_texture, const char* t_filepath) {
	
	assert(t_texture);
	
	unsigned int error = lodepng_encode32_file(t_filepath, (unsigned char*)t_texture->buffer, t_texture->width, t_texture->height);
	if (error) {
		
		GRAPHICS_ERROR("failed to save texture: %s", lodepng_error_text(error));
		
		return 0;
	}
	
	return 1;
}

