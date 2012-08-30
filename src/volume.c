/* volume.c */


/*
 * Vis5D system for visualizing five dimensional gridded data sets.
 * Copyright (C) 1990 - 2000 Bill Hibbard, Johan Kellum, Brian Paul,
 * Dave Santek, and Andre Battaiola.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * As a special exception to the terms of the GNU General Public
 * License, you are permitted to link Vis5D with (and distribute the
 * resulting source and executables) the LUI library (copyright by
 * Stellar Computer Inc. and licensed for distribution with Vis5D),
 * the McIDAS library, and/or the NetCDF library, where those
 * libraries are governed by the terms of their own licenses.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "../config.h"

/*
 * Volume rendering.  Requires alpha blending capability.  This is
 * supported in on SGI hardware with VGX or higher graphics.  Otherwise,
 * it's done in software on some other systems.
 *
 * Volume rendering is also supported on everthing running OpenGL!
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "globals.h"
#include "graphics.h"
#include "grid.h"
#include "memory.h"
#include "proj.h"
#include "volume.h"

#if HAVE_OPENGL
#  include <GL/gl.h>
#elif HAVE_SGI_GL
#  include <gl/gl.h>
#endif

#define ABS(A)  ( (A) < 0 ? -(A) : (A) )


/* Direction of slices: */
#define BOTTOM_TO_TOP  0
#define TOP_TO_BOTTOM  1
#define EAST_TO_WEST   2
#define WEST_TO_EAST   3
#define NORTH_TO_SOUTH 4
#define SOUTH_TO_NORTH 5

#define LUT_SIZE 255




/*
 * Allocate a new volume structure and return a pointer to it.  If we
 * can't do volume rendering return NULL.
 * Input:  nr, nc, nl - dimensions of largest volume we'll encounter.
 * Return:  pointer to volume struct or NULL
 */
struct volume *alloc_volume( Context ctx, int nr, int nc, int nl )
{
   int volren = 0;
   struct volume *v = NULL;

   if (ctx->dpy_ctx->Projection == PROJ_CYLINDRICAL ||
       ctx->dpy_ctx->Projection == PROJ_SPHERICAL){
      ctx->dpy_ctx->VolRender = 0;
      return 0;
   }
   if (nl <= 1){
      ctx->dpy_ctx->VolRender = 0;
      return 0; /* no volume variables */
   }

#if defined (HAVE_SGI_GL) || defined (DENALI)
   alphabits = getgdesc( GD_BLEND ); 
   if (alphabits==0) {
      ctx->dpy_ctx->VolRender = 0; 
      /* no alpha support means no volume rendering */
      return NULL;
   }
   volren = 1;
#endif

#if defined(HAVE_OPENGL)
   volren = 1;
#endif

  /* MJK 12.15.98 */
#ifdef HAVE_PEX
   volren = 1;
#endif

   
   if (volren) {
      v = (struct volume *) malloc( sizeof(struct volume) );
      /* initialize the volume struct */
      v->valid = 0;
      /* No matter which way we slice it, we need the same size arrays for */
      /* storing the vertices and colors. */
    //    fprintf(stderr,"totalvolumebytes=%d\n",nl*nr*nc*3*sizeof(float));
      v->vertex = (float *) allocate( ctx, nl*nr*nc*3*sizeof(float) );
      v->index = (uint_1 *) allocate( ctx, nl*nr*nc*sizeof(uint_1) );
      if (!v->vertex || !v->index) {
         printf("WARNING:  insufficient memory for volume rendering (%d bytes needed)\n",
                nl * nr * nc * ((int) (3*sizeof(float)+sizeof(uint_1))) );
         ctx->dpy_ctx->VolRender = 0; 
         return NULL;
      }
      v->oldnr = nr;
      v->oldnc = nc;
      v->oldnl = nl;
    /* MJK 12.15.98 */
#ifdef HAVE_PEX 
      v->dir = -1;
#endif

   }
   if (v){
      ctx->dpy_ctx->VolRender = 1;
   }
   else{
      ctx->dpy_ctx->VolRender = 0;
   }
   return v;
}

void free_volume( Context ctx)
{

  int j;
  int truenumvars;

  // JCM
  truenumvars=(ctx->NumVars > MAXVOLUMEVARS) ? MAXVOLUMEVARS : ctx->NumVars;

  // JCM
  for(j=0;j<truenumvars;j++){
    if(ctx->Volume[j]){
      deallocate( ctx, ctx->Volume[j]->vertex,
		  ctx->Volume[j]->oldnl*ctx->Volume[j]->oldnr*ctx->Volume[j]->oldnc*3*sizeof(float));
      deallocate( ctx, ctx->Volume[j]->index,
		  ctx->Volume[j]->oldnl*ctx->Volume[j]->oldnr*ctx->Volume[j]->oldnc*sizeof(uint_1));
      free(ctx->Volume[j]);
      ctx->Volume[j] = NULL;
    }
  }
}



/*
 * Compute a volume rendering of the given grid.
 * Input:  data - 3-D data grid.
 *         time, var - time step and variable
 *         nr, nc, nl - size of 3-D grid.
 *         min, max - min and max data value in data grid.
 *         dir - direction to do slicing
 *         v - pointer to a volume struct with the vertex and index
 *             fields pointing to sufficiently large buffers.
 * Output:  v - volume struct describing the computed volume.
 */
static int compute_volume( Context ctx, float data[],
                           int time, int var, int volvar, int numvolvars,
                           int nr, int nc, int nl, int lowlev,
                           float min, float max,
                           int dir,
                           struct volume *v)
{
   Display_Context dtx;
   float zs[MAXLEVELS];
   register int ir, ic, il, i, j;
   register float x, y, dx, dy;
   register float dscale, val;
   register int ilnrnc, icnr;           /* to help speed up array indexing */
  float dfrac,dz;

  // setup shift for each variable
  dfrac=(double)volvar*0.1/((double)numvolvars);

   dtx = ctx->dpy_ctx;

  if(dir==BOTTOM_TO_TOP){
    /* compute graphics Z for each grid level */
    for (il=0; il<nl; il++) {
      if(il!=0){
	dz = gridlevel_to_z(ctx, time, var, (float) (il + lowlev)) - gridlevel_to_z(ctx, time, var, (float) (il-1 + lowlev));
      }
      else{
	dz = gridlevel_to_z(ctx, time, var, (float) (il+1 + lowlev)) - gridlevel_to_z(ctx, time, var, (float) (il + lowlev));
      }
      zs[il] = gridlevel_to_z(ctx, time, var, (float) (il + lowlev))         +dz*dfrac;
    }
  }
  else if(dir==TOP_TO_BOTTOM){
    /* compute graphics Z for each grid level */
    for (il=0; il<nl; il++) {
      if(il!=0){
	dz = gridlevel_to_z(ctx, time, var, (float) (il + lowlev)) - gridlevel_to_z(ctx, time, var, (float) (il-1 + lowlev));
      }
      else{
	dz = gridlevel_to_z(ctx, time, var, (float) (il+1 + lowlev)) - gridlevel_to_z(ctx, time, var, (float) (il + lowlev));
      }
      zs[il] = gridlevel_to_z(ctx, time, var, (float) (il + lowlev))         -dz*dfrac;
    }
  }
  else{
   /* compute graphics Z for each grid level */
   for (il=0; il<nl; il++) {
     zs[il] = gridlevel_to_z(ctx, time, var, (float) (il + lowlev));
   }
  }

   /* compute some useful values */
   dx = (dtx->Xmax-dtx->Xmin) / (nc-1);
   dy = (dtx->Ymax-dtx->Ymin) / (nr-1);
   dscale = (float) (LUT_SIZE-1) / (max-min);

   v->dir = dir;

   switch (dir) {
      case BOTTOM_TO_TOP:
         /* compute slices from bottom to top */
         v->slices = nl;
         v->rows = nr;
         v->cols = nc;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
         for (il=0; il<nl; il++) {
            y = dtx->Ymax;
            ilnrnc = il * nr * nc;
            for (ir=0;ir<nr;ir++) {
               x = dtx->Xmin;
               for (ic=0;ic<nc;ic++) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  val = data[ ilnrnc + ic * nr + ir ];
                  if (IS_MISSING(val) ||
                      val < min || val > max)
                    v->index[j++] = 255;
                  else
                    v->index[j++] = (uint_1) (int) ((val-min) * dscale);

                  x += dx;
               }
	y -= dy; // JCM LEFT HAND GODMARK (and others as such)
            }
         }
         break;

      case TOP_TO_BOTTOM:
         /* compute slices from top to bottom */
         v->slices = nl;
         v->rows = nr;
         v->cols = nc;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
         for (il=nl-1; il>=0; il--) {
            y = dtx->Ymax;
            ilnrnc = il * nr * nc;
            for (ir=0;ir<nr;ir++) {
               x = dtx->Xmin;
               for (ic=0;ic<nc;ic++) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  val = data[ ilnrnc + ic * nr + ir ];
                  if (IS_MISSING(val) ||
                      val < min || val > max)
                    v->index[j++] = 255;
                  else
                    v->index[j++] = (uint_1) (int) ((val-min) * dscale);

                  x += dx;
               }
               y -= dy;
            }
         }
         break;

      case WEST_TO_EAST:
         /* compute slices from west to east */
         v->slices = nc;
         v->rows = nl;
         v->cols = nr;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
    x = dtx->Xmin                                  +dx*dfrac;
         for (ic=0; ic<nc; ic++) {
            icnr = ic * nr;
            for (il=nl-1;il>=0;il--) {
               y = dtx->Ymin;
               ilnrnc = il * nr * nc + icnr;
               for (ir=nr-1;ir>=0;ir--) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  val = data[ ilnrnc + ir ];
                  if (IS_MISSING(val) ||
                      val < min || val > max)
                    v->index[j++] = 255;
                  else
                    v->index[j++] = (uint_1) (int) ((val-min) * dscale);

                  y += dy;
               }
            }
            x += dx;
         }
         break;

      case EAST_TO_WEST:
         /* compute slices from east to west */
         v->slices = nc;
         v->rows = nl;
         v->cols = nr;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
    x = dtx->Xmax                                  -dx*dfrac;
         for (ic=nc-1; ic>=0; ic--) {
            icnr = ic*nr;
            for (il=nl-1;il>=0;il--) {
               y = dtx->Ymin;
               ilnrnc = il * nr * nc + icnr;
               for (ir=nr-1;ir>=0;ir--) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  val = data[ ilnrnc + ir ];
                  if (IS_MISSING(val) ||
                      val < min || val > max)
                    v->index[j++] = 255;
                  else
                    v->index[j++] = (uint_1) (int) ((val-min) * dscale);

                  y += dy;
               }
            }
            x -= dx;
         }
         break;

      case NORTH_TO_SOUTH:
         /* compute slices from north to south */
         v->slices = nr;
         v->rows = nl;
         v->cols = nc;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
    y = dtx->Ymax                                  -dy*dfrac;
         for (ir=0; ir<nr; ir++) {
            for (il=nl-1;il>=0;il--) {
               x = dtx->Xmin;
               ilnrnc = il * nr * nc;
               for (ic=0;ic<nc;ic++) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  val = data[ ilnrnc + ic*nr + ir ];
                  if (IS_MISSING(val) ||
                      val < min || val > max)
                    v->index[j++] = 255;
                  else
                    v->index[j++] = (uint_1) (int) ((val-min) * dscale);

                  x += dx;
               }
            }
            y -= dy;
         }
         break;

      case SOUTH_TO_NORTH:
         /* compute slices from south to north */
         v->slices = nr;
         v->rows = nl;
         v->cols = nc;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
    y = dtx->Ymin                                  +dy*dfrac;
         for (ir=nr-1; ir>=0; ir--) {
            for (il=nl-1;il>=0;il--) {
               x = dtx->Xmin;
               ilnrnc = il * nr * nc;
               for (ic=0;ic<nc;ic++) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  val = data[ ilnrnc + ic*nr + ir ];
                  if (IS_MISSING(val) ||
                      val < min || val > max)
                    v->index[j++] = 255;
                  else
                    v->index[j++] = (uint_1) (int) ((val-min) * dscale);

                  x += dx;
               }
            }
            y += dy;
         }
         break;

      default:
         printf("Error in compute_volume!\n");

   } /* switch */

   v->valid = 1;
   return 1;
}

static int compute_volumePRIME( Context ctx, float data[],
				int time, int var, int volvar, int numvolvars,
                           int nr, int nc, int nl, int lowlev,
                           float min, float max,
                           int dir,
				struct volume *v)
{
   Display_Context dtx;
   float zs[MAXLEVELS];
   register int ir, ic, il, i, j;
   register float x, y, dx, dy;
   register float dscale, val;
   register int ilnrnc, icnr;           /* to help speed up array indexing */
   float s1,s2,s3,s4,s5,s6,s7,s8;
   float grow, gcol, glev;
   float row, col, lev;
   int gr0,gr1,gc0,gc1,gl0,gl1;
   float ger, gec, gel;
  float dfrac,dz;

  // setup shift for each variable
  dfrac=(double)volvar*0.1/((double)numvolvars);

   dtx = ctx->dpy_ctx;

  if(dir==BOTTOM_TO_TOP){
   /* compute graphics Z for each grid level */
   for (il=0; il<nl; il++) {
      if(il!=0){
	dz = gridlevel_to_z(ctx, time, var, (float) (il + lowlev)) - gridlevel_to_z(ctx, time, var, (float) (il-1 + lowlev));
      }
      else{
	dz = gridlevel_to_z(ctx, time, var, (float) (il+1 + lowlev)) - gridlevel_to_z(ctx, time, var, (float) (il + lowlev));
      }
      zs[il] = gridlevel_to_z(ctx, time, var, (float) (il + lowlev))         +dz*dfrac;
    }
  }
  else if(dir==TOP_TO_BOTTOM){
    /* compute graphics Z for each grid level */
    for (il=0; il<nl; il++) {
      if(il!=0){
	dz = gridlevel_to_z(ctx, time, var, (float) (il + lowlev)) - gridlevel_to_z(ctx, time, var, (float) (il-1 + lowlev));
      }
      else{
	dz = gridlevel_to_z(ctx, time, var, (float) (il+1 + lowlev)) - gridlevel_to_z(ctx, time, var, (float) (il + lowlev));
      }
      zs[il] = gridlevel_to_z(ctx, time, var, (float) (il + lowlev))         -dz*dfrac;
    }
  }
  else{
    /* compute graphics Z for each grid level */
    for (il=0; il<nl; il++) {
      zs[il] = gridlevel_to_z(ctx, time, var, (float) (il + lowlev));
    }
   }

   /* compute some useful values */
   dx = (dtx->Xmax-dtx->Xmin) / (nc-1);
   dy = (dtx->Ymax-dtx->Ymin) / (nr-1);
   dscale = (float) (LUT_SIZE-1) / (max-min);

   v->dir = dir;

   switch (dir) {
      case BOTTOM_TO_TOP:
         /* compute slices from bottom to top */
         v->slices = nl;
         v->rows = nr;
         v->cols = nc;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
         for (il=0; il<nl; il++) {
            y = dtx->Ymax;
            ilnrnc = il * nr * nc;
            for (ir=0;ir<nr;ir++) {
               x = dtx->Xmin;
               for (ic=0;ic<nc;ic++) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  row = ir;
                  col = ic;
                  lev = il;
                  gridPRIME_to_grid( ctx, time, var, 1, &row, &col, &lev,
                                      &grow, &gcol, &glev);
                  if ( grow < 0 || gcol < 0 || glev < 0 ||
                    grow >= ctx->Nr || gcol >= ctx->Nc || glev >= ctx->Nl[var]){
                     v->index[j++] = 255;
                  }
                  else{
                     gr0= (int) grow;
                     gr1= gr0+1;
                     if (gr0 == ctx->Nr-1){
                        gr1 = gr0;
                     }
                     gc0 = (int) gcol;
                     gc1 = gc0 + 1;
                     if (gc0 == ctx->Nc-1){
                        gc1 = gc0;
                     }
                     gl0 = (int) glev;
                     gl1 = gl0+1;
                     if (gl0 == ctx->Nl[var]-1){
                        gl1 = gl0;
                     }

                     ger = grow - (float) gr0; /* in [0,1) */
                     gec = gcol - (float) gc0; /* in [0,1) */
                     gel = glev - (float) gl0; /* in [0,1) */

                     if (ger==0.0){
                        gr1 = gr0;
                     }
                     if (gec==0.0){
                        gc1 = gc0;
                     }
                     if (gel==0.0){
                        gl1 = gl0;
                     }

                     s1 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s2 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s3 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s4 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr1];
                     s5 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s6 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s7 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s8 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr1];

                     if (IS_MISSING(s1) || IS_MISSING(s2) ||
                         IS_MISSING(s3) || IS_MISSING(s4) ||
                         IS_MISSING(s5) || IS_MISSING(s6) ||
                         IS_MISSING(s7) || IS_MISSING(s8)){
                        v->index[j++] = 255;
                     }
                     else{
                        val = ( s1 * (1.0-ger) * (1.0-gec)
                             + s2 * ger       * (1.0-gec)
                             + s3 * (1.0-ger) * gec
                             + s4 * ger       * gec        ) * (1.0-gel)
                          +  ( s5 * (1.0-ger) * (1.0-gec)
                             + s6 * ger       * (1.0-gec)
                             + s7 * (1.0-ger) * gec
                             + s8 * ger       * gec        ) * gel;
                        if (val < min || val > max){
                           v->index[j++] = 255;
                        }
                        else{
                           v->index[j++] = (uint_1) (int) ((val-min) * dscale);
                        }
                     }
                  }
                  x += dx;
               }
               y -= dy;
            }
         }
         break;

      case TOP_TO_BOTTOM:
         /* compute slices from top to bottom */
         v->slices = nl;
         v->rows = nr;
         v->cols = nc;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
         for (il=nl-1; il>=0; il--) {
            y = dtx->Ymax;
            ilnrnc = il * nr * nc;
            for (ir=0;ir<nr;ir++) {
               x = dtx->Xmin;
               for (ic=0;ic<nc;ic++) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  row = ir;
                  col = ic;
                  lev = il;
                  gridPRIME_to_grid( ctx, time, var, 1, &row, &col, &lev,
                                      &grow, &gcol, &glev);
                  if ( grow < 0 || gcol < 0 || glev < 0 ||
                    grow >= ctx->Nr || gcol >= ctx->Nc || glev >= ctx->Nl[var]){
                     v->index[j++] = 255;
                  }
                  else{
                     gr0= (int) grow;
                     gr1= gr0+1;
                     if (gr0 == ctx->Nr-1){
                        gr1 = gr0;
                     }
                     gc0 = (int) gcol;
                     gc1 = gc0 + 1;
                     if (gc0 == ctx->Nc-1){
                        gc1 = gc0;
                     }
                     gl0 = (int) glev;
                     gl1 = gl0+1;
                     if (gl0 == ctx->Nl[var]-1){
                        gl1 = gl0;
                     }

                     ger = grow - (float) gr0; /* in [0,1) */
                     gec = gcol - (float) gc0; /* in [0,1) */
                     gel = glev - (float) gl0; /* in [0,1) */

                     if (ger==0.0){
                        gr1 = gr0;
                     }
                     if (gec==0.0){
                        gc1 = gc0;
                     }
                     if (gel==0.0){
                        gl1 = gl0;
                     }

                     s1 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s2 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s3 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s4 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr1];
                     s5 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s6 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s7 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s8 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr1];

                     if (IS_MISSING(s1) || IS_MISSING(s2) ||
                         IS_MISSING(s3) || IS_MISSING(s4) ||
                         IS_MISSING(s5) || IS_MISSING(s6) ||
                         IS_MISSING(s7) || IS_MISSING(s8)){
                        v->index[j++] = 255;
                     }
                     else{
                        val = ( s1 * (1.0-ger) * (1.0-gec)
                             + s2 * ger       * (1.0-gec)
                             + s3 * (1.0-ger) * gec
                             + s4 * ger       * gec        ) * (1.0-gel)
                          +  ( s5 * (1.0-ger) * (1.0-gec)
                             + s6 * ger       * (1.0-gec)
                             + s7 * (1.0-ger) * gec
                             + s8 * ger       * gec        ) * gel;
                        if (val < min || val > max){
                           v->index[j++] = 255;
                        }
                        else{
                           v->index[j++] = (uint_1) (int) ((val-min) * dscale);
                        }
                     }
                  }
                  x += dx;
               }
               y -= dy;
            }
         }
         break;

      case WEST_TO_EAST:
         /* compute slices from west to east */
         v->slices = nc;
         v->rows = nl;
         v->cols = nr;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
    x = dtx->Xmin                                 +dx*dfrac;
         for (ic=0; ic<nc; ic++) {
            icnr = ic * nr;
            for (il=nl-1;il>=0;il--) {
               y = dtx->Ymin;
               ilnrnc = il * nr * nc + icnr;
               for (ir=nr-1;ir>=0;ir--) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */

                  row = ir;
                  col = ic;
                  lev = il;
                  gridPRIME_to_grid( ctx, time, var, 1, &row, &col, &lev,
                                      &grow, &gcol, &glev);
                  if ( grow < 0 || gcol < 0 || glev < 0 ||
                    grow >= ctx->Nr || gcol >= ctx->Nc || glev >= ctx->Nl[var]){
                     v->index[j++] = 255;
                  }
                  else{
                     gr0= (int) grow;
                     gr1= gr0+1;
                     if (gr0 == ctx->Nr-1){
                        gr1 = gr0;
                     }
                     gc0 = (int) gcol;
                     gc1 = gc0 + 1;
                     if (gc0 == ctx->Nc-1){
                        gc1 = gc0;
                     }
                     gl0 = (int) glev;
                     gl1 = gl0+1;
                     if (gl0 == ctx->Nl[var]-1){
                        gl1 = gl0;
                     }

                     ger = grow - (float) gr0; /* in [0,1) */
                     gec = gcol - (float) gc0; /* in [0,1) */
                     gel = glev - (float) gl0; /* in [0,1) */

                     if (ger==0.0){
                        gr1 = gr0;
                     }
                     if (gec==0.0){
                        gc1 = gc0;
                     }
                     if (gel==0.0){
                        gl1 = gl0;
                     }

                     s1 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s2 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s3 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s4 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr1];
                     s5 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s6 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s7 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s8 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr1];

                     if (IS_MISSING(s1) || IS_MISSING(s2) ||
                         IS_MISSING(s3) || IS_MISSING(s4) ||
                         IS_MISSING(s5) || IS_MISSING(s6) ||
                         IS_MISSING(s7) || IS_MISSING(s8)){
                        v->index[j++] = 255;
                     }
                     else{
                        val = ( s1 * (1.0-ger) * (1.0-gec)
                             + s2 * ger       * (1.0-gec)
                             + s3 * (1.0-ger) * gec
                             + s4 * ger       * gec        ) * (1.0-gel)
                          +  ( s5 * (1.0-ger) * (1.0-gec)
                             + s6 * ger       * (1.0-gec)
                             + s7 * (1.0-ger) * gec
                             + s8 * ger       * gec        ) * gel;
                        if (val < min || val > max){
                           v->index[j++] = 255;
                        }
                        else{
                           v->index[j++] = (uint_1) (int) ((val-min) * dscale);
                        }
                     }
                  }
                  y += dy;
               }
            }
            x += dx;
         }
         break;

      case EAST_TO_WEST:
         /* compute slices from east to west */
         v->slices = nc;
         v->rows = nl;
         v->cols = nr;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
    x = dtx->Xmax                                     -dx*dfrac;
         for (ic=nc-1; ic>=0; ic--) {
            icnr = ic*nr;
            for (il=nl-1;il>=0;il--) {
               y = dtx->Ymin;
               ilnrnc = il * nr * nc + icnr;
               for (ir=nr-1;ir>=0;ir--) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  row = ir;
                  col = ic;
                  lev = il;
                  gridPRIME_to_grid( ctx, time, var, 1, &row, &col, &lev,
                                      &grow, &gcol, &glev);
                  if ( grow < 0 || gcol < 0 || glev < 0 ||
                    grow >= ctx->Nr || gcol >= ctx->Nc || glev >= ctx->Nl[var]){
                     v->index[j++] = 255;
                  }
                  else{
                     gr0= (int) grow;
                     gr1= gr0+1;
                     if (gr0 == ctx->Nr-1){
                        gr1 = gr0;
                     }
                     gc0 = (int) gcol;
                     gc1 = gc0 + 1;
                     if (gc0 == ctx->Nc-1){
                        gc1 = gc0;
                     }
                     gl0 = (int) glev;
                     gl1 = gl0+1;
                     if (gl0 == ctx->Nl[var]-1){
                        gl1 = gl0;
                     }

                     ger = grow - (float) gr0; /* in [0,1) */
                     gec = gcol - (float) gc0; /* in [0,1) */
                     gel = glev - (float) gl0; /* in [0,1) */

                     if (ger==0.0){
                        gr1 = gr0;
                     }
                     if (gec==0.0){
                        gc1 = gc0;
                     }
                     if (gel==0.0){
                        gl1 = gl0;
                     }

                     s1 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s2 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s3 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s4 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr1];
                     s5 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s6 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s7 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s8 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr1];

                     if (IS_MISSING(s1) || IS_MISSING(s2) ||
                         IS_MISSING(s3) || IS_MISSING(s4) ||
                         IS_MISSING(s5) || IS_MISSING(s6) ||
                         IS_MISSING(s7) || IS_MISSING(s8)){
                        v->index[j++] = 255;
                     }
                     else{
                        val = ( s1 * (1.0-ger) * (1.0-gec)
                             + s2 * ger       * (1.0-gec)
                             + s3 * (1.0-ger) * gec
                             + s4 * ger       * gec        ) * (1.0-gel)
                          +  ( s5 * (1.0-ger) * (1.0-gec)
                             + s6 * ger       * (1.0-gec)
                             + s7 * (1.0-ger) * gec
                             + s8 * ger       * gec        ) * gel;
                        if (val < min || val > max){
                           v->index[j++] = 255;
                        }
                        else{
                           v->index[j++] = (uint_1) (int) ((val-min) * dscale);
                        }
                     }
                  }
                  y += dy;
               }
            }
            x -= dx;
         }
         break;

      case NORTH_TO_SOUTH:
         /* compute slices from north to south */
         v->slices = nr;
         v->rows = nl;
         v->cols = nc;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
    y = dtx->Ymax                                  -dy*dfrac;
         for (ir=0; ir<nr; ir++) {
            for (il=nl-1;il>=0;il--) {
               x = dtx->Xmin;
               ilnrnc = il * nr * nc;
               for (ic=0;ic<nc;ic++) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  row = ir;
                  col = ic;
                  lev = il;
                  gridPRIME_to_grid( ctx, time, var, 1, &row, &col, &lev,
                                      &grow, &gcol, &glev);
                  if ( grow < 0 || gcol < 0 || glev < 0 ||
                    grow >= ctx->Nr || gcol >= ctx->Nc || glev >= ctx->Nl[var]){
                     v->index[j++] = 255;
                  }
                  else{
                     gr0= (int) grow;
                     gr1= gr0+1;
                     if (gr0 == ctx->Nr-1){
                        gr1 = gr0;
                     }
                     gc0 = (int) gcol;
                     gc1 = gc0 + 1;
                     if (gc0 == ctx->Nc-1){
                        gc1 = gc0;
                     }
                     gl0 = (int) glev;
                     gl1 = gl0+1;
                     if (gl0 == ctx->Nl[var]-1){
                        gl1 = gl0;
                     }

                     ger = grow - (float) gr0; /* in [0,1) */
                     gec = gcol - (float) gc0; /* in [0,1) */
                     gel = glev - (float) gl0; /* in [0,1) */

                     if (ger==0.0){
                        gr1 = gr0;
                     }
                     if (gec==0.0){
                        gc1 = gc0;
                     }
                     if (gel==0.0){
                        gl1 = gl0;
                     }

                     s1 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s2 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s3 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s4 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr1];
                     s5 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s6 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s7 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s8 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr1];

                     if (IS_MISSING(s1) || IS_MISSING(s2) ||
                         IS_MISSING(s3) || IS_MISSING(s4) ||
                         IS_MISSING(s5) || IS_MISSING(s6) ||
                         IS_MISSING(s7) || IS_MISSING(s8)){
                        v->index[j++] = 255;
                     }
                     else{
                        val = ( s1 * (1.0-ger) * (1.0-gec)
                             + s2 * ger       * (1.0-gec)
                             + s3 * (1.0-ger) * gec
                             + s4 * ger       * gec        ) * (1.0-gel)
                          +  ( s5 * (1.0-ger) * (1.0-gec)
                             + s6 * ger       * (1.0-gec)
                             + s7 * (1.0-ger) * gec
                             + s8 * ger       * gec        ) * gel;
                        if (val < min || val > max){
                           v->index[j++] = 255;
                        }
                        else{
                           v->index[j++] = (uint_1) (int) ((val-min) * dscale);
                        }
                     }
                  }
                  x += dx;
               }
            }
            y -= dy;
         }
         break;

      case SOUTH_TO_NORTH:
         /* compute slices from south to north */
         v->slices = nr;
         v->rows = nl;
         v->cols = nc;

         i = 0;  /* index into vertex array */
         j = 0;  /* index into index array */
    y = dtx->Ymin                                +dy*dfrac;
         for (ir=nr-1; ir>=0; ir--) {
            for (il=nl-1;il>=0;il--) {
               x = dtx->Xmin;
               ilnrnc = il * nr * nc;
               for (ic=0;ic<nc;ic++) {
                  /* compute vertex */
                  v->vertex[i++] = x;
                  v->vertex[i++] = y;
                  v->vertex[i++] = zs[il];

                  /* compute color table index */
                  row = ir;
                  col = ic;
                  lev = il;
                  gridPRIME_to_grid( ctx, time, var, 1, &row, &col, &lev,
                                      &grow, &gcol, &glev);
                  if ( grow < 0 || gcol < 0 || glev < 0 ||
                    grow >= ctx->Nr || gcol >= ctx->Nc || glev >= ctx->Nl[var]){
                     v->index[j++] = 255;
                  }
                  else{
                     gr0= (int) grow;
                     gr1= gr0+1;
                     if (gr0 == ctx->Nr-1){
                        gr1 = gr0;
                     }
                     gc0 = (int) gcol;
                     gc1 = gc0 + 1;
                     if (gc0 == ctx->Nc-1){
                        gc1 = gc0;
                     }
                     gl0 = (int) glev;
                     gl1 = gl0+1;
                     if (gl0 == ctx->Nl[var]-1){
                        gl1 = gl0;
                     }

                     ger = grow - (float) gr0; /* in [0,1) */
                     gec = gcol - (float) gc0; /* in [0,1) */
                     gel = glev - (float) gl0; /* in [0,1) */

                     if (ger==0.0){
                        gr1 = gr0;
                     }
                     if (gec==0.0){
                        gc1 = gc0;
                     }
                     if (gel==0.0){
                        gl1 = gl0;
                     }

                     s1 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s2 = data[(gl0*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s3 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s4 = data[(gl0*ctx->Nc+gc1)*ctx->Nr+gr1];
                     s5 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr0];
                     s6 = data[(gl1*ctx->Nc+gc0)*ctx->Nr+gr1];
                     s7 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr0];
                     s8 = data[(gl1*ctx->Nc+gc1)*ctx->Nr+gr1];

                     if (IS_MISSING(s1) || IS_MISSING(s2) ||
                         IS_MISSING(s3) || IS_MISSING(s4) ||
                         IS_MISSING(s5) || IS_MISSING(s6) ||
                         IS_MISSING(s7) || IS_MISSING(s8)){
                        v->index[j++] = 255;
                     }
                     else{
                        val = ( s1 * (1.0-ger) * (1.0-gec)
                             + s2 * ger       * (1.0-gec)
                             + s3 * (1.0-ger) * gec
                             + s4 * ger       * gec        ) * (1.0-gel)
                          +  ( s5 * (1.0-ger) * (1.0-gec)
                             + s6 * ger       * (1.0-gec)
                             + s7 * (1.0-ger) * gec
                             + s8 * ger       * gec        ) * gel;
                        if (val < min || val > max){
                           v->index[j++] = 255;
                        }
                        else{
                           v->index[j++] = (uint_1) (int) ((val-min) * dscale);
                        }
                     }
                  }
                  x += dx;
               }
            }
            y += dy;
         }
         break;

      default:
         printf("Error in compute_volumePRIME!\n");

   } /* switch */

   v->valid = 1;
   return 1;
}



/*
 * Render the volume described by the given volume struct.
 * Return:  1 = ok
 *          0 = bad volume struct.
 */

//  error=render_volume( ctx, volumevarnum, volumevarlist, volumelist, ctablelist, numallslices, totalslices );

static int render_volume( Context ctx, int volumevarnum, int *volumevarlist, struct volume **volumelist, unsigned int **ctablelist, int  numallslices, int **totalslices )
{
  int var;
  int volvar;
  int realvar;
  struct volume *v;
  unsigned int *ctable;
   register int rows, cols, slices, i, j, s;
	register int rows1, cols1;
   register uint_1 *cp0, *cp1;
   register float *vp0, *vp1;
	int	fastdraw;
	int	stride = 1;


  //  fprintf(stderr,"totalslices=%d %d\n",totalslices[0][0],totalslices[1][0]);


#if defined (HAVE_SGI_GL) || defined (DENALI)
   lmcolor( LMC_COLOR );             /* no shading */
   blendfunction( BF_SA, BF_MSA );   /* enable alpha blending */
#endif
#ifdef HAVE_OPENGL
   glEnable( GL_BLEND );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
   check_gl_error( "render_volume (glBlendFunc)" );
#endif


  /* loop over slices from back to front! */
  int alls;
  for (alls=0;alls<numallslices;alls++) {
    //+=stride // need to stride in a smarter way GODMARK
    
    s=totalslices[0][alls]; // which true slice 
    volvar=totalslices[1][alls]; // which variable in reduced list
    realvar=volumevarlist[volvar]; // which variable in original list
    v=volumelist[volvar]; // which volume pointer
    ctable=ctablelist[volvar];

   
    // DEBUG:
    //    fprintf(stderr,"alls=%d s=%d volvar=%d realvar=%d v=%ld ctable=%ld\n",alls,s,volvar,realvar,v,ctable);
    //    fflush(stderr);

    // ensure memory really there
    if (!v || !v->slices)  return 0;


   /* put rows, cols, slices values into registers */
   rows = v->rows-1;  /* number of quad strips = number of data rows - 1 */
   cols = v->cols;
   /* setup color and vertex pointers */
   cp0 = v->index;
   cp1 = cp0 + cols;
   vp0 = v->vertex;
   vp1 = vp0 + cols * 3;   /* 3 floats per vertex */




      /* MJK 12.15.98 */
#ifdef HAVE_PEX

    if(alls==0){
   rows++;
   j = rows * cols;
    }

      draw_volume_quadmesh (rows, cols, vp0, cp0, ctable);
      vp0 += j * 3;
      cp0 += j;
    
   return 1;
#endif



   vis5d_check_fastdraw(ctx->dpy_ctx->dpy_context_index, &fastdraw);

    
   if (fastdraw) {
	  stride = ctx->dpy_ctx->VStride;
   } 
	/* sanity check */
	if(stride<=0)
	  stride = 1;


   /*
   ** adjust rows and cols based on stride. N.B. appears to be one more
   ** row than we actually use
   */
   rows1 = (rows + 1 - 1) / stride;
   cols1 = ((cols - 1) / stride) + 1;
  

    cp0 = v->index + (s * rows * cols) + (s * cols);	/* skip a row after each slice */

    vp0 = v->vertex + (s * rows * cols * 3) + (s * cols * 3);	/* skip a row after each slice */

     cp1 = cp0 + (cols * stride);
     vp1 = vp0 + (cols * stride * 3);   /* 3 floats per vertex */
  

  
    //////////////////
    //
    // draw 'rows' quadrilateral strips
    //
    //////////////////
	  for (i=0;i<rows1;i++) {
#if defined(SGI_GL) || defined(DENALI)
		 bgnqstrip();
		 for (j=0;j<cols1;j++) {
			cpack( ctable[cp0[i*stride*cols+j*stride]] );
			v3f( &vp0[i*stride*cols+j*stride] );
			cpack( ctable[cp1[i*stride*cols+j*stride]] );
			v3f( &vp1[i*stride*cols+j*stride] );
		 }
		 endqstrip();
#endif
#ifdef HAVE_OPENGL
      // START SINGLE QUADRALATERAL STRIP
      // Bottom Left, Bottom Right, Top Right, Top left
		 glBegin( GL_QUAD_STRIP );
		 for (j=0;j<cols1;j++) {
	glColor4ubv( (const GLubyte *) &ctable[cp0[i*stride*cols+j*stride]] );
			glVertex3fv( &vp0[(i*stride*cols+j*stride)*3] );
			
	glColor4ubv( (const GLubyte *) &ctable[cp1[i*stride*cols+j*stride]] );
			glVertex3fv( &vp1[(i*stride*cols+j*stride)*3] );
		 }
		 glEnd();
      //      swap_3d_window(); // DEBUG
      // END SINGLE QUADRALATERAL STRIP
#endif
    }// over rows
  }// over all slices
	  
	
  /////////////
  //
  // Disable blending (default)
  //
  /////////////	
#if defined(HAVE_SGI_GL) || defined(DENALI)
   blendfunction( BF_ONE, BF_ZERO );  /* disable alpha blending */
#endif
#ifdef HAVE_OPENGL
  //  glDisable( GL_BLEND ); // JCM (get error in Mesa)
  //  check_gl_error( "render_volume (glDisable)" ); // JCM
#endif

   return 1;

}



/* MJK 12.15.98 */
#ifdef HAVE_PEX


void draw_volume( Context ctx, int it, unsigned int (*Colors)[256] )
{
  
  unsigned int *ctable;
   float *data;
   static int prev_it[VIS5D_MAX_CONTEXTS];
   static int prev_ip[VIS5D_MAX_CONTEXTS];
   static int do_once = 1;
   int dir;
   float x, y, z, ax, ay, az;
   float xyz[3], xy[3][2], xy0[2];
   Display_Context dtx;
  int ip,whichVolume;


   dtx = ctx->dpy_ctx;

  whichVolume=ip=dtx->CurrentVolume;
  ctable=Colors[ctx->context_index*MAXVARS+ip];


   if (do_once){
      int yo;
      for (yo=0; yo<VIS5D_MAX_CONTEXTS; yo++){
         prev_it[yo] = -1;
         prev_ip[yo] = -1;
      }
      do_once = 0;
   }


   xyz[0] = xyz[1] = xyz[2] = 0.0;
   project (xyz, &xy0[0], &xy0[1]);

   xyz[0] = 1.0, xyz[1] = xyz[2] = 0.0;
   project (xyz, &xy[0][0], &xy[0][1]);
   xy[0][0] -= xy0[0], xy[0][1] -= xy0[1];

   xyz[1] = 1.0, xyz[0] = xyz[2] = 0.0;
   project (xyz, &xy[1][0], &xy[1][1]);
   xy[1][0] -= xy0[0], xy[1][1] -= xy0[1];

   xyz[2] = 1.0, xyz[0] = xyz[1] = 0.0;
   project (xyz, &xy[2][0], &xy[2][1]);
   xy[2][0] -= xy0[0], xy[2][1] -= xy0[1];

   ax = (xy[0][0] * xy[0][0]) + (xy[0][1] * xy[0][1]);
   ay = (xy[1][0] * xy[1][0]) + (xy[1][1] * xy[1][1]);
   az = (xy[2][0] * xy[2][0]) + (xy[2][1] * xy[2][1]);

   if ((ax <= ay) && (ax <= az))
   {
      x = (xy[1][1] * xy[2][0]) - (xy[1][0] * xy[2][1]);
      dir = (x > 0.0) ? WEST_TO_EAST : EAST_TO_WEST;
   }
   else if ((ay <= ax) && (ay <= az))
   {
      x = (xy[2][1] * xy[0][0]) - (xy[2][0] * xy[0][1]);
      dir = (x > 0.0) ? SOUTH_TO_NORTH : NORTH_TO_SOUTH;
   }
   else
   {
      x = (xy[0][1] * xy[1][0]) - (xy[0][0] * xy[1][1]);
      dir = (x > 0.0) ? BOTTOM_TO_TOP : TOP_TO_BOTTOM;
   }

   /* If this is a new time step or variable then invalidate old volumes */
   if (it!=prev_it[ctx->context_index] || ip!=prev_ip[ctx->context_index]) {
    ctx->Volume[whichVolume]->valid = 0;
      prev_it[ctx->context_index] = it;
      prev_ip[ctx->context_index] = ip;
   }

   /* Determine if we have to compute a set of slices for the direction. */
  if (ctx->Volume[whichVolume]->dir!=dir || ctx->Volume[whichVolume]->valid==0) {
      data = get_grid( ctx, it, ip );
      if (data) {
         if (ctx->GridSameAsGridPRIME){
            compute_volume( ctx, data, it, ip, ctx->Nr, ctx->Nc, ctx->Nl[ip],
                            ctx->Variable[ip]->LowLev, ctx->Variable[ip]->MinVal, ctx->Variable[ip]->MaxVal,
			dir, ctx->Volume[whichVolume]);
         }
         else{
            compute_volumePRIME( ctx, data, it, ip, dtx->Nr, dtx->Nc, dtx->Nl,
                            dtx->LowLev, ctx->Variable[ip]->MinVal, ctx->Variable[ip]->MaxVal,
			     dir, ctx->Volume[whichVolume]);
         }
         release_grid( ctx, it, ip, data );
      }
   }
  render_volume( ctx, ctx->Volume[whichVolume], whichVolume, ctable );
}
#else

/*
 * Draw a volumetric rendering of the grid for timestep it and variable ip.
 * Input: it - timestep
 *        ip - variable
 */
void draw_volume( Context ctx, int it, unsigned int (*Colors)[256] )
{
  int volumevarnum;
  int volumevarlist[MAXVARS];
  unsigned int *ctablelist[MAXVARS];
  struct volume *volumelist[MAXVARS];
  struct volume *curvol;
  int truenl[MAXVARS];
  int *totalslices[2];
  PTRINT numallslices;
  int numslicepervar[MAXVARS];
  int sliceorig,slicenew;
  PTRINT largestslice;
  int volvar;
  int origvar;
  int nr,nc,nl;
  int renewedvolumememory;
  //
  int error;
   float *data;
  static int prevvolumevarnum[VIS5D_MAX_CONTEXTS];
   static int prev_it[VIS5D_MAX_CONTEXTS];
  static int prev_ip[VIS5D_MAX_CONTEXTS][MAXVARS];
   static int do_once = 1;
   int dir;
   float x, y, z, ax, ay, az;
   MATRIX ctm, proj;
   Display_Context dtx;

   dtx = ctx->dpy_ctx;
   if (do_once){
      int yo;
#if(MULTIVOLUMERENDER==1)
    for(origvar=0;origvar<MAXVARS;origvar++)
#else
    origvar=0;
#endif
    {
      for (yo=0; yo<VIS5D_MAX_CONTEXTS; yo++){
	prevvolumevarnum[yo]=0;
         prev_it[yo] = -1;
	prev_ip[yo][origvar] = -1;
      }
    }// end over origvar
      do_once = 0;
   }

   /* Get 3rd column values from transformation matrix */
#if defined (HAVE_SGI_GL) || defined (DENALI)
   /* Compute orientation of 3-D box with respect to current matrices */
   /* with no assumptions about the location of the camera.  This was */
   /* done for the CAVE. */
   mmode( MPROJECTION );
   getmatrix( proj );
   mmode( MVIEWING );
   getmatrix( ctm );
#endif
#if defined(HAVE_OPENGL)
  // JCM
   glGetFloatv( GL_PROJECTION_MATRIX, (GLfloat *) proj );
   glGetFloatv( GL_MODELVIEW_MATRIX, (GLfloat *) ctm );
   check_gl_error( "draw_volume" );
#endif




  ////////////////////
  //
  // get total true variables that will plot volume for
  //
  ///////////////////
  volumevarnum=0;
#if(MULTIVOLUMERENDER==0)
  origvar=dtx->CurrentVolume;// true variable rendered for non-multi-volume method
#else
  for(origvar=0;origvar<ctx->NumVars;origvar++)
#endif
  {
    if(MULTIVOLUMERENDER==1 && ctx->DisplayVolume[origvar]==0){ // we don't use DisplayVolume when MULTIVOLUMERENDER==0
      continue;
    }
    else{
      // here if MULTIVOLUMERENDER==0 or MULTIVOLUMERENDER==1 but wanting to render this volume
      //      fprintf(stderr,"Volume rendering origvar=%d\n",origvar);
      volumevarlist[volumevarnum]=origvar;
      ctablelist[volumevarnum]=Colors[ctx->context_index*MAXVARS+origvar];
      truenl[volumevarnum]=ctx->Nl[origvar];
      // iterate real number of volumes to be rendered
      volumevarnum++;
    }
  }




  // Check if new and old volumes match.  If not, then set renewvolumememory so got new volume memory needed
  // If this is a new time step or variable then invalidate old volumes
  renewedvolumememory=0;
  if(
     volumevarnum!=prevvolumevarnum[ctx->context_index]
     || it!=prev_it[ctx->context_index]
     ){
    // then number of volumes doesn't match
    renewedvolumememory=1;
    prev_it[ctx->context_index] = it;
    prevvolumevarnum[ctx->context_index]=volumevarnum;
  }
  else{
    // number of volumes match, check that volume numbers doing match
    // check that all volume origvar numbers match
    for (volvar=0;volvar<volumevarnum;volvar++) { // over real list of volumes
      if(volumevarlist[volvar]!=prev_ip[ctx->context_index][volvar]){
	// then volume lists don't match
	renewedvolumememory=1;
      }
      // assign correct (or same) volume origvar number to "previous" storage for next comparison
      prev_ip[ctx->context_index][volvar] = volumevarlist[volvar]; // store real "origvar" value of variable
    }
  }



  // If created new variable or something, then deallocate ALL and allocate new needed memory
  if(renewedvolumememory){
    renew_volume_memory(ctx->context_index,ctx->dpy_ctx->dpy_context_index);
  }



  // obtain (new) volume *memory* list
  for (volvar=0;volvar<volumevarnum;volvar++) {
    origvar=volumevarlist[volvar];

    if(ctx->Volume[origvar]==NULL){
      fprintf(stderr,"Volume=%d still NULL\n",origvar);
      exit(1);
    }

    //    fprintf(stderr,"Volume rendering volvar=%d origvar=%d\n",volvar,origvar);
    volumelist[volvar]=ctx->Volume[origvar];
    truenl[volvar]=ctx->Nl[origvar];
  }




  // invalidate current volumes
  for (volvar=0;volvar<volumevarnum;volvar++){
    origvar=volumevarlist[volvar];
    curvol=volumelist[volvar];

    if(curvol==NULL){
      fprintf(stderr,"Volume=%d (volvar=%d) still NULL\n",origvar,volvar);
      exit(1);
    }
    // invalidate
    curvol->valid = 0;
  }




  /////////////////////////////////////////
  //
  // see if anything to do
  //
  /////////////////////////////////////////
  if(volumevarnum==0) return;


  // DEBUG:
  //  fprintf(stderr,"volumevarnum=%d\n",volumevarnum);


  if (ctx->GridSameAsGridPRIME){
    nr=ctx->Nr;
    nc=ctx->Nc;
    // nl is multi-valued
  }
  else{
    nr=dtx->Nr;
    nc=dtx->Nc;
    nl=dtx->Nl;
  }


#if(0) // DEBUG:
  int llll;
  unsigned int tempi;
  GLubyte * glbytearray;
  int CV;
  CV = volumevarlist[0];
  if(volumevarnum>0){
    for(llll=0;llll<256;llll++){
      glbytearray=(GLubyte *) &(Colors[ctx->context_index*MAXVARS+CV][llll]);
      // RBGA
      fprintf(stderr,"CV=%d color=%d %d %d %d\n",CV,glbytearray[0],glbytearray[1],glbytearray[2],glbytearray[3]);
    }
  }
#endif



   /* compute third column values in the product of ctm*proj */
   x = ctm[0][0]*proj[0][2] + ctm[0][1]*proj[1][2]
     + ctm[0][2]*proj[2][2] + ctm[0][3]*proj[3][2];
   y = ctm[1][0]*proj[0][2] + ctm[1][1]*proj[1][2]
     + ctm[1][2]*proj[2][2] + ctm[1][3]*proj[3][2];
   z = ctm[2][0]*proj[0][2] + ctm[2][1]*proj[1][2]
     + ctm[2][2]*proj[2][2] + ctm[2][3]*proj[3][2];

   /* examine values to determine how to draw slices */
   ax = ABS(x);
   ay = ABS(y);
   az = ABS(z);
   if (ax>=ay && ax>=az) {
      /* draw x-axis slices */
      dir = (x<0.0) ? WEST_TO_EAST : EAST_TO_WEST;
    //         v->slices = nc;
    //         v->rows = nl;
    //         v->cols = nr;
    for (volvar=0;volvar<volumevarnum;volvar++) { // must be on inner loop!
      numslicepervar[volvar]=nc;
    }
    largestslice=nc;
   }
   else if (ay>=ax && ay>=az) {
      /* draw y-axis slices */
      dir = (y<0.0) ? SOUTH_TO_NORTH : NORTH_TO_SOUTH;
    //v->slices = nr;
    //      v->rows = nl;
    //      v->cols = nc;
    for (volvar=0;volvar<volumevarnum;volvar++) { // must be on inner loop!
      numslicepervar[volvar]=nr;
    }
    largestslice=nr;
   }
   else {
      /* draw z-axis slices */
      dir = (z<0.0) ? BOTTOM_TO_TOP : TOP_TO_BOTTOM;
    //         v->slices = nl;
    //         v->rows = nr;
    //         v->cols = nc;

    largestslice=0;
    if (ctx->GridSameAsGridPRIME){
      for (volvar=0;volvar<volumevarnum;volvar++) { // must be on inner loop!
	numslicepervar[volvar]=truenl[volvar]; // for which variable
	if(numslicepervar[volvar]>largestslice) largestslice=numslicepervar[volvar];
      }
    }
    else{
      for (volvar=0;volvar<volumevarnum;volvar++) { // must be on inner loop!
	numslicepervar[volvar]=nl; // for which variable
      }
      largestslice=nl;
    }
   }


  // DEBUG:
  //  fprintf(stderr,"dir=%d isspecialnl=%d\n",dir,ctx->GridSameAsGridPRIME);


  //////////////////////////////
  //
  // allocate totalslices
  //
  //////////////////////////////
  numallslices=0;
  for(volvar=0;volvar<volumevarnum;volvar++){
    //    numallslices+=numslicepervar[volvar]; // to complicated to keep track of this with multi-volumes
    numallslices+=largestslice; // just use maximum slice to do everything
  }

  totalslices[0]=(int *)malloc(numallslices*sizeof(int)); // true slice number of a variable
  totalslices[1]=(int *)malloc(numallslices*sizeof(int)); // variable for that slice

  //////////////////////////////
  //
  // check totalslice allocations
  //
  //////////////////////////////
  if(totalslices[0]==NULL || totalslices[1]==NULL){
    fprintf(stderr,"Couldn't allocate totalslices %ld %ld\n",(PTRINT)totalslices[0],(PTRINT)totalslices[1]);
    exit(1);
   }


  // DEBUG:
  //  fprintf(stderr,"Before ipcheck: %d\n",ctx->context_index);




  // DEBUG:
  //  fprintf(stderr,"Before compute:\n");

  //////////////////////////////
  //
  // Compute all volumes wanted to display if necessary to recompute them
  //
  // Both compute_volume() and compute_volumePRIME() have been corrected for multi-volumes
  //////////////////////////////
  for (volvar=0;volvar<volumevarnum;volvar++) {
    int ip;
    ip=volumevarlist[volvar];
    curvol=volumelist[volvar];
    // DEBUG:
    //fprintf(stderr,"volvar=%d %d %ld\n",volvar,ip,curvol); fflush(stderr);

   /* Determine if we have to compute a set of slices for the direction. */
    if (curvol->dir!=dir || curvol->valid==0) {

      // DEBUG:
      //fprintf(stderr,"Get data: it=%d ip=%d\n",it,ip);


      data = get_grid( ctx, it, ip );
      if (data) {
         if (ctx->GridSameAsGridPRIME){
	  compute_volume( ctx, data, it, ip, volvar, volumevarnum, ctx->Nr, ctx->Nc, ctx->Nl[ip],
                            ctx->Variable[ip]->LowLev, ctx->Variable[ip]->MinVal, ctx->Variable[ip]->MaxVal,
			  dir, curvol);
         }
         else{
	  compute_volumePRIME( ctx, data, it, ip, volvar, volumevarnum, dtx->Nr, dtx->Nc, dtx->Nl,
                            dtx->LowLev, ctx->Variable[ip]->MinVal, ctx->Variable[ip]->MaxVal,
			       dir, curvol);
         }
         release_grid( ctx, it, ip, data );
      }
   }
  }



  // DEBUG:
  //fprintf(stderr,"Before totalslices setup\n");

  //////////////////////////////
  // TODOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
  // Setup totalslices (using "z" information from compute_volume() )
  //
  //////////////////////////////
  slicenew=0;
  //  for(sliceorig=0;sliceorig<numslicepervar[volvar];sliceorig++){
  //  for(sliceorig=0;sliceorig<numslicepervar[0];sliceorig++){
  for(sliceorig=0;sliceorig<largestslice;sliceorig++){
    for (volvar=0;volvar<volumevarnum;volvar++) {
      
      // repeat if necessary to make all variables have the same number of slices to make multi-volume rendering easier
      totalslices[0][slicenew]=(int)(sliceorig*(numslicepervar[volvar]-1)/(largestslice-1));
      // ensure don't go over!
      if(totalslices[0][slicenew]>numslicepervar[volvar]-1) totalslices[0][slicenew]=numslicepervar[volvar]-1;

      totalslices[1][slicenew]=volvar;

      // DEBUG:
      //      fprintf(stderr,"ts: %d %d :: slice=%d var=%d\n",volvar,sliceorig,totalslices[0][slicenew],totalslices[1][slicenew]);

      slicenew++;
    }
  }





  ////////////
  //
  // Render the volumes
  //
  ////////////
  //  fprintf(stderr,"Rendering begin:\n");
  error=render_volume( ctx, volumevarnum, volumevarlist, volumelist, ctablelist, numallslices, totalslices );
  //fprintf(stderr,"Rendering done: error=%d\n",error);

   
  ////////////
  //
  // Free totalslices memory
  //
  ////////////
  free(totalslices[0]);
  free(totalslices[1]);

}
#endif
