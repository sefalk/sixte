#include "impactlistfile.h"


ImpactListFile* newImpactListFile(int* const status)
{
  ImpactListFile* file = (ImpactListFile*)malloc(sizeof(ImpactListFile));
  if (NULL==file) {
    *status = EXIT_FAILURE;
    HD_ERROR_THROW("Error: Memory allocation for ImpactListFile failed!\n", 
		   *status);
    return(file);
  }

  // Initialize pointers with NULL.
  file->fptr=NULL;

  // Initialize values.
  file->nrows=0;
  file->row  =0;
  file->ctime=0;
  file->cenergy=0;
  file->cx   =0;
  file->cy   =0;

  return(file);
}



void destroyImpactListFile(ImpactListFile** file, int* const status)
{
  if (NULL!=*file) {
    if (NULL!=(*file)->fptr) {
      fits_close_file((*file)->fptr, status);
      (*file)->fptr=NULL;
    }
    free(*file);
    *file=NULL;
  }
}



ImpactListFile* openImpactListFile(const char* const filename,
				   const int mode, int* const status)
{
  ImpactListFile* file = newImpactListFile(status);
  if (EXIT_SUCCESS!=*status) return(file);

  headas_chat(4, "open impact list file '%s' ...\n", filename);

  // Open the FITS file table for reading:
  if (fits_open_table(&file->fptr, filename, mode, status)) return(file);;

  // Get the HDU type.
  int hdutype;
  if (fits_get_hdu_type(file->fptr, &hdutype, status)) return(file);;
  // Image HDU results in an error message.
  if (IMAGE_HDU==hdutype) {
    *status=EXIT_FAILURE;
    char msg[MAXMSG];
    sprintf(msg, "Error: no table extension available in FITS file '%s'!\n", 
	    filename);
    HD_ERROR_THROW(msg, *status);
    return(file);
  }

  // Determine the number of rows in the impact list.
  if (fits_get_num_rows(file->fptr, &file->nrows, status)) return(file);
  // Set internal row counter to the beginning of the file (starting at 0).
  file->row = 0;

  // Determine the individual column numbers:
  // REQUIRED columns:
  if(fits_get_colnum(file->fptr, CASEINSEN, "TIME", &file->ctime, status)) 
    return(file);
  if(fits_get_colnum(file->fptr, CASEINSEN, "ENERGY", &file->cenergy, status)) 
    return(file);
  if(fits_get_colnum(file->fptr, CASEINSEN, "X", &file->cx, status)) 
    return(file);
  if(fits_get_colnum(file->fptr, CASEINSEN, "Y", &file->cy, status)) 
    return(file);

  return(file);
}



ImpactListFile* openNewImpactListFile(const char* const filename,
				      const char* const template,
				      int* const status)
{
  ImpactListFile* file = newImpactListFile(status);
  if (EXIT_SUCCESS!=*status) return(file);

  // Remove old file if it exists.
  remove(filename);

  // Create a new event list FITS file from the template file.
  char buffer[MAXMSG];
  sprintf(buffer, "%s(%s)", filename, template);
  headas_chat(4, "create new impact list file '%s' from template '%s' ...\n", 
	      filename, template);
  if (fits_create_file(&file->fptr, buffer, status)) return(file);

  // Set the time-keyword in the Event List Header.
  // See also: Stevens, "Advanced Programming in the UNIX environment",
  // p. 155 ff.
  time_t current_time;
  if (0 != time(&current_time)) {
    struct tm* current_time_utc = gmtime(&current_time);
    if (NULL != current_time_utc) {
      char current_time_str[MAXMSG];
      if (strftime(current_time_str, MAXMSG, "%Y-%m-%dT%H:%M:%S", 
		   current_time_utc) > 0) {
	// Return value should be == 19 !
	if (fits_update_key(file->fptr, TSTRING, "DATE-OBS", current_time_str, 
			    "Start Time (UTC) of exposure", status)) 
	  return(file);
      }
    }
  } 
  // END of writing time information to Event File FITS header.

  // Add header information about program parameters.
  // The second parameter "1" means that the headers are written
  // to the first extension.
  HDpar_stamp(file->fptr, 1, status);

  // Move to the right (second) HDU with the binary table extension.
  int hdutype;
  if (fits_movabs_hdu(file->fptr, 2, &hdutype, status)) return(file);

  // Determine the column numbers.
  if(fits_get_colnum(file->fptr, CASEINSEN, "TIME", &file->ctime, status)) 
    return(file);
  if(fits_get_colnum(file->fptr, CASEINSEN, "ENERGY", &file->cenergy, status)) 
    return(file);
  if(fits_get_colnum(file->fptr, CASEINSEN, "X", &file->cx, status)) 
    return(file);
  if(fits_get_colnum(file->fptr, CASEINSEN, "Y", &file->cy, status)) 
    return(file);

  return(file);
}



int getNextImpactListFileRow(ImpactListFile* ilf, Impact* impact) 
{
  int status = EXIT_SUCCESS;
  int anynul = 0;

  // Move counter to next line.
  ilf->row++;

  // Check if there is still a row available.
  if (ilf->row > ilf->nrows) {
    status = EXIT_FAILURE;
    HD_ERROR_THROW("Error: impact list file contains no further entries!\n", status);
    return(status);
  }

  // Read in the data.
  impact->time = 0.;
  if (0<ilf->ctime) fits_read_col(ilf->fptr, TDOUBLE, ilf->ctime, ilf->row, 1, 1, 
				  &impact->time, &impact->time, &anynul, &status);
  impact->energy = 0.;
  if (0<ilf->cenergy) fits_read_col(ilf->fptr, TFLOAT, ilf->cenergy, ilf->row, 1, 1, 
				    &impact->energy, &impact->energy, &anynul, &status);
  impact->position.x = 0.;
  if (0<ilf->cx) fits_read_col(ilf->fptr, TDOUBLE, ilf->cx, ilf->row, 1, 1, 
			       &impact->position.x, &impact->position.x, &anynul, &status);
  impact->position.y = 0.;
  if (0<ilf->cy) fits_read_col(ilf->fptr, TDOUBLE, ilf->cy, ilf->row, 1, 1, 
			       &impact->position.y, &impact->position.y, &anynul, &status);
  
  // Check if an error occurred during the reading process.
  if (0!=anynul) {
    status = EXIT_FAILURE;
    HD_ERROR_THROW("Error: reading from impact list failed!\n", status);
    return(status);
  }

  return(status);
}



int ImpactListFile_EOF(ImpactListFile* ilf) {
  if (ilf->row >= ilf->nrows) {
    return(1);
  } else {
    return(0);
  }
}


