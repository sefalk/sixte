#ifndef GENDETSIM_H
#define GENDETSIM_H 1


#include "sixt.h"
#include "gendet.h"
#include "impact.h"

#define TOOLSUB gendetsim_main
#include "headas_main.c"


////////////////////////////////////////////////////////////////////////
// Type declarations.
////////////////////////////////////////////////////////////////////////


struct Parameters {
  char xml_filename[MAXMSG];
  char event_filename[MAXMSG];
};


////////////////////////////////////////////////////////////////////////
// Function declarations.
////////////////////////////////////////////////////////////////////////


// Reads the program parameters using PIL
int getpar(struct Parameters* const parameters);


#endif /* GENDETSIM_H */

