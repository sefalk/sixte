#include "genpat.h"


static inline GenEvent emptyEvent() 
{
  GenEvent empty_event = {.frame=0};
  assert(0==empty_event.rawx);
  assert(0==empty_event.rawy);
  assert(0==empty_event.pileup);
  assert(0==empty_event.pha);
  assert(0.==empty_event.charge);
  assert(0==empty_event.frame);

  return(empty_event);
}


static inline void clearGenPatPixels(GenDet* const det, 
				     GenEvent** const pixels) 
{
  int ii, jj;
  for (ii=0; ii<det->pixgrid->xwidth; ii++) {
    for (jj=0; jj<det->pixgrid->ywidth; jj++) {
      pixels[ii][jj] = emptyEvent();
    }
  }
}


			     
static void add2GenPatList(GenDet* const det, 
			   GenEvent** const pixels, 
			   const int x, const int y, 
			   const float split_threshold,
			   GenEvent** const list, 
			   int* const nlist)
{
  // Check if the pixel is already contained in the list.
  int ii; 
  for (ii=0; ii<*nlist; ii++) {
    if (list[ii]==&pixels[x][y]) {
      // The pixel is already contained in the list.
      return;
    }
  }

  // Add the event to the list.
  list[*nlist] = &pixels[x][y];
  (*nlist)++;
  assert(*nlist<1000);

  // Check the surrounding pixels.
#ifdef DIAGONAL_PATTERN_PILEUP
  // Check the diagonal pixels for pattern pile-up.
  int jj;
  int xmin = MAX(0, x-1);
  int xmax = MIN(det->pixgrid->xwidth-1, x+1);
  int ymin = MAX(0, y-1);
  int ymax = MIN(det->pixgrid->ywidth-1, y+1);
  for (ii=xmin; ii<=xmax; ii++) {
    for (jj=ymin; jj<=ymax; jj++) {
      if (pixels[ii][jj].charge > split_threshold) {
	add2GenPatList(det, pixels, ii, jj, split_threshold, list, nlist);
      }
    }
  }
#else
  // Simple Pattern check: do NOT check diagonal pixels.
  int min = MAX(0, x-1);
  int max = MIN(det->pixgrid->xwidth-1, x+1);
  for (ii=min; ii<=max; ii++) {
    if (pixels[ii][y].charge > split_threshold) {
      add2GenPatList(det, pixels, ii, y, split_threshold, list, nlist);
    }
  }
  min = MAX(0, y-1);
  max = MIN(det->pixgrid->ywidth-1, y+1);
  for (ii=min; ii<=max; ii++) {
    if (pixels[x][ii].charge > split_threshold) {
      add2GenPatList(det, pixels, x, ii, split_threshold, list, nlist);
    }
  }
#endif // END of neglect diagonal pixels.
}



static void findMaxCharge(GenDet* const det,
			  GenEvent** const pixels,
			  int* const x, 
			  int* const y)
{
  int xn = *x;
  int yn = *y;

#ifdef DIAGONAL_PATTERN_PILEUP
  // Check the diagonal pixels for pattern pile-up.
  int ii, jj;
  int xmin = MAX(0, *x-1);
  int xmax = MIN(det->pixgrid->xwidth-1, *x+1);
  int ymin = MAX(0, *y-1);
  int ymax = MIN(det->pixgrid->ywidth-1, *y+1);
  for (ii=xmin; ii<=xmax; ii++) {
    for (jj=ymin; jj<=ymax; jj++) {
      if (pixels[ii][jj].charge > pixels[xn][yn].charge) {
	xn = ii;
	yn = jj;
      }
    }
  }
#else
  // Simple Pattern check: do NOT check diagonal pixels.
  int ii;
  int min = MAX(0, *x-1);
  int max = MIN(det->pixgrid->xwidth-1, *x+1);
  for (ii=min; ii<=max; ii++) {
    if (pixels[ii][*y].charge > pixels[xn][yn].charge) {
      xn = ii;
      yn = *y;
    }
  }
  min = MAX(0, *y-1);
  max = MIN(det->pixgrid->ywidth-1, *y+1);
  for (ii=min; ii<=max; ii++) {
    if (pixels[*x][ii].charge > pixeks[xn][yn].charge) {
      xn = *x;
      yn = ii;
    }
  }
#endif

  // If there is a pixel in the neigborhood with a bigger charge than
  // the current maximum, perform an iterative function call.
  if ((xn!=*x) || (yn!=*y)) {
    findMaxCharge(det, pixels, &xn, &yn);
    // Return the new maximum.
    *x = xn;
    *y = yn;
  }
}



static void GenPatIdentification(GenDet* const det, 
				 GenEvent** const pixels, 
				 GenPatternFile* const file, 
				 struct PatternStatistics* const patstat,
				 int* const status)
{
  GenEvent* list[1000];
  int nlist;

  // Loop over all pixels, searching charges/PHA values above 
  // the primary event threshold.
  int ii, jj;
  for (ii=0; ii<det->pixgrid->xwidth; ii++) {
    for (jj=0; jj<det->pixgrid->ywidth; jj++) {
      if (pixels[ii][jj].charge > det->threshold_event_lo_keV) {
	// Found an event above the primary event threshold.

	// Find the local charge maximum.
	int maxx, maxy;
	findMaxCharge(det, pixels, &maxx, &maxy);
	
	// Create a temporary event list of all pixels in the
	// neighborhood above the split threshold.
	float split_threshold;
	if (det->threshold_split_lo_fraction > 0) {
	  split_threshold = det->threshold_split_lo_fraction*pixels[maxx][maxy].charge;
	} else {
	  split_threshold = det->threshold_split_lo_keV;
	}
	nlist=0;
	add2GenPatList(det, pixels, maxx, maxy, split_threshold, list, &nlist);
	// Now 'list' contains all events contributing to this pattern.

	// Check if the pattern lies at the borders of the detectors.
	// In that case it is flagged as border pattern and will be
	// treated as invalid.
	int kk;
	int border=0;
	for (kk=0; kk<nlist; kk++) {
	  if ((0==list[kk]->rawx) || (det->pixgrid->xwidth-1==list[kk]->rawx) ||
	      (0==list[kk]->rawy) || (det->pixgrid->ywidth-1==list[kk]->rawy)) {
	    border=1;
	    break;
	  }
	}

	// Check if the pattern covers a larger area than a 3x3 matrix.
	int large=0;
	for (kk=1; kk<nlist; kk++) {
	  if ((abs(list[kk]->rawx-list[0]->rawx)>1) ||
	      (abs(list[kk]->rawy-list[0]->rawy)>1)) {
	    large = 1;
	    break;
	  }
	}

	// Determine the pattern code.
	float total_charge = pixels[maxx][maxy].charge;
	int pattern_code = 0;
	for (kk=1; kk<nlist; kk++) {
	  if (list[kk]->rawy == maxy-1) {
	    if (list[kk]->rawx == maxx-1) {
	      pattern_code += 1;
	      total_charge += list[kk]->charge;
	    } else if (list[kk]->rawx == maxx) {
	      pattern_code += 2;
	      total_charge += list[kk]->charge;
	    } else if (list[kk]->rawx == maxx+1) {
	      pattern_code += 4;
	      total_charge += list[kk]->charge;
	    }
	  } else if (list[kk]->rawy == maxy) {
	    if (list[kk]->rawx == maxx-1) {
	      pattern_code += 8;
	      total_charge += list[kk]->charge;
	    } else if (list[kk]->rawx == maxx+1) {
	      pattern_code += 16;
	      total_charge += list[kk]->charge;
	    }
	  } else if (list[kk]->rawy == maxy+1) {
	    if (list[kk]->rawx == maxx-1) {
	      pattern_code += 32;
	      total_charge += list[kk]->charge;
	    } else if (list[kk]->rawx == maxx) {
	      pattern_code += 64;
	      total_charge += list[kk]->charge;
	    } else if (list[kk]->rawx == maxx+1) {
	      pattern_code += 128;
	      total_charge += list[kk]->charge;
	    }
	  }
	}
	// END of determine the pattern code.

	// Determine the pattern grade.
	GenPattern pattern = {
	  .pat_type = getGenPatGrade(det->pattern_identifier,
				     pattern_code, border, large),
	  .event = pixels[maxx][maxy]
	};

	// Combine the PHA values of the individual events.
	pattern.event.pha = getEBOUNDSChannel(total_charge, det->rmf);

	// Store the PHA values of the pixels above the threshold in  the 
	// 3x3 matrix around the central pixel.
	for (kk=0; kk<9; kk++) {
	  pattern.phas[kk] = 0;
	}
	for (kk=0; kk<nlist; kk++) {
	  if ((abs(list[kk]->rawx-maxx)<2) && (abs(list[kk]->rawy-maxy)<2)) {
	    pattern.phas[(list[kk]->rawx-maxx+1) + (3*(list[kk]->rawy-maxy+1))] = 
	      list[kk]->pha;
	  }
	}

	
	// Store the pattern in the output file.
#ifdef ONLY_VALID_PATTERNS
	if (pattern.pat_type != det->pattern_identifier->invalid) {
#endif
	  addGenPattern2File(file, &pattern, status);	  
#ifdef ONLY_VALID_PATTERNS
	}
#endif
	// END of storing the pattern data in the output file.


	// Store the information about the pattern grade in the
	// statistics data structure.
	if (pattern.pat_type==det->pattern_identifier->invalid) {
	  patstat->ninvalids++;
	  break;
	} else if (pattern.pat_type==1) {
	  patstat->ngrade1++;
	  break;
	} else if (pattern.pat_type==2) {
	  patstat->ngrade2++;
	  break;
	} else if (pattern.pat_type==3) {
	  patstat->ngrade3++;
	  break;
	} else if (pattern.pat_type==4) {
	  patstat->ngrade4++;
	  break;
	} else {
	  *status=EXIT_FAILURE;
	  HD_ERROR_THROW("Error: Unknown Pattern Type!\n", *status);
	  return;
	}

	// Check if it's a valid pattern.
	if (pattern.pat_type != det->pattern_identifier->invalid) {
	  patstat->nvalids++;
	}

	// Check for pile-up.
	if (pattern.event.pileup > 0) {
	  patstat->npileup++;
	  if (pattern.pat_type != det->pattern_identifier->invalid) {
	    patstat->npileup_valid++;
	  } else {
	    patstat->npileup_invalid++;
	  }
	  if (1==pattern.pat_type) {
	    patstat->npileup_grade1++;
	  }
	}
	// END of gathering statistical data about the pattern type.
      }
      // END of found an event above the primary threshold.
    }
  }
  // END of loop over all pixels searching for events above the
  // primary event threshold.
}



////////////////////////////////////
/** Main procedure. */
int genpat_main() {

  // Containing all programm parameters read by PIL
  struct Parameters parameters; 
  // Detector data structure (containing the pixel array, its width, ...).
  GenDet* det=NULL;
  // Output event file. 
  GenPatternFile* output_file=NULL;
  // Detector pixel array.
  GenEvent** pixels=NULL;
  // Pattern statistics. Count the numbers of the individual pattern types
  // and store this information in the output event file.
  struct PatternStatistics patstat={
    .ngrade1=0,
    .ngrade2=0,
    .ngrade3=0,
    .ngrade4=0,
    .nvalids=0,
    .ninvalids=0,
    .npileup=0,
    .npileup_grade1=0,
    .npileup_valid=0,
    .npileup_invalid=0
  };

  int status=EXIT_SUCCESS; // Error status.


  // Register HEATOOL:
  set_toolname("genpat");
  set_toolversion("0.01");


  do { // Beginning of the ERROR handling loop (will at most be run once).

    // --- Initialization ---

    headas_chat(3, "initialization ...\n");

    // Initialize HEADAS random number generator.
    HDmtInit(SIXT_HD_RANDOM_SEED);

    // Read parameters using PIL library:
    if ((status=getpar(&parameters))) break;

    // Initialize the detector data structure.
    det = newGenDet(parameters.xml_filename, &status);
    if (EXIT_SUCCESS!=status) break;

    // Set the input event file.
    det->eventfile=openGenEventFile(parameters.eventlist_filename, 
				    READWRITE, &status);
    if (EXIT_SUCCESS!=status) break;


    // Create and open the output event file.
    // Filename of the template file.
    char template[MAXMSG];
    // Get the name of the FITS template directory.
    // Try to read it from the environment variable.
    char* buffer;
    if (NULL!=(buffer=getenv("SIXT_FITS_TEMPLATES"))) {
      strcpy(template, buffer);
    } else {
      status=EXIT_FAILURE;
      HD_ERROR_THROW("Error: Could not read environment variable 'SIXT_FITS_TEMPLATES'!\n", 
		     status);
      break;
    }
    // Append the filename of the template file itself.
    strcat(template, "/");
    strcat(template, det->patternfile_template);
    // Open a new event file from the specified template.
    output_file = openNewGenPatternFile(parameters.patternlist_filename, template, &status);
    if (EXIT_SUCCESS!=status) break;

    // Copy header keywords from the input to the output event file.
    char comment[MAXMSG]; // Buffer.

    // Total number of detected photons.
    long n_detected_photons=0; 
    if (fits_read_key(det->eventfile->fptr, TLONG, "NDETECTD", 
		      &n_detected_photons, comment, &status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NDETECTD", 
			&n_detected_photons, "number of detected photons", 
			&status)) break;

    // Number of EBOUNDS channels (DETCHANS).
    long detchans=0; 
    if (fits_read_key(det->eventfile->fptr, TLONG, "DETCHANS", 
		      &detchans, comment, &status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "DETCHANS", 
			&detchans, comment, &status)) break;

    // First EBOUNDS channel.
    long tlmin1=0; 
    if (fits_read_key(det->eventfile->fptr, TLONG, "TLMIN1", 
		      &tlmin1, comment, &status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "TLMIN1", 
			&tlmin1, comment, &status)) break;    

    // Last EBOUNDS channel.
    long tlmax1=0; 
    if (fits_read_key(det->eventfile->fptr, TLONG, "TLMAX1", 
		      &tlmax1, comment, &status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "TLMAX1", 
			&tlmax1, comment, &status)) break;    

    // Number of pixels in x-direction.
    long nxdim=0; 
    if (fits_read_key(det->eventfile->fptr, TINT, "NXDIM", 
		      &nxdim, comment, &status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TINT, "NXDIM", 
			&nxdim, comment, &status)) break;

    // Number of pixels in y-direction.
    long nydim=0; 
    if (fits_read_key(det->eventfile->fptr, TINT, "NYDIM", 
		      &nydim, comment, &status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TINT, "NYDIM", 
			&nydim, comment, &status)) break;    
    // END of copying header keywords.


    // Allocate memory for the pixel array used for the pattern identification.
    pixels=(GenEvent**)malloc(det->pixgrid->xwidth*sizeof(GenEvent*));
    if (NULL==pixels) {
      status = EXIT_FAILURE;
      HD_ERROR_THROW("Error: Memory allocation for pixel array failed!\n", status);
      break;
    }
    int ii;
    for (ii=0; ii<det->pixgrid->xwidth; ii++) {
      pixels[ii]=(GenEvent*)malloc(det->pixgrid->ywidth*sizeof(GenEvent));
      if (NULL==pixels[ii]) {
	status = EXIT_FAILURE;
	HD_ERROR_THROW("Error: Memory allocation for pixel array failed!\n", status);
	break;
      }
      // Initialize the event data structures with 0 values.
      int jj;
      for (jj=0; jj<det->pixgrid->ywidth; jj++) {
	pixels[ii][jj].time = 0.;
	pixels[ii][jj].pha  = 0;
	pixels[ii][jj].charge = 0.;
	pixels[ii][jj].rawx = 0;
	pixels[ii][jj].rawy = 0;
	pixels[ii][jj].frame  = 0;
	pixels[ii][jj].pileup = 0;
      }
    }
    if (EXIT_SUCCESS!=status) break;

    // --- END of Initialization ---


    // --- Beginning of Pattern Identification Process ---

    headas_chat(3, "start pattern identification ...\n");

    // Loop over all events in the FITS file. The last detector 
    // frame is NOT neglected.
    GenEvent event;
    long row;
    long frame=0;
    int last_loop=0;
    for (row=0; row<=det->eventfile->nrows; row++) {

      if (row<det->eventfile->nrows) {
	last_loop=0;
	// Read the next event from the file.
	getGenEventFromFile(det->eventfile, row+1, &event, &status);
	if (EXIT_SUCCESS!=status) break;
      } else {
	last_loop = 1;
      }

      // If the event belongs to a new frame, perform the
      // Pattern Identification on the pixel array before
      // starting a new frame.
      if ((event.frame > frame) || (1==last_loop)) {

	// Run the pattern identification and store the pattern 
	// information in the event file.
	GenPatIdentification(det, pixels, output_file, &patstat, &status);
	
	// Delete the old events in the pixel array.
	clearGenPatPixels(det, pixels);

	// Update the frame counter.
	frame = event.frame;
	headas_printf("\rframe: %ld ", frame);
	fflush(NULL);
      }

      if (0==last_loop) {
	// Add the event to the pixel array.
	pixels[event.rawx][event.rawy] = event;
      }
    };
    if (EXIT_SUCCESS!=status) break;
    // END of loop over all events in the FITS file.
    headas_printf("\n");

    // Store the pattern statistics in the FITS header.
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NSINGLES", 
			&patstat.ngrade1, "number of single patterns", 
			&status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NDOUBLES", 
			&patstat.ngrade2, "number of double patterns", 
			&status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NTRIPLES", 
			&patstat.ngrade3, "number of triple patterns", 
			&status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NQUADRUP", 
			&patstat.ngrade4, "number of quadruple patterns", 
			&status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NVALIDS", 
			&patstat.nvalids, "number of valid patterns", 
			&status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NINVALID", 
			&patstat.ninvalids, "number of invalid patterns", 
			&status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NPILEUP", 
			&patstat.npileup, "number of pile-up patterns", 
			&status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NPILEUPS", 
			&patstat.npileup_grade1, 
			"number of singles marked as pile-up", 
			&status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NPILEUPV", 
			&patstat.npileup_valid, 
			"number of valid patterns marked as pile-up", 
			&status)) break;
    if (fits_update_key(output_file->geneventfile->fptr, TLONG, "NPILEUPI", 
			&patstat.npileup_invalid, 
			"number of invalid patterns marked as pile-up", 
			&status)) break;

  } while(0); // END of the error handling loop.

  // --- END of pattern identification process ---


  // --- Cleaning up ---
  headas_chat(3, "cleaning up ...\n");

  // Release HEADAS random number generator.
  HDmtFree();

  // Release memory from pixel array.
  if (NULL!=pixels) {
    int ii;
    for (ii=0; ii<det->pixgrid->xwidth; ii++) {
      if (NULL!=pixels[ii]) {
	free(pixels[ii]);
      }
    }
    free(pixels);
    pixels=NULL;
  }

  // Destroy the detector data structure.
  destroyGenDet(&det, &status);
  
  // Close the output eventfile.
  destroyGenPatternFile(&output_file, &status);
  
  if (status == EXIT_SUCCESS) headas_chat(3, "finished successfully\n\n");
  return(status);
}



////////////////////////////////////////////////////////////////
// This routine reads the program parameters using the PIL.
int getpar(struct Parameters* const parameters)
{
  int status=EXIT_SUCCESS; // Error status

  // Get the name of the input event list file (FITS file).
  if ((status = PILGetFname("eventlist_filename", 
			    parameters->eventlist_filename))) {
    HD_ERROR_THROW("Error reading the name of the input event list file!\n", status);
  }

  // Get the name of the output event list file (FITS file).
  else if ((status = PILGetFname("patternlist_filename", 
				 parameters->patternlist_filename))) {
    HD_ERROR_THROW("Error reading the name of the output pattern list file!\n", status);
  }

  // Get the name of the detector XML description file (FITS file).
  else if ((status = PILGetFname("xml_filename", parameters->xml_filename))) {
    HD_ERROR_THROW("Error reading the name of the detector definition XML file!\n", status);
  }

  return(status);
}


