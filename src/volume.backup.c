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
                           int time, int var,
                           int nr, int nc, int nl, int lowlev,
                           float min, float max,
                           int dir,
                           struct volume *v,
			   int whichVolume )
{
   Display_Context dtx;
   float zs[MAXLEVELS];
   register int ir, ic, il, i, j;
   register float x, y, dx, dy;
   register float dscale, val;
   register int ilnrnc, icnr;           /* to help speed up array indexing */

   dtx = ctx->dpy_ctx;
   /* compute graphics Z for each grid level */
   for (il=0; il<nl; il++) {
     zs[il] = gridlevel_to_z(ctx, time, var, (float) (il + lowlev));
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
         x = dtx->Xmin;
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
         x = dtx->Xmax;
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
         y = dtx->Ymax;
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
         y = dtx->Ymin;
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
                           int time, int var,
                           int nr, int nc, int nl, int lowlev,
                           float min, float max,
                           int dir,
			   struct volume *v,
			   int whichVolume )
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

   dtx = ctx->dpy_ctx;
   /* compute graphics Z for each grid level */
   for (il=0; il<nl; il++) {
     zs[il] = gridlevelPRIME_to_zPRIME(dtx, time, var, (float) (il + lowlev));
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
         x = dtx->Xmin;
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
         x = dtx->Xmax;
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
         y = dtx->Ymax;
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
         y = dtx->Ymin;
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
static int render_volume( Context ctx,
                          struct volume **allv, struct volume *onev, int whichVolume, unsigned int ctable[] )
{
  int var;
  struct volume *v;
#if(0)
   register int rows, cols, slices, i, j, s;
	register int rows1, cols1;
   register uint_1 *cp0, *cp1;
   register float *vp0, *vp1;
#else
   int rows, cols, slices, i, j, s;
   int rows1, cols1;
   uint_1 *cp0, *cp1;
   float *vp0, *vp1;
#endif
   float vp0temp[3], vp1temp[3];
	int	fastdraw;
	int	stride = 1;


#if defined (HAVE_SGI_GL) || defined (DENALI)
   lmcolor( LMC_COLOR );             /* no shading */
   blendfunction( BF_SA, BF_MSA );   /* enable alpha blending */
#endif
#ifdef HAVE_OPENGL
   if(0 && whichVolume==0){ // JCM
     glEnable( GL_BLEND );
     glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
     check_gl_error( "render_volume (glBlendFunc)" );
   }
#endif

   for(whichVolume=0;whichVolume<ctx->NumVars;whichVolume++){
   //   for(whichVolume=ctx->NumVars-1;whichVolume>=0;whichVolume--){

     if(ctx->DisplayVolume[whichVolume]==0){
       fprintf(stderr,"whichVolume=%d  not done\n",whichVolume);
       continue;
     }
     v=allv[whichVolume];
     //     v=onev;

     if (!v || !v->slices)  return 0;

     fprintf(stderr,"rendering var=%d\n",var);

   /* put rows, cols, slices values into registers */
   rows = v->rows-1;  /* number of quad strips = number of data rows - 1 */
   cols = v->cols;
   slices = v->slices;
   /* setup color and vertex pointers */
   cp0 = v->index;
   cp1 = cp0 + cols;
   vp0 = v->vertex;
   vp1 = vp0 + cols * 3;   /* 3 floats per vertex */

   //   fprintf(stderr,"cp0=%ld cp1=%ld vp0=%ld vp1=%ld\n",cp0,cp1,vp0,vp1);

/* MJK 12.15.98 */
#ifdef HAVE_PEX

   rows++;
   j = rows * cols;

   for (s = 0; s < slices; s++)
   {
      draw_volume_quadmesh (rows, cols, vp0, cp0, ctable);
      vp0 += j * 3;
      cp0 += j;
   }
   return 1;
#endif

   vis5d_check_fastdraw(ctx->dpy_ctx->dpy_context_index, &fastdraw);
   
   fprintf(stderr,"fastdraw=%d\n",fastdraw);

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


   fprintf(stderr,"rows=%d cols=%d rows1=%d cols1=%d slices=%d stride=%d\n",rows,cols,rows1,cols1,slices,stride);
   fprintf(stderr,"v->index=%ld v->vertex=%ld\n",v->index,v->vertex);
  

	/* loop over slices */
   for (s=0;s<slices;s+=stride) {

     cp0 = v->index + (s * rows * cols) +
		 (s * cols);	/* skip a row after each slice */

     vp0 = v->vertex + (s * rows * cols * 3) +
		 (s * cols * 3);	/* skip a row after each slice */

     cp1 = cp0 + (cols * stride);
     vp1 = vp0 + (cols * stride * 3);   /* 3 floats per vertex */
  
	  /* draw 'rows' quadrilateral strips */
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
#if(1)
		   // JCM
		   float factor;
		   int cond;
		   //		   cond=whichVolume==6 || whichVolume==0 && (s<32);
		   cond=whichVolume==6  || whichVolume==0 && (s==slices-1 || s==0);
		   //		   if(whichVolume==6){
		   //		     vp0[(i*stride*cols+j*stride)*3]*=1.1;
		   //		     vp1[(i*stride*cols+j*stride)*3]*=1.1;
		   //		   }

		   if(whichVolume==6){
		     factor=0.9;
		   }
		   else factor=1.0;
		   vp0temp[0]=vp0[(i*stride*cols+j*stride)*3]*factor;
		   vp0temp[1]=vp0[(i*stride*cols+j*stride)*3+1]*factor;
		   vp0temp[2]=vp0[(i*stride*cols+j*stride)*3+2]*factor;
		   
		   vp1temp[0]=vp1[(i*stride*cols+j*stride)*3]*factor;
		   vp1temp[1]=vp1[(i*stride*cols+j*stride)*3+1]*factor;
		   vp1temp[2]=vp1[(i*stride*cols+j*stride)*3+2]*factor;

		   //		   if(s==32 && i==32){
		   //		     fprintf(stderr,"whichVolume=%d : %g %g %g : %g %g %g\n",whichVolume,vp0temp[0],vp0temp[1],vp0temp[2],vp1temp[0],vp1temp[1],vp1temp[2]);
		   //		   }

		   if(cond){
		     //		   if(whichVolume==6 || whichVolume==0 && s==0){ // JCM
		     glColor4ubv( (const GLubyte *) &ctable[cp0[i*stride*cols+j*stride]] );
		     //		     glVertex3fv( &vp0[(i*stride*cols+j*stride)*3] );
		     glVertex3fv( &vp0temp[0] );
		     
		     glColor4ubv( (const GLubyte *) &ctable[cp1[i*stride*cols+j*stride]] );
		     //		     glVertex3fv( &vp1[(i*stride*cols+j*stride)*3] );
		     glVertex3fv( &vp1temp[0] );
		   }
#else
		   glColor4ubv( (const GLubyte *) &ctable[cp0[i*stride*cols+j*stride]] );
		   glVertex3fv( &vp0[(i*stride*cols+j*stride)*3] );
		   
		   glColor4ubv( (const GLubyte *) &ctable[cp1[i*stride*cols+j*stride]] );
		   glVertex3fv( &vp1[(i*stride*cols+j*stride)*3] );
#endif
		 }
		 //glDisable( GL_BLEND ); // JCM
		 glEnd();
		 //swap_3d_window();
		 // END SINGLE QUADRALATERAL STRIP
#endif
	  }
	  
     }
   }
	
#if defined(HAVE_SGI_GL) || defined(DENALI)
   blendfunction( BF_ONE, BF_ZERO );  /* disable alpha blending */
#endif
#ifdef HAVE_OPENGL
   if(0 && whichVolume==0){
     glDisable( GL_BLEND ); // JCM
     check_gl_error( "render_volume (glDisable)" ); // JCM
   }
#endif
   return 1;
}


/* MJK 12.15.98 */
#ifdef HAVE_PEX
void draw_volume( Context ctx, int it, int ip, int whichVolume, unsigned int *ctable )
{
   float *data;
   static int prev_it[VIS5D_MAX_CONTEXTS];
   static int prev_ip[VIS5D_MAX_CONTEXTS];
   static int do_once = 1;
   int dir;
   float x, y, z, ax, ay, az;
   float xyz[3], xy[3][2], xy0[2];
   Display_Context dtx;

   dtx = ctx->dpy_ctx;
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
                            dir, ctx->Volume[whichVolume] , whichVolume );
         }
         else{
            compute_volumePRIME( ctx, data, it, ip, dtx->Nr, dtx->Nc, dtx->Nl,
                            dtx->LowLev, ctx->Variable[ip]->MinVal, ctx->Variable[ip]->MaxVal,
                            dir, ctx->Volume[whichVolume] , whichVolume  );
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
void draw_volume( Context ctx, int it, int ip, int whichVolume, unsigned int *ctable, MATRIX ctm, MATRIX proj )
{
  int error;
   float *data;
   static int prev_it[VIS5D_MAX_CONTEXTS];
   static int prev_ip[VIS5D_MAX_CONTEXTS];
   static int do_once = 1;
   int dir;
   float x, y, z, ax, ay, az;
   //   MATRIX ctm, proj;
   Display_Context dtx;

   dtx = ctx->dpy_ctx;
   if (do_once&&0){ // JCM
      int yo;
      for (yo=0; yo<VIS5D_MAX_CONTEXTS; yo++){
         prev_it[yo] = -1;
         prev_ip[yo] = -1;
      }
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
   //   glGetFloatv( GL_PROJECTION_MATRIX, (GLfloat *) proj );
   //   glGetFloatv( GL_MODELVIEW_MATRIX, (GLfloat *) ctm );
   //   check_gl_error( "draw_volume" );
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
   }
   else if (ay>=ax && ay>=az) {
      /* draw y-axis slices */
      dir = (y<0.0) ? SOUTH_TO_NORTH : NORTH_TO_SOUTH;
   }
   else {
      /* draw z-axis slices */
      dir = (z<0.0) ? BOTTOM_TO_TOP : TOP_TO_BOTTOM;
   }

   /* If this is a new time step or variable then invalidate old volumes */
#if(0) // JCM try
   //   if (it!=prev_it[ctx->context_index] || ip!=prev_ip[ctx->context_index]) {
   if (it!=prev_it[ctx->context_index]) { // JCM
     ctx->Volume[whichVolume]->valid = 0;
     prev_it[ctx->context_index] = it;
     prev_ip[ctx->context_index] = ip;
   }
#endif


   for (whichVolume=0;whichVolume<ctx->NumVars;whichVolume++) {
     if(ctx->DisplayVolume[whichVolume]==0) continue;
     ip=whichVolume;

   /* Determine if we have to compute a set of slices for the direction. */
   if (ctx->Volume[whichVolume]->dir!=dir || ctx->Volume[whichVolume]->valid==0) {
     fprintf(stderr,"Get data: it=%d ip=%d\n",it,ip);
      data = get_grid( ctx, it, ip );
      if (data) {
         if (ctx->GridSameAsGridPRIME){
            compute_volume( ctx, data, it, ip, ctx->Nr, ctx->Nc, ctx->Nl[ip],
                            ctx->Variable[ip]->LowLev, ctx->Variable[ip]->MinVal, ctx->Variable[ip]->MaxVal,
                            dir, ctx->Volume[whichVolume], whichVolume );
         }
         else{
            compute_volumePRIME( ctx, data, it, ip, dtx->Nr, dtx->Nc, dtx->Nl,
                            dtx->LowLev, ctx->Variable[ip]->MinVal, ctx->Variable[ip]->MaxVal,
                            dir, ctx->Volume[whichVolume], whichVolume );
         }
         release_grid( ctx, it, ip, data );
      }
   }
   }
   whichVolume=0;
   fprintf(stderr,"Rendering begin: %d %ld\n",whichVolume,ctx->Volume[whichVolume]);
   fprintf(stderr,"whichVolume=%d\n",whichVolume);
   //   if(whichVolume==6){
   //fprintf(stderr,"\n",whichVolume);
     glEnable( GL_BLEND );
     glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
     //     error=render_volume( ctx, ctx->Volume, ctx->Volume[0], 0, ctable );
     //     error=render_volume( ctx, ctx->Volume, ctx->Volume[6], 6, ctable );
     error=render_volume( ctx, ctx->Volume, ctx->Volume[whichVolume], whichVolume, ctable );
     fprintf(stderr,"Rendering done: error=%d\n",error);
     glDisable( GL_BLEND ); // JCM
     check_gl_error( "render_volume (glDisable)" ); // JCM
     //   }
   //   system("sleep 2");
}
#endif
