/*
   This file is part of SIXTE.

   SIXTE is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   any later version.

   SIXTE is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   For a copy of the GNU General Public License see
   <http://www.gnu.org/licenses/>.


   Copyright 2007-2014 Christian Schmid, Mirjam Oertel, FAU
*/

#ifndef COMARECON_H
#define COMARECON_H 1


#include "sixt.h"
#include "comaevent.h"
#include "comaeventfile.h"
#include "squarepixels.h"
#include "codedmask.h"
#include "sourceimage.h"
#include "reconstruction.h"
#include "eventarray.h"
#include "fft_array.h"
#include "fftw3.h"
#include "balancing.h"
#include "find_position.h"


#define TOOLSUB comarecon_main
#include "headas_main.c"


struct Parameters {
  char Mask[MAXMSG]; // input: coded mask reconstruction file
  char EventList[MAXMSG];
  char EventListTemplate[MAXMSG];
  char Image[MAXMSG]; // output: reconstructed source image
  char PositionList[MAXMSG]; // output: table of found sources

  //detector pointing direction
  double RA, DEC;

  /** Detector width in [pixel]. */
  int width;
  /** Width of one detector pixel in [m]. */
  double pixelwidth;
  /** Width of re-pixeled detector pixels in [m]. */
  double RePixSize;  //0.0 means detector shouldn't be repixeled, else the value is given

  /** Distance between the coded mask and the detector plane ([m]). */
  double MaskDistance;  
};


////////////////////////////////////////////////////////////////////////
// Function declarations.
////////////////////////////////////////////////////////////////////////


/** Reads the program parameters using PIL. */
int comarecon_getpar(struct Parameters* parameters);


#endif /* COMARECON2_H */
