#include "pattern_recombination.h"


int pattern_recombination_getpar(struct Parameters* parameters)
{
  int status = EXIT_SUCCESS;

  if ((status = PILGetFname("eventlist_filename", parameters->eventlist_filename))) {
    HD_ERROR_THROW("Error reading the name of the input file!\n", status);
  }

  else if ((status = PILGetFname("pattern_filename", parameters->pattern_filename))) {
    HD_ERROR_THROW("Error reading the name of the output file!\n", status);
  }

  else if ((status = PILGetFname("response_filename", parameters->response_filename))) {
    HD_ERROR_THROW("Error reading the name of the detector response file!\n", status);
  }

  // Get the name of the FITS template directory.
  // First try to read it from the environment variable.
  // If the variable does not exist, read it from the PIL.
  else { 
    char* buffer;
    if (NULL!=(buffer=getenv("SIXT_FITS_TEMPLATES"))) {
      strcpy(parameters->templatedir, buffer);
    } else {
      if ((status = PILGetFname("templatedir", parameters->templatedir))) {
	HD_ERROR_THROW("Error reading the path of the FITS templates!\n", status);
      }
    }
  }

  return(status);
}



WFIEvent mark(int row, int column, int rows, int columns, WFIEvent ccdarr[columns][rows], 
	      WFIEvent* maxevent, WFIEvent* evtlist, long* nevtlist, struct RMF* rmf) 
{
  // Return immediately if the pixel is empty.
  if (-1 == ccdarr[column][row].patnum) {
    return(ccdarr[column][row]);
  }

  // Remember this event ...
  WFIEvent myevent = ccdarr[column][row];
  myevent.patnum = 1;
  
  // ... and mark as deleted.
  ccdarr[column][row].patnum = -1;

  evtlist[*nevtlist++] = myevent;
  if (myevent.pha > maxevent->pha) {
    *maxevent = myevent;
  }

  // Visit neighbours:
  int visitrow[8] = { row-1, row+1, row  , row  , row-1, row-1, row+1, row+1 };
  int visitcol[8] = { row  , row  , row-1, row+1, row-1, row+1, row-1, row+1 };

  int i;
  for (i=0; i<8; i++) {
    // Border event?
    if ((visitrow[i] < 0) || (visitrow[i] >= rows) ||
	(visitcol[i] < 0) || (visitcol[i] >= columns)) {
      myevent.patnum = 1000;
    } else {
      // Normal events:
      WFIEvent event = mark(visitrow[i], visitcol[i], rows, columns, ccdarr,
			    maxevent, evtlist, nevtlist, rmf);
      if (-1 != event.patnum ) {
	myevent.pileup += event.pileup; // Remember if pileup occurred in subevent
	myevent.patnum += event.patnum;
	myevent.pha = getChannel(getEnergy(myevent.pha, rmf)+getEnergy(event.pha, rmf), rmf);
      }
    }
  } // End of loop over all neighbours

  // Return combined event:
  myevent.xi = maxevent->xi;
  myevent.yi = maxevent->yi;

  return(myevent);

} // End of mark()



long min(long* array, long nelements)
{
  long minidx=0;
  long i;
  for (i=0; i<nelements; i++) {
    if (array[minidx] > array[i]) {
      minidx = i;
    }
  }
  return(minidx);
}



long max(long* array, long nelements)
{
  long maxidx=0;
  long i;
  for (i=0; i<nelements; i++) {
    if (array[maxidx] < array[i]) {
      maxidx = i;
    }
  }
  return(maxidx);
}



WFIEvent pattern_id(WFIEvent* components, long ncomponents, WFIEvent event)
{
  long i;

  // Get rid of first element in list (emptyevent)
  //  for (i=0; i<*ncomponents-1; i++) {
  //    components[i] = components[i+1];
  //  }
  //  (*ncomponents)--;

  // Get events with maximum and minimum energy:
  long maximum=0, minimum=1000000;
  long nmaxidx=0, nminidx=0;
  long amaxidx[ARRAY_LENGTH], aminidx[ARRAY_LENGTH];
  for (i=0; i<ncomponents; i++) {
    if (components[i].pha > maximum) maximum = components[i].pha;
    if (components[i].pha < minimum) minimum = components[i].pha;
  }
  for (i=0; i<ncomponents; i++) {
    if (components[i].pha == maximum) {
      amaxidx[nmaxidx++] = i;
    }
    if (components[i].pha == minimum) {
      aminidx[nminidx++] = i;
    }
  }

  // Case with 2 events have same energy, pick first in list.
  int equalenergyfound = 0;
  long maxidx, minidx, mididx;
  maxidx = min(amaxidx, nmaxidx);
  if (nmaxidx > 1) {
    mididx = max(amaxidx, nmaxidx);
    equalenergyfound = 1;
  }
  minidx = min(aminidx, nminidx);
  if (nminidx > 1) {
    mididx = max(aminidx, nminidx);
    equalenergyfound = 1;
  }

  // Check if event with maximum PHA is our main event:
  if ((event.xi == components[maxidx].xi) && (event.yi == components[maxidx].yi)) {
    
    // We found doubles:
    if (2 == event.patnum) {
      if ((1 == equalenergyfound) && (minidx == maxidx)) {
	minidx = 1;
	// Because we have two components and the event with the maximum energy
	// was already checked for to be the components[maxidx].
	// For equalenergyfound maxidx is the first in the amaxidx list,
	// therefore the minidx MUST be the other one (1).
      }

      // Check position of second event.
      if (event.yi > components[minidx].yi) event.patid = 1;
      if (event.xi < components[minidx].xi) event.patid = 2;
      if (event.yi < components[minidx].yi) event.patid = 3;
      if (event.xi > components[minidx].xi) event.patid = 4;

      // Case of bad corner event, xi and yi differ.
      if ((event.xi != components[minidx].xi) && (event.yi != components[minidx].yi))
	event.patid = -2;
    } // END if double.

    // Triples:
    if (3 == event.patnum) {
      // Get index of 3rd element.
      if (0 == equalenergyfound) {
	long amididx[ARRAY_LENGTH], nmididx=0;
	for (i=0; i<ncomponents; i++) {
	  if ((components[i].pha < maximum) && (components[i].pha > minimum)) {
	    amididx[nmididx++] = i;
	  }
	}
	// Consistency check:
	assert (1 == nmididx);
	mididx = amididx[0];
      } // Else use setting found above.

      // Search for position of element with minimum energy with respect to event
      // with maximum energy. Then find position  of third element.
      if ((event.yi-1 == components[minidx].yi) && (event.xi == components[minidx].xi)) {
	if ((event.xi-1 == components[mididx].xi) && (event.yi == components[mididx].yi))
	  event.patid = 5;
	if ((event.xi+1 == components[mididx].xi) && (event.yi == components[mididx].yi))
	  event.patid = 6;
      }

      if ((event.yi+1 == components[minidx].yi) && (event.xi == components[minidx].xi)) {
	if ((event.xi-1 == components[mididx].xi) && (event.yi == components[mididx].yi))
	  event.patid = 8;
	if ((event.xi+1 == components[mididx].xi) && (event.yi == components[mididx].yi))
	  event.patid = 7;
      }

      if ((event.xi-1 == components[minidx].xi) && (event.yi == components[minidx].yi)) {
	if ((event.yi-1 == components[mididx].yi) && (event.xi == components[mididx].xi))
	  event.patid = 5;
	if ((event.yi+1 == components[mididx].yi) && (event.xi == components[mididx].xi))
	  event.patid = 8;
      }

      if ((event.xi+1 == components[minidx].xi) && (event.yi == components[minidx].yi)) {
	if ((event.yi-1 == components[mididx].yi) && (event.xi == components[mididx].xi))
	  event.patid = 6;
	if ((event.yi+1 == components[mididx].yi) && (event.xi == components[mididx].xi))
	  event.patid = 7;
      }

      // If event build out of 3 events doesn't have patid up to now,
      // it is not a valid triple.
      if (-1 == event.patid) event.patid = 3000;
    } // END triples.

    // Quadruples:
    if (4 == event.patnum) {
      // Check if events build a 2x2 matrix.

      // A quad has four elements; here we determine the elements which are not
      // min or max elements of the components list. !UGLY!
      long extreme[2] = {minidx, maxidx};
      // Index of the central components:
      long centralidx[4], ncentralidx=0;
      for (i=0; i<4; i++) {
	if ((i != extreme[0]) && (i != extreme[1])) {
	  centralidx[ncentralidx++] = i;
	}
      }
      assert(2==ncentralidx);
      int pass = 0;

      // Now we test if the central elements share column (xi) and row (yi) 
      // with the min / max events. Both criteria must be satisfied.
      // The central events are on the corners of a 2x2 matrix.
      for (i=0; i<2; i++) {
	if (((components[centralidx[i]].yi == components[minidx].yi) &&
	     (components[centralidx[i]].xi == components[maxidx].xi)) ||
	    ((components[centralidx[i]].yi == components[maxidx].yi) &&
	     (components[centralidx[i]].xi == components[minidx].xi))) {
	  pass++;
	}
      }

      // If pass is 2 it has the valid shape of a quad.
      // Now determine the orientation of the pattern.
      if (2 == pass) {
	// Position of events with minimum and maximum energy must be over corner.
	// If this is true, set patid number.
	if ((event.yi-1 == components[minidx].yi) && 
	    (event.xi-1 == components[minidx].xi)) event.patid = 9;
	if ((event.yi-1 == components[minidx].yi) && 
	    (event.xi+1 == components[minidx].xi)) event.patid = 10;
	if ((event.yi+1 == components[minidx].yi) && 
	    (event.xi+1 == components[minidx].xi)) event.patid = 11;
	if ((event.yi+1 == components[minidx].yi) && 
	    (event.xi-1 == components[minidx].xi)) event.patid = 12;
      }

      // If event build out of 4 events doesn't have a patid up to now,
      // it is not a valid quadruple.
      if (-1 == event.patid) event.patid = 4000;
    } // END if quadruple

  } // END check if event with maximum energy is main event.
      
  return(event);
} // END pattern_id()



WFIEvent* pattern_recognition(WFIEvent* evlist, long* nevlist, struct RMF* rmf, 
			      int columns, int rows)
{
  // The pixel number for a combined pattern is the pixel number with
  // the main energy.

  // Starting values for the emptyevent in the pattern search.
  WFIEvent emptyevent = {
    .pha = -1,
    .xi = -1,
    .yi = -1,
    .frame = -1,
    .patnum = -1,
    .patid = 0,
    .pileup = 0
  };

  // Conservative assumption: we only have singles --> evtmax is really the maximum
  // number of events; the real number will be smaller.
  long i, j;
  for (i=0; i<*nevlist; i++) {
    evlist[i].patnum = -1;
  }
  long evtpos = 0;
  WFIEvent ccdarr[columns][rows];
  for (i=0; i<columns; i++) {
    for (j=0; j<rows; j++) {
      ccdarr[i][j] = emptyevent;
    }
  }
  long idx = 0;

  // Note: we throw away the last frame time. This makes life much easier in the 2nd
  // while loop below.
  long maxframe = evlist[(*nevlist)-1].frame;
  while (evlist[idx].frame < maxframe) {
    long idx1 = idx;
    while (evlist[idx1].frame == evlist[idx].frame) {
      // Search for index of next frame.
      idx1++;
    }
    
    WFIEvent photons[ARRAY_LENGTH]; // Photons of one frame.
    long nphotons = idx1-idx;
    for (i=0; i<nphotons; i++) {
      photons[i] = evlist[i+idx]; 
    }

    // Write current events onto Depfet matrix (the pointer structure):
    for (i=0; i<nphotons; i++) {
      ccdarr[photons[i].xi][photons[i].yi] = photons[i];
      ccdarr[photons[i].xi][photons[i].yi].patnum = 0;
    }
    idx = idx1;

    // Find singles and multiples:
    for (i=0; i<nphotons; i++) {
      // Combine event at this position:
      int row    = photons[i].yi;
      int column = photons[i].xi;
      // Check event, if it has not yet been dealt with:
      if (-1 != ccdarr[column][row].patnum) { // Choose only pixels with events.
	// Initialize maxevent (holding the recombined photon energy at the position
	// of the event with the maximum photon energy).
	WFIEvent maxevent = emptyevent;
	// Becomes a list, containing the events the current pattern is build from.
	WFIEvent components[ARRAY_LENGTH];
	long ncomponents = 0;

	WFIEvent event = mark(row, column, rows, columns, ccdarr, &maxevent, 
			      components, &ncomponents, rmf);

	// Call pattern_id to check for valid patterns and label them.
	// Not needed for border events and singles.
	// Here the generated pattern is labeled.
	WFIEvent labeledevent;
	if (1 == event.patnum) {
	  event.patid = 0; // Set single patid.
	  labeledevent = event;
	} else { // if (event.patnum < 1000)
	  labeledevent = pattern_id(components, ncomponents, event);
	}
	evlist[evtpos] = labeledevent;
	evtpos++;
      } // END choose only pixels with events.
    } // END of loop over all photons.
    // NB ccdarr is now empty again as we have dealt with all photons.
  } // END while() - frame is done
  
  *nevlist = evtpos;
  
  return(evlist);
} // END of pattern_recognition()



int pattern_recombination_main() {
  struct Parameters parameters;
  WFIEventFile eventfile;
  WFIEventFile patternfile;
  WFIEvent* evlist = NULL; // Event list

  int status = EXIT_SUCCESS;


  // Register HEATOOL
  set_toolname("pattern_recombination");
  set_toolversion("0.01");

  do { // ERROR handling loop

    // Read parameters by PIL:
    status = pattern_recombination_getpar(&parameters);
    if (EXIT_SUCCESS!=status) break;

    // Open the INPUT event file:
    status = openWFIEventFile(&eventfile, parameters.eventlist_filename, READWRITE);
    if (EXIT_SUCCESS!=status) break;

    // Create a new OUTPUT event / pattern file:
    // Set the event list template file for the different WFI modes:
    char template_filename[MAXMSG];
    strcpy(template_filename, parameters.templatedir);
    strcat(template_filename, "/");
    if (16==eventfile.columns) {
      strcat(template_filename, "wfi.window16.eventlist.tpl");
    } else if (1024==eventfile.columns) {
      strcat(template_filename, "wfi.full1024.eventlist.tpl");
    } else {
      status = EXIT_FAILURE;
      char msg[MAXMSG];
      sprintf(msg, "Error: detector width (%d pixels) is not supported!\n", eventfile.columns);
      HD_ERROR_THROW(msg, status);
      return(status);
    }
    // Create and open the new file
    status = openNewWFIEventFile(&patternfile, parameters.pattern_filename, template_filename);
    if (EXIT_SUCCESS!=status) return(status);



    // Read in all events from the event file.
    long nevlist = 0;
    evlist = (WFIEvent*)malloc(eventfile.generic.nrows*sizeof(WFIEvent));
    if (NULL==evlist) {
      status=EXIT_FAILURE;
      HD_ERROR_THROW("Error: memory allocation for event list failed!\n", status);
      break;
    }
    while ((EXIT_SUCCESS==status) && (0==EventFileEOF(&eventfile.generic))) {
      // Read the next event from the FITS file.
      status=WFIEventFile_getNextRow(&eventfile, &(evlist[nevlist++]));
      if(EXIT_SUCCESS!=status) break;
    } // END of loop over all events in the event file.
    

    // Read the EBOUNDS from the detector response file.
    struct RMF* rmf = loadRMF(parameters.response_filename, &status);
    
    // Perform the pattern recognition algorithm.
    /*evlist =*/ pattern_recognition(evlist, &nevlist, rmf, eventfile.columns, eventfile.rows);
    
    
  } while(0); // End of error handling loop


  // --- Clean Up ---

  // Free memory from the event list.
  if (NULL!=evlist) free(evlist);

  // Close the event file:
  closeWFIEventFile(&eventfile);

  return(status);
}
