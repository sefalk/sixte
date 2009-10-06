#include "linlightcurve.h"


LinLightCurve* getLinLightCurve(long nvalues, int* status)
{
  // Create an empty LinLightCurve object by allocating the required memory.
  LinLightCurve* lc=(LinLightCurve*)malloc(sizeof(LinLightCurve));
  if (NULL==lc) {
    *status=EXIT_FAILURE;
    HD_ERROR_THROW("Error: Could not allocate memory for LinLightCurve!\n", 
		   EXIT_FAILURE);
    return(NULL);
  }
  lc->a = (double*)malloc(nvalues*sizeof(double));
  if (NULL==lc->a) {
    free(lc);
    *status=EXIT_FAILURE;
    HD_ERROR_THROW("Error: Could not allocate memory for LinLightCurve!\n", 
		   EXIT_FAILURE);
    return(NULL);
  }
  lc->b = (double*)malloc(nvalues*sizeof(double));
  if (NULL==lc->b) {
    free(lc->a);
    free(lc);
    *status=EXIT_FAILURE;
    HD_ERROR_THROW("Error: Could not allocate memory for LinLightCurve!\n", 
		   EXIT_FAILURE);
    return(NULL);
  }
  lc->u = (double*)malloc(nvalues*sizeof(double));
  if (NULL==lc->u) {
    free(lc->a);
    free(lc->b);
    free(lc);
    *status=EXIT_FAILURE;
    HD_ERROR_THROW("Error: Could not allocate memory for LinLightCurve!\n", 
		   EXIT_FAILURE);
    return(NULL);
  }
  // Set the number of data points in the light curve.
  lc->nvalues = nvalues;  

  return(lc);
}



int initConstantLinLightCurve(LinLightCurve* lc, double mean_rate, double t0,
			      double step_width)
{
  if (NULL==lc) { 
    // The LinLightCurve object already must exist.
    return(EXIT_FAILURE);
  }
  if (1>lc->nvalues) { 
    // The LinLightCurve object must provide memory for at least 1 data point.
    return(EXIT_FAILURE);
  }
  // Set basic properties of the light curve.
  lc->t0 = t0;
  lc->step_width = step_width;

  // Set the data points of the light curve.
  long index;
  for (index=0; index<lc->nvalues; index++) {
    lc->a[index] = 0.;
    lc->b[index] = mean_rate;
    lc->u[index] = 1.-exp(-mean_rate*step_width);
  }

  return(EXIT_SUCCESS);
}



int initTimmerKoenigLinLightCurve(LinLightCurve* lc, double t0, double step_width,
				  double mean_rate, double sigma, 
				  /** Pointer to GSL random number generator. */
				  gsl_rng *gsl_random_g)
{
  if (NULL==lc) { 
    // The LinLightCurve object already must exist.
    return(EXIT_FAILURE);
  }
  if (1>lc->nvalues) { 
    // The LinLightCurve object must provide memory for at least 1 data point.
    return(EXIT_FAILURE);
  }

  // Set basic properties of the light curve.
  lc->t0 = t0;
  lc->step_width = step_width;

  
  // Create light curve data according to the algorithm proposed by Timmer & Koenig.
  long count;
  // First create a PSD.
  double* psd = (double*)malloc(lc->nvalues*sizeof(double));
  if (NULL==psd) {
    HD_ERROR_THROW("Error: Could not allocate memory for PSD (light curve generation)!\n",
		   EXIT_FAILURE);
    return(EXIT_FAILURE);
  }
  for (count=0; count<lc->nvalues; count++) {
    psd[count] = pow((double)count, -2.);
  }

  // Create Fourier components.
  double* fcomp = (double*)malloc(lc->nvalues*sizeof(double));
  if (NULL==fcomp) {
    HD_ERROR_THROW("Error: Could not allocate memory for Fourier components of light curve!\n",
		   EXIT_FAILURE);
    return(EXIT_FAILURE);
  }
  fcomp[0] = 0.;
  for (count=1; count<lc->nvalues/2; count++) {
    REAL(fcomp, count) = gsl_ran_ugaussian(gsl_random_g)*sqrt(psd[count]);
    IMAG(fcomp, count) = gsl_ran_ugaussian(gsl_random_g)*sqrt(psd[count]);
  }
  fcomp[lc->nvalues/2] = 
    gsl_ran_ugaussian(gsl_random_g)*sqrt(psd[lc->nvalues/2]); // TODO 

  // Perform Fourier transformation.
  gsl_fft_halfcomplex_radix2_inverse(fcomp, 1, lc->nvalues);
  
  // Calculate mean and variance
  double mean=0., variance=0.;
  for (count=0; count<lc->nvalues; count++) {
    mean += fcomp[count];
    variance += pow(fcomp[count], 2.); 
  }
  mean = mean/(double)lc->nvalues;
  variance = variance/(double)lc->nvalues;
  variance-= pow(mean, 2.);   // var = <x^2>-<x>^2

  // Determine the normalized rates from the FFT.
  double* rate = (double*)malloc(lc->nvalues*sizeof(double));
  if (NULL==rate) {
    HD_ERROR_THROW("Error: Could not allocate memory for PSD (light curve generation)!\n",
		   EXIT_FAILURE);
    return(EXIT_FAILURE);
  }
  for (count=0; count<lc->nvalues-1; count++) {
    rate[count] = (fcomp[count]-mean) *sigma/sqrt(variance) + mean_rate;
    // Avoid negative photon rates (no physical meaning):
    if (rate[count]<0.) { rate[count] = 0.; }
  }

  // Determine the auxiliary values for the light curve.
  for (count=0; count<lc->nvalues-1; count++) {
    lc->a[count] = (rate[count+1]-rate[count])/lc->step_width;
    lc->b[count] = rate[count]-(lc->t0+count*lc->step_width)*lc->a[count];
    lc->u[count] = 1.-exp(-lc->a[count]/2.*(pow((count+1)*lc->step_width,2.)-
					    pow(count*lc->step_width,2.)) 
			  -lc->b[count]*lc->step_width);
  }
  // Set the values for the last data point in the light curve.
  lc->a[count] = 0.;
  lc->b[count] = rate[count];
  lc->u[count] = 1.-exp(-rate[count]*lc->step_width);


  return(EXIT_SUCCESS);
}



void freeLinLightCurve(LinLightCurve* lightcurve)
{
  if (NULL!=lightcurve) {
    if (NULL!=lightcurve->a) {
      free(lightcurve->a);
      lightcurve->a=NULL;
    }
    if (NULL!=lightcurve->b) {
      free(lightcurve->b);
      lightcurve->b=NULL;
    }
    if (NULL!=lightcurve->u) {
      free(lightcurve->u);
      lightcurve->u=NULL;
    }
    lightcurve->nvalues=0;
  }
}



double getPhotonTime(LinLightCurve* lc, double time)
{
  if (1>lc->nvalues) {
    // Invalid LinLightCurve!
    return(-1.);
  }

  // The LinLightCurve contains data points, so the
  // general algorithm proposed by Klein & Roberts has to 
  // be applied.

  // Step 1 in the algorithm.
  double u = get_random_number();
  
  // Determine the respective index k of the light curve.
  long k = (long)((time-lc->t0)/lc->step_width);
  assert(k>=0);
  while (k < lc->nvalues) {
    // Step 2 in the algorithm is not required, as the u_k are already stored as
    // auxiliary data in the LinLightCurve object.
    // Step 3 in the algorithm.
    if (u <= lc->u[k]) {
      if (0. != lc->a[k]) {
	return((-lc->b[k] + sqrt(pow(lc->b[k],2.) + pow(lc->a[k]*time,2.) + 
				 2.*lc->a[k]*lc->b[k]*time - 2.*lc->a[k]*log(1.-u)))
	       /lc->a[k]);
      } else { // a_k == 0
	return(time-log(1.-u)/lc->b[k]);
      }

    } else {
      // Step 4 (u > u_k).
      u = (u-lc->u[k])/(1-lc->u[k]);
      k++;
      time = lc->t0 + k*lc->step_width;
    }
  }

  // The range of the light curve was exceeded.
  return(-1.);
}


