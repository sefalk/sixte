#include "framestoredetector.h"


int initFramestoreDetector(FramestoreDetector* fd, 
			   struct FramestoreDetectorParameters* parameters)
{
  int status = EXIT_SUCCESS;

  // Call the initialization routines of the underlying data structures.
  status = initGenericDetector(&fd->generic, &parameters->generic);
  if (EXIT_SUCCESS!=status) return(status);
  status = initSquarePixels(&fd->pixels, &parameters->pixels);
  if (EXIT_SUCCESS!=status) return(status);

  // Set up the framestore configuration.
  fd->integration_time = parameters->integration_time;

  // Set the first readout time such that the first readout is performed 
  // immediately at the beginning of the simulation.
  fd->readout_time = parameters->t0;
  fd->frame = 0;

  // Create and open new event list file.
  status = openNeweROSITAEventFile(&fd->eventlist, parameters->eventlist_filename,
				       parameters->eventlist_template);
  if (EXIT_SUCCESS!=status) return(status);

  return(status);
}



int cleanupFramestoreDetector(FramestoreDetector* fd) 
{
  int status=EXIT_SUCCESS;

  // Call the cleanup routines of the underlying data structures.
  cleanupSquarePixels(&fd->pixels);
  status+=closeeROSITAEventFile(&fd->eventlist);

  return(status);
}



int checkReadoutFramestoreDetector(FramestoreDetector* fd, double time)
{
  int status = EXIT_SUCCESS;

  // Check, if the detector integration time was exceeded. 
  // In that case, read out the detector.
  if (time > fd->readout_time) {

    // Readout the detector and create eventlist entries for the actual time:
    status = readoutFramestoreDetector(fd);

    // Clear the detector array.
    clearSquarePixels(&fd->pixels);

    // Update the detector frame time to the next frame until the current
    // time is within the detector->readout interval.
    // This CAN ONLY BE DONE for FRAMESTORE detectors!
    // For detectors with individual readout lines a more complicated method is required.
    while (time > fd->readout_time) {
      fd->readout_time += fd->integration_time;
      fd->frame += 2;
    }

    // Print the time of the current events in order (program status
    // information for the user).
    headas_printf("\rtime: %.3lf s ", fd->readout_time);
    fflush(NULL);
  }

  return(status);
}




inline int readoutFramestoreDetector(FramestoreDetector* fd) 
{
  int x, y;
  int status = EXIT_SUCCESS;

  // Read out the entire detector array.
  for (x=0; x<fd->pixels.xwidth; x++) {
    for (y=0; y<fd->pixels.ywidth; y++) {
      if (fd->pixels.array[x][y].charge > 1.e-6) {
	eROSITAEvent event;
	// Determine the detector channel that corresponds to the charge stored
	// in the detector pixel.
	event.pha = getChannel(fd->pixels.array[x][y].charge, fd->generic.rmf);

	// Check lower threshold (PHA and energy):
	if ((event.pha>=fd->generic.pha_threshold) && 
	    (fd->pixels.array[x][y].charge>=fd->generic.energy_threshold)) { 

	  // REMOVE TODO
	  assert(event.pha >= 0);
	  // Maybe: if (event.pha < 0) continue;
	
	  // There is an event in this pixel, so insert it into the eventlist:
	  event.time = fd->readout_time;
	  event.energy = fd->pixels.array[x][y].charge * 1.e3; // [eV]
	  event.xi = x;
	  event.yi = y;
	  event.frame = fd->frame;

	  status=addeROSITAEvent2File(&fd->eventlist, &event);
	  if (EXIT_SUCCESS!=status) return(status);
	} // END of check for threshold
      } // END of check whether  charge > 1.e-6
    } // END of loop over y
  } // END of loop over x

  return(status);
}



int addImpact2FramestoreDetector(FramestoreDetector* fd, Impact* impact)
{
  int status=EXIT_SUCCESS;

  // Before adding the new impact to the detector check whether
  // a readout has to be performed in advance.
  status=checkReadoutFramestoreDetector(fd, impact->time);

  // Determine a detector channel (PHA channel) according to the RMF.
  // The channel is obtained from the RMF using the corresponding
  // HEAdas routine which is based on drawing a random number.
  long channel;
  ReturnChannel(fd->generic.rmf, impact->energy, 1, &channel);

  // Check if the photon is really measured. If the
  // PHA channel returned by the HEAdas RMF function is '-1', 
  // the photon is not detected.
  // This can happen as the RMF is actually an RSP including, e.g., 
  // the detector quantum efficiency and filter transmission.
  if (-1==channel) {
    return(status); // Break the function (and continue with the next photon).
  }
  assert(channel>=0);

  // Get the corresponding created charge.
  // NOTE: In this simulation the charge is represented by the nominal
  // photon energy which corresponds to the PHA channel according to the
  // EBOUNDS table.
  float charge = getEnergy(channel, fd->generic.rmf);
  
  if (charge > 0.) {
    int x[4], y[4];
    double fraction[4];
    
    // Determine the affected detector pixels.
    int npixels = getSquarePixelsSplits(&fd->pixels, &fd->generic, impact->position, 
					x, y, fraction);
    
    // Add the charge created by the photon to the affected detector pixels.
    int count;
    for (count=0; count<npixels; count++) {
      if (x[count] != INVALID_PIXEL) {
	fd->pixels.array[x[count]][y[count]].charge += 
	  charge * fraction[count]; /*   * 
	  // |      |-> charge fraction due to split events
	  // |-> charge created by incident photon
	  FramestoreDetectorIsSensitive(x[count], y[count], fd, impact->time);
	  // |-> "1" if pixel can measure charge, "0" else */
      }
    }
  } // END if(charge>0.)

  return(status);
}



/*
inline int FramestoreDetectorIsSensitive(int x, int y, FramestoreDetector* fd, double time)
{
  // The specified detector pixel is active at the moment.
  return(1);
}
*/


/*
// Consistency check for size of charge cloud:
if (detector->ccsize > detector->pixelwidth) {
status=EXIT_FAILURE;
HD_ERROR_THROW("Error: charge cloud size greater than pixel width!\n", status);
break;
}
*/

