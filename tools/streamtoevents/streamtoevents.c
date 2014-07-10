/*
   This file is part of SIXTE.

   SIXTE is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   any later version.

   SIXTE is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   For a copy of the GNU General Public License see
   <http://www.gnu.org/licenses/>.


   Copyright 2014 Philippe Peille, IRAP
*/

#include "streamtoevents.h"

////////////////////////////////////
/** Main procedure. */
int streamtoevents_main() {
  // Containing all programm parameters read by PIL.
  struct Parameters par;
  
  // Error status.
  int status=EXIT_SUCCESS;
  
  //Pointers
  PixImpFile* impfile=NULL;
  TESDataStream* stream=NULL;
  AdvDet *det=NULL;
  TesStreamFile* tesfile=NULL;
  TesEventFile** outputFiles=NULL;

  // Keywords
  char telescop[MAXMSG];
  char instrume[MAXMSG];
  char filter[MAXMSG];
  char ancrfile[MAXMSG];
  char respfile[MAXMSG];
  double mjdref;
  double timezero;
  double tstart;
  double tstop;
  float monoen;
  
  int ii;
  int hdu_type=0;
  int act=0;

  char comment[MAXMSG];

  // Register HEATOOL:
  set_toolname("streamtoevents");
  set_toolversion("0.05");
  
  do { // Beginning of the ERROR handling loop (will at 
       // most be run once).
       
    headas_chat(3, "initialize ...\n");
    // Get program parameters.
    status=getpar(&par);
    CHECK_STATUS_BREAK(status);

    // Open the pixel impact file
    //impfile=openPixImpFile(par.PixImpList, READONLY, &status);
    //CHECK_STATUS_BREAK(status);
  
    // Load the detector structure
    det=loadAdvDet(par.XMLFile, &status);
    
    CHECK_STATUS_BREAK(status);
    
    // Check and eventually modifies the active pixels
    if(par.Nactive==-1){
      par.Nactive=det->npix;
      par.nlo=0;
      par.nhi=det->npix-1;
    }
    for(ii=0; ii<det->npix; ii++){
      if(ii>=par.nlo && ii<=par.nhi){
	act++;
      }
    }
    if(act!=par.Nactive){
      status=EXIT_FAILURE;
      char msg[MAXMSG];
      sprintf(msg, "Number of active pixels has been corrupted. Counted %d instead of %d pixels.", act, par.Nactive);
      SIXT_ERROR(msg);
      CHECK_STATUS_BREAK(status);
    }
    
    //Open TesStream File
    tesfile = openTesStreamFile(par.streamname,par.nlo,par.nhi,READONLY, &status);
    CHECK_STATUS_BREAK(status);


    // Read keywords from tes file
    if (fits_movabs_hdu(tesfile->fptr,1, &hdu_type, &status)) break;
    sixt_read_fits_stdkeywords(tesfile->fptr,
			       telescop,
			       instrume,
			       filter,
			       ancrfile,
			       respfile,
			       &mjdref,
			       &timezero,
			       &tstart,
			       &tstop, 
			       &status);
    //Read monochromatic energy
    if (fits_movabs_hdu(tesfile->fptr,2, &hdu_type, &status)) break;
    fits_read_key(tesfile->fptr, TFLOAT, "MONOEN", &monoen, comment, &status);    
    CHECK_STATUS_BREAK(status);

    double tstart_stream;
    printf("TES stream file reaches from %lfs-%lfs .\n", tstart, tstop);
    tstart_stream = tstart;
    if(tstart>par.tstart){
      puts("Program parameter tstart smaller as in TES stream file.");    
    }else{
      tstart=par.tstart;
    }
    if(tstop<par.tstop){
      puts("Program parameter tstop larger as in TES stream file.");
    }else{
      tstop=par.tstop;
    }
    printf("Retrieving TES Stream from %lfs-%lfs .\n", tstart, tstop);

    //Create TESDataStream from file
    stream = newTESDataStream(&status);
    CHECK_STATUS_BREAK(status);
    stream = generateTESDataStreamFromFile(stream,tesfile,tstart_stream,tstart,tstop,
					   det->SampleFreq,&status);
    CHECK_STATUS_BREAK(status);


    
    //Open output files
    outputFiles = (TesEventFile**)malloc((tesfile->Npix)*sizeof(TesEventFile*));
    if(outputFiles==NULL){
      status=EXIT_FAILURE;
      SIXT_ERROR("memory allocation for ouputFiles failed");
      CHECK_STATUS_BREAK(status);
    }

    for (int i =0;i<tesfile->Npix;i++){
      outputFiles[i] = opennewTesEventFile(par.tesEventFile,
					   telescop,
					   instrume,
					   filter,
					   ancrfile,
					   respfile,
					   par.XMLFile,
					   par.PixImpList,
					   mjdref,
					   timezero,
					   tstart,
					   tstop,
					   par.nlo+i+1,
					   par.eventSize,
					   par.preBufferSize,
					   det->SampleFreq,
					   par.clobber,
					   &status);

    }
    

    ////////////////////////////////
    //Triggering part
    //For the moment, perfect trigger using impact file
    ////////////////////////////////


    // Open the pixel impact file
    impfile=openPixImpFile(par.PixImpList, READONLY, &status);
    CHECK_STATUS_BREAK(status);
    double tstartImp=0;
    double tstopImp=0;
    fits_read_key(impfile->fptr, TDOUBLE, "TSTART", &tstartImp, comment, &status);
    fits_read_key(impfile->fptr, TDOUBLE, "TSTOP", &tstopImp, comment, &status);
    CHECK_STATUS_BREAK(status);

    //Check if tstart/tstop are compatible with pix impact file and correct if necessary
    double tstartTES = tstart;
    printf("Pix impact file reaches from %lfs-%lfs .\n", tstartImp, tstopImp);
    if(tstartImp>tstart){
      puts("TES stream file compatible tstart is smaller than in pix impact file.");    
    }else{
      tstart=tstartImp;
    }
    if(tstopImp<tstop){
      puts("TES stream file compatible tstop is larger than in pix impact file.");
    }else{
      tstop=par.tstop;
    }
    printf("Simulate from %lfs-%lfs .\n", tstart, tstop);

    // Write TES events in output file
    writeEvents2FITS(outputFiles,stream,impfile,par.nlo,tesfile->Npix,tstart,tstartTES,
		     tstop,det->SampleFreq,par.eventSize,par.preBufferSize,monoen,&status);

  } while(0); // END of the error handling loop.

  //Free memory
  if (outputFiles!=NULL){
    for (ii=0;ii<tesfile->Npix;ii++){
      freeTesEventFile(&(outputFiles[ii]), &status);
    }
  }
  

  destroyAdvDet(&det);
  freePixImpFile(&impfile, &status);
  destroyTESDataStream(stream);
  freeTesStreamFile(&tesfile,&status);

  if (EXIT_SUCCESS==status) {
    headas_chat(3, "finished successfully!\n\n");
    return(EXIT_SUCCESS);
  } else {
    return(EXIT_FAILURE);
  }
}

int getpar(struct Parameters* const par)
{
  // String input buffer.
  char* sbuffer=NULL;

  // Error status.
  int status=EXIT_SUCCESS; 

  // Read all parameters via the ape_trad_ routines.

  status=ape_trad_query_string("PixImpList", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the pixel impact list");
    return(status);
  } 
  strcpy(par->PixImpList, sbuffer);
  free(sbuffer);

  status=ape_trad_query_string("XMLFile", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the XML file");
    return(status);
  } 
  strcpy(par->XMLFile, sbuffer);
  free(sbuffer);

  status=ape_trad_query_string("Streamfile", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the Stream file");
    return(status);
  } 
  strcpy(par->streamname, sbuffer);
  free(sbuffer); 

  status=ape_trad_query_string("TesEventfile", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the TES Event output file");
    return(status);
  } 
  strcpy(par->tesEventFile, sbuffer);
  free(sbuffer);
  
  status=ape_trad_query_int("EventSize", &par->eventSize);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the EventSize parameter");
    return(status);
  }

  status=ape_trad_query_int("PreBufferSize", &par->preBufferSize);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the PreBufferSize parameter");
    return(status);
  }
  
  status=ape_trad_query_bool("clobber", &par->clobber);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the clobber parameter");
    return(status);
  }

  status=ape_trad_query_double("tstart", &par->tstart);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the tstart parameter");
    return(status);
  }
  
  status=ape_trad_query_double("tstop", &par->tstop);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the tstop parameter");
    return(status);
  }

  status=ape_trad_query_bool("history", &par->history);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the history parameter");
    return(status);
  }
  
  char *pix=NULL;
  status=ape_trad_query_string("pixels", &pix);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the pixel parameter");
    return(status);
  }
  if(!strcmp(pix, "all")){
    par->Nactive=-1;
  } else {
    char *ptr=strchr(pix, '-');
    if(ptr!=NULL){
      sscanf(pix, "%d - %d", &(par->nlo), &(par->nhi));
      par->nlo--;
      par->nhi--;
      par->Nactive=par->nhi-par->nlo+1;
      printf("Activate pixels %d-%d (%d pixels).\n", par->nlo+1, par->nhi+1, par->Nactive);
    } else {
      sscanf(pix, "%d", &(par->nlo));
      par->nlo--;
      par->nhi=par->nlo;
      par->Nactive=1;
    }
    if(par->Nactive<0 || par->nlo<0){
      status=EXIT_FAILURE;
      SIXT_ERROR("Input values for active pixels corrupted.");
      return(status);
    }
  }
  free(pix);

  return(status);
}
