#ifndef HTRSEVENTFILE_H
#define HTRSEVENTFILE_H 1

#include "sixt.h"
#include "htrsevent.h"
#include "eventfile.h"


typedef struct {

  /** Generic EventFile data structure. */
  EventFile generic; 

  /* Column numbers of the individual HTRS-specific event list entries.
   * The numbers start at 1. The number 0 means, that there 
   * is no corresponding column in the table. */
  int ctime, cpha, cpixel;

} HTRSEventFile;


/////////////////////////////////////////////////////////////////////


/** Opens an existing FITS file with a binary table event list.
 * Apart from opening the FITS file the function also determines the number of rows in 
 * the FITS table and initializes the HTRSEventFile data structure. 
 * The access_mode parameter can be either READONLY or READWRITE.
 */
int openHTRSEventFile(HTRSEventFile*, char* filename, int access_mode);

/** Create and open a new FITS event file for the HTRS detector from 
 * a given FITS template.
 * If the file already exists, the old file is deleted and replaced by an empty one.
 * Apart from opening the FITS file the function also initializes the HTRSEventFile 
 * data structure by calling openHTRSEventFile().
 * The access_mode parameter is always READWRITE.
 */
int openNewHTRSEventFile(HTRSEventFile*, char* filename, char* template);

/** Close an open HTRS event list FITS file. */
int closeHTRSEventFile(HTRSEventFile*);

/** Append a new HTRS event to the to event list. 
 * In the given HTRSEvent data structure the pixel numbering starts at 0, but in the
 * event file the numbering has to start at 1. So the routine adds a 1 to the pixel index. */
int addHTRSEvent2File(HTRSEventFile*, HTRSEvent*);


#endif /* HTRSEVENTFILE */