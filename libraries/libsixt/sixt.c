#include "sixt.h"


double sixt_get_random_number()
{
  // Return a value out of the interval [0,1):
  return(HDmtDrand());
}


double rndexp(const double avgdist)
{
  assert(avgdist>0.);

  double rand = sixt_get_random_number();
  if (rand < 1.E-15) {
    rand = 1.E-15;
  }

  return(-log(rand)*avgdist);
}


void get_gauss_random_numbers(double* const x, double* const y)
{
  double sqrt_2rho = sqrt(-log(sixt_get_random_number())*2.);
  double phi = sixt_get_random_number()*2.*M_PI;

  *x = sqrt_2rho * cos(phi);
  *y = sqrt_2rho * sin(phi);
}


void strtoupper(char* const string) 
{
  int count=0;
  while (string[count] != '\0') {
    string[count] = toupper(string[count]);
    count++;
  };
}


void sixt_error(const char* const func, const char* const msg)
{
  // Use the HEADAS error output routine.
  char output[MAXMSG];
  sprintf(output, "Error in %s: %s!\n", func, msg);
  HD_ERROR_THROW(output, EXIT_FAILURE);
}


void sixt_warning(const char* const msg)
{
  // Print the formatted output message.
  headas_chat(1, "### Warning: %s!\n", msg);
}


void sixt_get_XMLFile(char* const filename,
		      const char* const xmlfile,
		      const char* const mission,
		      const char* const instrument,
		      const char* const mode,
		      int* const status)
{
  // Convert the user input to capital letters.
  char Mission[MAXMSG];
  char Instrument[MAXMSG];
  char Mode[MAXMSG];
  strcpy(Mission, mission);
  strcpy(Instrument, instrument);
  strcpy(Mode, mode);
  strtoupper(Mission);
  strtoupper(Instrument);
  strtoupper(Mode);

  // Check the available missions, instruments, and modes.
  char XMLFile[MAXFILENAME];
  strcpy(XMLFile, xmlfile);
  strtoupper(XMLFile);
  if (0==strcmp(XMLFile, "NONE")) {
    // Determine the base directory containing the XML
    // definition files.
    strcpy(filename, SIXT_DATA_PATH);
    strcat(filename, "/instruments");

    // Determine the XML filename according to the selected
    // mission, instrument, and mode.
    if (0==strcmp(Mission, "SRG")) {
      strcat(filename, "/srg");
      if (0==strcmp(Instrument, "EROSITA")) {
	strcat(filename, "/erosita.xml");
      } else {
	*status=EXIT_FAILURE;
	SIXT_ERROR("selected instrument is not supported");
	return;
      }

    } else if (0==strcmp(Mission, "IXO")) {
      strcat(filename, "/ixo");
      if (0==strcmp(Instrument, "WFI")) {
	strcat(filename, "/wfi");
	if (0==strcmp(Mode, "FULLFRAME")) {
	  strcat(filename, "/fullframe.xml");
	} else {
	  *status=EXIT_FAILURE;
	  SIXT_ERROR("selected mode is not supported");
	  return;
	}
      } else {
	*status=EXIT_FAILURE;
	SIXT_ERROR("selected instrument is not supported");
	return;
      }

    } else if (0==strcmp(Mission, "ATHENA")) {
      strcat(filename, "/athena");
      if (0==strcmp(Instrument, "WFI")) {
	strcat(filename, "/wfi");
	if (0==strcmp(Mode, "FULLFRAME")) {
	  strcat(filename, "/fullframe.xml");
	} else {
	  *status=EXIT_FAILURE;
	  SIXT_ERROR("selected mode is not supported");
	  return;
	}
      } else {
	*status=EXIT_FAILURE;
	SIXT_ERROR("selected instrument is not supported");
	return;
      }

    } else if (0==strcmp(Mission, "GRAVITAS")) {
      strcat(filename, "/gravitas");
      if (0==strcmp(Instrument, "HIFI")) {
	strcat(filename, "/hifi.xml");
      } else {
	*status=EXIT_FAILURE;
	SIXT_ERROR("selected instrument is not supported");
	return;
      }
      
    } else {
      *status=EXIT_FAILURE;
      SIXT_ERROR("selected mission is not supported");
      return;
    }
    
  } else {
    // The XML filename has been given explicitly.
    strcpy(filename, xmlfile);
  }
}


void sixt_get_LADXMLFile(char* const filename,
			 const char* const xmlfile)
{
  // Check the available missions, instruments, and modes.
  char XMLFile[MAXFILENAME];
  strcpy(XMLFile, xmlfile);
  strtoupper(XMLFile);
  if (0==strcmp(XMLFile, "NONE")) {
    // Set default LAD XML file.
    strcpy(filename, SIXT_DATA_PATH);
    strcat(filename, "/instruments/loft/lad.xml");
    
  } else {
    // The XML filename has been given explicitly.
    strcpy(filename, xmlfile);
  }
}


void writeMissionKeys(fitsfile* const fptr, 
		      const char* const instrument, 
		      int* const status)
{
  // Distinguish between different missions.
  char buffer[MAXMSG];
  strcpy(buffer, instrument);
  strtoupper(buffer);
  if (0==strcmp(buffer, "EROSITA")) {

    fits_update_key(fptr, TSTRING, "MISSION", "SRG",
		    "mission name", status);
    fits_update_key(fptr, TSTRING, "TELESCOP", "eROSITA",
		    "telescope name", status);
    fits_update_key(fptr, TSTRING, "INSTRUME", "FM4",
		    "instrument name", status);

    fits_update_key(fptr, TSTRING, "FILTER", "OPEN",
		    "filter", status);

    float frametime=50.0;
    fits_update_key(fptr, TFLOAT, "FRAMETIM", &frametime,
		    "[ms] nominal frame time", status);
    float timezero=0.0;
    fits_update_key(fptr, TFLOAT, "TIMEZERO", &timezero,
		    "clock correction", status);
    fits_update_key(fptr, TSTRING, "TIMEUNIT", "s",
		    "Time unit", status);
    fits_update_key(fptr, TSTRING, "TIMESYS", "TT",
		    "Time system (Terrestial Time)", status);

    fits_update_key(fptr, TSTRING, "RADECSYS", "FK5", "", status);
    float equinox=2000.0;
    fits_update_key(fptr, TFLOAT, "EQUINOX", &equinox, "", status);
    fits_update_key(fptr, TSTRING, "LONGSTR", "OGIP 1.0",
		    "to support multi-line COMMENT oder HISTORY records",
		    status);

    int nxdim=384, nydim=384;
    fits_update_key(fptr, TINT, "NXDIM", &nxdim, "", status);
    fits_update_key(fptr, TINT, "NYDIM", &nydim, "", status);
    
    float pixlen=75.0;
    fits_update_key(fptr, TFLOAT, "PIXLEN_X", &pixlen, "[micron]", status);
    fits_update_key(fptr, TFLOAT, "PIXLEN_Y", &pixlen, "[micron]", status);

    // Determine the number of the PHA column and set the 
    // TLMIN and TLMAX keywords.
    int colnum;
    fits_get_colnum(fptr, 0, "PHA", &colnum, status);
    CHECK_STATUS_VOID(*status);
    char keyword[MAXMSG];
    int tlmin=0, tlmax=4095;
    sprintf(keyword, "TLMIN%d", colnum);
    fits_update_key(fptr, TINT, keyword, &tlmin, "", status);
    sprintf(keyword, "TLMAX%d", colnum);
    fits_update_key(fptr, TINT, keyword, &tlmax, "", status);

    // END of eROSITA
  }

}

