
#include "fbx_import.h"

#include "assert.h"
#include "stdio.h"
#include "zlib.h"

#define INFLATE_CHUNK 1024

#ifndef FBX_DEBUG
#define FBX_DEBUG 0
#endif

#ifndef FBX_DEBUG_LOG
#define FBX_DEBUG_LOG 0
#endif

#ifndef FBX_DEBUG_LOG_LOAD
#define FBX_DEBUG_LOG_LOAD 0
#endif

#ifndef FBX_DEBUG_LOG_STRINGIFY
#define FBX_DEBUG_LOG_STRINGIFY 0
#endif

#ifndef FBX_DEBUG_LOG_FINAL
#define FBX_DEBUG_LOG_FINAL 0
#endif

#ifndef FBX_DEBUG_VERBOSE
#define FBX_DEBUG_VERBOSE 0
#endif

#ifndef FBX_DEBUG_PROPERTY
#define FBX_DEBUG_PROPERTY 0
#endif

#ifndef FBX_DEBUG_STACK_COUNT
#define FBX_DEBUG_STACK_COUNT 0
#endif

#ifndef FBX_NOP
#define FBX_NOP (void)printf
#endif

#if FBX_DEBUG && FBX_DEBUG_LOG
#if FBX_DEBUG_VERBOSE
#define FBX_LOG_DEFINITION(...) (printf("[%s:%i] FBX LOG\n", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n"))
#define FBX_LOG_VERBOSE(...) (FBX_LOG_DEFINITION(__VA_ARGS__))
#else
#define FBX_LOG_DEFINITION(...) (printf(__VA_ARGS__), printf("\n"))
#endif
#else
#define FBX_LOG_DEFINITION(...) FBX_NOP
#endif

#define FBX_LOG(...) FBX_LOG_DEFINITION(__VA_ARGS__) 

#if FBX_DEBUG && FBX_DEBUG_STACK_COUNT 
#define FBX_STACK_COUNT(T_COUNT, ...) { int FBX_STACK_COUNT_INT = T_COUNT; for (; FBX_STACK_COUNT_INT; --FBX_STACK_COUNT_INT) printf(">"); printf(" " __VA_ARGS__); printf("\n"); }
#else
#define FBX_STACK_COUNT(T_COUNT, ...) FBX_NOP
#endif

#if FBX_DEBUG && FBX_DEBUG_PROPERTY 
#define FBX_PROPERTY_LOG(...) FBX_LOG(__VA_ARGS__)
#else
#define FBX_PROPERTY_LOG(...) FBX_NOP
#endif

#define FBX_LOAD_ERR_MESSAGE() (FBX_LOG("FBX FAILURE"))

#ifndef FBX_LOG_VERBOSE
#define FBX_LOG_VERBOSE(...) FBX_NOP
#endif

#if !FBX_DEBUG_LOG_LOAD
#undef FBX_LOG
#define FBX_LOG(...) FBX_NOP
#else
#undef FBX_LOG
#define FBX_LOG(...) FBX_LOG_DEFINITION(__VA_ARGS__)
#endif

int inflate_impl(buffer* t_input, buffer* t_output)
{
	assert(t_input && t_output);
	
	int ret;
	unsigned have;
	z_stream strm;
	int offset = 0;
	buffer_init(t_output, INFLATE_CHUNK);
	
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	
	if (ret != Z_OK)
	{
		return 0;
	}
	
	FBX_LOG("\tbegin decoding...");
	do
	{
		strm.avail_in = t_input->size - offset;
		if (strm.avail_in == 0)
		{
			break;
		}
		strm.next_in = t_input->data;
		
		do
		{
			buffer_resize(t_output, t_output->size + INFLATE_CHUNK);
			strm.avail_out = INFLATE_CHUNK;
			strm.next_out = ((Bytef*)t_output->data) + offset;
			ret = inflate(&strm, Z_NO_FLUSH);
			if (ret == Z_STREAM_ERROR)
			{
				FBX_LOG("\tdecoding stream error!");
			}
			switch (ret)
			{
			case Z_NEED_DICT:
			{
				FBX_LOG("\tneed dictionary error!");
				(void)inflateEnd(&strm);
				return 0;
			} break;
			case Z_DATA_ERROR:
			{
				FBX_LOG("\tdata error!");
				(void)inflateEnd(&strm);
				return 0;
			} break;
			case Z_MEM_ERROR:
			{
				FBX_LOG("\tmemory error!");
				(void)inflateEnd(&strm);
				return 0;
			} break;
			}
			have = INFLATE_CHUNK - strm.avail_out;
			offset += have;
			
			FBX_LOG("\tdecoding, offset %i...", offset);
		} while (strm.avail_out == 0);
		
		FBX_LOG("\tno stream end yet...");
	} while (ret != Z_STREAM_END);
	
	FBX_LOG("\t... end decoding");
	return 1;
}

int fread_property(FILE* t_file, fbx_property* t_property, size_t t_element_size)
{
	char value[8];
	int have = fread(value, 1, t_element_size, t_file);
	if (have != t_element_size)
	{
		FBX_LOG("have %i does not equal element size %u", have, (unsigned int)t_element_size);
		return 0;
	}
	int result = buffer_init(&t_property->data, t_element_size);
	if (!result)
	{
		return 0;
	}
	memcpy(t_property->data.data, value, t_element_size);
	return 1;
}

int fread_property_array(FILE* t_file, fbx_property* t_property, size_t t_element_size)
{
	unsigned int array_length = 0;
	unsigned int encoding = 0;
	unsigned int compressed_length = 0;
	int have = 0;
	
	have = fread(&array_length, 4, 1, t_file);
	if (!have)
	{
		FBX_LOG("have %i for array_length 1 is wrong", have);
		return 0;
	}
	have = fread(&encoding, 4, 1, t_file);
	if (!have)
	{
		FBX_LOG("have %i for encoding 1 is wrong", have);
		return 0;
	}
	have = fread(&compressed_length, 4, 1, t_file);
	if (!have)
	{
		FBX_LOG("have %i for compressed_length 1 is wrong", have);
		return 0;
	}
	int result = buffer_init(&t_property->data, sizeof(fbx_array_property));
	if (!result)
	{
		FBX_LOG("failed to init property data");
		return 0;
	}
	fbx_array_property* array_property = (fbx_array_property*)t_property->data.data;
	array_property->length = array_length;
	
	if (encoding)
	{
		FBX_LOG("\t{ array_length = %u, encoding = %u, compressed_length = %u }", array_length, encoding, compressed_length);
		
		buffer encoded_buffer;
		result = buffer_init(&encoded_buffer, compressed_length);
		if (!result)
		{
			FBX_LOG("failed to init encoded_buffer with compressed_length of %u", compressed_length);
			buffer_final(&t_property->data);
			return 0;
		}
		have = fread(encoded_buffer.data, 1, compressed_length, t_file);
		if (have != compressed_length)
		{
			FBX_LOG("have %i for encoded_buffer %u is wrong", have, compressed_length);
			buffer_final(&t_property->data);
			buffer_final(&encoded_buffer);
			return 0;
		}
		result = inflate_impl(&encoded_buffer, &array_property->data);
		buffer_final(&encoded_buffer);
		if (!result)
		{
			FBX_LOG("failed to inflate encoded_buffer");
			buffer_final(&t_property->data);
			buffer_final(&array_property->data);
			return 0;
		}
	}
	else
	{
		result = buffer_init(&array_property->data, array_length * t_element_size);
		if (!result)
		{
			FBX_LOG("failed to init array_property data");
			buffer_final(&t_property->data);
			return 0;
		}
		have = fread(array_property->data.data, t_element_size, array_length, t_file);
		if (have != array_length)
		{
			FBX_LOG("have %i for data %i is wrong", have, array_length);
			buffer_final(&t_property->data);
			buffer_final(&array_property->data);
			return 0;
		}
	}
	
	return 1;
}

int fbx_load(fbx* t_fbx, const char* t_string)
{
	assert(t_fbx && t_string);
	
	FILE* file = fopen(t_string, "rb");
	if (!file)
	{
		FBX_LOAD_ERR_MESSAGE();
		return 0;
	}
	
	fseek(file, 0, SEEK_END);
	long int file_length = ftell(file);

	fseek(file, 0, SEEK_SET);
	fbx_header header;
	
	char fbx_magic_string[21] = "Kaydara FBX Binary  ";
	char fbx_magic_number[2] = { 0x1A, 0x00 };
	
	int have = fread(&header, 1, 27, file);
	if (have != 27)
	{
		FBX_LOAD_ERR_MESSAGE();
		fclose(file);
		return 0;
	}
	if (memcmp(fbx_magic_string, header.magic_string, 21) != 0)
	{
		FBX_LOAD_ERR_MESSAGE();
		fclose(file);
		return 0;
	}
	
	if (memcmp(fbx_magic_number, header.magic_number, 2) != 0)
	{
		FBX_LOAD_ERR_MESSAGE();
		fclose(file);
		return 0;
	}
	
	t_fbx->version = header.version_number;
	int result = vector_init(&t_fbx->nodes, sizeof(fbx_node_record));
	if (!result)
	{
		FBX_LOAD_ERR_MESSAGE();
		fclose(file);
		return 0;
	}
	result = vector_init(&t_fbx->root_nodes, sizeof(int));
	if (!result)
	{
		FBX_LOAD_ERR_MESSAGE();
		vector_final(&t_fbx->nodes);
		fclose(file);
		return 0;
	}
	
	FBX_LOG("version_number %u", t_fbx->version);
	
	vector node_stack;
	result = vector_init(&node_stack, sizeof(int));
	if (!result)
	{
		FBX_LOAD_ERR_MESSAGE();
		fbx_final(t_fbx);
		fclose(file);
		return 0;
	}
	result = vector_push(&node_stack, 0);
	if (!result)
	{
		FBX_LOAD_ERR_MESSAGE();
		fbx_final(t_fbx);
		vector_final(&node_stack);
		fclose(file);
		return 0;
	}
	result = vector_push(&t_fbx->nodes, 0);
	if (!result)
	{
		FBX_LOAD_ERR_MESSAGE();
		fbx_final(t_fbx);
		vector_final(&node_stack);
		fclose(file);
		return 0;
	}
	
	fbx_node_record* node = 0;
	int node_index = 0;
	
	node = (fbx_node_record*)vector_get_index(&t_fbx->nodes, node_index);
	
#define FBX_LOAD_FAILURE() (FBX_LOAD_ERR_MESSAGE(), fclose(file), fbx_final(t_fbx), 0)

	while (node && ftell(file) != file_length)
	{
		have = fread(&node->header, 1, 13, file);
		if (have != 13)
		{
			return FBX_LOAD_FAILURE(); /* not enough to fill a header */
		}
		
		/* if a node header is 0, 0, 0, 0, then it is denoting the end of a child array */
		if (!node->header.end_offset && !node->header.num_properties && !node->header.property_list_length && !node->header.name_length)
		{
			vector_remove(&node_stack, node_stack.element_count - 1);
			vector_remove(&t_fbx->nodes, node_index);
			
			/* the last element should be 0, 0, 0, 0, denoting the end of file, as the file is a root child array */
			if (!node_stack.element_count)
			{
				FBX_LOG("\tEnd Of File at %li out of %li", (long int)ftell(file), (long int)file_length);
				
				break;
			}
			
			node_index = *((int*)vector_get_index(&node_stack, node_stack.element_count - 1));
			node = (fbx_node_record*)vector_get_index(&t_fbx->nodes, node_index);
			vector_remove(&node->children, node->children.element_count - 1);
			
			goto node_pop_stack;
		}
		
		/* when element_count is 1, we are looking at a root node */
		if (node_stack.element_count == 1)
		{
			vector_push(&t_fbx->root_nodes, &node_index);
		}
		result = buffer_init(&node->name, node->header.name_length + 1);
		if (!result)
		{
			return FBX_LOAD_FAILURE();
		}
		((char*)node->name.data)[node->header.name_length] = '\0';
		have = fread(node->name.data, 1, node->header.name_length, file);
		if (have != node->header.name_length)
		{
			return FBX_LOAD_FAILURE();
		}
		result = vector_init(&node->properties, sizeof(fbx_property));
		if (!result)
		{
			buffer_final(&node->name);
			return FBX_LOAD_FAILURE();
		}
		result = vector_init(&node->children, sizeof(int));
		if (!result)
		{
			buffer_final(&node->name);
			vector_final(&node->properties);
			return FBX_LOAD_FAILURE();
		}
		
		FBX_STACK_COUNT(node_stack.element_count, "\topen '%s'", (const char*)node->name.data);
		
		if (node_stack.element_count > 1)
		{
			int parent_index = *((int*)vector_get_index(&node_stack, node_stack.element_count - 2));
			fbx_node_record* parent = (fbx_node_record*)vector_get_index(&t_fbx->nodes, parent_index);
			((void)parent);
			FBX_LOG("\topen node %i '%s' on stack %i with parent '%s' child count %i", node_index, (const char*)node->name.data, (node_stack.element_count - 1), (char*)parent->name.data, (int)parent->children.element_count);
		}
		else
		{
			FBX_LOG("\topen node %i '%s' on stack %i", node_index, (const char*)node->name.data, (node_stack.element_count - 1));
		}
		
		int no_children = (ftell(file) + node->header.property_list_length) == node->header.end_offset;
		
		int i = 0;
		for (; i < node->header.num_properties; ++i)
		{
			fbx_property property;
			have = fread(&property.typecode, 1, 1, file);
			if (!have)
			{
				result = 0;
				break;
			}
			switch (property.typecode)
			{
				case 'C':
				{
					FBX_PROPERTY_LOG("\tChar / Bool property");
					result = fread_property(file, &property, 1);
				} break;
				case 'Y':
				{
					FBX_PROPERTY_LOG("\tShort property");
					result = fread_property(file, &property, 2);
				} break;
				case 'F':
				case 'I':
				{
					FBX_PROPERTY_LOG("\tInt / Float property");
					result = fread_property(file, &property, 4);
				} break;
				case 'D':
				case 'L':
				{
					FBX_PROPERTY_LOG("\tLong / Double property");
					result = fread_property(file, &property, 8);
				} break;
				case 'b':
				{
					FBX_PROPERTY_LOG("\tBool Array property");
					result = fread_property_array(file, &property, 1);
				} break;
				case 'i':
				case 'f':
				{
					FBX_PROPERTY_LOG("\tInt / Float Array property");
					result = fread_property_array(file, &property, 4);
				} break;
				case 'l':
				case 'd':
				{
					FBX_PROPERTY_LOG("\tLong / Double Array property");
					result = fread_property_array(file, &property, 8);
				} break;
				case 'S':
				{
					FBX_PROPERTY_LOG("\tString property");
					unsigned int length;
					have = fread(&length, 4, 1, file);
					if (!have)
					{
						result = 0;
						break;
					}
					result = buffer_init(&property.data, length + 1);
					if (!result)
					{
						break;
					}
					have = fread(property.data.data, 1, length, file);
					if (have != length)
					{
						result = 0;
						break;
					}
					((char*)property.data.data)[length] = '\0';
					FBX_PROPERTY_LOG("%s", (char*)property.data.data);
				} break;
				case 'R':
				{
					FBX_PROPERTY_LOG("\tData property");
					unsigned int length;
					have = fread(&length, 4, 1, file);
					if (!have)
					{
						result = 0;
						break;
					}
					result = buffer_init(&property.data, length);
					if (!result)
					{
						break;
					}
					have = fread(property.data.data, 1, length, file);
					if (have != length)
					{
						result = 0;
						break;
					}
				} break;
				default:
				{
					FBX_LOG("what in the world is %c", property.typecode);
					result = 0;
				} break;
			}
			if (!result)
			{
				FBX_LOG("\tProperty Fail");
				break;
			}
			result = vector_push(&node->properties, &property);
			if (!result)
			{
				FBX_LOG("\tVector Fail");
				break;
			}
		}
		if (!result)
		{
			int j = 0;
			for (; j < node->properties.element_count; ++j)
			{
				fbx_property* property = vector_get_index(&node->properties, j);
				switch (property->typecode)
				{
					case 'b':
					case 'i':
					case 'f':
					case 'l':
					case 'd':
					{
						fbx_array_property* array_property = (fbx_array_property*)property->data.data;
						buffer_final(&array_property->data);
					} break;
					case 'S':
					case 'R':
					{
						buffer_final(&property->data);
					} break;
					default: break;
				}
				buffer_final(&property->data);
			}
			buffer_final(&node->name);
			vector_final(&node->properties);
			vector_final(&node->children);
			return FBX_LOAD_FAILURE();
		}
		if (no_children)
		{
node_pop_stack:

			FBX_STACK_COUNT(node_stack.element_count, "\tclose '%s'", (const char*)node->name.data);
			
			FBX_LOG("\tclose node %i '%s' on stack %i", node_index, (const char*)node->name.data, (node_stack.element_count - 1));
			
			vector_remove(&node_stack, node_stack.element_count - 1);
			if (node_stack.element_count)
			{
				node_index = *((int*)vector_get_index(&node_stack, node_stack.element_count - 1));
				node = (fbx_node_record*)vector_get_index(&t_fbx->nodes, node_index);
			}
			else
			{
				node_index = -1;
				node = 0;
			}
		}
		result = vector_push(&t_fbx->nodes, 0);
		if (!result)
		{
			return FBX_LOAD_FAILURE();
		}
		node = node_index == -1 ? 0 : (fbx_node_record*)vector_get_index(&t_fbx->nodes, node_index);
		node_index = t_fbx->nodes.element_count - 1;
		if (node)
		{
			result = vector_push(&node->children, &node_index);
			if (!result)
			{
				return FBX_LOAD_FAILURE();
			}
		}
		result = vector_push(&node_stack, &node_index);
		if (!result)
		{
			return FBX_LOAD_FAILURE();
		}
		node = (fbx_node_record*)vector_get_index(&t_fbx->nodes, node_index);
	}
	
	fclose(file);
	
	return 1;
}

#if !FBX_DEBUG_LOG_STRINGIFY
#undef FBX_LOG
#define FBX_LOG(...) FBX_NOP
#else
#undef FBX_LOG
#define FBX_LOG(...) FBX_LOG_DEFINITION(__VA_ARGS__)
#endif

int fbx_string_push(vector* t_vector, const char* t_string)
{
	if (!t_vector || !t_string)
	{
		return 0;
	}
	
	const char* c = t_string; 
	for (; (*c) != '\0'; ++c)
	{
		int result = vector_push(t_vector, c);
		if (!result)
		{
			return 0;
		}
	}
	
	return 1;
}

int fbx_string_push_limit(vector* t_vector, const char* t_string, unsigned int t_char_limit)
{
	if (!t_vector || !t_string)
	{
		return 0;
	}
	
	const char* c = t_string; 
	unsigned int i = 0;
	for (; i < t_char_limit && (*c) != '\0'; ++c, ++i)
	{
		int result = vector_push(t_vector, c);
		if (!result)
		{
			return 0;
		}
	}
	
	return 1;
}

int fbx_stringify_node(fbx_node_record* t_node, vector* t_nodes, vector* t_string)
{
	if (!t_node || !t_string)
	{
		return 0;
	}
	
	FBX_LOG("stringify node \"%s\" entered", (char*)t_node->name.data);
	
	int result = fbx_string_push(t_string, (char*)t_node->name.data);
	if (!result)
	{
		return 0;
	}
	
	FBX_LOG("stringify count");
	
	if (t_node->properties.element_count || t_node->children.element_count)
	{
		result = fbx_string_push(t_string, " : ");
		if (!result)
		{
			return 0;
		}
	}
	
	FBX_LOG("stringify properties");
	
	int i = 0;
	for (; i < t_node->properties.element_count; ++i)
	{
		char temp[256];
		if (i != 0)
		{
			result = fbx_string_push(t_string, temp);
			if (!result)
			{
				return 0;
			}
		}
		fbx_property* property = (fbx_property*)vector_get_index(&t_node->properties, i);
		switch (property->typecode)
		{
			case 'C':
			{
				int value = (int)*((char*)property->data.data);
				sprintf(temp, "%i", value);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'Y':
			{
				int value = (int)*((short*)property->data.data);
				sprintf(temp, "%i", value);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'I':
			{
				int value = *((int*)property->data.data);
				sprintf(temp, "%i", value);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'L':
			{
				long long int value = *((long long int*)property->data.data);
				sprintf(temp, "%lli", value);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'F':
			{
				double value = (double)*((float*)property->data.data);
				sprintf(temp, "%Lf.4", value);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'D':
			{
				double value = *((double*)property->data.data);
				sprintf(temp, "%Lf.4", value);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'b':
			{
				fbx_array_property* array_property = (fbx_array_property*)property->data.data;
				sprintf(temp, "bool_array[%i]", (int)array_property->length);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'i':
			{
				fbx_array_property* array_property = (fbx_array_property*)property->data.data;
				sprintf(temp, "int_array[%i]", (int)array_property->length);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'f':
			{
				fbx_array_property* array_property = (fbx_array_property*)property->data.data;
				sprintf(temp, "float_array[%i]", (int)array_property->length);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'l':
			{
				fbx_array_property* array_property = (fbx_array_property*)property->data.data;
				sprintf(temp, "long_array[%i]", (int)array_property->length);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'd':
			{
				fbx_array_property* array_property = (fbx_array_property*)property->data.data;
				sprintf(temp, "double_array[%i]", (int)array_property->length);
				result = fbx_string_push(t_string, temp);
			} break;
			case 'S':
			{
				result = fbx_string_push(t_string, "\"");
				if(!result)
				{
					return 0;
				}
				result = fbx_string_push_limit(t_string, ((char*)property->data.data), property->data.size);
				if (!result)
				{
					return 0;
				}
				result = fbx_string_push(t_string, "\"");
				if(!result)
				{
					return 0;
				}
				
			} break;
			case 'R':
			{
				result = fbx_string_push(t_string, "RAW_DATA");
			} break;
		}
		if (!result)
		{
			return 0;
		}
	}
	
	
	if (t_node->children.element_count)
	{
		result = fbx_string_push(t_string, " {\n");
		if (!result)
		{
			return 0;
		}
		i = 0;
		for (; i < t_node->children.element_count; ++i)
		{
			unsigned int child_index = *((unsigned int*)vector_get_index(&t_node->children, i));
			fbx_node_record* child = (fbx_node_record*)vector_get_index(t_nodes, child_index);
			result = fbx_stringify_node(child, t_nodes, t_string);
			if (!result)
			{
				return 0;
			}
		}
		
		result = fbx_string_push(t_string, "}");
		if (!result)
		{
			return 0;
		}
	}
	
	result = fbx_string_push(t_string, "\n");
	if (!result)
	{
		return 0;
	}
	
	FBX_LOG("stringify node exited");
	
	return 1;
}

int fbx_stringify(fbx* t_fbx, buffer* t_out_buffer)
{
	if (!t_fbx || !t_out_buffer)
	{
		return 0;
	}
	
	FBX_LOG("stringify entered");
	
	vector string;
	int result = vector_init(&string, 1);
	if (!result)
	{
		return 0;
	}
	
	fbx_node_record* node = 0;
	int i = 0;
	
	
	FBX_LOG("stringify entering record");
	
	for (; i < t_fbx->root_nodes.element_count; ++i)
	{
		FBX_LOG("stringify element %i", i);
		
		node = (fbx_node_record*)vector_get_index(&t_fbx->nodes, *((int*)vector_get_index(&t_fbx->root_nodes, i)));
		
		result = fbx_stringify_node(node, &t_fbx->nodes, &string);
		if (!result)
		{
			goto stringify_fail;
		}
	}
	
	FBX_LOG("stringify exiting record");
	
	char null_term = '\0';
	result = vector_push(&string, &null_term);
	
	if (!result)
	{
stringify_fail:
		FBX_LOG("stringify failed");
		vector_final(&string);
		return 0;
	}
	
	*t_out_buffer = string.buffer;
	
	FBX_LOG("stringify exited");
	
	return 1;
}

#if !FBX_DEBUG_LOG_FINAL
#undef FBX_LOG
#define FBX_LOG(...) FBX_NOP
#else
#undef FBX_LOG
#define FBX_LOG(...) FBX_LOG_DEFINITION(__VA_ARGS__)
#endif

void fbx_final(fbx* t_fbx)
{
	if (t_fbx)
	{
		fbx_node_record* node = 0;
		while (t_fbx->nodes.element_count)
		{
			node = (fbx_node_record*)vector_get_index(&t_fbx->nodes, t_fbx->nodes.element_count - 1);
			
			FBX_LOG("removing node %i '%s'", (int)(t_fbx->nodes.element_count - 1), (char*)node->name.data);
			
			int i = 0;
			for (; i < node->properties.element_count; ++i)
			{
				fbx_property* property = vector_get_index(&node->properties, i);
				
				FBX_PROPERTY_LOG("\tProperty Typecode: %c", property->typecode);
				
				switch (property->typecode)
				{
					case 'b':
					case 'i':
					case 'f':
					case 'l':
					case 'd':
					{
						fbx_array_property* array_property = (fbx_array_property*)property->data.data;
						buffer_final(&array_property->data);
					} break;
					case 'S':
					case 'R':
					{
						buffer_final(&property->data);
					} break;
					default: break;
				}
				buffer_final(&property->data);
			}
			FBX_LOG("removing name");
			buffer_final(&node->name);
			FBX_LOG("removing properties");
			vector_final(&node->properties);
			FBX_LOG("removing children");
			vector_final(&node->children);
			FBX_LOG("removing node");
			vector_remove(&t_fbx->nodes, t_fbx->nodes.element_count - 1);
		}
		vector_final(&t_fbx->nodes);
		vector_final(&t_fbx->root_nodes);
	}
}
