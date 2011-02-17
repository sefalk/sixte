#ifndef PHOGEN_H
#define PHOGEN_H 1

#include "sixt.h"

#ifndef HEASP_H
#define HEASP_H 1
#include "heasp.h"
#endif

#include <wcslib/wcslib.h>

#include "sixt_string.h"
#include "sourceimage.h"
#include "pointsources.h"
#include "pointsourcefile.h"
#include "pointsourcecatalog.h"
#include "pointsourcelist.h"
#include "extendedsources.h"
#include "vector.h"
#include "spectrum.h"
#include "photon.h"
#include "photonlistfile.h"
#include "astrosources.h"
#include "telescope.h"
#include "attitudecatalog.h"
#include "arf.h"
#include "check_fov.h"
#include "kdtree.h"
#include "gendet.h"

#define TOOLSUB phogen_main
#include "headas_main.c"


struct Parameters {
  char xml_filename[MAXMSG];
  char attitude_filename[MAXMSG];
  char sources_filename[MAXMSG];
  char photonlist_filename[MAXMSG];
  char photonlist_template[MAXMSG];
  int overwrite_photonlist;

  double t0, timespan;
};


int phogen_getpar(struct Parameters* parameters);


#endif /* PHOGEN_H */

