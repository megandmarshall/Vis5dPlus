C v5df.h

C Include file for using v5d functions from FORTRAN programs


C Function prototypes.  See the README file for details.  These are
C the functions you will want to use for writing v5d file converters.

      integer v5dcreate

      integer v5dcreatesimple

      integer v5dwrite

      integer v5dmcfile

      integer v5dclose


C 5-D grid limits, must match those in v5d.h!!!
      integer MAXVARS, MAXTIMES, MAXROWS, MAXCOLUMNS, MAXLEVELS

      parameter (MAXVARS=200)
      parameter (MAXTIMES=1000)
      parameter (MAXROWS=1000)
      parameter (MAXCOLUMNS=1600)
      parameter (MAXLEVELS=600)

C Missing values
      real MISSING
      integer IMISSING

      parameter (MISSING=1.0E35)
      parameter (IMISSING=-987654)

