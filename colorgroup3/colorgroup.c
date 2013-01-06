#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <png.h>
#include <SDL.h>

#include "mtwist.h"

#define ACCEPT_SIZE 262144

typedef struct  {
	uint32_t *pixels;
	int width;
	int height;
} bitmap_t;


//This is quick and dirty code to use libpng's high level interface.
//It probably won't always work for all PNGs - I did not add a filler byte
//so, PNGs that use RGB without alpha likely won't load correctly.
//Also, this code is probably not endian independant.
// -use PNG_TRANSFORM_BGR to change endianness-
bitmap_t *load_png_from_file(const char *path){
	bitmap_t *b = malloc(sizeof(bitmap_t *));
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_byte ** row_pointers = NULL;
	FILE *fp;

	if((fp = fopen(path,"rb")) == NULL){
		fprintf(stderr,"error opening file '%s'\n",path);
		goto err1;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	
	if(png_ptr == NULL) {
		printf("png_create_read_struct returned NULL\n");
		fclose(fp);
		goto err2;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if(info_ptr == NULL){
		printf("png_create_info_struct returned NULL\n");
		goto err2;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		goto err3;
	}

	png_init_io(png_ptr,fp);

	png_set_sig_bytes(png_ptr,0);

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 |
		PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND , NULL);
	b->height = png_get_image_height(png_ptr,info_ptr);
	b->width = png_get_image_width(png_ptr,info_ptr);
	png_set_bgr(png_ptr);

	png_read_update_info(png_ptr,info_ptr);

	b->pixels = malloc(4*b->width*b->height);
	row_pointers = png_get_rows(png_ptr,info_ptr);


	int y;
	for(y = 0;y < b->height;y++) {
		memcpy(&b->pixels[b->width*y],row_pointers[y],b->width*4);
	}

	//read image data
	png_destroy_read_struct(&png_ptr,&info_ptr,NULL);
	return b;
err3:
	png_destroy_read_struct(&png_ptr,&info_ptr,NULL);
err2:
	fclose(fp);
err1:
	return NULL;
}

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

uint64_t calculate_total_fitness(bitmap_t *bmp){
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

inline int pix_fitness(int p1,int p2){
	int fitness = 0;
	fitness += ((p1&0xff) - (p2&0xff))*((p1&0xff) - (p2&0xff));
	fitness += (((p1>>8)&0xff) - ((p2>>8)&0xff))*(((p1>>8)&0xff) - ((p2>>8)&0xff));
	fitness += (((p1>>16)&0xff) - ((p2>>16)&0xff))*(((p1>>16)&0xff) - ((p2>>16)&0xff));
	return fitness;
}

int calculate_fitness(bitmap_t *bmp,int p1,uint32_t p1_val){
	int x, y;

	int fitness = 0;
	int bmpsize = bmp->width*bmp->height;
	//this code wraps top/bottom and left/right
	//This allows the final images to be tiled
	if(p1 - bmp->width >= 0)
		fitness += pix_fitness(p1_val,bmp->pixels[p1 - bmp->width]);
	else
		fitness += pix_fitness(p1_val,bmp->pixels[bmpsize - p1]);
	if(p1 + bmp->width < bmpsize)
		fitness += pix_fitness(p1_val,bmp->pixels[p1 + bmp->width]);
	else
		fitness += pix_fitness(p1_val,bmp->pixels[p1%bmp->width]);
	if(p1 - 1 >= 0)
		fitness += pix_fitness(p1_val,bmp->pixels[p1 - 1]);
	if(p1 + 1 < bmpsize)
		fitness += pix_fitness(p1_val,bmp->pixels[p1 + 1]);

	return fitness;
}

void swap_values(uint32_t *p1,uint32_t *p2){
	uint32_t tmp = *p1;
	*p1 = *p2;
	*p2 = tmp;
}

bool sdl_init(int width,int height,SDL_Surface **screen)
{
	//Initialize SDL
	if( SDL_Init( SDL_INIT_EVERYTHING ) < 0 )
	{
		printf("SDL_Init failed\n");
		return false;
	}

	atexit(SDL_Quit);

	//Create Window
	*screen = SDL_SetVideoMode( width, height, 32, 0 );
	if(screen == NULL )
	{
		printf("SDL_SetVideoMode failed\n");
		return false;
	}

	//Set caption
	SDL_WM_SetCaption( "colorgroup", NULL );

	return true;
}

void build_acceptance_prob(uint32_t *acceptance_prob,int sz,double t){
	int i;
	for(i = 0;i < sz;i++){
		acceptance_prob[i] = 0xFFFFFFFF*exp(-((double)i)/(((double)sz)*t));
	}
}

void do_simulated_annealing(double *t,uint64_t *improvements,double decay_rate,double sweep_rate,bitmap_t *bmp){
	uint32_t accept_prob[ACCEPT_SIZE];
	build_acceptance_prob(accept_prob,ACCEPT_SIZE,*t);
	int img_size = bmp->width*bmp->height;
	uint64_t i;
	uint64_t sweep_amount = sweep_rate*img_size;
	for(i = 0;i < sweep_amount;i++){
		//select 2 pixel locations
		int p1 = mt_lrand()%img_size;
		int p2 = mt_lrand()%img_size;

		const int p1_pix = bmp->pixels[p1];
		const int p2_pix = bmp->pixels[p2];

		//calculate unswapped fitness
		int fitness = 0;
		fitness += calculate_fitness(bmp,p1,p1_pix);
		fitness += calculate_fitness(bmp,p2,p2_pix);

		//calculate swapped fitness
		int fitness_swapped = 0;
		fitness_swapped += calculate_fitness(bmp,p1,p2_pix);
		fitness_swapped += calculate_fitness(bmp,p2,p1_pix);

		int delta_fitness = fitness_swapped - fitness;
		bool accept=true;
		if(delta_fitness < 0){
			accept = true;
		} else {
			if(delta_fitness >= ACCEPT_SIZE){
				accept = false;
			} else if(mt_lrand() > accept_prob[delta_fitness]){
				accept = false;
			}
		}

		if(accept){
			swap_values(&bmp->pixels[p1],&bmp->pixels[p2]);
			(*improvements)++;
		}
	}
	(*t) = (*t)*decay_rate;
}

int main(int argc,char *argv[]){
	bool help=false;
	bool saveoutput=true;
	bool sdloutput=false;
	int width=4096, height=4096;
	int initialmethod=0;
	int i=0;
	double initial_temp=0.2;
	double decay_rate=0.9999;
	double sweep_rate=0.3;
	SDL_Surface *screen;

	char resumefilename[4096];
	resumefilename[0] = 0;
	char imagefiletoload[4096];
	imagefiletoload[0] = 0;


	uint64_t iteration=0;
	uint64_t improvements=0;

	for(i=1;i < argc;i++){
		int arglen = strlen(argv[i]);
		char *arg = (char *)malloc(sizeof(char)*(arglen+1));
		strcpy(arg,argv[i]);

		if(strcmp(arg,"-h") == 0){
			help=true;
		}

		if(strcmp(arg,"-l") == 0){
			strcpy(imagefiletoload,&arg[2]);
		}

		if(arglen > 2 && arg[0] == '-' && arg[1] == 'i')
		{
			//printf("arg: %s\n",arg);
			char *commaindex = strchr(&arg[2],',');
			if(!commaindex){
				printf("malformed option -i, no <width,height> specified.\n");
				return 1;
				continue;
			}
			width = atoi(&arg[2]);
			height = atoi(commaindex+1);
		}
		if(arglen > 2 && arg[0] == '-' && arg[1] == 'p'){
			if(arg[2] > '0' && arg[2] < '9')
				initialmethod=atoi(&arg[2]);
		}
		if(strcmp(arg,"-S") == 0){
			saveoutput=false;
		}
		if(arglen > 2 && arg[0] == '-' && arg[1] == 't')
		{
			initial_temp = atof(&arg[2]);
		}
		
		if(arglen > 2 && arg[0] == '-' && arg[1] == 'r')
		{
			decay_rate = atof(&arg[2]);
		}
		if(arglen > 2 && arg[0] == '-' && arg[1] == 'w')
		{
			sweep_rate = atof(&arg[2]);
		}
		if(strcmp(arg,"-o") == 0){
			sdloutput=true;
		}
		//if a resume file is specified...
		if(arg[0] != '-'){
			if(strlen(arg) < 4096)
				strcpy(resumefilename,arg);
		}
		free(arg);
	}
	if(help){
		printf("Usage: colorgroup -h -l -i<WIDTH,HEIGHT> -p<0-10> -s/-S -t<INITIAL_TEMP> -r<DECAY_RATE> -w<SWEEP_RATE> -o <RESUME_FILENAME>\n");
		printf("-h Show this help message and exit.\n");
		printf("-l load image from output.png\n");
		printf("-i<WIDTH,HEIGHT> specify width and height of newly created image (default 4096x4096)\n");
		printf("-p<0-1> Specify the method of generating initial image\n");
		printf("\t\t 0 - Fill image with unique colors in the RGB spectrum (default)\n");
		printf("\t\t 1 - Fill with red\\blue gradient\n");
		printf("-s save to output.png and state_info periodically(default)  -S skip saving\n");
		printf("-o display output to SDL\n");
		printf("-t<INITIAL_TEMP> set initial simulated annealing temperature (default 0.2)\n");
		printf("-r<DECAY_RATE> set simulated annealing decay rate (default 0.9999)\n");
		printf("-w<SWEEP_RATE> Set simulated annealing sweep rate. (default 0.3)\n");
		printf("\tSweeps are done using Sweep Rate as a percentage of the area of the image\n");
		printf("<RESUME_FILENAME> This points to a state_info file that was previously saved.\n");
		printf("For loading parameters from that file and resuming");
		return 0;
	}

	//if a resume file was specified, then we actually need to load parameters from it.
	if(resumefilename[0] != 0){
		FILE *fp = fopen(resumefilename,"r");
		if(fp == NULL){
			printf("ERROR Opening Resume File: %s\n",resumefilename);
			return 1;
		}
		char line[4096];
		//read skipping comment lines that start with #
		while(1){
			fgets(line,4096,fp);
			if(line[0] != '#')break;
		}
		char *tok;
		tok = strtok(line,",\r\n");
		iteration = atoi(tok);
		tok = strtok(NULL,",\r\n");
		initial_temp = atof(tok);
		tok = strtok(NULL,",\r\n");
		decay_rate = atof(tok);
		tok = strtok(NULL,",\r\n");
		sweep_rate = atof(tok);
		tok = strtok(NULL,",\r\n");
		strcpy(imagefiletoload,tok);
		fclose(fp);
	}


	printf("colorgroup - Program to group colors via simulated annealing\n");
    printf("NOTE: use -h to see help for supported arguments\n");
	printf("Running with arguments: \n");
	printf("\tload image from output.png: %s\n",(imagefiletoload[0])?imagefiletoload:"false");
	if(!imagefiletoload[0]){
		printf("\twidth: %d height: %d\n",width,height);
		printf("\timage generation method (-p): %d\n",initialmethod);
	}
	printf("\tInitial Temperature: %lf\n",initial_temp);
	printf("\tDecay Rate: %lf\n",decay_rate);
	printf("\tSweep Rate: %lf\n",sweep_rate);
	printf("\tperiodically save to output.png: %s\n",(saveoutput)?"true":"false");
	printf("\tSDL output: %s\n",(sdloutput)?"true":"false");


	mt_seed();
	bitmap_t *b;
	if(!imagefiletoload[0]){
			//Generate initial image
			b = (bitmap_t *)malloc(sizeof(bitmap_t));
			b->pixels = (uint32_t *)malloc(sizeof(uint32_t)*width*height);
			b->width = width;
			b->height = height;

			int32_t *tmp_pixels = (int32_t *)malloc(sizeof(uint32_t)*4096*4096);

			//fill tmp_pixels with ordered set of all possible colors
			int x;
			for(x = 0;x < 4096*4096;x++){
				if(initialmethod == 1)
					tmp_pixels[x] = x&0xFFFF00FF;
				else
					tmp_pixels[x] = x;
			//select values randomly out of tmp_pixels to fill p->pixels
			i = width*height;
			printf("doing random selection...\n");
			while(i > 0){
				int selection = mt_lrand()%(4096*4096);
				if(tmp_pixels[selection] == -1)
					continue;
				b->pixels[i] = tmp_pixels[selection];
				tmp_pixels[selection] = -1;
				i--;
			}
			printf("done random selection...\n");
			free(tmp_pixels);

		}
	} else { //load_image
		b = load_png_from_file(imagefiletoload);
		if(b == NULL){
			printf("Error Loading PNG\n");
			return 1;
		}
		width = b->width;
		height = b->height;
	}

	//initialize SDL (if applicable)
	if(sdloutput){
		if(!sdl_init(width,height,&screen)){
			return 1;
		}
	}

	int img_size = width*height;


	//setup simulated annealing
	double t=initial_temp;

	//setp SDL stuff
	bool quit=false;
	SDL_Event event;
	Uint32 rmask, gmask, bmask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
#endif
	SDL_Surface *bitmap_surface = SDL_CreateRGBSurfaceFrom(b->pixels,b->width,b->height,32,b->width*4,rmask,gmask,bmask,0);


	while(!quit){
		//SDL_GetTicks();
		if(sdloutput && iteration%2==0){
			
			while(SDL_PollEvent(&event)){
				if(event.type == SDL_QUIT){
					quit=true;
				}
			}
			//render image to SDL
			//todo: time to 60hz

			SDL_Rect rect={0,0,width,height};
			SDL_BlitSurface(bitmap_surface,&rect,screen,&rect);
			SDL_Flip(screen);
			
		}

		do_simulated_annealing(&t,&improvements,decay_rate,sweep_rate,b);
		if(iteration%50 == 0){
			uint64_t total_fitness = calculate_total_fitness(b);
			printf("iteration %ld, temperature %lf, total_fitness %ld\n",iteration,
					t,total_fitness);
			improvements=0;
		}
			
		if(saveoutput && iteration%1000 == 0){
			printf("Saving PNG and State Files ... ");
			if(save_png_to_file(b,"output.png") == -1){
				printf("Error saving\n");
			}
			FILE *statefile=fopen("state_info","w");
			fprintf(statefile,"#iteration,temperature,decay_rate,sweep_rate,filename\n");
			fprintf(statefile,"%ld,%lf,%lf,%lf,%s\n",iteration,t,decay_rate,sweep_rate,"output.png");
			fclose(statefile);
			printf("Done\n");
			
		}
		iteration++;
	}


	free(b->pixels);
	free(b);

	return 0;
}
