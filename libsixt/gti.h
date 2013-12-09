#ifndef GTI_H
#define GTI_H 1

// This module constitutes a wrapper around the HEADAS HDgti_...() routines.

#include "sixt.h"
#include "headas_gti.h"


/////////////////////////////////////////////////////////////////
// Type Declarations.
/////////////////////////////////////////////////////////////////


typedef struct gti_struct GTI;


/////////////////////////////////////////////////////////////////
// Function Declarations.
/////////////////////////////////////////////////////////////////


/** Constructor. Returns a pointer to an empty GTI data structure. */
GTI* newGTI(int* const status);

/** Destructor. */
void freeGTI(GTI** const file);

/** Load an existing GTI file. */
GTI* loadGTI(char* const filename, int* const status);

/** Store the GTI collection in a FITS file. */
void saveGTI(GTI* const gti,
	     const char* const filename,
	     const char clobber,
	     int* const status);

/** Store the GTI collection in a FITS file extension. */
void saveGTIExt(fitsfile* const fptr,
		char* const extname,
		GTI* const gti,
		int* const status);

/** Append a new GTI to the GTI collection. */
void appendGTI(GTI* const gti,
	       const double start,
	       const double stop,
	       int* const status);

/** Sum all GTIs. */
double sumGTI(GTI* const gti);

/** If a file name is specified, the GTI is loaded from the
    corresponding file. If not, a simple GTI is set up covering
    continuously the interval from tstart to tstop. */
GTI* getGTIFromFileOrContinuous(char* const filename,
				const double tstart,
				const double tstop,
				const double mjdref,
				int* const status);

#endif /* GTI_H */
