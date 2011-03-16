#ifndef HTRS_SIMULATION_H
#define HTRS_SIMULATION_H 1

#include "sixt.h"
#include "htrsdetector.h"
#include "eventfile.h"
#include "point.h"
#include "impact.h"
#include "impactlistfile.h"

#define TOOLSUB htrs_simulation_main
#include "headas_main.c"


////////////////////////////////////////////////////////////////////////
// Type declarations.
////////////////////////////////////////////////////////////////////////

struct Parameters {
  char impactlist_filename[MAXFILENAME];
  char rmf_filename[MAXFILENAME];
  char arf_filename[MAXFILENAME];
  char eventlist_filename[MAXFILENAME];
  char eventlist_template[MAXFILENAME];
  double t0;
  double timespan;
  
  double ccsigma;
  long pha_threshold;
  float energy_threshold;

  float background_rate; /**< Rate of background events. */

  // HTRS Detector specific parameters:
#ifdef HTRS_HEXPIXELS
  double pixelwidth;
#endif
#ifdef HTRS_ARCPIXELS
  double mask_spoke_width;
#endif 
  double slow_shaping_time;
  double fast_shaping_time;
  double reset_time;
};


////////////////////////////////////////////////////////////////////////
// Function declarations.
////////////////////////////////////////////////////////////////////////


// Reads the program parameters using PIL
static int getpar(struct Parameters* parameters);


#endif /* HTRS_SIMULATION_H */

