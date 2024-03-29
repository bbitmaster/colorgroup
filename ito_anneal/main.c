#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>
#include <stdbool.h>

long long ebest, ene;

// Bits per plane. Must be even and <=8, namely either 2, 4, 6, 8
#define CBIT 6
// Misc. consts
#define VMAX (1<<CBIT)
#define CBMASK ( VMAX-1)
#define L (1<<(CBIT + CBIT/2))
#define LMASKX (L-1)
#define L2 (L*L)
#define LMASK (L2-1)
#define LMASKY (LMASK ^ LMASKX)

// main color data
unsigned int col[L2];
// save the best snapshot
unsigned int colbest[L2];
void
saveit ()
{
  int i;
  for (i = 0; i < L2; i++)
	colbest[i] = col[i];
}

// color->pos index
unsigned int cidx[L2];

/////// color conversion macros
// put 8bit r,g,b data into one 32 bit int separated by 2 padding bits which allows addition
//  of upto 4 data (=number of neighbors) without rgb interference
#define PADBIT 10
#define PADMASK ((1<<PADBIT)-1)
#define rgb2col(r,g,b) ( (r)|((g)<<PADBIT)|((b)<<(PADBIT*2)))
#define col2rgb(col, r,g,b) { r=(col)&PADMASK; g = ((col)>>PADBIT)&PADMASK; b=((col)>>(2*PADBIT))&PADMASK;}
// serial color index
#define rgb2idx(r,g,b) ( (r)|((g)<<CBIT)|((b)<<(CBIT*2)))


 // max possible 1-pixel energy
#define E1MAX (VMAX*VMAX*4*3)
// Metropolis threshold table
 // energy difference of swapping 2 pixels can be as large as 2*E1MAX
static int thtab[E1MAX * 2 + 10];

void
init_th (double beta, int imax)
{
  int i;
  thtab[0] = imax / 2;			// 50% chance for microcanonical change
  if (beta < 0.0)
	{
	  // quench case
	  for (i = 1; i < E1MAX * 2 + 10; i++)
		thtab[i] = 0;
	  return;
	}
  for (i = 1; i < E1MAX * 2 + 10; i++)
	{
	  thtab[i] = (int) (imax * exp (-i * beta));
	}
}

// dump color to a file
void
dump (int idx, unsigned int *col)
{
  unsigned char *pbuf = (unsigned char *) malloc (L2 * 3);
  char fn[128];
  sprintf (fn, "ppm/%04d.ppm", idx);
  FILE *fp = fopen (fn, "w");
  if (fp == NULL)
	{
	  printf ("cannot open %s for dump\n", fn);
	  exit (1);
	}
  int i;
  int vv = 255 / (VMAX - 1);
  for (i = 0; i < L2; i++)
	{
	  int r, g, b;
	  col2rgb (col[i], r, g, b);
	  pbuf[i * 3 + 0] = r * vv;
	  pbuf[i * 3 + 1] = g * vv;
	  pbuf[i * 3 + 2] = b * vv;
	}
  fprintf (fp, "P6\n%d %d\n255\n", L, L);
  fwrite (pbuf, 1, L2 * 3, fp);
  fclose (fp);
  free (pbuf);
}

// resume from an image file
//  or import smaller size image and expand, add lower bit data
void
load_ppm (char *fn)
{
  char buf[128];
  FILE *fp = fopen (fn, "r");
  if (fp == NULL)
	{
	  printf ("# cannot open file %s\n", fn);
	  exit (1);
	}
  int ll;
  fgets (buf, 99, fp);
  fgets (buf, 99, fp);
  sscanf (buf, "%d", &ll);
  fgets (buf, 99, fp);
  printf ("#Loading %s L=%d\n", fn, ll);

  if (ll == L)
	{
	  //resume
	  printf ("  #Same size data\n");
	  unsigned char *pbuf = (unsigned char *) malloc (L2 * 3);
	  fread (pbuf, 1, L2 * 3, fp);
	  fclose (fp);
	  printf ("  # read complete\n");
	  int vv = 255 / (VMAX - 1);
	  int i;
	  for (i = 0; i < L2; i++)
		{
		  unsigned char *c = &pbuf[i * 3];
		  col[i] = rgb2col (c[0] / vv, c[1] / vv, c[2] / vv);
		}
	  free (pbuf);
	  return;
	}
  if (ll == L / 8)
	{
	  // expand CBIT-2 image
	  printf ("  # 1/8 size data\n");
	  unsigned char *pbuf = (unsigned char *) malloc (L2 * 3 / 64);
	  fread (pbuf, 1, L2 * 3 / 64, fp);
	  fclose (fp);
	  printf ("  # read complete\n");

	  int ofs[8][8][3];
	  int x, y, xx, yy;
	  int vv = 255 / (VMAX / 4 - 1);

	  // fine bit part
	  for (x = 0; x < 8; x++)
		{
		  for (y = 0; y < 8; y++)
			{
			  ofs[x][y][0] = x / 2;
			  ofs[x][y][1] = y / 2;
			  ofs[x][y][2] = (x & 1) + (y & 1) * 2;
			}
		}

	  //block
	  for (x = 0; x < L; x += 8)
		{
		  for (y = 0; y < L; y += 8)
			{
			  int ad0 = y * L + x;
			  int ad00 = ((y / 8) * ll + (x / 8)) * 3;
			  int r = 4 * (pbuf[ad00 + 0] / vv);
			  int g = 4 * (pbuf[ad00 + 1] / vv);
			  int b = 4 * (pbuf[ad00 + 2] / vv);

			  for (xx = 0; xx < 8; xx++)
				{
				  for (yy = 0; yy < 8; yy++)
					{
					  int rr = r + ofs[xx][yy][0];
					  int gg = g + ofs[xx][yy][1];
					  int bb = b + ofs[xx][yy][2];
					  col[ad0 + xx + yy * L] = rgb2col (rr, gg, bb);
				}}
		}}

	  free (pbuf);
	  return;
	}
  printf ("#Couldn't load %s\n", fn);
  exit (1);
}

// old;left+right+up+down, screw boundary config
// left+right+up+down, periodic bndry
int
sjsum (int ad)
{
  int x = ad & LMASKX;
  int ad0 = ad & LMASKY;
  int x1 = (x - 1) & LMASKX;
  int x2 = (x + 1) & LMASKX;

  return +col[ad0 | x1]			//left
	+ col[ad0 | x2]				//right
	+ col[(ad + L) & LMASK]		//down
	+ col[(ad - L) & LMASK];	//up
}


// note that (s1-s2)^2 = s1^2 + s2^2 - 2 s1 s2
//  and \sum s^2 remains constant
//  therefore energy= const -2 \sum_(all neighbor pixel pair si, sj) si*sj
//                  = const -2 \sum_(all pixel si)  si * (left+right+up+down)  /2 (/2 to cancel double count)
//   actually use energy/2 = \sum_ij -si*sj for MC
int
getene1 (int ad)
{
  int sj = sjsum (ad);
  int sjr, sjg, sjb;
  col2rgb (sj, sjr, sjg, sjb);

  int r, g, b, c;
  c = col[ad];
  col2rgb (c, r, g, b);

  return -r * sjr - g * sjg - b * sjb;
}

// total energy
//  with x2 factor 
long long
calc_ene ()
{
  int i;
  long long res = 0;
  for (i = 0; i < L2; i++)
	{
	  int r, g, b;
	  int c = col[i];
	  col2rgb (c, r, g, b);
	  res += 4 * (r * r + g * g + b * b) + getene1 (i);
	}
  return res;
}

// standard Metropolis step, attempt swapping pixel at ad1 and ad2
bool
domc (int ad1, int ad2)
{
  int c1 = col[ad1];
  int c2 = col[ad2];

  //int sj1 = sjsum(ad1);
  //int sj2 = sjsum(ad2);

  int y1 = ad1 & LMASKY;
  int y2 = ad2 & LMASKY;
  int x1 = ad1 & LMASKX;
  int x2 = ad2 & LMASKX;

  int x1L = (x1 - 1) & LMASKX;
  int x1R = (x1 + 1) & LMASKX;
  int x2L = (x2 - 1) & LMASKX;
  int x2R = (x2 + 1) & LMASKX;
printf("LL=%d, LMASKY=%d, LMASKX=%d,y1=%d x1L=%d x1R=%d\n",L2,LMASKY,LMASKX,y1, x1L,x1R);
  int c1L = col[y1 | x1L];
  int c1R = col[y1 | x1R];
  int c1U = col[(ad1 - L) & LMASK];
  int c1D = col[(ad1 + L) & LMASK];
  int sj1 = c1L + c1R + c1U + c1D;
  int sj2 =
	col[y2 | x2L] + col[y2 | x2R] + col[(ad2 - L) & LMASK] +
	col[(ad2 + L) & LMASK];

  // undo double count
  if (c2 == c1L || c2 == c1R || c2 == c1D || c2 == c1U)
	{
	  sj1 -= c2;
	  sj2 -= c1;
	}

  int sj1r, sj1g, sj1b;
  int sj2r, sj2g, sj2b;
  col2rgb (sj1, sj1r, sj1g, sj1b);
  col2rgb (sj2, sj2r, sj2g, sj2b);

  int r1, g1, b1;
  int r2, g2, b2;
  col2rgb (c1, r1, g1, b1);
  col2rgb (c2, r2, g2, b2);

  // energy increase by swapping
  int de =
	(sj1r - sj2r) * (r1 - r2) + (sj1g - sj2g) * (g1 - g2) + (sj1b -
															 sj2b) * (b1 -
																	  b2);

  //printf("%d: %d %d %d  %d %d %d\n",de, r1,g1,b1,r2,g2,b2);

  bool accp = true;
  if (de >= 0)
	{
	  if (rand () > thtab[de])
		accp = false;
	}

  if (accp)
	{
	  //swap
	  col[ad1] = c2;
	  col[ad2] = c1;
	  ene += de;
	  cidx[rgb2idx (r1, g1, b1)] = ad2;
	  cidx[rgb2idx (r2, g2, b2)] = ad1;
	  //printf("%lld\n",ene);

	  if (ene < ebest)
		{
		  ebest = ene;
		  saveit ();
		}

	  return true;
	}
  return false;
}

//////////////////////////////////////////////////////////
//// Misc Metropolis wrapper

// find the best pixel for the spot ad1, and try to move it to ad1
void
optimize1 (int ad1)
{
  int sj1r, sj1g, sj1b;
  int sj1 = sjsum (ad1) + rgb2col (2, 2, 2);
  col2rgb (sj1, sj1r, sj1g, sj1b);
  sj1r >>= 2;
  sj1g >>= 2;
  sj1b >>= 2;

  int ad2 = cidx[rgb2idx (sj1r, sj1g, sj1b)];

  int di = (ad1 - ad2) & LMASK;
  if (di == 0)
	return;
  bool acp = domc (ad1, ad2);
  //if (acp) optimize1(ad2);
}

// sweep using optimize1: seems biased to low energy
//  useful for quenching
void
sweep_o ()
{
  int i;
  for (i = 0; i < L2; i++)
	optimize1 (i);
}

// swap pixels based on real-space positions
void
sweep_x (int dx)
{
  int i, x, y;
  for (x = 0; x <= dx; x++)
	{
	  for (y = 0; y <= dx; y++)
		{
		  if (x + y == 0)
			continue;
		  if (x + y > dx)
			continue;
		  int da = x + y * L;

		  for (i = 0; i < L2; i++)
			domc (i, (i + da) & LMASK);
		}
	}
}

// swap pixels based on RGB-space positions
//  most efficient
void
sweep_c (int dc)
{
  int r, g, b, ad, x, y;
  int sr, sg, sb;

  for (sr = 0; sr <= dc; sr++)
	{
	  for (sg = 0; sg <= dc; sg++)
		{
		  for (sb = 0; sb <= dc; sb++)
			{
			  if (sr + sg + sb == 0)
				continue;
			  if (sr + sg + sb > dc)
				continue;

			  for (g = 0; g < VMAX - sg; g++)
				{
				  for (b = 0; b < VMAX - sb; b++)
					{
					  for (r = 0; r < VMAX - sr; r++)
						{
						  int ad1 = cidx[rgb2idx (r, g, b)];
						  int ad2 = cidx[rgb2idx (r + sr, g + sg, b + sb)];
						  domc (ad1, ad2);
				}}}
	}}}
}

// energy-corr
int nE;
#define DMAX 100000
double edat[DMAX];

void
storee ()
{
  edat[nE++] = ene * 1.0 / L2;
}

void
ecor (double *e1, double *cv, double *cor)
{
  double e1s = 0.0;
  double e2s = 0.0;
  double ecs = 0.0;
  int i, s;

  for (i = 0; i < nE; i++)
	{
	  double e = edat[i];
	  e1s += e;
	  e2s += e * e;
	}
  e1s /= nE;
  e2s /= nE;
  e2s -= e1s * e1s;

  for (s = 1; s < nE / 10; s++)
	{
	  double sum = 0;
	  for (i = 0; i < nE - s; i++)
		sum += (edat[i] - e1s) * (edat[i + s] - e1s);
	  sum /= (nE - s);
	  sum /= e2s;

	  ecs += sum;
	}

  *e1 = e1s;
  *cv = e2s;
  *cor = ecs;
}

int
main (int argc, char **argv)
{

  int i;

  //init
  for (i = 0; i < L2; i++)
	{
	  int r = i & CBMASK;
	  int g = (i >> CBIT) & CBMASK;
	  int b = (i >> (CBIT * 2)) & CBMASK;
	  col[i] = rgb2col (r, g, b);
	}

  if (argc == 2)
	load_ppm (argv[1]);

  for (i = 0; i < L2; i++)
	{
	  int c = col[i];
	  int r, g, b;
	  col2rgb (c, r, g, b);
	  cidx[rgb2idx (r, g, b)] = i;
	}
  ene = calc_ene () / 2;
  ebest = ene;
  saveit ();

  double beta_max = 3.0;
  double beta_min = 3.0 * 1e-3;
  int tdiv = 1000;				// number of temperature
  int maxs = 500;				// number of sweeps per temperature
  int t, s;

  FILE *fp = fopen ("elog", "w");

  while (1)
	{

	  //Begin
	  for (t = 0; t < tdiv; t++)
		{
		  double beta = beta_min * exp (log (beta_max / beta_min) / tdiv * t);
		  init_th (beta, RAND_MAX);
		  nE = 0;

		  for (s = 0; s < maxs; s++)
			{
			  sweep_c (1);
			  //sweep_x(1);
			  //sweep_o();
			  if (s > maxs / 10)
				storee ();
			}
		  double e1, cv, cr;
		  ecor (&e1, &cv, &cr);
		  fprintf (fp, "%e %f %e %f\n", beta, e1, cv, cr);
		  fflush (fp);
		  dump (2, col);
		  dump (0, colbest);
		}
	  fprintf (fp, "\n\n");

	}							//loop

  fclose (fp);

}
