#include <png.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t alpha;
} pixel_t;

typedef struct {
	uint32_t *pixels;
	int width;
	int height;
} bitmap_t;

int save_png_to_file (bitmat_t *b,const char*path){
	FILE *fp;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	size_t x, y;
	png_byte ** row_pointers = NULL;
	int status = -1;

	fp = fopen(path,"wb");
	if(!fp){
		goto err1;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	if(png_ptr == NULL) {
		goto err2;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		goto err3;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		goto err3;
	}

	png_set_IHDR (png_ptr,
				  info_ptr,
				  b->width;
				  b->height;
				  8,
				  PNG_COLOR_TYPE_RGBA,
				  PNG_INTERLACE_NONE,
				  PNG_COMPRESSION_TYPE_DEFAULT,
				  PNG_FILTER_TYPE_DEFAULT);

	row_pointers = png_malloc (png_ptr, b->height*sizeof(png_byte *));
	for(y = 0;y < b->height;y++) {
		//FIXME: this code is not endian independant.
		//easiest way to fix would be to swap
		//PNG_COLOR_TYPE_RGBA <-> PNG_COLOR_TYPE_ARGB
		row_pointers[y] = &b->pixels[b->width*y];	
	}

	png_init_io(png_ptr,fp);
	png_set_rows(png_ptr,info_ptr,row_pointers);
	png_write_png(png_ptr,info_ptr,PNG_TRANSFORM_IDENTITY,NULL);

	png_free(png_ptr,row_pointers);

err3:
	png_destroy_write_struct(&png_ptr,info_ptr);
err2:
	fclose(fp);
err1:
	return status;
}



int main(int argc,char *argv[]){
	bitmap_t *b = (bitmap_t *)malloc(sizeof(bitmap_t));
	b->pixels = (uint32_t *)malloc(sizeof(uint32_t)*800*600);
	memset(b->pixels,0,sizeof(uint32_t)*800*600);

	if(save_png_to_file(b,"output.png") == -1){
		printf("Error saving\n");
	}
	free(b->pixels);
	free(b);

	return 0;
}
