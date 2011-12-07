#include "simputsrc.h"


float getFlux(const float* const energy, 
	      const float* const flux,
	      const long nrows,
	      const float emin, 
	      const float emax) 
{
  // Determine the current flux in the reference band.
  // Conversion factor from [keV] -> [erg].
  const float keV2erg = 1.602e-9;
  // Flux in reference energy band.
  float isflux=0.; 
  long jj;
  for (jj=0; jj<nrows; jj++) {
    float binmin, binmax;
    if (0==jj) {
      binmin=energy[0];
    } else {
      binmin=0.5*(energy[jj]+energy[jj-1]);
    }
    if (nrows-1==jj) {
      binmax=energy[nrows-1];
    } else {
      binmax=0.5*(energy[jj]+energy[jj+1]);
    }
    if ((binmax>emin)&&(binmin<emax)) {
      float min = MAX(binmin, emin);
      float max = MIN(binmax, emax);
      assert(max>min);
      isflux += (max-min)*flux[jj]*energy[jj];
    }
  }
  // Convert units of 'flux' from [keV/s/cm^2] -> [erg/s/cm^2].
  isflux *= keV2erg;
  return(isflux);
}


int simputsrc_main() 
{
  // Program parameters.
  struct Parameters par;

  // Temporary files for ISIS interaction.
  FILE* cmdfile=NULL;
  char cmdfilename[MAXFILENAME]="";
  fitsfile* specfile=NULL;

  // ISIS parameter file containing an explicit spectral model.
  char ISISFile[MAXFILENAME]="";

  // Buffers for spectral components.
  float* flux=NULL;

  // SIMPUT data structures.
  SimputMIdpSpec* simputspec=NULL;
  SimputSource* src=NULL;
  SimputCatalog* cat=NULL;
  SimputPSD* psd = NULL;

  // Error status.
  int status=EXIT_SUCCESS;


  // Register HEATOOL
  set_toolname("simputsrc");
  set_toolversion("0.08");


  do { // Beginning of ERROR HANDLING Loop.

    // ---- Initialization ----
    
    // Read the parameters using PIL.
    status=simputsrc_getpar(&par);
    CHECK_STATUS_BREAK(status);

    // ---- END of Initialization ----


    // ---- Main Part ----

    // Check if the SIMPUT file already exists and remove the old 
    // one if necessary.
    int exists;
    fits_file_exists(par.Simput, &exists, &status);
    CHECK_STATUS_BREAK(status);
    if (0!=exists) {
      if (0!=par.clobber) {
	// Delete the file.
	remove(par.Simput);
      } else {
	// Throw an error.
	char msg[MAXMSG];
	sprintf(msg, "file '%s' already exists", par.Simput);
	SIXT_ERROR(msg);
	status=EXIT_FAILURE;
	break;
      }
    }


    // -- Create the spectrum.

    // Check if an ISIS spectral parameter file is given. In that 
    // case the spectral model therein is used instead of the 
    // individually defined components.
    strcpy(ISISFile, par.ISISFile);
    strtoupper(ISISFile);
    if ((strlen(ISISFile)>0)&&(strcmp(ISISFile, "NONE"))) {
      // Copy again the name, this time without upper-case conversion.
      strcpy(ISISFile, par.ISISFile);
    } else {
      strcpy(ISISFile, "");
    }

    // Open the ISIS command file.
    sprintf(cmdfilename, "%s.isis", par.Simput);
    cmdfile=fopen(cmdfilename,"w+");
    CHECK_NULL_BREAK(cmdfile, status, "opening temporary file failed");

    // Write the header.
    fprintf(cmdfile, "require(\"isisscripts\");\n");
    fprintf(cmdfile, "()=xspec_abund(\"wilm\");\n");
    fprintf(cmdfile, "use_localmodel(\"relline\");\n");
    
    // Define the energy grid.
    fprintf(cmdfile, "variable lo=[0.05:100.0:0.01];\n");
    fprintf(cmdfile, "variable hi=make_hi_grid(lo);\n");
    fprintf(cmdfile, "variable flux;\n");
    fprintf(cmdfile, "variable spec;\n");

    // Distinguish whether, the individual spectral components or
    // an ISIS spectral parameter file should be used.
    if (strlen(ISISFile)==0) {
    
      // Loop over the different components of the spectral model.
      int ii;
      for (ii=0; ii<4; ii++) {

	// Define the spectral model and set the parameters.
	switch(ii) {
	case 0:
	  fprintf(cmdfile, "fit_fun(\"phabs(1)*powerlaw(1)\");\n");
	  fprintf(cmdfile, "set_par(\"powerlaw(1).PhoIndex\", %e);\n", 
		  par.plPhoIndex);
	  break;
	case 1:
	  fprintf(cmdfile, "fit_fun(\"phabs(1)*bbody(1)\");\n");
	  fprintf(cmdfile, "set_par(\"bbody(1).kT\", %e);\n", par.bbkT);	
	  break;
	case 2:
	  fprintf(cmdfile, "fit_fun(\"phabs(1)*egauss(1)\");\n");
	  fprintf(cmdfile, "set_par(\"egauss(1).center\", 6.4);\n");	
	  fprintf(cmdfile, "set_par(\"egauss(1).sigma\", %e);\n", par.flSigma);	
	  break;
	case 3:
	  fprintf(cmdfile, "fit_fun(\"phabs(1)*relline(1)\");\n");
	  fprintf(cmdfile, "set_par(\"relline(1).lineE\", 6.4);\n");	
	  fprintf(cmdfile, "set_par(\"relline(1).a\", %f);\n", par.rflSpin);	
	  break;
	default:
	  status=EXIT_FAILURE;
	  break;
	}
	CHECK_STATUS_BREAK(status);

	// Absorption is the same for all spectral components.
	fprintf(cmdfile, "set_par(\"phabs(1).nH\", %e);\n", par.NH);

	// Evaluate the spectral model and store the data in a temporary 
	// FITS file.
	fprintf(cmdfile, "flux=eval_fun_keV(lo, hi)/(hi-lo);\n");
	fprintf(cmdfile, "spec=struct{ENERGY=0.5*(lo+hi), FLUX=flux};\n");
	fprintf(cmdfile, 
		"fits_write_binary_table(\"%s.spec%d\",\"SPECTRUM\", spec);\n", 
		par.Simput, ii);
      } 
      CHECK_STATUS_BREAK(status);
      // END of loop over the different spectral components.

    } else { 
      // An ISIS parameter file with an explizit spectral
      // model is given.
      fprintf(cmdfile, "load_par(\"%s\");\n", ISISFile);
      fprintf(cmdfile, "flux=eval_fun_keV(lo, hi)/(hi-lo);\n");
      fprintf(cmdfile, "spec=struct{ENERGY=0.5*(lo+hi), FLUX=flux};\n");
      fprintf(cmdfile, 
	      "fits_write_binary_table(\"%s.spec0\",\"SPECTRUM\", spec);\n", 
	      par.Simput);
    }
    // END of using an explicit spectral model given in an ISIS 
    // parameter file.

    fprintf(cmdfile, "exit;\n");

    // End of writing the ISIS file.
    fclose(cmdfile);
    cmdfile=NULL;

    // Construct the shell command to run ISIS.
    char command[MAXMSG];
    strcpy(command, "/data/system/software/local/bin/isis ");
    strcat(command, cmdfilename);
      
    // Run ISIS.
    status=system(command);
    CHECK_STATUS_BREAK(status);

    // Add the spectra and insert the total spectrum in the SIMPUT file.
    simputspec=getSimputMIdpSpec(&status);
    CHECK_STATUS_BREAK(status);

    // Loop over the different components of the spectral model.
    long nrows=0;
    int ii;
    for (ii=0; ii<4; ii++) {

      // Read the spectrum.
      char filename[MAXFILENAME];
      sprintf(filename, "%s.spec%d", par.Simput, ii);
      fits_open_table(&specfile, filename, READONLY, &status);
      CHECK_STATUS_BREAK(status);

      // Load the data from the file.
      int anynull;
      if (0==ii) {
	// Determine the number of rows.
	fits_get_num_rows(specfile, &nrows, &status);
	CHECK_STATUS_BREAK(status);

	// Allocate memory.
	simputspec->nentries=nrows;
	simputspec->energy=(float*)malloc(nrows*sizeof(float));
	CHECK_NULL_BREAK(simputspec->energy, status, "memory allocation failed");
	simputspec->pflux=(float*)malloc(nrows*sizeof(float));
	CHECK_NULL_BREAK(simputspec->energy, status, "memory allocation failed");
	flux=(float*)malloc(nrows*sizeof(float));
	CHECK_NULL_BREAK(flux, status, "memory allocation failed");

	// Read the energy column.
	fits_read_col(specfile, TFLOAT, 1, 1, 1, nrows, 0, simputspec->energy, 
		      &anynull, &status);
	CHECK_STATUS_BREAK(status);

      } else {
	// Check whether the number of entries is
	// consistent with the previous files.
	fits_get_num_rows(specfile, &nrows, &status);
	CHECK_STATUS_BREAK(status);
	
	if (nrows!=simputspec->nentries) {
	  SIXT_ERROR("inconsistent sizes of spectra");
	  status=EXIT_FAILURE;
	  break;
	}
      }

      // Read the flux column.
      fits_read_col(specfile, TFLOAT, 2, 1, 1, nrows, 0, flux, 
		    &anynull, &status);
      CHECK_STATUS_BREAK(status);

      fits_close_file(specfile, &status);
      specfile=NULL;
      CHECK_STATUS_BREAK(status);

      // If the spectrum is given via individual components, they
      // have to be normalized according to their respective fluxes.
      if (strlen(ISISFile)==0) {
	// Determine the required flux in the reference band.
	float shouldflux=0.;
	switch(ii) {
	case 0:
	  shouldflux = par.plFlux;
	  break;
	case 1:
	  shouldflux = par.bbFlux;
	  break;
	case 2:
	  shouldflux = par.flFlux;
	  break;
	case 3:
	  shouldflux = par.rflFlux;
	  break;
	default:
	  status = EXIT_FAILURE;
	  break;
	}
	CHECK_STATUS_BREAK(status);

	float factor;
	if (shouldflux==0.) {
	  factor=0.;
	} else {
	  // Determine the factor between the current flux in the reference 
	  // band and the desired flux.
	  factor = shouldflux/
	    getFlux(simputspec->energy, flux, nrows, par.Emin, par.Emax);
	}

	// Add the normalized component to the total spectrum.
	if (factor>0.) {
	  long jj;
	  for (jj=0; jj<nrows; jj++) {
	    simputspec->pflux[jj]+=flux[jj]*factor;
	  }
	}

      } else {
	// The spectral model is given in an ISIS parameter file.
	// Therefore we do not have to normalize it, but can directly
	// add it to the SIMPUT spectrum.
	long jj;
	for (jj=0; jj<nrows; jj++) {
	  simputspec->pflux[jj]=flux[jj];
	}
	
	// Since there are no further components, we can skip
	// the further processing of the loop.
	break;
      }
    }
    CHECK_STATUS_BREAK(status);
    // END of loop over the different spectral components.

    long jj;
    for (jj=0; jj<simputspec->nentries; jj++) {
      // Check if the flux has a physically reasonable value.
      if ((simputspec->pflux[jj]<0.)||(simputspec->pflux[jj]>1.e12)) {
	SIXT_ERROR("flux out of boundaries");
	status=EXIT_FAILURE;
      }
    }
    saveSimputMIdpSpec(simputspec, par.Simput, "SPECTRUM", 1, &status);
    CHECK_STATUS_BREAK(status);
    // -- END of creating the spectrum.

    // -- Create PSD

    if(par.LFQ != 0) {
      psd = getSimputPSD(&status);
      CHECK_STATUS_BREAK(status);

      saveSimputPSD(psd, par.Simput, "LIGHTCUR", 1, &status);
      CHECK_STATUS_BREAK(status);
    }

    // -- END of creating PSD

    // -- Create a new SIMPUT catalog.
    cat=openSimputCatalog(par.Simput, READWRITE, &status);
    CHECK_STATUS_BREAK(status);

    // Insert a point-like source.
    float totalFlux = 	    
      getFlux(simputspec->energy, simputspec->pflux, nrows, par.Emin, par.Emax);
    char src_name[MAXMSG];
    strcpy(src_name, par.Src_Name);
    strtoupper(src_name);
    if ((strlen(src_name)>0)&&(strcmp(src_name, "NONE"))) {
      // Copy again the name, this time without upper-case conversion.
      strcpy(src_name, par.Src_Name);
    } else {
      strcpy(src_name, "");
    }
    
    // Get a new source entry.
    src=getSimputSourceV(1, src_name, par.RA, par.Dec, 0., 1., 
			 par.Emin, par.Emax, totalFlux, 
			 "[SPECTRUM,1]", "", "", &status);
    CHECK_STATUS_BREAK(status);
    appendSimputSource(cat, src, &status);
    CHECK_STATUS_BREAK(status);

    // -- END of creating the source catalog.

    // ---- END of Main Part ----

  } while(0); // END of error handling loop.

  // Close the temporary files.
  if (NULL!=cmdfile) {
    fclose(cmdfile);
    cmdfile=NULL;
  }
  if (NULL!=specfile) {
    fits_close_file(specfile, &status);
    specfile=NULL;
  }
  // Remove the temporary files.
  if (strlen(cmdfilename)>0) {
    remove(cmdfilename);
  }
  int ii;
  for (ii=0; ii<4; ii++) {
    char filename[MAXFILENAME];
    sprintf(filename, "%s.spec%d", par.Simput, ii);
    remove(filename);
    
    // Check whether there is only one file.
    if (strlen(ISISFile)>0) break;
  }

  // Release memory.
  freeSimputMIdpSpec(&simputspec);
  freeSimputPSD(&psd);
  freeSimputSource(&src);
  freeSimputCatalog(&cat, &status);

  if (NULL!=flux) {
    free(flux);
    flux=NULL;
  }

  if (EXIT_SUCCESS==status) headas_chat(3, "finished successfully!\n\n");
  return(status);
}


int simputsrc_getpar(struct Parameters* const par)
{
  // String input buffer.
  char* sbuffer=NULL;

  // Error status.
  int status = EXIT_SUCCESS; 

  // Read all parameters via the ape_trad_ routines.

  status=ape_trad_query_float("plPhoIndex", &par->plPhoIndex);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the plPhoIndex parameter failed");
    return(status);
  }

  status=ape_trad_query_float("plFlux", &par->plFlux);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the plFlux parameter failed");
    return(status);
  }

  status=ape_trad_query_float("bbkT", &par->bbkT);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the bbkT parameter failed");
    return(status);
  }

  status=ape_trad_query_float("bbFlux", &par->bbFlux);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the bbFlux parameter failed");
    return(status);
  }

  status=ape_trad_query_float("flSigma", &par->flSigma);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the flSigma parameter failed");
    return(status);
  }

  status=ape_trad_query_float("flFlux", &par->flFlux);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the flFlux parameter failed");
    return(status);
  }

  status=ape_trad_query_float("rflSpin", &par->rflSpin);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the rflSpin parameter failed");
    return(status);
  }

  status=ape_trad_query_float("rflFlux", &par->rflFlux);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the rflFlux parameter failed");
    return(status);
  }

  status=ape_trad_query_float("NH", &par->NH);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the N_H parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Emin", &par->Emin);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Emin parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Emax", &par->Emax);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Emax parameter failed");
    return(status);
  }

  status=ape_trad_query_float("LFQ", &par->LFQ);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the LFQ parameter failed");
    return(status);
  }

  status=ape_trad_query_float("LFrms", &par->LFrms);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the LFrms parameter failed");
    return(status);
  }

  status=ape_trad_query_float("HBOf", &par->HBOf);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the HBOf parameter failed");
    return(status);
  }

  status=ape_trad_query_float("HBOQ", &par->HBOQ);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the HBOQ parameter failed");
    return(status);
  }

  status=ape_trad_query_float("HBOrms", &par->HBOrms);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the HBOrms parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Q1f", &par->Q1f);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Q1f parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Q1Q", &par->Q1Q);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Q1Q parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Q1rms", &par->Q1rms);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Q1rms parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Q2f", &par->Q2f);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Q2f parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Q2Q", &par->Q2Q);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Q2Q parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Q2rms", &par->Q2rms);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Q2rms parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Q3f", &par->Q3f);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Q3f parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Q3Q", &par->Q3Q);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Q3Q parameter failed");
    return(status);
  }

  status=ape_trad_query_float("Q3rms", &par->Q3rms);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the Q3rms parameter failed");
    return(status);
  }

  status=ape_trad_query_float("RA", &par->RA);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the right ascension failed");
    return(status);
  }

  status=ape_trad_query_float("Dec", &par->Dec);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the declination failed");
    return(status);
  }

  status=ape_trad_query_string("Src_Name", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the source name failed");
    return(status);
  }
  strcpy(par->Src_Name, sbuffer);
  free(sbuffer);

  status=ape_trad_query_string("ISISFile", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the name of the ISIS spectral parameter file failed");
    return(status);
  }
  strcpy(par->ISISFile, sbuffer);
  free(sbuffer);

  status=ape_trad_query_file_name("Simput", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("reading the name of the output SIMPUT catalog file failed");
    return(status);
  } 
  strcpy(par->Simput, sbuffer);
  free(sbuffer);

  status=ape_trad_query_bool("clobber", &par->clobber);
  if (EXIT_SUCCESS!=status) {
    HD_ERROR_THROW("Error reading the clobber parameter!\n", status);
    return(status);
  }

  return(status);
}


