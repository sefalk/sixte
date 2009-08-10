#include "htrseventfile.h"


int openHTRSEventFile(HTRSEventFile* hef, char* filename, int access_mode)
{
  int status = EXIT_SUCCESS;

  // Call the corresponding routine of the underlying structure.
  status = openEventFile(&hef->generic, filename, access_mode);
  if (EXIT_SUCCESS!=status) return(status);

  // Determine the HTRS-specific elements of the event list.
  // Determine the individual column numbers:
  // REQUIRED columns:
  if(fits_get_colnum(hef->generic.fptr, CASEINSEN, "TIME", &hef->ctime, &status)) 
    return(status);
  if(fits_get_colnum(hef->generic.fptr, CASEINSEN, "PHA", &hef->cpha, &status)) 
    return(status);
  if(fits_get_colnum(hef->generic.fptr, CASEINSEN, "PIXEL", &hef->cpixel, &status)) 
    return(status);

  return(status);
}


int openNewHTRSEventFile(HTRSEventFile* hef, char* filename, char* template)
{
  int status=EXIT_SUCCESS;

  // Set the FITS file pointer to NULL. In case that an error occurs during the file
  // generation, we want to avoid that the file pointer points somewhere.
  hef->generic.fptr = NULL;

  // Remove old file if it exists.
  remove(filename);

  // Create a new event list FITS file from a FITS template.
  fitsfile* fptr=NULL;
  char buffer[MAXMSG];
  sprintf(buffer, "%s(%s)", filename, template);
  if (fits_create_file(&fptr, buffer, &status)) return(status);

  // Set the time-keyword in the Event List Header.
  // See also: Stevens, "Advanced Programming in the UNIX environment", p. 155 ff.
  time_t current_time;
  if (0 != time(&current_time)) {
    struct tm* current_time_utc = gmtime(&current_time);
    if (NULL != current_time_utc) {
      char current_time_str[MAXMSG];
      if (strftime(current_time_str, MAXMSG, "%Y-%m-%dT%H:%M:%S", current_time_utc) > 0) {
	// Return value should be == 19 !
	if (fits_update_key(fptr, TSTRING, "DATE-OBS", current_time_str, 
			    "Start Time (UTC) of exposure", &status)) return(status);
      }
    }
  } // END of writing time information to Event File FITS header.

  if (fits_close_file(fptr, &status)) return(status);


  // Open the newly created FITS file.
  status = openHTRSEventFile(hef, filename, READWRITE);

  return(status);
}



int closeHTRSEventFile(HTRSEventFile* hef)
{
  // Call the corresponding routine of the underlying structure.
  return(closeEventFile(&hef->generic));
}



int addHTRSEvent2File(HTRSEventFile* hef, HTRSEvent* event)
{
  int status=EXIT_SUCCESS;

  // Insert a new, empty row to the table:
  if (fits_insert_rows(hef->generic.fptr, hef->generic.row, 1, &status)) return(status);
  hef->generic.row++;
  hef->generic.nrows++;

  if (fits_write_col(hef->generic.fptr, TDOUBLE, hef->ctime, hef->generic.row, 
		     1, 1, &event->time, &status)) return(status);
  if (fits_write_col(hef->generic.fptr, TLONG, hef->cpha, hef->generic.row, 
		     1, 1, &event->pha, &status)) return(status);
  if (fits_write_col(hef->generic.fptr, TINT, hef->cpixel, hef->generic.row, 
		     1, 1, &event->pixel, &status)) return(status);

  return(status);
}


