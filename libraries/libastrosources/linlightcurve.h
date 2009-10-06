#ifndef LINLIGHTCURVE_H
#define LINLIGHTCURVE_H 1

#include "sixt.h"
#include "sixt_random.h"

// GSL header files
#include <gsl/gsl_errno.h>
#include <gsl/gsl_fft_halfcomplex.h>


////////////////////////////////////////////////////////////////////////
// Definitions.
////////////////////////////////////////////////////////////////////////


/** Light curve type: constant light curve. */
#define T_LC_CONSTANT (0)
/** Light curve type: red noise according to Timmer & Koenig (1995). */
#define T_LC_TIMMER_KOENIG (-1)

/** Width of the time steps in the light curve generated with the 
 * Timmer & Koenig algorithm. */
#define TK_LC_STEP_WIDTH (0.0001)

// The following macros are used to the store light curve and the PSD 
// in the right format for the GSL routines.
#define REAL(z,i) ((z)[(i)])
#define IMAG(z,i) ((z)[lc->nvalues-(i)])


////////////////////////////////////////////////////////////////////////
// Type Declarations.
////////////////////////////////////////////////////////////////////////


/** Piece-wise linear light curve giving the average source photon rate 
 * for a particular X-ray source. */
typedef struct {
  /** Number of data points in the light curve. */
  long nvalues;

  /** Width of the interval between the individual data points ([s]). */
  double step_width;

  /** Time of the first light curve data point ([s]). */
  double t0;

  /** Piece-wise linear light curve data points ([photons/s]). 
   * The value a_k represents the gradient of the light curve between 
   * the time t_k (= t0 + k*step_width) and t_{k+1} (slope). 
   * The value b_k represents the contant contribution (intercept) at t_k. */
  double *a, *b;

  /** Auxiliary data.
   * The values u_k are required in the algorithm proposed by Klein & Roberts
   * and are calculated in advance in order to accelerate the calculation
   * of the photon creation times. */
  double* u;

} LinLightCurve;


//////////////////////////////////////////////////////////////////////////
//   Function declarations
//////////////////////////////////////////////////////////////////////////


/** Constructor allocating memory for a LinLightCurve object.
 * The routine allocates memory required for a LinLightCurve object with
 * given number of data points. The values of the individual data points are not 
 * initialized, i.e., may have arbitrary values. */
LinLightCurve* getLinLightCurve(long nvalues, int* status);

/** Initialization routine creating a light curve with a constant photon rate. 
 * The light curve object already has to be allocated in advance by the
 * constructor getLinLightCurve().
 * The light curve starts at t0 ([s]) and has the specified length ([s]). 
 * The return value of the function is either EXIT_SUCCESS or EXIT_FAILURE. */
int initConstantLinLightCurve(LinLightCurve* lc, double mean_rate, double t0, 
			      double step_width);

/** Initialization routine generating a light curve according to the algorithm proposed
 * by Timmer & Koenig. 
 * The LinLightCurve object already must exist and the memory must be allocated.
 * The light curve data are generated using the algorithm proposed by Timmer & Koenig (1995),
 * which is a phase and amplitude randomization process. The generated light curve
 * is normalized to fullfill the specified mean count rate and rms. */
int initTimmerKoenigLinLightCurve(LinLightCurve* lc, double t0, double step_width, 
				  double mean_rate, double sigma);

/** Destructor. 
 * Releases the memory allocated for the LinLightCurve object by the constructor. */
void freeLinLightCurve(LinLightCurve*);

/** Determine next photon generation time from current point of time and given
 * light curve. 
 * The time is obtained from a time-varying Poisson arrival process generator
 * proposed by Klein & Roberts (1984). This paper describes a Poisson 
 * generator for piecewise-linear light curves. */
double getPhotonTime(LinLightCurve*, double time);


#endif /* LINLIGHTCURVE_H */
