#include "simputmerge.h"


int simputmerge_main() 
{
  // Program parameters.
  struct Parameters par;

  // Filenames of the input catalogs.
  char infilenames[2][MAXFILENAME];

  // SIMPUT source catalogs.
  SimputSourceCatalog* incat[2]={NULL, NULL};
  SimputSourceCatalog* outcat  = NULL;

  // Simput data structures (used as buffers).
  SimputMissionIndepSpec* spec=NULL;
  SimputImg*              img =NULL;
  SimputLC*               lc  =NULL;

  // HDU extension references used in the catalog.
  char** specextrefs[2] = {NULL, NULL};
  long  nspecextrefs[2] = {   0,    0};
  char** imgextrefs[2] = {NULL, NULL};
  long  nimgextrefs[2] = {   0,    0};
  char** lcextrefs[2] = {NULL, NULL};
  long  nlcextrefs[2] = {   0,    0};

  // Error status.
  int status=EXIT_SUCCESS; 


  // Register HEATOOL
  set_toolname("simputmerge");
  set_toolversion("0.01");


  do { // Beginning of ERROR HANDLING Loop.

    // ---- Initialization ----
    
    // Read the parameters using PIL.
    status=simputmerge_getpar(&par);
    CHECK_STATUS_BREAK(status);

    // Load the input catalogs.
    strcpy(infilenames[0], par.Infile1);
    strcpy(infilenames[1], par.Infile2);
    int ii;
    for (ii=0; ii<2; ii++) {
      incat[ii] = loadSimputSourceCatalog(infilenames[ii], &status);
      CHECK_STATUS_BREAK(status);
    }
    CHECK_STATUS_BREAK(status);

    // Get an empty object for the output catalog.
    outcat = getSimputSourceCatalog(&status);
    CHECK_STATUS_BREAK(status);

    // Allocate sufficient memory to store the entries of both
    // input catalogs in the output catalog.
    outcat->entries = 
      (SimputSourceEntry**)
      malloc((incat[0]->nentries+incat[1]->nentries)*sizeof(SimputSourceEntry*));
    CHECK_NULL_BREAK(outcat->entries, status, "memory allocation failed!\n");

    // Smallest still available SRC_ID.
    long min_src_id=1; 
    // Loop over both source catalogs.
    for (ii=0; ii<2; ii++) {
      // Loop over all entries in the source catalog.
      long jj;
      for (jj=0; jj<incat[ii]->nentries; jj++) {

	// Check if the SRC_ID of the new source is already contained 
	// in the output catalog. The SRC_ID entry must be unique.
	long src_id = incat[ii]->entries[jj]->src_id;
	if (src_id<min_src_id) {
	  src_id=min_src_id;
	}
	long kk;
	do {
	  for (kk=0; kk<outcat->nentries; kk++) {
	    if (src_id==outcat->entries[kk]->src_id) {
	      // This SRC_ID is not available any more.
	      if (src_id==min_src_id) {
		min_src_id++;
	      }
	      src_id++;
	      break;
	    }
	  } 
	} while (kk<outcat->nentries);
	if (src_id==min_src_id) {
	  min_src_id++;
	}

	// Handle spectrum, image, and lightcur extensions.
	char spectrum[MAXFILENAME];
	char lightcur[MAXFILENAME];
	char image[MAXFILENAME];

	// Check whether the extensions should remain in their current
	// place or if they should by copied to the new output file.
	if (0==par.FetchExtensions) {
	  // Extensions should remain in their original place.
	  // Check if they are local references to the same file
	  // containing the catalog.
	  // Spectrum.
	  if ('['==incat[ii]->entries[jj]->spectrum[0]) {
	    strcpy(spectrum, infilenames[ii]);
	    strcat(spectrum, incat[ii]->entries[jj]->spectrum);
	  } else {
	    strcpy(spectrum, incat[ii]->entries[jj]->spectrum);
	  }
	  // Image.
	  if ('['==incat[ii]->entries[jj]->image[0]) {
	    strcpy(image, infilenames[ii]);
	    strcat(image, incat[ii]->entries[jj]->image);
	  } else {
	    strcpy(image, incat[ii]->entries[jj]->image);
	  }
	  // Light curve.
	  if ('['==incat[ii]->entries[jj]->lightcur[0]) {
	    strcpy(lightcur, infilenames[ii]);
	    strcat(lightcur, incat[ii]->entries[jj]->lightcur);
	  } else {
	    strcpy(lightcur, incat[ii]->entries[jj]->lightcur);
	  }
	  // TODO What happens if the input file resides in another 
	  // directory than the output file.
	} else {
	  // Extensions should be copied to the new output file.

	  // Spectrum extensions.
	  if ((strlen(incat[ii]->entries[jj]->spectrum)>0) &&
	      (0!=strcmp(incat[ii]->entries[jj]->spectrum, "NULL"))) {
	    // Check if this reference has already been used.
	    long kk;
	    for (kk=0; kk<nspecextrefs[ii]; kk++) {
	      if (0==strcmp(specextrefs[ii][kk], incat[ii]->entries[jj]->spectrum)) {
		break;
	      }
	    }
	    if (kk==nspecextrefs[ii]) {
	      // If not, append it to the list of used references.
	      specextrefs[ii] = 
		(char**)realloc(specextrefs[ii],
				(nspecextrefs[ii]+1)*sizeof(char*));
	      CHECK_NULL_BREAK(specextrefs[ii], status, "memory allocation failed");
	      specextrefs[ii][kk] = 
		(char*)malloc((strlen(incat[ii]->entries[jj]->spectrum)+1)*sizeof(char));
	      CHECK_NULL_BREAK(specextrefs[ii][kk], status, "memory allocation failed");
	      nspecextrefs[ii]++;
	      strcpy(specextrefs[ii][kk], incat[ii]->entries[jj]->spectrum);
	    }
	    // Remove the preceeding file path and name.
	    strcpy(spectrum, strchr(incat[ii]->entries[jj]->spectrum, '['));
	  } else {
	    strcpy(spectrum, "");
	  }
	  
	  // Image extensions.
	  if ((strlen(incat[ii]->entries[jj]->image)>0) &&
	      (0!=strcmp(incat[ii]->entries[jj]->image, "NULL"))) {
	    // Check if this reference has already been used.
	    for (kk=0; kk<nimgextrefs[ii]; kk++) {
	      if (0==strcmp(imgextrefs[ii][kk], incat[ii]->entries[jj]->image)) {
		break;
	      }
	    }
	    if (kk==nimgextrefs[ii]) {
	      // If not, append it to the list of used references.
	      imgextrefs[ii] = 
		(char**)realloc(imgextrefs[ii],
				(nimgextrefs[ii]+1)*sizeof(char*));
	      CHECK_NULL_BREAK(imgextrefs[ii], status, "memory allocation failed");
	      imgextrefs[ii][kk] = 
	      (char*)malloc((strlen(incat[ii]->entries[jj]->image)+1)*sizeof(char));
	      CHECK_NULL_BREAK(imgextrefs[ii][kk], status, "memory allocation failed");
	      nimgextrefs[ii]++;
	      strcpy(imgextrefs[ii][kk], incat[ii]->entries[jj]->image);
	    }
	    // Remove the preceeding file path and name.
	    strcpy(image, strchr(incat[ii]->entries[jj]->image, '['));
	  } else {
	    strcpy(image, "");
	  }

	  // Light curve extensions.
	  if ((strlen(incat[ii]->entries[jj]->lightcur)>0) &&
	      (0!=strcmp(incat[ii]->entries[jj]->lightcur, "NULL"))) {
	    // Check if this reference has already been used.
	    for (kk=0; kk<nlcextrefs[ii]; kk++) {
	      if (0==strcmp(lcextrefs[ii][kk], incat[ii]->entries[jj]->lightcur)) {
		break;
	      }
	    }
	    if (kk==nlcextrefs[ii]) {
	      // If not, append it to the list of used references.
	      lcextrefs[ii] = 
		(char**)realloc(lcextrefs[ii],
				(nlcextrefs[ii]+1)*sizeof(char*));
	      CHECK_NULL_BREAK(lcextrefs[ii], status, "memory allocation failed");
	      lcextrefs[ii][kk] = 
		(char*)malloc((strlen(incat[ii]->entries[jj]->lightcur)+1)*sizeof(char));
	      CHECK_NULL_BREAK(lcextrefs[ii][kk], status, "memory allocation failed");
	      nlcextrefs[ii]++;
	      strcpy(lcextrefs[ii][kk], incat[ii]->entries[jj]->lightcur);
	    }
	    // Remove the preceeding file path and name.
	    strcpy(lightcur, strchr(incat[ii]->entries[jj]->lightcur, '['));  
	  } else {
	    strcpy(lightcur, "");
	  }
	}
	// END of extensions should be copied to the new output file.

	// Copy the entry from the input to the output catalog.
	outcat->nentries++;
	outcat->entries[outcat->nentries-1] = 
	  getSimputSourceEntryV(src_id, 
				incat[ii]->entries[jj]->src_name,
				incat[ii]->entries[jj]->ra,
				incat[ii]->entries[jj]->dec,
				incat[ii]->entries[jj]->imgrota,
				incat[ii]->entries[jj]->imgscal,
				incat[ii]->entries[jj]->e_min,
				incat[ii]->entries[jj]->e_max,
				incat[ii]->entries[jj]->flux,
				spectrum, image, lightcur,
				&status);
	CHECK_STATUS_BREAK(status);

	// Output of progress.
	if (0==outcat->nentries % 100) {
	  headas_chat(1, "\r%ld/%ld (%.1lf%%) entries", 
		      outcat->nentries, incat[0]->nentries+incat[1]->nentries,
		      outcat->nentries*100./(incat[0]->nentries+incat[1]->nentries));
	  fflush(NULL);
	}
      }
      CHECK_STATUS_BREAK(status);
      // END of loop over all entries in the source catalog.
    }
    CHECK_STATUS_BREAK(status);
    headas_chat(1, "\n");
    // END of loop over both source catalogs.

    // Store the output catalog in the FITS file.
    saveSimputSourceCatalog(outcat, par.Outfile, &status);
    CHECK_STATUS_BREAK(status);

    // Copy the used extensions to the new output file.
    if (0!=par.FetchExtensions) {

      // Spectra.
      // Check for duplicates.
      for (ii=0; ii<2; ii++) {
	long jj;
	for (jj=0; jj<nspecextrefs[ii]; jj++) {
	  if (strlen(specextrefs[ii][jj])>0) {
	    int kk;
	    for (kk=0; kk<ii; kk++) {
	      long ll;
	      for (ll=0; ll<nspecextrefs[kk]; ll++) {
		if (0==strcmp(specextrefs[ii][jj], specextrefs[kk][ll])) {
		  headas_printf("Warning: reference to spectrum '%s' might "
				"not be unique!\n (used in '%s' and '%s')\n",
				specextrefs[ii][jj],
				infilenames[ii], infilenames[kk]);
		  strcpy(specextrefs[ii][jj], "");
		}
	      }
	    }
	  }
	}
      }

      // Images.
      // Check for duplicates.
      for (ii=0; ii<2; ii++) {
	long jj;
	for (jj=0; jj<nimgextrefs[ii]; jj++) {
	  if (strlen(imgextrefs[ii][jj])>0) {
	    int kk;
	    for (kk=0; kk<ii; kk++) {
	      long ll;
	      for (ll=0; ll<nimgextrefs[kk]; ll++) {
		if (0==strcmp(imgextrefs[ii][jj], imgextrefs[kk][ll])) {
		  headas_printf("Warning: reference to image '%s' might "
				"not be unique!\n (used in '%s' and '%s')\n",
				imgextrefs[ii][jj],
				infilenames[ii], infilenames[kk]);
		  strcpy(imgextrefs[ii][jj], "");
		}
	      }
	    }
	  }
	}
      }

      // Light curves.
      // Check for duplicates.
      for (ii=0; ii<2; ii++) {
	long jj;
	for (jj=0; jj<nlcextrefs[ii]; jj++) {
	  if (strlen(lcextrefs[ii][jj])>0) {
	    int kk;
	    for (kk=0; kk<ii; kk++) {
	      long ll;
	      for (ll=0; ll<nlcextrefs[kk]; ll++) {
		if (0==strcmp(lcextrefs[ii][jj], lcextrefs[kk][ll])) {
		  headas_printf("Warning: reference to light curve '%s' might "
				"not be unique!\n (used in '%s' and '%s')\n",
				lcextrefs[ii][jj],
				infilenames[ii], infilenames[kk]);
		  strcpy(lcextrefs[ii][jj], "");
		}
	      }
	    }
	  }
	}
      }

      // Go through the lists and, load the HDU data, and store it
      // in the new output file.
      // Spectra.
      for (ii=0; ii<2; ii++) {
	long jj;
	for (jj=0; jj<nspecextrefs[ii]; jj++) {
	  if (strlen(specextrefs[ii][jj])>0) {
	    char filename[MAXFILENAME];
	    if ('['==specextrefs[ii][jj][0]) {
	      strcpy(filename, infilenames[ii]);
	      strcat(filename, specextrefs[ii][jj]);
	    } else {
	      strcpy(filename, specextrefs[ii][jj]);
	    }

	    // Load the spectrum.
	    spec=loadSimputMissionIndepSpec(filename, &status);
	    CHECK_STATUS_BREAK(status);
	    
	    // Determine the EXTNAME and EXTVER.
	    char extname[MAXFILENAME];
	    int extver;
	    fitsfile* fptr=NULL;
	    fits_open_file(&fptr, filename, READONLY, &status);
	    fits_read_key(fptr, TSTRING, "EXTNAME", extname, NULL, &status);
	    fits_read_key(fptr, TINT, "EXTVER", &extver, NULL, &status);
	    fits_close_file(fptr, &status);
	    CHECK_STATUS_BREAK(status);

	    // Store it in the output file.
	    saveSimputMissionIndepSpec(spec, par.Outfile, extname, extver, &status);
	    CHECK_STATUS_BREAK(status);
	  }
	}
	CHECK_STATUS_BREAK(status);
      }
      CHECK_STATUS_BREAK(status);
    }

    // TODO Images and LCs.

    // END of copy extensions to the new output file.

  } while(0); // END of ERROR HANDLING Loop.

  // --- Clean up ---
  headas_chat(3, "\ncleaning up ...\n");

  // Release memory.
  freeSimputSourceCatalog(&incat[0]);
  freeSimputSourceCatalog(&incat[1]);
  freeSimputSourceCatalog(&outcat);

  freeSimputMissionIndepSpec(&spec);
  freeSimputImg(&img);
  freeSimputLC(&lc);

  int ii;
  for (ii=0; ii<2; ii++) {
    if (NULL!=specextrefs[ii]) {
      long jj;
      for (jj=0; jj<nspecextrefs[ii]; jj++) {
	if (NULL!=specextrefs[ii][jj]) {
	  free(specextrefs[ii][jj]);
	}
      }
      free(specextrefs[ii]);
    }
  }
  for (ii=0; ii<2; ii++) {
    if (NULL!=imgextrefs[ii]) {
      long jj;
      for (jj=0; jj<nimgextrefs[ii]; jj++) {
	if (NULL!=imgextrefs[ii][jj]) {
	  free(imgextrefs[ii][jj]);
	}
      }
      free(imgextrefs[ii]);
    }
  }
  for (ii=0; ii<2; ii++) {
    if (NULL!=lcextrefs[ii]) {
      long jj;
      for (jj=0; jj<nlcextrefs[ii]; jj++) {
	if (NULL!=lcextrefs[ii][jj]) {
	  free(lcextrefs[ii][jj]);
	}
      }
      free(lcextrefs[ii]);
    }
  }

  if (status==EXIT_SUCCESS) headas_chat(0, "finished successfully!\n\n");
  return(status);
}



int simputmerge_getpar(struct Parameters* const par)
{
  // String input buffer.
  char* sbuffer=NULL;

  // Error status.
  int status = EXIT_SUCCESS; 

  // Read all parameters via the ape_trad_ routines.

  status=ape_trad_query_file_name("Infile1", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    HD_ERROR_THROW("Error reading the name of the input file 1!\n", status);
    return(status);
  } 
  strcpy(par->Infile1, sbuffer);
  free(sbuffer);

  status=ape_trad_query_file_name("Infile2", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    HD_ERROR_THROW("Error reading the name of the input file 2!\n", status);
    return(status);
  } 
  strcpy(par->Infile2, sbuffer);
  free(sbuffer);

  status=ape_trad_query_file_name("Outfile", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    HD_ERROR_THROW("Error reading the name of the output file!\n", status);
    return(status);
  } 
  strcpy(par->Outfile, sbuffer);
  free(sbuffer);

  status=ape_trad_query_bool("FetchExtensions", &par->FetchExtensions);
  if (EXIT_SUCCESS!=status) {
    HD_ERROR_THROW("Error reading the FetchExtensions parameter!\n", status);
    return(status);
  }

  status=ape_trad_query_bool("clobber", &par->clobber);
  if (EXIT_SUCCESS!=status) {
    HD_ERROR_THROW("Error reading the clobber parameter!\n", status);
    return(status);
  }

  return(status);
}

