#ifndef EROSITAEVENTFILE_H
#define EROSITAEVENTFILE_H 1

#include "sixt.h"
#include "erositaevent.h"
#include "eventfile.h"


typedef struct {
  EventFile generic; /**< Generic EventFile data structure. */

  /* Column numbers of the individual eROSITA-specific event list entries.
   * The numbers start at 1. The number 0 means, that there 
   * is no corresponding column in the table. */
  int ctime, cpha, cenergy, crawx, crawy, cframe;
  int cra, cdec, cskyx, cskyy;

} eROSITAEventFile;


/////////////////////////////////////////////////////////////////////


/** Opens an existing FITS file with a binary table event list.
 * Apart from opening the FITS file the function also determines the number of rows in 
 * the FITS table and initializes the eROSITAEventFile data structure. 
 * The access_mode parameter can be either READONLY or READWRITE.
 */
int openeROSITAEventFile(eROSITAEventFile*, char* filename, int access_mode);

/** Create and open a new event list FITS file for the eROSITA detector from 
 * a given FITS template.
 * If the file already exists, the old file is deleted and replaced by an empty one.
 * Apart from opening the FITS file the function also initializes the eROSITAEventFile 
 * data structure. 
 * The access_mode parameter is always READWRITE.
 */
int openNeweROSITAEventFile(eROSITAEventFile*, char* filename, char* template);

/** Close an open eROSITA event list FITS file. */
int closeeROSITAEventFile(eROSITAEventFile*);

/** Append a new eROSITA event to the to event list. */
int addeROSITAEvent2File(eROSITAEventFile*, eROSITAEvent*);

/** Read the next eROSITAEvent from the eROSITAEventFile.
 * This routine increases the internal counter of the eROSITAEventFile data structure.
 * The return value is the error status. */
int eROSITAEventFile_getNextRow(eROSITAEventFile*, eROSITAEvent*);


#endif /* EROSITAEVENTFILE */
