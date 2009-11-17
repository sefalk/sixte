#if HAVE_CONFIG_H
#include <config.h>
#else
#error "Do not compile outside Autotools!"
#endif

#include "comarecon.h"


////////////////////////////////////
/** Main procedure. */
int comarecon_main() {
  struct Parameters parameters;
  
  CoMaEventFile* eventfile=NULL;
  SquarePixels* detector_pixels=NULL;
  SquarePixels* sky_pixels=NULL;

  int status=EXIT_SUCCESS; // Error status.


  // Register HEATOOL:
  set_toolname("comarecon");
  set_toolversion("0.01");


  do {  // Beginning of the ERROR handling loop (will at most be run once)

    // --- Initialization ---

    // Read the program parameters using the PIL library.
    if ((status=comarecon_getpar(&parameters))) break;
    
    // Open the event file.
    eventfile = openCoMaEventFile(parameters.eventlist_filename, READONLY, &status);
    if (EXIT_SUCCESS!=status) break;

    // TODO Read the decoding array from the FITS file.
    

    // DETECTOR setup.
    struct SquarePixelsParameters dspp = {
      .xwidth = parameters.width,
      .ywidth = parameters.width,
      .xpixelwidth = parameters.pixelwidth,
      .ypixelwidth = parameters.pixelwidth 
    };
    detector_pixels=getSquarePixels(&dspp, &status);
    if(EXIT_SUCCESS!=status) break;
    // END of DETECTOR CONFIGURATION SETUP    

    // SKY IMAGE setup.
    struct SquarePixelsParameters sspp = {
      .xwidth = parameters.width + 2*(parameters.width-1),
      .ywidth = parameters.width + 2*(parameters.width-1),
      .xpixelwidth = parameters.pixelwidth, // TODO replace by some [deg] value.
      .ypixelwidth = parameters.pixelwidth 
    };
    sky_pixels=getSquarePixels(&sspp, &status);
    if(EXIT_SUCCESS!=status) break;
    // END of SKY IMAGE CONFIGURATION SETUP    
    
    // --- END of Initialization ---


    // --- Beginning of Imaging Process ---

    // Beginning of actual detector simulation (after loading required data):
    headas_chat(5, "start image reconstruction process ...\n");
    CoMaEvent event;

    // Loop over all events in the FITS file.
    while ((EXIT_SUCCESS==status)&&(0==EventFileEOF(&eventfile->generic))) {

      status=CoMaEventFile_getNextRow(eventfile, &event);
      if(EXIT_SUCCESS!=status) break;

      // Add the event to the SquarePixels array.
      detector_pixels->array[event.xi][event.yi].charge += 1.0;

    } // END of scanning the impact list.
    if (EXIT_SUCCESS!=status) break;

    // TODO Perform the image reconstruction algorithm.
    // (Correlation: S = D * G)
    /*
    int skyx, skyy, detx, dety; // Counters.
    for (skyx=0; skyx < sky_pixels->xwidth // TODO
	   ; skyx++) {
      for (detx=0; detx < detector_pixels->xwidth; detx++) {

      }
    }
    */

    // TODO Write the reconstructed source function to the output FITS file.


  } while(0);  // END of the error handling loop.


  // --- cleaning up ---
  headas_chat(5, "cleaning up ...\n");

  // Free the Detector pixels.
  freeSquarePixels(detector_pixels);

  // Close the FITS files.
  status += closeCoMaEventFile(eventfile);

  if (status == EXIT_SUCCESS) headas_chat(5, "finished successfully!\n\n");
  return(status);
}



int comarecon_getpar(struct Parameters* parameters)
{
  int status=EXIT_SUCCESS; // Error status.

  // Get the filename of the mask reconstruction file (FITS input file).
  if ((status = PILGetFname("mask_filename", 
			    parameters->mask_filename))) {
    HD_ERROR_THROW("Error reading the filename of the mask reconstruction image "
		   "file!\n", status);
  }

  // Get the filename of the event list file (FITS input file).
  else if ((status = PILGetFname("eventlist_filename", 
				 parameters->eventlist_filename))) {
    HD_ERROR_THROW("Error reading the filename of the event list input "
		   "file!\n", status);
  }

  // Get the filename of the image file (FITS output file).
  else if ((status = PILGetFname("image_filename", 
				 parameters->image_filename))) {
    HD_ERROR_THROW("Error reading the filename of the image output "
		   "file!\n", status);
  }

  // Read the width of the detector in [pixel].
  else if ((status = PILGetInt("width", &parameters->width))) {
    HD_ERROR_THROW("Error reading the detector width!\n", status);
  }

  // Read the width of one detector pixel in [m].
  else if ((status = PILGetReal("pixelwidth", &parameters->pixelwidth))) {
    HD_ERROR_THROW("Error reading the width detector pixels!\n", status);
  }
  if (EXIT_SUCCESS!=status) return(status);

  // Get the name of the FITS template directory.
  // First try to read it from the environment variable.
  // If the variable does not exist, read it from the PIL.
  char* buffer;
  if (NULL!=(buffer=getenv("SIXT_FITS_TEMPLATES"))) {
    strcpy(parameters->eventlist_template, buffer);
  } else {
    if ((status = PILGetFname("fits_templates", 
			      parameters->eventlist_template))) {
      HD_ERROR_THROW("Error reading the path of the FITS templates!\n", status);
      
    }
  }
  // Set the impact list template file:
  strcat(parameters->eventlist_template, "/coma.eventlist.tpl");

  return(status);
}



