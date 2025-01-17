/*
   This file is part of SIXTE.

   SIXTE is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   any later version.

   SIXTE is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   For a copy of the GNU General Public License see
   <http://www.gnu.org/licenses/>.


   Copyright 2007-2014 Christian Schmid, FAU
   Copyright 2015-2019 Remeis-Sternwarte, Friedrich-Alexander-Universitaet
                       Erlangen-Nuernberg
*/

#include "sixt.h"
#include "vector.h"
#include "telescope.h"
#include "attitude.h"
#include "check_fov.h"
#include "vignetting.h"

#define TOOLSUB ero_exposure_main
#include "headas_main.c"


/* Program parameters */
struct Parameters {
  char* Attitude;  // filename of the attitude file
  char Vignetting[MAXFILENAME];  // filename of the vignetting file
  char Exposuremap[MAXFILENAME]; // output: exposure map
  char ProgressFile[MAXFILENAME];

  /** Telescope Pointing direction [deg]. */
  float RA, Dec, rollangle;

  double TSTART;
  double timespan;
  /** Step width for the exposure map calculation [s]. */
  double dt;

  double fov_diameter;

  /** Right ascension range [rad]. */
  double ra1 , ra2;
  /** Declination range [rad]. */
  double dec1, dec2;
  /** Number of pixels in right ascension and declination. */
  long ra_bins, dec_bins;

  /** Projection method (1: AIT, 2: SIN). */
  int projection;

  /** Number of interim maps to be stored. */
  int intermaps;

  char clobber;
};


int ero_exposure_getpar(struct Parameters *parameters);


void saveExpoMap(float** const map,
		 const char* const filename,
		 const long naxis1, const long naxis2,
		 struct wcsprm* const wcs,
		 const char clobber,
		 int* const status)
{
  // 1d image buffer for storing in FITS image.
  float* map1d=NULL;

  // FITS file pointer for exposure map image.
  fitsfile* fptr=NULL;

  // String buffer for FITS header.
  char* headerstr=NULL;

  // Store the exposure map in a FITS file image.
  headas_chat(3, "\nstore exposure map in FITS image '%s' ...\n",
	      filename);

  do { // Beginning of error handling loop.

    // Check if the file already exists.
    int exists;
    fits_file_exists(filename, &exists, status);
    CHECK_STATUS_VOID(*status);
    if (0!=exists) {
      if (0!=clobber) {
	// Delete the file.
	remove(filename);
      } else {
	// Throw an error.
	char msg[MAXMSG];
	sprintf(msg, "file '%s' already exists", filename);
	SIXT_ERROR(msg);
	*status=EXIT_FAILURE;
	return;
      }
    }

    // Create a new FITS-file (remove existing one before):
    fits_create_file(&fptr, filename, status);
    CHECK_STATUS_BREAK(*status);

    // Convert the exposure map to a 1d-array to store it in the FITS image.
    map1d=(float*)malloc(naxis1*naxis2*sizeof(float));
    CHECK_NULL_BREAK(map1d, *status,
		    "memory allocation for 1d exposure map buffer failed");
    long x;
    for (x=0; x<naxis1; x++) {
      long y;
      for (y=0; y<naxis2; y++) {
	map1d[x + y*naxis1]=map[x][y];
      }
    }

    // Create an image in the FITS-file (primary HDU):
    long naxes[2]={ naxis1, naxis2 };
    fits_create_img(fptr, FLOAT_IMG, 2, naxes, status);
    //                               |-> naxis
    CHECK_STATUS_BREAK(*status);

    // Add header information about program parameters.
    // The second parameter "1" means that the headers are written
    // to the first extension.
    HDpar_stamp(fptr, 1, status);
    CHECK_STATUS_BREAK(*status);

    // Write WCS header keywords.
    int nkeyrec;
    if (0!=wcshdo(0, wcs, &nkeyrec, &headerstr)) {
      SIXT_ERROR("construction of WCS header failed");
      *status=EXIT_FAILURE;
      break;
    }
    char* strptr=headerstr;
    while (strlen(strptr)>0) {
      char strbuffer[81];
      strncpy(strbuffer, strptr, 80);
      strbuffer[80]='\0';
      fits_write_record(fptr, strbuffer, status);
      CHECK_STATUS_BREAK(*status);
      strptr+=80;
    }
    CHECK_STATUS_BREAK(*status);

    // Write the image to the file.
    long fpixel[2]={1, 1}; // Lower left corner.
    //              |--|--> FITS coordinates start at (1,1), NOT (0,0).
    // Upper right corner.
    long lpixel[2]={naxis1, naxis2};
    fits_write_subset(fptr, TFLOAT, fpixel, lpixel, map1d, status);
    CHECK_STATUS_BREAK(*status);

  } while(0); // End of error handling loop.

  // Close the exposure map FITS file.
  if(NULL!=fptr) fits_close_file(fptr, status);

  // Release memory.
  if (NULL!=map1d) {
    free(map1d);
  }
  if (NULL!=headerstr) {
    free(headerstr);
  }
}


int ero_exposure_main()
{
  // Program parameters.
  struct Parameters par;

  Attitude* ac=NULL;
  Vignetting* vignetting=NULL;

  // Array for the calculation of the exposure map.
  float** expoMap=NULL;

  // WCS data structure used for projection.
  struct wcsprm wcs={ .flag=-1 };

  // Output file for progress status.
  FILE* progressfile=NULL;

  // Error status.
  int status=EXIT_SUCCESS;


  // Register HEATOOL:
  set_toolname("ero_exposure");
  set_toolversion("0.12");


  do { // Beginning of the ERROR handling loop.

    // --- Initialization ---

    // Read the program parameters using PIL library.
    if ((status=ero_exposure_getpar(&par))) break;

    // Get memory for the exposure map.
    expoMap=(float**)malloc(par.ra_bins*sizeof(float*));
    if (NULL!=expoMap) {
      long x;
      for (x=0; x<par.ra_bins; x++) {
	expoMap[x]=(float*)malloc(par.dec_bins*sizeof(float));
	if (NULL!=expoMap[x]) {
	  // Clear the exposure map.
	  long y;
	  for (y=0; y<par.dec_bins; y++) {
	    expoMap[x][y]=0.0;
	  }
	} else {
	  status=EXIT_FAILURE;
	  SIXT_ERROR("memory allocation for exposure map failed");
	  break;
	}
      }
    } else {
      status=EXIT_FAILURE;
      SIXT_ERROR("memory allocation for exposure map failed");
      break;
    }
    CHECK_STATUS_BREAK(status);

    // Set up the WCS data structure.
    if (0!=wcsini(1, 2, &wcs)) {
      SIXT_ERROR("initalization of WCS data structure failed");
      status=EXIT_FAILURE;
      break;
    }
    wcs.naxis=2;
    wcs.crpix[0]=par.ra_bins/2 +0.5;
    wcs.crpix[1]=par.dec_bins/2+0.5;
    wcs.crval[0]=0.5*(par.ra1 +par.ra2 )*180./M_PI;
    wcs.crval[1]=0.5*(par.dec1+par.dec2)*180./M_PI;
    wcs.cdelt[0]=(par.ra2 -par.ra1 )*180./M_PI/par.ra_bins*-1.0; // needs to be <0 as coordinates increase to the left
    wcs.cdelt[1]=(par.dec2-par.dec1)*180./M_PI/par.dec_bins;
    strcpy(wcs.cunit[0], "deg");
    strcpy(wcs.cunit[1], "deg");
    if (1==par.projection) {
      strcpy(wcs.ctype[0], "RA---AIT");
      strcpy(wcs.ctype[1], "DEC--AIT");
    } else if (2==par.projection) {
      strcpy(wcs.ctype[0], "RA---SIN");
      strcpy(wcs.ctype[1], "DEC--SIN");
    } else if (3==par.projection) {
      strcpy(wcs.ctype[0], "GLON-AIT");
      strcpy(wcs.ctype[1], "GLAT-AIT");
    } else {
      SIXT_ERROR("projection type not supported");
      status=EXIT_FAILURE;
      break;
    }

    // Calculate the minimum cos-value for sources inside the FOV:
    // (angle(x0,source) <= 1/2 * diameter)
    const double fov_min_align=cos(par.fov_diameter/2.);
    double field_min_align;
    if ((par.ra2-par.ra1 > M_PI/6.) ||
	(par.dec2-par.dec1 > M_PI/6.)) {
      // Actually -1 should be sufficient, but -2 is even safer.
      field_min_align=-2.;
    } else {
      field_min_align=
	cos((sqrt(pow(par.ra2-par.ra1, 2.)+pow(par.dec2-par.dec1, 2.))+
	     par.fov_diameter)/2.);
    }

    // Initialize the random number generator.
    sixt_init_rng((int)time(NULL), &status);
    CHECK_STATUS_BREAK(status);

    // Set the progress status output file.
    char ucase_buffer[MAXFILENAME];
    strcpy(ucase_buffer, par.ProgressFile);
    strtoupper(ucase_buffer);
    if (0!=strcmp(ucase_buffer, "STDOUT")) {
      progressfile=fopen(par.ProgressFile, "w+");
      char msg[MAXMSG];
      sprintf(msg, "could not open file '%s' for output of progress status",
	      par.ProgressFile);
      CHECK_NULL_BREAK(progressfile, status, msg);
    }

    // Set up the Attitude.
    if (par.Attitude==NULL) {
      // Set up a simple pointing attitude.
      ac=getPointingAttitude(0., par.TSTART, par.TSTART+par.timespan,
			     par.RA*M_PI/180., par.Dec*M_PI/180., par.rollangle*M_PI/180., &status);
      CHECK_STATUS_BREAK(status);

    } else {
      // Load the attitude from the given file.
      ac=loadAttitude(par.Attitude, &status);
      CHECK_STATUS_BREAK(status);

      // Check if the required time interval for the simulation
      // is a subset of the time described by the attitude file.
      // Note that MJDREF is assumed as the value from the attitude file.
      checkAttitudeTimeCoverage(ac, ac->mjdref, par.TSTART,
				par.TSTART+par.timespan, &status);
      CHECK_STATUS_BREAK(status);
    }
    // END of setting up the attitude.

    // Load the Vignetting data.
    if (strlen(par.Vignetting)>0) {
      strcpy(ucase_buffer, par.Vignetting);
      strtoupper(ucase_buffer);
      if (0!=strcmp(ucase_buffer, "NONE")) {
	vignetting=newVignetting(par.Vignetting, &status);
	CHECK_STATUS_BREAK(status);
      }
    }

    // --- END of Initialization ---


    // --- Beginning of Exposure Map calculation
    headas_chat(3, "calculate the exposure map ...\n");

    // Simulation progress status (running from 0 to 100).
    unsigned int progress=0;
    if (NULL==progressfile) {
      headas_chat(2, "\r%.0lf %%", 0.);
      fflush(NULL);
    } else {
      rewind(progressfile);
      fprintf(progressfile, "%.2lf", 0.);
      fflush(progressfile);
    }

    // LOOP over the given time interval from TSTART to TSTART+timespan in steps of dt.
    int intermaps=0;
    double time;
    for (time=par.TSTART; time<par.TSTART+par.timespan; time+=par.dt) {

      // Check if an interim map should be saved now.
      if (par.intermaps>0) {
	if (time >= par.TSTART+intermaps*(par.timespan/par.intermaps)) {
	  // Construct the filename.
	  char filename[MAXFILENAME];
	  strncpy(filename, par.Exposuremap,
		  strlen(par.Exposuremap)-5);
	  filename[strlen(par.Exposuremap)-5]='\0';
	  char buffer[MAXFILENAME];
	  sprintf(buffer, "_%d.fits", intermaps);
	  strcat(filename, buffer);

	  // Save the interim map.
	  saveExpoMap(expoMap, filename, par.ra_bins, par.dec_bins,
		      &wcs, par.clobber, &status);
	  CHECK_STATUS_BREAK(status);

	  intermaps++;
	}
      }
      // END of saving an interim map.

      // Determine the telescope pointing direction at the current time.
      Vector telescope_nz=getTelescopeNz(ac, time, &status);
      CHECK_STATUS_BREAK(status);

      // Calculate the RA and DEC of the pointing direction.
      double telescope_ra, telescope_dec;
      calculate_ra_dec(telescope_nz, &telescope_ra, &telescope_dec);

      // Check if the specified field of the sky might be within the FOV.
      // Otherwise break this run and continue at the beginning of the loop
      // with the next time step.
      Vector pixpos=
	unit_vector(0.5*(par.ra1+par.ra2), 0.5*(par.dec1+par.dec2));
      if (check_fov(&pixpos, &telescope_nz, field_min_align)!=0) {
	continue;
      }

      // 2d Loop over the exposure map in order to determine all pixels that
      // are currently within the FOV.
      long x;
      for (x=0; x<par.ra_bins; x++) {
	long y;
	for (y=0; y<par.dec_bins; y++) {

	  double pixcrd[2]={ x+1., y+1. };
	  double imgcrd[2], world[2];
	  double phi, theta;
	  int status2=0;
	  wcsp2s(&wcs, 1, 2, pixcrd, imgcrd, &phi, &theta, world, &status2);
	  if (3==status2) {
	    // Pixel does not correspond to valid world coordinates.
	    continue;
	  } else if (0!=status2) {
	    SIXT_ERROR("projection failed");
	    status=EXIT_FAILURE;
	    break;
	  }


		// galactic projection -> need to convert coordinates
		if (par.projection>=3){
			convert_galLB2RAdec(world);
		}


	  // Determine a unit vector for the calculated RA and Dec.
	  pixpos=unit_vector(world[0]*M_PI/180., world[1]*M_PI/180.);

	  // Check if the current pixel lies within the FOV.
	  if (check_fov(&pixpos, &telescope_nz, fov_min_align)==0) {
	    // Pixel lies inside the FOV!

	    // Calculate the off-axis angle ([rad])
	    double delta=acos(scalar_product(&telescope_nz, &pixpos));

	    // Add the exposure time step weighted with the vignetting
	    // factor for this particular off-axis angle at 1 keV.
	    double addvalue=par.dt;
	    if (NULL!=vignetting) {
	      addvalue*=get_Vignetting_Factor(vignetting, 1., delta, 0.);
	    }
	    expoMap[x][y]+=addvalue;
	  }
	}
	CHECK_STATUS_BREAK(status);
      }
      CHECK_STATUS_BREAK(status);

      // Program progress output.
      while((unsigned int)((time-par.TSTART)*100./par.timespan)>progress) {
	progress++;
	if (NULL==progressfile) {
	  headas_chat(2, "\r%.0lf %%", progress*1.);
	  fflush(NULL);
	} else {
	  rewind(progressfile);
	  fprintf(progressfile, "%.2lf", progress*1./100.);
	  fflush(progressfile);
	}
      }
    }
    CHECK_STATUS_BREAK(status);
    // END of LOOP over the specified time interval.

    // Progress output.
    if (NULL==progressfile) {
      headas_chat(2, "\r%.0lf %%\n", 100.);
      fflush(NULL);
    } else {
      rewind(progressfile);
      fprintf(progressfile, "%.2lf", 1.);
      fflush(progressfile);
    }

    // END of generating the exposure map.

    // Store the exposure map in the output file.
    saveExpoMap(expoMap, par.Exposuremap, par.ra_bins, par.dec_bins,
		&wcs, par.clobber, &status);
    CHECK_STATUS_BREAK(status);

  } while(0); // END of the error handling loop.


  // --- Cleaning up ---
  headas_chat(3, "cleaning up ...\n");

  // Clean up the random number generator.
  sixt_destroy_rng();

  // Release memory.
  freeAttitude(&ac);
  destroyVignetting(&vignetting);
  wcsfree(&wcs);

  // Release memory of exposure map.
  if (NULL!=expoMap) {
    long x;
    for (x=0; x<par.ra_bins; x++) {
      if (NULL!=expoMap[x]) {
	free(expoMap[x]);
	expoMap[x]=NULL;
      }
    }
    free(expoMap);
  }

  if (EXIT_SUCCESS==status) headas_chat(3, "finished successfully!\n\n");
  return(status);
}


int ero_exposure_getpar(struct Parameters *par)
{
  // String input buffer.
  char* sbuffer=NULL;

  // Error status.
  int status=EXIT_SUCCESS;

  // Read all parameters via the ape_trad_ routines.

  query_simput_parameter_file_name("Attitude", &(par->Attitude), &status);

  // only load RA,Dec if Attitude is not given
  if (par->Attitude==NULL) {
	  query_simput_parameter_float("RA",&(par->RA),&status);
	  query_simput_parameter_float("Dec",&(par->Dec),&status);
	  query_simput_parameter_float("rollangle",&(par->rollangle),&status);
	  headas_chat(3, "using RA=%.3f, Dec=%.3f as no Attitude file is given\n",par->RA,par->Dec);
  } else {
	  // set to default values
	  par->RA=0.0;
	  par->Dec=0.0;
	  par->rollangle=0.0;
	  headas_chat(3, "using Attitude File: %s \n",par->Attitude);
  }

  // Get the filename of the vignetting data file (FITS file).
  status=ape_trad_query_string("Vignetting", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the vignetting file");
    return(status);
  }
  strcpy(par->Vignetting, sbuffer);
  free(sbuffer);

  // Get the filename of the output exposure map (FITS file).
  status=ape_trad_query_file_name("Exposuremap", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the filename of the exposure map");
    return(status);
  }
  strcpy(par->Exposuremap, sbuffer);
  free(sbuffer);

  // Read the diameter of the FOV (in arcmin).
  status=ape_trad_query_double("fov_diameter", &par->fov_diameter);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the diameter of the FOV");
    return(status);
  }

  // Get the start time of the exposure map calculation.
  status=ape_trad_query_double("TSTART", &par->TSTART);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the 'TSTART' parameter");
    return(status);
  }

  // Get the timespan for the exposure map calculation.
  status=ape_trad_query_double("timespan", &par->timespan);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the 'timespan' parameter");
    return(status);
  }

  // Get the time step for the exposure map calculation.
  status=ape_trad_query_double("dt", &par->dt);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the 'dt' parameter");
    return(status);
  }

  // Get the position of the desired section of the sky
  // (right ascension and declination range).
  status=ape_trad_query_double("ra1", &par->ra1);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the 'ra1' parameter");
    return(status);
  }
  status=ape_trad_query_double("ra2", &par->ra2);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the 'ra2' parameter");
    return(status);
  }
  status=ape_trad_query_double("dec1", &par->dec1);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the 'dec1' parameter");
    return(status);
  }
  status=ape_trad_query_double("dec2", &par->dec2);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the 'dec2' parameter");
    return(status);
  }
  // Get the number of x- and y-bins for the exposure map.
  status=ape_trad_query_long("ra_bins", &par->ra_bins);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the number of RA bins");
    return(status);
  }
  status=ape_trad_query_long("dec_bins", &par->dec_bins);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the number of DEC bins");
    return(status);
  }

  status=ape_trad_query_int("projection", &par->projection);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the projection type");
    return(status);
  }

  status=ape_trad_query_int("intermaps", &par->intermaps);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the number of inter-maps");
    return(status);
  }

  status=ape_trad_query_string("ProgressFile", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the progress status file");
    return(status);
  }
  strcpy(par->ProgressFile, sbuffer);
  free(sbuffer);

  status=ape_trad_query_bool("clobber", &par->clobber);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the clobber parameter");
    return(status);
  }

  // Convert angles from [deg] to [rad].
  par->ra1 *=M_PI/180.;
  par->ra2 *=M_PI/180.;
  par->dec1*=M_PI/180.;
  par->dec2*=M_PI/180.;

  // Convert angles from [arc min] to [rad].
  par->fov_diameter*=M_PI/180./60.;

  return(status);
}
