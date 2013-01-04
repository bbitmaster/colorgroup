#include <png.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mtwist.h"

typedef struct  {
	uint32_t *pixels;
	int width;
	int height;
} bitmap_t;

int save_png_to_file(bitmap_t *b,const char *path){
	FILE *fp;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	size_t x, y;
	png_byte ** row_pointers = NULL;
	int status = -1;

	fp = fopen(path,"wb");
	if(!fp){
		fprintf(stderr,"error opening file\n");
		goto err1;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	if(png_ptr == NULL) {
		fprintf(stderr,"error creating write struct\n");
		goto err2;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fprintf(stderr,"error creating info struct\n");
		goto err3;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr,"setjmp error\n");
		goto err3;
	}

	png_set_IHDR (png_ptr,
				  info_ptr,
				  b->width,
				  b->height,
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
		row_pointers[y] = (png_byte *)&b->pixels[b->width*y];	
	}

	png_init_io(png_ptr,fp);
	png_set_rows(png_ptr,info_ptr,row_pointers);
	png_write_png(png_ptr,info_ptr,PNG_TRANSFORM_IDENTITY | PNG_TRANSFORM_INVERT_ALPHA,NULL);
	status = 0;

	png_free(png_ptr,row_pointers);
err3:
	png_destroy_write_struct(&png_ptr,&info_ptr);
err2:
	fclose(fp);
err1:
	return status;
}


uint64_t calculate_fitness(bitmap_t *bmp){
	int x, y;
	uint64_t fitness = 0;
	int r,g,b,r_up,g_up,b_up,r_left,g_left,b_left;

	for(x = 1;x < bmp->width;x++){
		for(y = 1;y < bmp->height;y++){
			r = bmp->pixels[y*bmp->width + x]      &0xFF;
			g = (bmp->pixels[y*bmp->width + x]>>8) &0xFF;
			b = (bmp->pixels[y*bmp->width + x]>>16)&0xFF;
			r_up = bmp->pixels[(y-1)*bmp->width + x]      &0xFF;
			g_up = (bmp->pixels[(y-1)*bmp->width + x]>>8) &0xFF;
			b_up = (bmp->pixels[(y-1)*bmp->width + x]>>16)&0xFF;
			r_left = bmp->pixels[y*bmp->width + x-1]      &0xFF;
			g_left = (bmp->pixels[y*bmp->width + x-1]>>8) &0xFF;
			b_left = (bmp->pixels[y*bmp->width + x-1]>>16)&0xFF;
			fitness += (r - r_up)*(r - r_up);
			fitness += (g - g_up)*(g - g_up);
			fitness += (b - b_up)*(b - b_up);
			fitness += (r - r_left)*(r - r_left);
			fitness += (g - g_left)*(g - g_left);
			fitness += (b - b_left)*(b - b_left);
		}
	}
	return fitness;
}

void swap_values(uint32_t *p1,uint32_t *p2){
	uint32_t tmp = *p1;
	*p1 = *p2;
	*p2 = tmp;
}

int main(int argc,char *argv[]){
	mt_seed();
	bitmap_t *b = (bitmap_t *)malloc(sizeof(bitmap_t));
	b->pixels = (uint32_t *)malloc(sizeof(uint32_t)*4096*4096);
	b->width = 4096;
	b->height = 4096;
	bitmap_t *b_cpy = (bitmap_t *)malloc(sizeof(bitmap_t));
	b_cpy->pixels = (uint32_t *)malloc(sizeof(uint32_t)*4096*4096);
	b_cpy->width = 4096;
	b_cpy->height = 4096;

	int32_t *tmp_pixels = (int32_t *)malloc(sizeof(uint32_t)*4096*4096);

	const int img_size = 4096*4096;
	//fill tmp_pixels with ordered set of all possible colors
	int x;
	for(x = 0;x < img_size;x++){
		tmp_pixels[x] = x;
	}

	//select values randomly out of tmp_pixels to fill p->pixels
	int i = 4096*4096;
	printf("doing random selection...\n");
	while(i > 0){
		int selection = mt_lrand()%img_size;
		if(tmp_pixels[selection] == -1)
			continue;
		b->pixels[i] = tmp_pixels[selection];
		tmp_pixels[selection] = -1;
		i--;
	}
	printf("done random selection...\n");
	free(tmp_pixels);

	//begin genetic algorithm...
	uint64_t iteration=0;
	uint64_t fitness = calculate_fitness(b);
	printf("initial fitness: %ld\n",fitness);

	//start by mutating half the image
	uint64_t mutation_amount = 4096*4096/2;

	//tell if we improved in the last 20 iterations
	const int mutation_decay_delay = 20;
	int improved[mutation_decay_delay];

	for(i = 0;i < mutation_decay_delay;i++)improved[i] = 1;

	while(1){
		//make a copy of the image
		memcpy(b_cpy->pixels,b->pixels,img_size*sizeof(uint32_t));
		//mutate the copy
		for(i = 0;i < mutation_amount;i++){
			swap_values(&b_cpy->pixels[mt_lrand()%img_size],&b_cpy->pixels[mt_lrand()%img_size]);
		}

		uint64_t new_fitness = calculate_fitness(b_cpy);
		//we have a better fitness!
		if(new_fitness < fitness){
			fitness = new_fitness;
			memcpy(b->pixels,b_cpy->pixels,img_size*sizeof(uint32_t));
			printf("iteration %d fitness %ld\n",iteration,fitness);
			improved[iteration%mutation_decay_delay] = 1;
		} else {
			improved[iteration%mutation_decay_delay] = 0;
		}

		//if we haven't improved in the last n iterations, then lower the mutation rate
		int improved_sum=0;
		for(i = 0;i < mutation_decay_delay;i++)improved_sum += improved[i];
		if(improved_sum == 0){
			mutation_amount = mutation_amount*0.9;
			printf("iteration %d, No improvements in the last %d iterations. Lowering rate to %ld\n",iteration,
					mutation_decay_delay,
					mutation_amount);
			//mark improvement so rate doesn't get lowered next time
			improved[iteration%mutation_decay_delay] = 1;
		}

		iteration++;
		if(iteration%200 == 0){
			printf("iteration %d, saving file...\n",iteration);
			if(save_png_to_file(b,"output.png") == -1){
				printf("Error saving\n");
			}
		}
	}


	free(b->pixels);
	free(b);
	free(b_cpy->pixels);
	free(b_cpy);

	return 0;
}
