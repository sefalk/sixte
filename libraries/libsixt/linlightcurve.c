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
  lc->a = NULL;
  lc->b = NULL;

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

  // Set initial values:
  lc->step_width = 0.;
  lc->t0         = 0.;
  // Clear the newly generated arrays.
  long count;
  for (count=0; count<nvalues; count++) {
    lc->a[count] = 0.;
    lc->b[count] = 0.;
  }
  // Set the number of data points in the light curve.
  lc->nvalues = nvalues;  

  return(lc);
}



LinLightCurve* loadLinLightCurveFromFile(char* filename, double source_rate, int* status)
{
  LinLightCurve* lc=NULL;
  fitsfile* fptr=NULL;

  int crate; // Column in the FITS table containing the rates.
  double* rate=NULL; // Input buffer for light curve rates.

  char msg[MAXMSG];

  do { // Beginning of  ERROR handling loop.

    // Open the FITS file.
    if(fits_open_table(&fptr, filename, READONLY, status)) break;

    int hdunum, hdutype;
    // After opening the FITS file, get the number of the current HDU.
    if (1==fits_get_hdu_num(fptr, &hdunum)) {
      // This is the primary array, so try to move to the first extension 
      // and see if it is a table.
      if (fits_movabs_hdu(fptr, 2, &hdutype, status)) break;
    } else {
      // Get the HDU type.
      if (fits_get_hdu_type(fptr, &hdutype, status)) break;
    }
    // If the current HDU is an image extension, throw an error message:
    if (IMAGE_HDU==hdutype) {
      *status=EXIT_FAILURE;
      sprintf(msg, "Error: FITS extension in file '%s' is not a table "
	      "but an image (HDU number: %d)\n", filename, hdunum);
      HD_ERROR_THROW(msg, *status);
      break;
    }

    // Determine the number of rows in the table.
    long nrows=0;
    if (fits_get_num_rows(fptr, &nrows, status)) break;
    
    // Get a LinLightCurve object from the standard constructor with the
    // required number of time bins.
    lc = getLinLightCurve(nrows, status);
    if (EXIT_SUCCESS!=*status) break;

    // Determine the start time and the step width in the light curve from the FITS header.
    char comment[MAXMSG]; // Buffer
    if (fits_read_key(fptr, TDOUBLE, "TSTART", &lc->t0, comment, status)) break;
    if (fits_read_key(fptr, TDOUBLE, "BINWIDTH", &lc->step_width, comment, status)) break;

    // Determine the column in the FITS table that contains the rates.
    if(fits_get_colnum(fptr, CASEINSEN, "RATE", &crate, status)) break;

    // Load the rates from the FITS table.
    rate = (double*)malloc(nrows*sizeof(double));
    if (NULL==rate) {
      *status=EXIT_FAILURE;
      HD_ERROR_THROW("Error: Could not allocate memory for light curve!\n", *status);
      break;
    }
    long row;
    int anynul=0;
    for (row=0; (row<nrows)&&(EXIT_SUCCESS==*status); row++) {
      rate[row]=0.;
      if (fits_read_col(fptr, TDOUBLE, crate, row+1, 1, 1, &(rate[row]), &(rate[row]), 
			&anynul, status)) break;
      if (0.>rate[row]) {
	*status=EXIT_FAILURE;	
	HD_ERROR_THROW("Error: Rate in light curve must be greater than zero!\n", *status);
	break;
      }
      // Scale the light curve to the desired source photon rate.
      rate[row] *= source_rate;
    }
    
    // Set the data (slopes, intercepts, ...) required by the LinLightCurve object.
    setLinLightCurveData(lc, rate);

  } while(0); // END of ERROR handling loop.

  // --- Clean up ---
  fits_close_file(fptr, status);
  free(rate);

  if (EXIT_SUCCESS==*status) {
    return(lc);
  } else {
    return(NULL);
  }
}



int initConstantLinLightCurve(LinLightCurve* lc, 
			      double t0, double step_width, 
			      double mean_rate)
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
  }

  return(EXIT_SUCCESS);
}



int initTimmerKoenigLinLightCurve(LinLightCurve* lc, double t0, double step_width,
				  double mean_rate, double sigma)
{
  // Create light curve data according to the algorithm 
  // proposed by Timmer & Koenig (1995).

  if (NULL==lc) { 
    // The LinLightCurve object already must exist.
    return(EXIT_FAILURE);
  }
  if (1>lc->nvalues) { 
    // The LinLightCurve object must provide memory for at least 
    // 1 data point.
    return(EXIT_FAILURE);
  }

  // Set basic properties of the light curve.
  lc->t0 = t0;
  lc->step_width = step_width;
  
  // First create a PSD: S(\omega).
  double* psd = (double*)malloc(lc->nvalues/2*sizeof(double));
  if (NULL==psd) {
    HD_ERROR_THROW("Error: Could not allocate memory for PSD "
		   "(light curve generation)!\n", EXIT_FAILURE);
    return(EXIT_FAILURE);
  }

  // Fill the PSD with data.
  long count;
#ifdef TK_LC_POWERLAW
  for (count=0; count<lc->nvalues/2; count++) {
    // Use a powerlaw index (beta) of 1 (for red noise).
    psd[count] = pow(((double)count+1.), -1.);
  }
#endif
#ifdef TK_LC_LORENTZ
  // Soft Intermediate State Cyg X-1 
  // (according to Moritz Boeck, 10.06.2010)
  const double norm = 0.001083116;
  const double index = -1.;
  const double cutoff = 30.;
  const double rms = 0.02537187;
  const double Q = 0.3860634;
  const double f0 = 3.245546/sqrt(1.+1./(4.*Q*Q));
  double f;
  for (count=0; count<lc->nvalues/2; count++) {
    // f_k = k / (N * delta t)
    f = (count+1)/(lc->nvalues*lc->step_width);
    psd[count] = 
      // cutoffpl2
      norm * pow(f,index) * exp(-f/cutoff) +
      // lorentzmb
      2.*pow(rms,2.)*Q*f0/(M_PI/2.+atan(2.*Q)) * 1/(f0*f0+4.*Q*Q*pow(f-f0,2.));
  }
#endif

  // Create Fourier components.
  double* fcomp = (double*)malloc(lc->nvalues*sizeof(double));
  if (NULL==fcomp) {
    HD_ERROR_THROW("Error: Could not allocate memory for Fourier "
		   "components of light curve!\n", EXIT_FAILURE);
    return(EXIT_FAILURE);
  }
  double randr, randi;
  get_gauss_random_numbers(&randr, &randi);
  fcomp[0]             = 0.;
  fcomp[lc->nvalues/2] = randi*sqrt(0.5*psd[lc->nvalues/2-1]);
  for (count=0; count<lc->nvalues/2-1; count++) {
    get_gauss_random_numbers(&randr, &randi);
    REAL(fcomp, count+1) = randr*sqrt(0.5*psd[count]);
    IMAG(fcomp, count+1) = randi*sqrt(0.5*psd[count]);
  }

  // Perform Fourier (back-)transformation.
  gsl_fft_halfcomplex_radix2_backward(fcomp, 1, lc->nvalues);

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
    HD_ERROR_THROW("Error: Could not allocate memory for Timmer & "
		   "Koenig light curve (light curve generation)!\n",
		   EXIT_FAILURE);
    return(EXIT_FAILURE);
  }
  for (count=0; count<lc->nvalues; count++) {
#ifdef TK_LC_POWERLAW
    rate[count] = (fcomp[count]-mean) *sigma/sqrt(variance) + mean_rate;
#else
    // TODO Normalization.
    rate[count] = (fcomp[count]-mean) *sigma/sqrt(variance) + mean_rate; // * mean_rate/mean;
#endif
    // Avoid negative photon rates (no physical meaning):
    if (rate[count]<0.) { rate[count] = 0.; }
  }

  // Set the data (slopes, intercepts, ...) required by the LinLightCurve object.
  setLinLightCurveData(lc, rate);

#ifdef TK_LC_PRINT_LIGHT_CURVE
  // Plot the light curve to an output file for testing.
  headas_printf("Output of light curve to file 'lightcurve.dat' ...\n");
  FILE* lightcurve_file=fopen("lightcurve.dat", "a+");
  if (NULL==lightcurve_file) return(EXIT_FAILURE);
  for (count=0; count<lc->nvalues; count++) {
    fprintf(lightcurve_file, "%lf %lf\n", 
	    lc->t0+count*lc->step_width, 
	    rate[count]);
  }
  fclose(lightcurve_file);
#endif

  // Release allocated memory
  free(psd);
  free(fcomp);
  free(rate);

  return(EXIT_SUCCESS);
}



void setLinLightCurveData(LinLightCurve* lc, double* rate)
{
  long count;

  // Determine the auxiliary values for the light curve.
  for (count=0; count<lc->nvalues-1; count++) {
    lc->a[count] = (rate[count+1]-rate[count])/lc->step_width;
    lc->b[count] = rate[count]; /*-(lc->t0+count*lc->step_width)*lc->a[count];*/
  }
  // Set the values for the last data point in the light curve.
  lc->a[count] = 0.;
  lc->b[count] = rate[count];
}



void freeLinLightCurve(LinLightCurve* lightcurve)
{
  if (NULL!=lightcurve) {
    // Free all memory for objects within the light curve object.
    if (NULL!=lightcurve->a) {
      free(lightcurve->a);
      lightcurve->a=NULL;
    }
    if (NULL!=lightcurve->b) {
      free(lightcurve->b);
      lightcurve->b=NULL;
    }
    lightcurve->nvalues=0;

    // Release memory of the light curve object itself.
    free(lightcurve);
  }
}



double getPhotonTime(LinLightCurve* lc, double time)
{
  if ((1>lc->nvalues)||(lc->step_width<=0.)) {
    // Invalid LinLightCurve!
    return(-1.);
  }

  if (time<lc->t0) {
    return(-1.);
  }

  // The LinLightCurve contains data points, so the
  // general algorithm proposed by Klein & Roberts has to 
  // be applied.

  // Step 1 in the algorithm.
  double u = sixt_get_random_number();

  // Determine the respective index k of the light curve.
  long k = (long)((time-lc->t0)/lc->step_width);
  // Determine the relative time within the k-th interval, i.e., t=0 lies
  // at the beginning of the k-th interval.
  double t;
  double uk;
  assert(k>=0);
  while (k < lc->nvalues) {
    // Determine the relative time within the k-th interval, i.e., t=0 lies
    // at the beginning of the k-th interval.
    t = time-(lc->t0+k*lc->step_width);

    // Step 2 in the algorithm.
    uk = 1.-exp(-lc->a[k]/2.*(pow(lc->step_width,2.)-pow(t,2.))
		-lc->b[k]*(lc->step_width-t));
    // Step 3 in the algorithm.
    if (u <= uk) {
      if ( fabs(lc->a[k]*lc->step_width) > fabs(lc->b[k]*1.e-6) ) { 
	// Instead of checking if a_k = 0. check, whether its product with the 
	// interval length is a very small number in comparison to b_k.
	// If a_k * step_width is much smaller than b_k, the rate in the interval
	// can be assumed to be approximately constant.
	return(lc->t0+k*lc->step_width + 
	       (-lc->b[k]+sqrt(pow(lc->b[k],2.) + pow(lc->a[k]*t,2.) + 
			       2*lc->a[k]*lc->b[k]*t - 2.*lc->a[k]*log(1.-u)))/lc->a[k]);
      } else { // a_k == 0
	return(time-log(1.-u)/lc->b[k]);
      }

    } else {
      // Step 4 (u > u_k).
      u = (u-uk)/(1-uk);
      k++;
      time = lc->t0 + k*lc->step_width;;
    }
  }

  // The range of the light curve has been exceeded.
  return(-1.);
}

