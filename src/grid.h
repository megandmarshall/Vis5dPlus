/* grid.h */

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


#ifndef GRID_H
#define GRID_H


#include "globals.h"


extern int McFile[MAXTIMES][MAXVARS];
extern int McGrid[MAXTIMES][MAXVARS];


extern int initially_open_gridfile( const char filename[], v5dstruct *v );

extern void free_grid_cache( Context ctx );

extern int open_gridfile( Context ctx, const char filename[] );

extern int init_grid_cache( Context ctx, PTRINT maxbytes, float *ratio );

extern void preload_cache( Context ctx );

extern float *get_grid( Context ctx, int time, int var );

extern float *get_grid2( Context toctx, Context fromctx, int time, int var, int numlevs );

extern int put_grid( Context ctx, int time, int var, float *griddata );

extern void release_compressed_grid( Context ctx, int time, int var );

extern void release_grid( Context ctx, int time, int var, float *data );

extern void release_grid2( Context ctx, int time, int var, int nl, float *data );

extern float get_grid_value( Context ctx, int time, int var,
                             int row, int col, int lev );

extern int query_gridfile( const char filename[], v5dstruct *v );

extern int get_empty_cache_pos( Context ctx );

extern float interpolate_grid_value( Context ctx, int time, int var,
                                     float row, float col, float lev );

extern int allocate_clone_variable( Context ctx, const char name[],
                                    int var_to_clone );

extern void min_max_init( Context ctx, int newvar );

extern int allocate_extfunc_variable( Context ctx, char name[] );

extern int allocate_computed_variable( Context ctx, const char *name );

extern int allocate_new_variable( Context ctx, const char *name, int nl, int lowlev );

extern int deallocate_variable( Context ctx, int var );

extern int install_new_grid( Context ctx, int time, int var,
                             float *griddata, int nl, int lowlev);

extern int write_gridfile( Context ctx, const char filename[] );

extern int set_ctx_from_internalv5d(Context ctx);

#endif
