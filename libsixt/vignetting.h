#ifndef VIGNETTING_H
#define VIGNETTING_H 1

#include "sixt.h"


////////////////////////////////////////////////////////////////////////
// Type Declarations.
////////////////////////////////////////////////////////////////////////


/** Data structure containing the mirror vignetting function. */
typedef struct {
  /** Number of energy bins. */
  int nenergies; 
  /** Number of off-axis angles. */
  int ntheta; 
  /** Number of azimuth angles. */
  int nphi; 

  /** Minimum energy of bin in [keV]. */
  float* energ_lo;
  /** Maximum energy of bin in [keV]. */ 
  float* energ_hi;
  /** Off-axis angle in [rad]. */
  float* theta; 
  /** Azimuthal angle in [rad]. */
  float* phi; 
  /** Vignetting data. Array[energy, theta, phi] */
  float*** vignet; 

  /** Minimum available energy [keV]. */
  float Emin; 
  /** Maximum available energy [keV]. */
  float Emax; 

} Vignetting;


///////////////////////////////////////////////////////////////////////////////
// Function declarations.
///////////////////////////////////////////////////////////////////////////////


/** Constructor of the Vignetting data structure. Loads the
    vignetting function from a given FITS file. The format of the
    FITS file is defined by OGIP Memo CAL/GEN/92-021. */
Vignetting* newVignetting(const char* const filename, int* const status);

/** Destructor for Vignetting data structure. */
void destroyVignetting(Vignetting** const vi);

/** Determine the Vignetting factor for given photon energy, off-axis
    angle, and azimuth angle. The energy has to be given in [keV], the
    angles in [rad]. If the pointer to the Vignetting data structure
    is NULL, a default value of 1. will be returned. */
/* TODO: So far the azimuth angle is neglected! */
float get_Vignetting_Factor(const Vignetting* const vi, 
			    const float energy, 
			    const float theta, 
			    const float phi);


#endif /* VIGNETTING_H */