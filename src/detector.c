#include "detector.h"


//////////////////////////////////////////////////////////////////////
void detector_action(
		     Detector* detector,
		     double time, 
		     struct Eventlist_File* eventlist_file,
		     int *status
		     ) 
{
  if (detector->readout != NULL) {
    detector->readout((void*)detector, time, eventlist_file, status);
  }
}





//////////////////////////////////////////////////////////////////////
void depfet_detector_action(
			    void* det,
			    double time, 
			    struct Eventlist_File* eventlist_file,
			    int *status
			    ) 
{
  Detector *detector = (Detector*) det;

  // The WFI detector is read out line by line, with two readout lines 
  // starting in the middle of the detector array. The readout of one 
  // individual line requires the deadtime. The readout is performed at 
  // the beginning of this interval. Then the charges in the line are cleared. 
  // During the cleartime the pixels in the detector line are inactive, i.e., 
  // they cannot receive new charge from incident photons during that time.
  // Photons that hit the pixel during the deadtime but after the cleartime are 
  // accepted and the created charge is stored in the pixel until the next readout
  // process.


  // TODO: Add background photons to the detector pixels
  //insert_background_photons(*detector, background, detector->integration_time);

  // Determine, which line / which 2 lines currently have to be read out.
  while (time > detector->readout_time + detector->dead_time) {
    // The current line number has to be decreased (readout process starts in the
    // middle of the detector).
    detector->readout_line--;
    if (detector->readout_line < 0) {       
      // Start new detector frame:
      detector->frame++; 
      detector->readout_line = 
	(1==detector->readout_directions)?(detector->width-1):(detector->offset-1);

      // Print the time of the current events in order (program status
      // information for the user).
      if (0 == detector->frame % 100000) {
	// Display this status information only each 100 000 frames in order to 
	// avoid numerical load due to the displaying on STDOUT.
	headas_chat(0, "\rtime: %.3lf s ", detector->readout_time);
	fflush(NULL);
      }
    }

    // Update the current detector readout time, which is used in the 
    // event list output.
    detector->readout_time += detector->dead_time;

    // Perform the readout on the 1 or 2 (!) current lines 
    // (i.e., the one or two new lines, chosen in the
    // step before) and write the data to the FITS file.
    // After the readout clear the  lines.
    if(2==detector->readout_directions) {
      readout_line(detector, detector->readout_line, eventlist_file, status);
      clear_detector_line(detector, detector->readout_line);
    }

    readout_line(detector, detector->width -detector->readout_line -1, 
		 eventlist_file, status);
    clear_detector_line(detector, detector->width -detector->readout_line -1);

  } 
}



/*
//////////////////////////////////////////////////////////////////////
void tes_detector_action(
			 void* det,
			 double time, 
			 struct Eventlist_File* eventlist_file,
			 int *fitsstatus
			 ) 
{
  // Do nothing.
}



//////////////////////////////////////////////////////////////////////
void htrs_detector_action(
			  void* det,
			  double time, 
			  struct Eventlist_File* eventlist_file,
			  int *fitsstatus
			  ) 
{
  // Do nothing.
}
*/




//////////////////////////////////////////////////////////////////////////////////
// Add background photons to the detector pixels  according to a given background 
// spectrum and count-rate.
// TODO: consider splits?
// TODO: zufällig Anzahl von Background-Photonen pro Zeitintervall?
static void insert_background_photons(
				      Detector* detector,
				      PointSource background, 
				      double integration_time
				      )
{
  /*
  // determine the number of required photons
  int N_photons = 
    (int)(detector->width*detector->width*background.rate*integration_time);

  int count;
  for (count=0; count<N_photons; count++) {
    // create individual photons
    detector->pixel[(int)(detector->width*get_random_number())]
      [(int)(detector->width*get_random_number())].charge += 
      photon_energy(background, detector);
  }
  */ // TODO
}








/////////////////////////////////////////
inline void readout(
			   Detector* detector,
			   struct Eventlist_File* eventlist_file,
			   int *status
			   ) 
{
  int line;

  // read out the entire detector array
  for (line=0; line<detector->width; line++) {
    readout_line(detector, line, eventlist_file, status);
  }
}





/////////////////////////////////////////////////
inline void readout_line(
				Detector* detector,
				int line,
				struct Eventlist_File* eventlist_file,
				int *status
				) 
{
  int xi;

  // Read out one particular line of the detector array.
  for (xi=0; xi<detector->width; xi++) {
    if (detector->pixel[xi][line].charge > 1.e-6) {
      struct Event event;
      // Determine the detector channel that corresponds to the charge in the 
      // detector pixel.
      event.pha = get_channel(detector->pixel[xi][line].charge, detector);

      // Check lower threshold (PHA and energy):
      if ((event.pha>=detector->pha_threshold) && 
	  (detector->pixel[xi][line].charge>=detector->energy_threshold)) { 

	// REMOVE TODO
	assert(event.pha >= 0);
	// Maybe: if (event.pha < 0) continue;
	
	// There is an event in this pixel, so insert it into the eventlist:
	event.time = detector->readout_time;
	event.xi = xi;
	event.yi = line;
	event.frame = detector->frame;
	event.ra = NAN;
	event.dec = NAN;
	event.sky_xi = 0;
	event.sky_yi = 0;
	add_eventlist_row(eventlist_file, event, status);
      }
    }
  }
}





////////////////////////////////////////////////////////////////////////
Detector* get_Detector(int* status)
{
  Detector* detector=NULL;

  do { // Outer ERROR handling loop.

    headas_chat(5, "allocate memory for detector data structure ...\n");

    // Allocate memory for the detector:
    detector = (Detector*)malloc(sizeof(Detector));
    if (NULL==detector) {
      *status = EXIT_FAILURE;
      HD_ERROR_THROW("Error: not enough memory available to store "
		     "the detector array!\n", *status);
      break;
    } else { // Memory has been allocated successfully.
      detector->pixel=NULL;
      detector->rmf=NULL;
      detector->specific=NULL;
    }

  } while (0); // END of Error handling loop.

  // Return a pointer to the newly created detector data structure.
  return(detector);
}




////////////////////////////////////////////////////////////////////////
int get_DetectorPixels(Detector* detector, int* status)
{

  do { // Outer ERROR handling loop

    headas_chat(5, "allocate memory for detector pixel array ...\n");

    // Allocate memory for the detector pixel array:
    int count;
    detector->pixel = (struct Pixel **) malloc(detector->width*sizeof(struct Pixel *));
    if (detector->pixel) {
      for (count=0; (count<detector->width)&&(*status==EXIT_SUCCESS); count++) {
	detector->pixel[count] = (struct Pixel *)
	  malloc(detector->width * sizeof(struct Pixel));
	
	if (!(detector->pixel[count])) {
	  *status = EXIT_FAILURE;
	  break;
	}
      }
    } else { *status = EXIT_FAILURE; }

    // Check if an error has occurred during memory allocation:
    if (*status==EXIT_FAILURE) {
      HD_ERROR_THROW("Error: not enough memory available to store "
		     "the detector array!\n", *status);
      break;
    }

    // Clear the detector array (at the beginning there are no charges).
    clear_detector(detector);

  } while (0); // END of Error handling loop

  return(*status);
}





////////////////////////////////////////////////////////////////////////////////
// Searches for the minimum distance in an array with 4 entries and returns the 
// corresponding index.
static inline int min_dist(double array[], int directions) 
{
  int count, index=0;
  double minimum=array[0];

  for (count=1; count<directions; count++) {
    if ( (minimum < 0.) ||
	 ((array[count]<=minimum)&&(array[count]>=0.)) ) {
      minimum = array[count];
      index = count;
    }
  }

  return(index);
}





//////////////////////////////////////////////////////////////////////////////// 
// Calculates the Gaussian integral using the GSL complementary error function.
static inline double gaussint(double x) 
{
  return(gsl_sf_erf_Q(x));
}





///////////////////////////////////////////////////////////////
int detector_assign_rsp(Detector *detector, char *filename) {
  fitsfile* fptr=NULL;

  int status=EXIT_SUCCESS;

  // Allocate memory:
  detector->rmf = (struct RMF*)malloc(sizeof(struct RMF));
  if (NULL==detector->rmf) {
    status=EXIT_FAILURE;
    HD_ERROR_THROW("Error: could not allocate memory for RMF!\n", status);
    return(status);
  }

  // Load the RMF from the FITS file using the HEAdas RMF access routines
  // (part of libhdsp).
  fits_open_file(&fptr, filename, READONLY, &status);
  if (status != EXIT_SUCCESS) return(status);
  
  // Read the SPECRESP MATRIX or MATRIX extension:
  if ((status=ReadRMFMatrix(fptr, 0, detector->rmf)) != EXIT_SUCCESS) return(status);

  // Print some information:
  headas_chat(5, "RMF loaded with %ld energy bins and %ld channels\n",
	      detector->rmf->NumberEnergyBins, detector->rmf->NumberChannels);

#ifdef NORMALIZE_RMF
  // Normalize the RMF:
  headas_printf("### Warning: RMF is explicitly renormalized! ###\n");
  NormalizeRMF(detector->rmf);
#else
  // Check if the RSP file contains Matrix Rows with a sum of more than 1.
  // In that case the RSP probably also contains the mirror ARF, what should 
  // normally not be the case for this simulation.
  long chancount, bincount;
  double maxsum = 0.;
  for (bincount=0; bincount<detector->rmf->NumberEnergyBins ; bincount++) {
    double sum = 0.;
    for (chancount=0; chancount<detector->rmf->NumberChannels; chancount++) {
      sum += ReturnRMFElement(detector->rmf, chancount, bincount);
    }
    if (sum > maxsum) {
      maxsum = sum;
    }
  }
  if (maxsum > 1.) {
    headas_printf("### Warning: RSP probably contains mirror ARF (row-sum = %lf)! ###\n", maxsum);
  }
#endif

  // Read the EBOUNDS extension:
  if ((status=ReadRMFEbounds(fptr, 0, detector->rmf)) !=EXIT_SUCCESS) return(status);

  fits_close_file(fptr, &status);
  
  return(status);
}




////////////////////////////////////////////////////////////////
inline void clear_detector(Detector* detector) {
  int y;

  for (y=0; y<detector->width; y++) {
    clear_detector_line(detector, y);
  }
}



////////////////////////////////////////////////////////////////
inline void clear_detector_line(Detector* detector, int line) {
  int x;

  for (x=0; x<detector->width; x++) {
    detector->pixel[x][line].charge  = 0.;
    detector->pixel[x][line].arrival = -detector->dead_time;
  }
}




////////////////////////////////////////////////////////////////
int detector_active(
		    int x, int y,
		    Detector* detector,
		    double time
		    )
{
  if (detector->type == WFI) {
    if ((y==detector->readout_line)||(y==detector->width -detector->readout_line -1)) {
      // If we are at the beginning of the readout interval of the regarded line
      // within the clear time, the photon is not measured.
      if (time - detector->readout_time < detector->clear_time) {
	// The specified line is cleared at the moment, so no photon can 
	// be detected.
	return(0);
      }
    }

    // The specified detector pixel is active at the moment.
    return(1);

  } else {  // Default: detector unknown.
    // The specified detector pixel is active at the moment.
    return(1);
  }
}




////////////////////////////////////////////////////////////////
int htrs_detector_active(
			 int x, int y,
			 Detector* detector,
			 double time
			 )
{
  if (time - detector->pixel[x][y].arrival < detector->dead_time) {
    return(0);  // Pixel is INactive!
  } else {
    return(1);  // Pixel is active!
  }
}




/*
////////////////////////////////////////////////////
long get_pha(
	     float charge, 
	     Detector* detector
	     )
{
  // Perform a binary search to obtain the detector PHA channel 
  // that corresponds to the given detector charge.
  long min, max, row;
  min = 0;
  max = detector->Nchannels-1;
  while (max-min > 1) {
    row = (int)(0.5*(min+max));
    if (detector->ebounds.row[row].E_max < charge) {
      min = row;
    } else {
      max = row;
    }
  }
  // now final decision, wheter max or min is right
  if (detector->ebounds.row[max].E_min < charge) {
    row = max;
  } else {
    row = min;
  }
  // Check if the charge is higher than the highest value in the EBOUNDS table.
  // In that case 'row' is set to 'Nchannels', so the returned channel row+1 is
  //  N O T  a valid PHA channel !!!
  if (detector->ebounds.row[row].E_max < charge) {
    row = detector->Nchannels;
  }

  // Return the PHA channel.
  return(row+1);
  //         |-> as channels start at 1
}
*/


////////////////////////////////////////////////////
long get_channel(
		 float energy, 
		 Detector* detector
		 )
{
  // Check if the charge is outside the range of the energy bins defined
  // in the EBOUNDS table. In that case the return value of this function is '-1'.
  if (detector->rmf->ChannelLowEnergy[0] > energy) {
    return(0); // TODO
  } else if (detector->rmf->ChannelHighEnergy[detector->rmf->NumberChannels-1] < energy) {
    return(detector->rmf->NumberChannels - 1 + detector->rmf->FirstChannel);
  }
  

  // Perform a binary search to obtain the detector PHA channel 
  // that corresponds to the given detector charge.
  long min, max, row;
  min = 0;
  max = detector->rmf->NumberChannels-1;
  while (max-min > 1) {
    row = (long)(0.5*(min+max));
    if (detector->rmf->ChannelHighEnergy[row] < energy) {
      min = row;
    } else {
      max = row;
    }
  }
  // Take the final decision wheter max or min is right:
  if (detector->rmf->ChannelLowEnergy[max] < energy) {
    row = max;
  } else {
    row = min;
  }
  
  // Return the PHA channel.
  return(row + detector->rmf->FirstChannel);
}





////////////////////////////////////////////////////
float get_energy(
		 long channel, 
		 Detector* detector
		 )
{
  channel -= detector->rmf->FirstChannel;
  if ((channel < 0) || (channel >= detector->rmf->NumberChannels)) {
    return(-1.);
  }

  // Return the mean of the energy that corresponds to the specified PHA channel
  // according to the EBOUNDS table.
  return(detector->rmf->ChannelLowEnergy[channel] +
	 get_random_number()*(detector->rmf->ChannelHighEnergy[channel]-
			      detector->rmf->ChannelLowEnergy[channel]));
}




///////////////////////////////////////////
static inline double linear_function(double x, double m, double t)
{
  return(m*x + t);
}





///////////////////////////////////////////
static inline int htrs_get_line(
				struct Point2d point, 
				double m, double dt, 
				Detector* detector
				)
{
  double dy = point.y - m * point.x;
  int line = (int)(dy/dt + detector->width + 1.) -1;

  // Check whether the point lies below the lowest or above the highest allowed line:
  if ((line < 0) || (line>= 2*detector->width)) line = INVALID_PIXEL;

  return(line);
}



///////////////////////////////////////////
static inline double htrs_distance2line(
					struct Point2d point,
					double m, double t
					)
{
  return(sqrt( pow(t-point.y+m*point.x, 2.) / (1+pow(m,2.)) ));
}



///////////////////////////////////////////
inline void htrs_get_lines(
				  struct Point2d point, 
				  Detector* detector, 
				  int* l
				  )
{
  l[0] = htrs_get_line(point,        0.,    detector->h, detector);
  l[1] = htrs_get_line(point, -sqrt(3.), 2* detector->h, detector);
  l[2] = htrs_get_line(point,  sqrt(3.), 2* detector->h, detector);
}




///////////////////////////////////////////
int htrs_get_pixel(
		   Detector* detector, 
		   struct Point2d point,
		   int* x, int* y,
		   double* fraction
		   )
{
  int npixels = 0;
  int l[3];

  htrs_get_lines(point, detector, l);

  // Store the pixel coordinates:
  int pixel = htrs_get_lines2pixel(l, detector);
  struct Point2i pixel_coordinates = htrs_get_pixel2icoordinates(pixel, detector);
  x[0] = pixel_coordinates.x;
  y[0] = pixel_coordinates.y;

  
  // Check for split events:
  int dl[3][6] =  {
    {1, 0, 0, -1, 0, 0},
    {0, 1, 0, 0, -1, 0},
    {0, 0, -1, 0, 0, 1}
  };

  // Distances to neighbouring pixel segments (equilateral triangles, CAN possibly
  // belong to the same pixel).
  double distances[6] = {
    // upper
    htrs_distance2line(point,        0., (l[0]+1 -detector->width)*   detector->h),
    // upper right
    htrs_distance2line(point, -sqrt(3.), (l[1]+1-detector->width)*2.*detector->h),
    // lower right
    htrs_distance2line(point,  sqrt(3.), (l[2]  -detector->width)*2.*detector->h),
    // lower
    htrs_distance2line(point,        0., (l[0]  -detector->width)*   detector->h),
    // lower left
    htrs_distance2line(point, -sqrt(3.), (l[1]  -detector->width)*2.*detector->h),
    // upper left
    htrs_distance2line(point,  sqrt(3.), (l[2]+1-detector->width)*2.*detector->h),
  };


  // Find the closest distance to the nearest neighbouring pixel.
  int count, mindist, secpixel;
  double minimum;
  for(count=0; count<3; count++) {
    mindist = min_dist(distances, 6);
    minimum = distances[mindist];
    distances[mindist] = -1.;

    int k[3] = {l[0]+dl[0][mindist], l[1]+dl[1][mindist], l[2]+dl[2][mindist]};
    secpixel = htrs_get_lines2pixel(k, detector);
    
    if (secpixel != pixel) break;
  }

  if ((minimum > detector->ccsize) || (secpixel == pixel)) {
    // Single event!
    npixels=1;
    fraction[0] = 1.;

  } else {
    // Double event!
    npixels=2;

    pixel_coordinates = htrs_get_pixel2icoordinates(secpixel, detector);
    x[1] = pixel_coordinates.x;
    y[1] = pixel_coordinates.y;
      
    double mindistgauss = gaussint(distances[mindist]/detector->ccsigma);

    fraction[0] = 1. - mindistgauss;
    fraction[1] =      mindistgauss;
  }

  return(npixels);
}





///////////////////////////////////////////
static inline int htrs_get_lines2pixel(int* l, Detector* detector)
{
  if ((l[0]<0)||(l[0]>=2*detector->width)||
      (l[1]<0)||(l[1]>=2*detector->width)||
      (l[2]<0)||(l[2]>=2*detector->width)) {
    return(INVALID_PIXEL);
  } else {
    return(detector->htrs_lines2pixel[l[0]][l[1]][l[2]]);
  }
}





///////////////////////////////////////////
static inline struct Point2i htrs_get_pixel2icoordinates(int pixel, 
							 Detector* detector)
{
  if (pixel != INVALID_PIXEL) {
    return(detector->htrs_pixel2icoordinates[pixel]);
  } else {
    struct Point2i point2i = {-1, -1};
    return(point2i);
  }
}








///////////////////////////////////////////
int htrs_init_Detector(Detector* detector)
{
  //  Detector* detector=NULL;
  struct Point2d* centers = NULL;

  int status=EXIT_SUCCESS;
  char msg[MAXMSG];        // buffer for error output messages

  // Determine hexagonal pixel dimensions:
  detector->h = detector->pixelwidth/2.;      
  detector->a = 2. * detector->h / sqrt(3.);

  do { // Error handling loop 
    
    // Allocate memory and set the relation between the two different 
    // numbering arrays of the pixels in the hexagonal structure.
    // (linear numbering starting at the left bottom and 2D numbering)
    detector->htrs_pixel2icoordinates = 
      (struct Point2i*)malloc(HTRS_N_PIXELS * sizeof(struct Point2i));
    if (detector->htrs_pixel2icoordinates == NULL) {
      status = EXIT_FAILURE;
      sprintf(msg, "Error: Not enough memory available for HTRS initialization!\n");
      HD_ERROR_THROW(msg, status);
      break;
    }

    detector->htrs_icoordinates2pixel = 
      (int**)malloc(detector->width * sizeof(int*));
    if (detector->htrs_icoordinates2pixel == NULL) {
      status = EXIT_FAILURE;
      sprintf(msg, "Error: Not enough memory available for HTRS initialization!\n");
      HD_ERROR_THROW(msg, status);
      break;
    }

    int xi, yi, pixel=0;
    for(xi=0; xi<detector->width; xi++) {
      detector->htrs_icoordinates2pixel[xi] = 
	(int*)malloc(detector->width * sizeof(int));
      if (detector->htrs_icoordinates2pixel[xi] == NULL) {
	status = EXIT_FAILURE;
	sprintf(msg, "Error: Not enough memory available for HTRS "
		"initialization!\n");
	HD_ERROR_THROW(msg, status);
	break;
      }

      for(yi=0 ; yi<detector->width; yi++) {
	detector->htrs_icoordinates2pixel[xi][yi] = INVALID_PIXEL;
      }
    }

    
    // Fill the 2 different arrays for the conversion
    //     pixel index <-> coordinates.
    detector->htrs_icoordinates2pixel[0][1] = 22;
    detector->htrs_icoordinates2pixel[0][2] = 21;
    detector->htrs_icoordinates2pixel[0][3] = 20;
    detector->htrs_icoordinates2pixel[0][4] = 37;
    
    detector->htrs_icoordinates2pixel[1][1] = 23;
    detector->htrs_icoordinates2pixel[1][2] = 9;
    detector->htrs_icoordinates2pixel[1][3] = 8;
    detector->htrs_icoordinates2pixel[1][4] = 19;
    detector->htrs_icoordinates2pixel[1][5] = 36;

    detector->htrs_icoordinates2pixel[2][0] = 24;
    detector->htrs_icoordinates2pixel[2][1] = 10;
    detector->htrs_icoordinates2pixel[2][2] = 2;
    detector->htrs_icoordinates2pixel[2][3] = 7;
    detector->htrs_icoordinates2pixel[2][4] = 18;
    detector->htrs_icoordinates2pixel[2][5] = 35;

    detector->htrs_icoordinates2pixel[3][0] = 25;
    detector->htrs_icoordinates2pixel[3][1] = 11;
    detector->htrs_icoordinates2pixel[3][2] = 3;
    detector->htrs_icoordinates2pixel[3][3] = 1;
    detector->htrs_icoordinates2pixel[3][4] = 6;
    detector->htrs_icoordinates2pixel[3][5] = 17;
    detector->htrs_icoordinates2pixel[3][6] = 34;

    detector->htrs_icoordinates2pixel[4][0] = 26;
    detector->htrs_icoordinates2pixel[4][1] = 12;
    detector->htrs_icoordinates2pixel[4][2] = 4;
    detector->htrs_icoordinates2pixel[4][3] = 5;
    detector->htrs_icoordinates2pixel[4][4] = 16;
    detector->htrs_icoordinates2pixel[4][5] = 33;

    detector->htrs_icoordinates2pixel[5][1] = 27;
    detector->htrs_icoordinates2pixel[5][2] = 13;
    detector->htrs_icoordinates2pixel[5][3] = 14;
    detector->htrs_icoordinates2pixel[5][4] = 15;
    detector->htrs_icoordinates2pixel[5][5] = 32;

    detector->htrs_icoordinates2pixel[6][1] = 28;
    detector->htrs_icoordinates2pixel[6][2] = 29;
    detector->htrs_icoordinates2pixel[6][3] = 30;
    detector->htrs_icoordinates2pixel[6][4] = 31;

    for(xi=0 ; xi<detector->width; xi++) {
      for(yi=0 ; yi<detector->width; yi++) {
	if (detector->htrs_icoordinates2pixel[xi][yi] > 0) {
	  // In the program the pixel index starts at 0, not at 1 !!
	  // So shift all given pixel indices!
	  detector->htrs_icoordinates2pixel[xi][yi]--;
	  detector->htrs_pixel2icoordinates
	    [detector->htrs_icoordinates2pixel[xi][yi]].x = xi;   
	  detector->htrs_pixel2icoordinates
	    [detector->htrs_icoordinates2pixel[xi][yi]].y = yi;		 
	}
      }
    }

    // Now the 2 different numbering schemes can be easily converted 
    // among each other.
    


    // Calculate the centers of the hexagonal HTRS pixels.
    centers = (struct Point2d*)malloc(HTRS_N_PIXELS * sizeof(struct Point2d));
    if (centers == NULL) {
      status = EXIT_FAILURE;
      sprintf(msg, "Error: Not enough memory available for HTRS initialization!\n");
      HD_ERROR_THROW(msg, status);
      break;
    }
    
    for(xi=0; xi<detector->width; xi++) {
      for(yi=0 ; yi<detector->width; yi++) {
	pixel = detector->htrs_icoordinates2pixel[xi][yi];
	if (pixel != INVALID_PIXEL) {
	  centers[pixel].x = (xi-3)                *1.5*detector->a;
	  centers[pixel].y = (yi-3+((xi+1)%2)*0.5) *2.0*detector->h;
	}
      }
    }


    int l[3];
    // Get memory and clear the array which is used to find the pixel
    // for given coordinates on the detector.
    detector->htrs_lines2pixel = 
      (int***) malloc(2*detector->width * sizeof(int**));
    for (l[0]=0; l[0]<2*detector->width; l[0]++) {
      detector->htrs_lines2pixel[l[0]] = 
	(int**) malloc(2*detector->width * sizeof(int*));
      for (l[1]=0; l[1]<2*detector->width; l[1]++) {
	detector->htrs_lines2pixel[l[0]][l[1]] = 
	  (int*) malloc(2*detector->width * sizeof(int));
	for (l[2]=0; l[2]<2*detector->width; l[2]++) {
	  detector->htrs_lines2pixel[l[0]][l[1]][l[2]] = INVALID_PIXEL;
	}
      }
    }

    int direction;
    struct Point2d point;
    const double sin30 = sin(M_PI/6.);
    const double cos30 = cos(M_PI/6.);

    for(pixel=0; pixel<HTRS_N_PIXELS; pixel++) {
      // For each pixel choose 6 points located around the center and
      // determine the line indices which define this pixel section.
    
      for(direction=0; direction<6; direction++) {
	point = centers[pixel];
	
	switch(direction) {
	case 0: // above center
	  point.y += detector->h/2;
	  break;
	case 1: // upper right section
	  point.x += detector->h/2*cos30;
	  point.y += detector->h/2*sin30;
	  break;
	case 2: // lower right section
	  point.x += detector->h/2*cos30;
	  point.y -= detector->h/2*sin30;
	  break;
	case 3: // below center
	  point.y -= detector->h/2;
	  break;
	case 4: // lower left section
	  point.x -= detector->h/2*cos30;
	  point.y -= detector->h/2*sin30;
	  break;
	case 5: // upper left section
	  point.x -= detector->h/2*cos30;
	  point.y += detector->h/2*sin30;
	  break;
	}

	htrs_get_lines(point, detector, l);
	detector->htrs_lines2pixel[l[0]][l[1]][l[2]] = pixel;
      }
    }

  } while (0);  // END of error handling loop


  // --- clean up ---

  free(centers);

  return(status);
}





///////////////////////////////////////
#ifdef GAUSSIAN_CHARGE_CLOUDS
int get_pixel_square(
		     Detector* detector, 
		     struct Point2d position, 
		     int* x, int* y, 
		     double* fraction
		     )
{
  int npixels = 0;  // number of affected pixels

  // The following array entries are used to transform between 
  // different array indices.
  int xe[4] = {1,0,-1,0};   
  int ye[4] = {0,1,0,-1};

  // Calculate pixel indices (integer) of central affected pixel:
  x[0] = (int)(position.x/detector->pixelwidth + (double)(detector->width/2) +1.)-1;
  y[0] = (int)(position.y/detector->pixelwidth + (double)(detector->width/2) +1.)-1;
  
  // If charge cloud size is 0, i.e. no splits are created.
  if (detector->ccsize < 1.e-20) {
    // Only single events are possible!
    npixels = 1;
    fraction[0] = 1.;

  } else {
    // Calculate the distances from the event to the borders of the 
    // surrounding pixel (in [m]):
    double distances[4] = { 
      // distance to right pixel edge
      (x[0]-detector->offset+1)*detector->pixelwidth - position.x,
      // distance to upper edge
      (y[0]-detector->offset+1)*detector->pixelwidth - position.y,
      // distance to left pixel edge
      position.x - (x[0]-detector->offset)*detector->pixelwidth,
      // distance to lower edge
      position.y - (y[0]-detector->offset)*detector->pixelwidth
    };

    int mindist = min_dist(distances, 4);
    if (distances[mindist] < detector->ccsize) {
      // Not a single event!
      x[1] = x[0] + xe[mindist];
      y[1] = y[0] + ye[mindist];

      double mindistgauss = gaussint(distances[mindist]/detector->ccsigma);

      double minimum = distances[mindist];
      // search for the next to minimum distance to an edge
      distances[mindist] = -1.;
      int secmindist = min_dist(distances, 4);
      distances[mindist] = minimum;

      if (distances[secmindist] < detector->ccsize) {
	// Quadruple!
	npixels = 4;

	x[2] = x[0] + xe[secmindist];
	y[2] = y[0] + ye[secmindist];
	x[3] = x[1] + xe[secmindist];
	y[3] = y[1] + ye[secmindist];

	// Calculate the different charge fractions in the 4 affected pixels.
	double secmindistgauss = gaussint(distances[secmindist]/detector->ccsigma);
	fraction[0] = (1.-mindistgauss)*(1.-secmindistgauss);
	fraction[1] =     mindistgauss *(1.-secmindistgauss);
	fraction[2] = (1.-mindistgauss)*    secmindistgauss ;
	fraction[3] =     mindistgauss *    secmindistgauss ;

      } else {
	// Double!
	npixels = 2;

	fraction[0] = 1. - mindistgauss;
	fraction[1] =      mindistgauss;

      } // END of double or Quadruple

    } else {
      // Single event!
      npixels = 1;
      fraction[0] = 1.;

    } // END of check for single event

  } // END of check for charge cloud size equals 0.

  // Check whether all pixels lie inside the detector:
  int count;
  for(count=0; count<npixels; count++) {
    if ((x[count]<0) || (x[count]>=detector->width) ||
	(y[count]<0) || (y[count]>=detector->width)) {
      x[count] = INVALID_PIXEL;
      y[count] = INVALID_PIXEL;
    }
  }

  // Return the number of affected pixel, and 0, if the position lies outside
  // the detector array.
  return(npixels);
}


#else 

// None-Gaussian Charge clouds:
// Calculate the carge splitting according to a concept proposed by Konrad Dennerl.

int get_pixel_square(
		     Detector* detector, 
		     struct Point2d position, 
		     int* x, int* y, 
		     double* fraction
		     )
{
  int npixels = 0;  // number of affected pixels

  // The following array entries are used to transform between 
  // different array indices.
  int xe[4] = {1,0,-1,0};   
  int ye[4] = {0,1,0,-1};

  // -- Determine the affected pixels:

  // Calculate pixel indices (integer) of central affected pixel:
  x[0] = (int)(position.x/detector->pixelwidth + (double)(detector->width/2) +1.)-1;
  y[0] = (int)(position.y/detector->pixelwidth + (double)(detector->width/2) +1.)-1;
  
  // Calculate the distances from the event to the borders of the 
  // surrounding pixel (in [m]):
  double distances[4] = { 
    // distance to right pixel edge
    (x[0]-detector->offset+1)*detector->pixelwidth - position.x,
    // distance to upper edge
    (y[0]-detector->offset+1)*detector->pixelwidth - position.y,
    // distance to left pixel edge
    position.x - (x[0]-detector->offset)*detector->pixelwidth,
    // distance to lower edge
    position.y - (y[0]-detector->offset)*detector->pixelwidth
  };

  int mindist = min_dist(distances, 4);
  x[1] = x[0] + xe[mindist];
  y[1] = y[0] + ye[mindist];

  double minimum = distances[mindist];
  // search for the next to minimum distance to an edge
  distances[mindist] = -1.;
  int secmindist = min_dist(distances, 4);
  distances[mindist] = minimum;

  x[2] = x[0] + xe[secmindist];
  y[2] = y[0] + ye[secmindist];
  x[3] = x[1] + xe[secmindist];
  y[3] = y[1] + ye[secmindist];

  // --- Now we know the affected pixels and can determine the charge fractions.

  fraction[0] = exp(-pow((sqrt(pow(detector->pixelwidth/2-distances[mindist],2.)+
			       pow(detector->pixelwidth/2-distances[secmindist],2.)))
			 /(0.35*75.e-6),2.));
  fraction[1] = exp(-pow((sqrt(pow(detector->pixelwidth/2+distances[mindist],2.)+
			       pow(detector->pixelwidth/2-distances[secmindist],2.)))
			 /(0.35*75.e-6),2.));
  fraction[2] = exp(-pow((sqrt(pow(detector->pixelwidth/2-distances[mindist],2.)+
			       pow(detector->pixelwidth/2+distances[secmindist],2.)))
			 /(0.35*75.e-6),2.));
  fraction[3] = exp(-pow((sqrt(pow(detector->pixelwidth/2+distances[mindist],2.)+
			       pow(detector->pixelwidth/2+distances[secmindist],2.)))
			 /(0.35*75.e-6),2.));
  double sum = fraction[0]+fraction[1]+fraction[2]+fraction[3];
  fraction[0] /= sum;
  fraction[1] /= sum;
  fraction[2] /= sum;
  fraction[3] /= sum;


  npixels = 4;


  // --- Check whether all pixels lie inside the detector:
  int count;
  for(count=0; count<npixels; count++) {
    if ((x[count]<0) || (x[count]>=detector->width) ||
	(y[count]<0) || (y[count]>=detector->width)) {
      x[count] = INVALID_PIXEL;
      y[count] = INVALID_PIXEL;
    }
  }

  // Return the number of affected pixel, and 0, if the position lies outside
  // the detector array.
  return(npixels);
}


#endif



///////////////////////////////////////////////////////
void htrs_free_Detector(Detector* detector)
{
  free(detector->htrs_pixel2icoordinates);

  int count;
  for(count=0; count<detector->width; count++) {
    free(detector->htrs_icoordinates2pixel[count]);
  }
  free(detector->htrs_icoordinates2pixel);

  int count2;
  for(count=0; count<detector->width; count++) {
    for(count2=0; count2<detector->width; count2++) {
      free(detector->htrs_lines2pixel[count][count2]);
    }
    free(detector->htrs_lines2pixel[count]);
  }
  free(detector->htrs_lines2pixel);

}






/*
//////////////////////////////////////////////////////////////////////////////
// Creates split events according to the size of the charge cloud.
// Input values are the detector coordinates given in [m] with the origin
// in the middle of the detector array (det_xa, det_ya),
// and the pixel coordinates with the origin at the edge of the detector.
// Assumption: +/- 3*ccsigma contain nearly the entire charge, so outside this
// region, the remaining charge is neglected.
void split_events(
		  double det_xa, double det_ya, // coordinates of event [m]
		  // pixel coordinates of surrounding pixel
		  int det_xi, int det_yi,       
		  struct Detector detector,     // detector data
		  // energy fractions falling on neighboring pixels (return value)
		  double partition[3][3] 
		  ) 
{
  // The following array entries are used to transform between 
  // different array indices.
  int xe[4] = {2,1,0,1};
  int ye[4] = {1,2,1,0};
  int xd[4][4], yd[4][4];

  xd[0][1] = 2;
  yd[0][1] = 2;
  xd[1][0] = 2;
  yd[1][0] = 2;

  xd[1][2] = 0;
  yd[1][2] = 2;
  xd[2][1] = 0;
  yd[2][1] = 2;

  xd[2][3] = 0;
  yd[2][3] = 0;
  xd[3][2] = 0;
  yd[3][2] = 0;

  xd[3][0] = 2;
  yd[3][0] = 0;
  xd[0][3] = 2;
  yd[0][3] = 0;

  // Calculate the distances from the event to the borders of the surrounding pixel 
  // (in [m]):
  double distances[4] = { 
    // distance to right hand pixel edge
    (det_xi-detector.offset+1)*detector.pixelwidth - det_xa,
    // distance to upper edge
    (det_yi-detector.offset+1)*detector.pixelwidth - det_ya,
    // distance to left hand pixel edge
    det_xa - (det_xi-detector.offset)*detector.pixelwidth,
    // distance to lower edge
    det_ya - (det_yi-detector.offset)*detector.pixelwidth
  };

  // Set the split fractions to their initial values 
  // (with all energy in central pixel):
  int countx, county;
  for (countx=0; countx<3; countx++) {
    for (county=0; county<3; county++) {
      partition[countx][county] = 0.;
    }
  }
  partition[1][1] = 1.;


  // If charge cloud size is 0, i.e. no splits are created, leave function here.
  if (detector.ccsize < 1.e-12) return;

  int mindist = min_dist(distances, 4);

  if (distances[mindist] < detector.ccsize) {
    // not a single event
    double minimum = distances[mindist];
    // search for the next to minimum distance to an edge
    distances[mindist] = -1.;
    int secmindist = min_dist(distances, 4);
    distances[mindist] = minimum;
    
    // check the different orientations according to the corner
    double mindistgauss = gaussint(distances[mindist]/detector.ccsigma);
    double secmindistgauss = gaussint(distances[secmindist]/detector.ccsigma);
    partition[1][1] = (1.-mindistgauss)*(1.-secmindistgauss);
    partition[xe[mindist]][ye[mindist]] = mindistgauss*(1.-secmindistgauss);
    partition[xe[secmindist]][ye[secmindist]] = (1.-mindistgauss)*secmindistgauss;
    partition[xd[mindist][secmindist]][yd[mindist][secmindist]] = 
      mindistgauss*secmindistgauss;
  }  // END of check for single event

}



///////////////////////////////////////////
int get_rmf(
	    Detector *detector,
	    char *rmf_name
	    )
{
  fitsfile *fptr=NULL;     // fitsfile pointer to RMF file
  int count, count2;

  int status=EXIT_SUCCESS; // error status
  char msg[MAXMSG];        // buffer for error output messages


  do {  // error handling loop (is only run once)
    headas_chat(5, "load detector redistribution matrix (RMF) "
		"from file '%s' ...\n", rmf_name);

    // First open the RMF file:
    if (fits_open_file(&fptr, rmf_name, READONLY, &status)) break;
  
    int hdunum, hdutype;
    // After opening the FITS file, get the number of the current HDU.
    if (fits_get_hdu_num(fptr, &hdunum) == 1) {
      // This is the primary array, so try to move to the first extension and see 
      // if it is a table.
      if (fits_movabs_hdu(fptr, 2, &hdutype, &status)) break;
    } else {
      // Get the HDU type.
      if (fits_get_hdu_type(fptr, &hdutype, &status)) break;
    }

    // If it is an image HDU, the program gives an error message.
    if (hdutype==IMAGE_HDU) {
      status=EXIT_FAILURE;
      sprintf(msg, "Error: FITS extension in file '%s' is not a table but "
	      "an image (HDU number: %d)\n", rmf_name, hdunum);
      HD_ERROR_THROW(msg,status);
      break;
    }

    // Determine the number of rows in the FITS file (number of 
    // given points of time).
    long Nrows;
    fits_get_num_rows(fptr, &Nrows, &status);
    detector->mrmf.Nrows = (int)Nrows;

    // Determine the number of columns in the RMF ( = max(sum(Nchan)) ).
    detector->mrmf.Ncols=0;
    int buff_cols, Nchan[1024], ngrp=0;
    int anynul=0;
    for (count=1;((count<=detector->mrmf.Nrows)&&(status==EXIT_SUCCESS));count++) {
      if (fits_read_col(fptr, TINT, 3, count, 1, 1, &ngrp, &ngrp, 
			&anynul, &status)) break;

      for (count2=0; count2<1024; count2++) {
	Nchan[count2] = 0;
      }
      if (fits_read_col(fptr, TINT, 5, count, 1, ngrp, Nchan, Nchan, 
			&anynul, &status))
	break;
      for (count2=0, buff_cols=0; count2<ngrp; count2++) {
	buff_cols += Nchan[count2];
      }

      if (buff_cols > detector->mrmf.Ncols) {
	detector->mrmf.Ncols = buff_cols;
      }
    }
    if (status!=EXIT_SUCCESS) break;
  

    // Get memory for detector redistribution matrix.      
    // -> nrows, ncols might not be equal to Nchannels!!
    detector->mrmf.row = (RMF_Row*) malloc(detector->mrmf.Nrows * sizeof(RMF_Row));
    if (detector->mrmf.row) {
      for (count=0; count<detector->mrmf.Nrows; count++) {
	detector->mrmf.row[count].matrix = (float*) malloc(detector->mrmf.Ncols * 
							  sizeof(float));
	if (detector->mrmf.row[count].matrix == NULL) {
	  status = EXIT_FAILURE;
	  sprintf(msg, "Error: Not enough memory available to store "
		  "the detector RMF!\n");
	  HD_ERROR_THROW(msg,status);
	  break;
	}
      }
    } else {
      status = EXIT_FAILURE;
      sprintf(msg, "Error: Not enough memory available to store "
	      "the detector RMF!\n");
      HD_ERROR_THROW(msg,status);
      break;
    }

    // Clear detector redistribution matrix:
    for (count=0; count<detector->mrmf.Nrows; count++) {
      detector->mrmf.row[count].N_grp = 0;
      detector->mrmf.row[count].E_low = 0.;
      detector->mrmf.row[count].E_high = 0.;
      for (count2=0; count2<1024; count2++) {
	detector->mrmf.row[count].F_chan[count2] = 0;
	detector->mrmf.row[count].N_chan[count2] = 0;
      }
      for (count2=0; count2<detector->mrmf.Ncols; count2++) {
	detector->mrmf.row[count].matrix[count2] = 0.;
      }
    }


    // Read the individual lines of the RMF:
    int renormalized_line = 0;
    for (count=1; (count<=detector->mrmf.Nrows)&&(status==EXIT_SUCCESS); count++) {
      if ((status=read_rmf_fitsrow(&(detector->mrmf.row[count-1].E_low),
				   &(detector->mrmf.row[count-1].E_high), 
				   &(detector->mrmf.row[count-1].N_grp), 
				   detector->mrmf.row[count-1].F_chan, 
				   detector->mrmf.row[count-1].N_chan, 
				   detector->mrmf.row[count-1].matrix, 
				   fptr, count
				   ))!=EXIT_SUCCESS) break;

      // Create probability distribution for each individual row in the matrix,
      // i.e. sum up the matrix columns for each row.
      float sum=0.;
      for (count2=0; count2<detector->mrmf.Ncols; count2++) {
	sum += detector->mrmf.row[count-1].matrix[count2];
	detector->mrmf.row[count-1].matrix[count2] = sum;
      }

      // Check if the sum of the row is 1. In a real RMF file this should 
      // be the case, as the effective area, the detector efficiency, etc. 
      // should be represented in the ARF.
      if (fabs((double)sum-1.0)>0.00001) {
	// Normalize the row, so that the sum is unity:
	for (count2=0; count2<detector->mrmf.Ncols; count2++) {
	  detector->mrmf.row[count-1].matrix[count2] = 
	    detector->mrmf.row[count-1].matrix[count2]/sum;
	}

	// Print a warning for the user, that the lines in the RMF are 
	// normalized by the program.
	if (renormalized_line == 0) {
	  headas_chat(0, "\nWarning: sum of elements in at least 1 RMF row was "
		      "not equal to 1! Possibly the given file is not an RMF but "
		      "an RSP file.\nRow normalizations are set to 1 by the "
		      "simulation!\n\n");
	  renormalized_line = 1;
	}
      }
    }

  } while (0);  // END of error handling loop.


  //---------------
  // Clean up:
  if (fptr) fits_close_file(fptr, &status);

  return(status);
}





///////////////////////////////////////////////////////////////
void free_rmf(mRMF* rmf) 
{
  long count;

  if (rmf) {
    if (rmf->row) {
      for (count=0; count < rmf->Nrows; count++) {
	if (rmf->row[count].matrix) {
	  free(rmf->row[count].matrix);
	}
      }
      free(rmf->row);
    }
  }
}




///////////////////////////////////////////////////////
long detector_rmf(
		  float energy,  // photon energy
		  mRMF* rmf       // detector RMF
		  )
{
  // Find the row in the RMF, which corresponds to the photon energy.
  long min, max, row;
  min = 0;
  max = rmf->Nrows-1;
  while (max-min > 1) {
    row = (int)(0.5*(min+max));
    if (rmf->row[row].E_high < energy) {
      min = row;
    } else {
      max = row;
    }
  }
  // now final decision, whether max or min is right
  if (rmf->row[max].E_low < energy) {
    row = max;
  } else {
    row = min;
  }
  // check if the energy is higher than the highest value in the RMF
  if (rmf->row[row].E_high < energy) {
    return(0);  // TODO
  }

  // For each photon energy there is a particular number of PHA channels with 
  // a finite probability of getting the photon event.
  if (rmf->row[row].F_chan[0] == 0) return(0);

  double p = get_random_number();
  long col;
  min = 0;
  max = rmf->Ncols-1;
  while (max-min > 1) {
    col = (int)(0.5*(min+max));
    if (rmf->row[row].matrix[col] < p) {
      min = col;
    } else {
      max = col;
    }
  }
  // decide whether max or min
  if (rmf->row[row].matrix[min] < p) {
    col = max;
  } else {
    col = min;
  }

  // As 'col' is a matrix index of the reduced RMF,
  // one still has to find the true PHA channel.
  long count;
  for (count=0; count<rmf->row[row].N_grp; count++) {
    col -= rmf->row[row].N_chan[count];
    if (col < 0) break;
  }

  return((long)(rmf->row[row].F_chan[count] + rmf->row[row].N_chan[count]+col));
}




/////////////////////////////////////////////////////////////////////////////////
int get_ebounds(Ebounds *ebounds, int *Nchannels, const char filename[])
{
  fitsfile *fptr=NULL;        // fitsfile pointer to RMF input file

  int status=EXIT_SUCCESS;    // error status
  char msg[MAXMSG];           // buffer for error output messages


  do {  // error handling loop (is only run once)
    headas_chat(5, "load EBOUNDS from file '%s' ...\n", filename);

    // First open the RMF FITS file:
    if (fits_open_file(&fptr, filename, READONLY, &status)) break;
  
    int hdunum, hdutype;
    // After opening the FITS file, get the number of the current HDU.
    if (fits_get_hdu_num(fptr, &hdunum) == 1) {
      // This is the primary array, so try to move to the first extension and 
      // see if it is a table.
      if (fits_movabs_hdu(fptr, 3, &hdutype, &status)) break;
    } else {
      // get the HDU type
      if (fits_get_hdu_type(fptr, &hdutype, &status)) break;
    }

    // image HDU results in an error message
    if (hdutype==IMAGE_HDU) {
      status=EXIT_FAILURE;
      sprintf(msg, "Error: FITS extension in file '%s' is not a table but an image "
	      "(HDU number: %d)\n", filename, hdunum);
      HD_ERROR_THROW(msg,status);
      break;
    }

    // get the number of PHA channels from the corresponding header keyword
    char comment[MAXMSG];
    if (fits_read_key(fptr, TLONG, "DETCHANS", Nchannels, comment, &status)) break;


    // get the number of rows in the FITS file (number of given points of time)
    long nrows;
    fits_get_num_rows(fptr, &nrows, &status);

    if (nrows != *Nchannels) {
      status=EXIT_FAILURE;
      sprintf(msg, "Error: Wrong number of data lines in FITS file '%s'!\n", 
	      filename);
      HD_ERROR_THROW(msg, status);
      break;
    }
  

    // get memory for detector EBOUNDS
    ebounds->row = (struct Ebounds_Row*) malloc(nrows * sizeof(struct Ebounds_Row));
    if (ebounds->row == NULL) {
      status = EXIT_FAILURE;
      sprintf(msg, "Not enough memory available to store the PHA channel data!\n");
      HD_ERROR_THROW(msg,status);
      break;
    }


    // Read the EBOUNDS data from the RMF-FITS file and store them in array
    long row;
    long channel=0;
    float Emin=0., Emax=0.;
    for (row=0; (row < nrows)&&(status==EXIT_SUCCESS); row++) {
      if ((status=read_ebounds_fitsrow(&channel, &Emin, &Emax, fptr, row+1))
	  !=EXIT_SUCCESS) break;
      ebounds->row[row].channel = channel;
      ebounds->row[row].E_min = Emin;
      ebounds->row[row].E_max = Emax;
    }
  } while (0);  // end of error handling loop


  // clean up:
  if (fptr) fits_close_file(fptr, &status);

  return (status);
}




/////////////////////////////////////////////
void free_ebounds(Ebounds* ebounds) {
  if (ebounds) {
    if (ebounds->row) {
      free(ebounds->row);
    }
  }
}
*/






