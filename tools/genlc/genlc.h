#ifndef GENLC_H
#define GENLC_H 1

#include "sixt.h"
#include "lad.h"
#include "ladeventlistfile.h"
#include "ladevent.h"

#define TOOLSUB genlc_main
#include "headas_main.c"


/* Program parameters */
struct Parameters {
  char EventList[MAXFILENAME];
  char LightCurve[MAXFILENAME];
  
  /** Length of the light curve [s]. */
  double length; 

  /** Time resolution [s]. */
  double dt; 
  
  char clobber;
};


int genlc_getpar(struct Parameters *par);


#endif /* GENLC_H */
