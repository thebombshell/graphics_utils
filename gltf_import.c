
#include "gltf_import.h"

#include "assert.h"
#include "stdio.h"

int gltf_load(gltf* t_gltf, const char* t_string)
{
	assert(t_gltf && t_string);
	
	FILE* file = fopen(t_string, "rb");
	if (!file)
	{
		return 0;
	}
	
	fseek(file, 0, SEEK_END);
	long int file_length = ftell(file);
	(void)file_length;
	
	fseek(file, 0, SEEK_SET);
	
	/* do the rest here */
	
	fclose(file);
	
	return 1;
}

void gltf_final(gltf* t_gltf)
{
	assert(t_gltf);
}