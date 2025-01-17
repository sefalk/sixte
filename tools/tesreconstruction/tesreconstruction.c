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


   Copyright 2015 Philippe Peille, IRAP
*/

#include "tesreconstruction.h"

////////////////////////////////////
/** Main procedure. */
int tesreconstruction_main() {
  // Containing all programm parameters read by PIL.
  struct Parameters par;
  
  // Error status.
  int status=EXIT_SUCCESS;

  // Register HEATOOL:
  set_toolname("tesreconstruction");
  set_toolversion("0.05");
  
  do { // Beginning of the ERROR handling loop (will at
       // most be run once).
    headas_chat(3, "initialize ...\n");
    // Get program parameters.
    status=getpar(&par);
    CHECK_STATUS_BREAK(status);
    //printf("%s","Paso0\n");
    
    // Read XML info
    //--------------
    AdvDet *det = newAdvDet(&status);
    CHECK_STATUS_BREAK(status);
    det = loadAdvDet(par.XMLFile, &status);
    CHECK_STATUS_BREAK(status);
    // Read the sampling rate from XML file
    int sf = -999.; 
    int div = 1;
    sf = det->SampleFreq;
    
    char* firstchar = strndup(par.RecordFile, 1);
    char firstchar2[2];
    strcpy(firstchar2,firstchar);
        
    //printf("%s %s %s","Fichero: ",par.RecordFile,"\n");
    // Check input file header is complete to work with xifusim/tessim simulated files
    // -------------------------------------------------------------------------------
    fitsfile* fptr = NULL;
    int numfits;
    if (strcmp(firstchar2,"@") == 0)
    {
            //printf("%s %s %s","Fichero: ",strndup(par.RecordFile+1, strlen(par.RecordFile)-1),"\n");
            FILE *filetxt = fopen(strndup(par.RecordFile+1, strlen(par.RecordFile)-1), "r");
            if (filetxt == NULL)    
            {
                    printf("%s","File given in RecordFile does not exist\n");
                    status = 104;
            }
            CHECK_STATUS_BREAK(status);
            
            char filefits[256];
            fgets(filefits, 256, filetxt);
            strtok(filefits, "\n");     // To delete '/n' from filefits (if not, 'fits_open_file' can not open the file)
            
            fits_open_file(&fptr, filefits, READONLY, &status);
            if (status != 0)    printf("%s","FITS file read from ASCII file does not exist\n");
            CHECK_STATUS_BREAK(status);  
            
            //printf("%s %s %s","FicheroFITS: ",filefits,"\n");
            
            fclose(filetxt);
            
            numfits = 0;
                   
            filetxt = fopen(strndup(par.RecordFile+1, strlen(par.RecordFile)-1), "r");
            
            while(fscanf(filetxt,"%s",filefits)!=EOF)
            {
                numfits++;
            }
            //printf("%s %d %s","numfits: ",numfits,"\n");
            
            fclose(filetxt);
    }
    else
    {
            numfits = 1;
            fits_open_file(&fptr, par.RecordFile, READWRITE, &status);
            if (status != 0)    printf("%s","File given in RecordFile does not exist\n");
    }
    
    CHECK_STATUS_BREAK(status);
    
    int hdunum; // Number of HDUs (RECORDS-file or TESRECORDS-file)
    fits_get_num_hdus(fptr, &hdunum,&status);
    
    if (hdunum == 8) //xifusim simulated file (with TESRECORDS)
    {    
        //printf("%s","Paso1\n");
        fits_movnam_hdu(fptr, ANY_HDU,"TESRECORDS", 0, &status);
        CHECK_STATUS_BREAK(status);
        
        // Read NETTOT keyword from "TESRECORDS" HDU
        // (used to check detection)
        long nettot_key;
        fits_read_key(fptr,TLONG,"NETTOT", &nettot_key,NULL,&status);  
        if (nettot_key == 0)
        {
            // Move to "Primary" HDU to obtain SAMPLING_RATE
            fits_movabs_hdu(fptr, 1, NULL, &status); 
            CHECK_STATUS_BREAK(status);
            // and read full Primary HDU and store it in 'headerPrimary'
            int numberkeywords;
            char *headerPrimary;
            fits_hdr2str(fptr, 0, NULL, 0,&headerPrimary, &numberkeywords, &status); 
            CHECK_STATUS_BREAK(status);
            
            // Pointer to where the text "sample_rate=" is in HISTORY block
            double sampling_rate = -999.0;
            char * sample_rate_pointer;
            sample_rate_pointer = strstr (headerPrimary,"sample_rate=");    
            if(!sample_rate_pointer)
            {
                // read it from xml file
                sampling_rate = sf;
            }
            else
            {
                // Pointer to the next character to "sample_rate=" (12 characters)   
                sample_rate_pointer = sample_rate_pointer + 12; 
                char each_character_after_srate[125];		
                snprintf(each_character_after_srate,125,"%c",*sample_rate_pointer);
                
                char characters_after_srate[125];
                snprintf(characters_after_srate,125,"%c",*sample_rate_pointer);
                
                while (*sample_rate_pointer != ' ')
                {
                    sample_rate_pointer = sample_rate_pointer + 1;
                    snprintf(each_character_after_srate,125,"%c",*sample_rate_pointer);
                    strcat(characters_after_srate,each_character_after_srate); 
                }
                
                sampling_rate = atof(characters_after_srate)-1;
            }
            
            // Get RECORD LENGTH from TRIGGERPARAM
            long reclen_key;
            fits_movnam_hdu(fptr, ANY_HDU,"TRIGGERPARAM", 0, &status);
            CHECK_STATUS_BREAK(status);
            fits_read_key(fptr,TLONG,"RECLEN", &reclen_key,NULL,&status);
            
            // Write missing/required keys to TESRECORDS
            fits_movnam_hdu(fptr, ANY_HDU,"TESRECORDS", 0, &status);
            CHECK_STATUS_BREAK(status);
            
            // Read DELTA_T keyword from "TESRECORDS" HDU
            double delta_t_key;            
            fits_read_key(fptr,TDOUBLE,"DELTA_T", &delta_t_key,NULL,&status);  
            div = sf/(1/delta_t_key);  // Grading i//strcpy(firstchar2,firstchar);nfo is unique in XML file -> adjust for different sf
            // Read NAXIS2 keyword from "TESRECORDS" HDU
            long naxis2_key;
            fits_read_key(fptr,TLONG,"NAXIS2", &naxis2_key,NULL,&status);  
            //long nettot_key_long;
            //nettot_key_long = naxis2_key*2;
            // Write TRIGGSZ, DELTAT & NETTOT keywords in "TESRECORDS" HDU
            fits_write_key(fptr,TULONG,"TRIGGSZ",&reclen_key,NULL,&status);
            double keyvalue_double;
            //keyvalue_double = delta_t_key * decimate_factor;
            keyvalue_double = 1./sampling_rate;
            fits_write_key(fptr,TDOUBLE,"DELTAT",&keyvalue_double,NULL,&status);
            //fits_update_key(fptr,TLONG,"NETTOT", &nettot_key_long,NULL,&status);
            fits_update_key(fptr,TLONG,"NETTOT", &naxis2_key,NULL,&status);
        }
    } //if hdunum==8 (xifusim file)
    fits_close_file(fptr,&status);
    CHECK_STATUS_BREAK(status);
    
    // Sixt standard keywords structure
    //----------------------------------
    SixtStdKeywords* keywords = newSixtStdKeywords(&status);
    CHECK_STATUS_BREAK(status);
    
    //Open outfile
    //------------
    TesEventFile * outfile = opennewTesEventFile(par.TesEventFile,
                                                 keywords,
                                                 par.clobber,
                                                 &status);
    CHECK_STATUS_BREAK(status);
    
    // Initialize PP data structures needed for pulse filtering
    //---------------------------------------------------------
    ReconstructInit* reconstruct_init = newReconstructInit(&status);
    CHECK_STATUS_BREAK(status);
    
    // Initialize SIRENA data structures needed for pulse filtering
    //-------------------------------------------------------------
    ReconstructInitSIRENA* reconstruct_init_sirena = newReconstructInitSIRENA(&status);
    CHECK_STATUS_BREAK(status);
    PulsesCollection* pulsesAll = newPulsesCollection(&status);
    CHECK_STATUS_BREAK(status);  
    OptimalFilterSIRENA* optimalFilter = newOptimalFilterSIRENA(&status);
    CHECK_STATUS_BREAK(status);// define a second structure for calibration
    
    // Read the grading data from the XML file and store it in 'reconstruct_init_sirena->grading'
    reconstruct_init_sirena->grading = NULL;
    reconstruct_init_sirena->grading = (Grading*)malloc(sizeof(Grading));
    
    reconstruct_init_sirena->grading->ngrades = 0;
    reconstruct_init_sirena->grading->value  = NULL;
    reconstruct_init_sirena->grading->gradeData = NULL;
    
    if (det->pix->grades == NULL)
    {
        SIXT_ERROR("The provided XMLFile does not have the grading info");
        return(EXIT_FAILURE);
    }
    //div = sf/(1/record_file->delta_t);  // Grading info is unique in XML file -> adjust for different sf
    reconstruct_init_sirena->grading->ngrades=det->pix->ngrades;
    //reconstruct_init_sirena->grading->value = gsl_vector_alloc(det->pix->ngrades);
    reconstruct_init_sirena->grading->gradeData = gsl_matrix_alloc(det->pix->ngrades,2);
    for (int i=0;i<det->pix->ngrades;i++)
    {
        //gsl_vector_set(reconstruct_init_sirena->grading->value,i,det->pix->(int) (grades[i].value/div));
        gsl_matrix_set(reconstruct_init_sirena->grading->gradeData,i,0,(int) (det->pix->grades[i].gradelim_pre)/div);
        gsl_matrix_set(reconstruct_init_sirena->grading->gradeData,i,1,(int) (det->pix->grades[i].gradelim_post)/div);
    }
    destroyAdvDet(&det);
    
    // Build up TesEventList to recover the results of the reconstruction
    TesEventList* event_list = newTesEventList(&status);
    allocateTesEventListTrigger(event_list,par.EventListSize,&status);
    CHECK_STATUS_BREAK(status);
            
    TesTriggerFile* record_file;
    TesRecord* record;
    int lastRecord = 0, nrecord = 0, nrecord_filei = 0;    //last record required for SIRENA library creation
    
    if (strcmp(firstchar2,"@") == 0)
    {
            FILE *filetxt = fopen(strndup(par.RecordFile+1, strlen(par.RecordFile)-1), "r");
            if (status != 0)    printf("%s","FITS file read from ASCII file does not exist\n");
            CHECK_STATUS_BREAK(status);  
            
            char filefits[256];
        
            for (int j=0;j<numfits;j++)   // For every FITS file
            {
                    fgets(filefits, 256, filetxt);
                    strtok(filefits, "\n");     // To delete '/n' from filefits (if not, 'fits_open_file' can not open the file)
                    //printf("%s %s %s","FITS file i: ",filefits,"\n");
            
                    // Open record file
                    // ----------------
                    //TesTriggerFile* record_file = openexistingTesTriggerFile(filefits,keywords,&status);
                    record_file = openexistingTesTriggerFile(filefits,keywords,&status);
                    CHECK_STATUS_BREAK(status);
                    
                    if(!strcmp(par.Rcmethod,"PP"))
                    {
                            initializeReconstruction(reconstruct_init,par.OptimalFilterFile,par.PulseLength,
                                                    par.PulseTemplateFile,par.Threshold,par.Calfac,par.NormalExclusion,
                                                    par.DerivateExclusion,par.SaturationValue,&status);
                    }
                    else
                    {
                            initializeReconstructionSIRENA(reconstruct_init_sirena, par.RecordFile, record_file->fptr, 
                                                    par.LibraryFile, par.TesEventFile, par.PulseLength, par.scaleFactor, par.samplesUp, 
                                                    par.samplesDown, par.nSgms, par.detectSP, par.opmode, par.detectionMode, par.LrsT, 
                                                    par.LbT, par.NoiseFile, par.FilterDomain, par.FilterMethod, par.EnergyMethod, 
                                                    par.filtEev, par.OFNoise, par.LagsOrNot, par.nLags, par.Fitting35, par.OFIter, 
                                                    par.OFLib, par.OFInterp, par.OFStrategy, par.OFLength, par.monoenergy, 
                                                    par.hduPRECALWN, par.hduPRCLOFWM, par.largeFilter, par.intermediate, par.detectFile, 
                                                    par.filterFile, par.clobber, par.EventListSize, par.SaturationValue, par.tstartPulse1, 
                                                    par.tstartPulse2, par.tstartPulse3, par.energyPCA1, par.energyPCA2, par.XMLFile, &status);
                    }  
                    CHECK_STATUS_BREAK(status);
                    
                    // Build up TesRecord to read the file
                    //TesRecord* record = newTesRecord(&status);
                    record = newTesRecord(&status);
                    allocateTesRecord(record,record_file->trigger_size,record_file->delta_t,0,&status);
                    CHECK_STATUS_BREAK(status);
                    
                    // Iterate of records and do the reconstruction
                    //int lastRecord = 0, nrecord = 0;    //last record required for SIRENA library creation
                    nrecord_filei = 0;
                    while(getNextRecord(record_file,record,&status))
                    {
                            if(!strcmp(par.Rcmethod,"PP"))
                            {
                                    reconstructRecord(record,event_list,reconstruct_init,0,&status);
                            }
                            else
                            {
                                    nrecord = nrecord + 1;
                                    nrecord_filei = nrecord_filei + 1;
                                    if ((nrecord_filei == record_file->nrows) && (j == numfits-1)) lastRecord=1;  // lastRecord of all the FITS files
                                    //if(nrecord == record_file->nrows) lastRecord=1;
                                   
                                    if ((strcmp(par.EnergyMethod,"I2R") == 0) || (strcmp(par.EnergyMethod,"I2RALL") == 0) 
                                        || (strcmp(par.EnergyMethod,"I2RNOL") == 0) || (strcmp(par.EnergyMethod,"I2RFITTED") == 0))
                                    {
                                            strcpy(reconstruct_init_sirena->EnergyMethod,par.EnergyMethod);
                                    }
                                
                                    //printf("%s %d %s","**TESRECONSTRUCTION nrecord = ",nrecord,"\n");
                                    reconstructRecordSIRENA(record,event_list,reconstruct_init_sirena,
                                                            lastRecord, nrecord, &pulsesAll, &optimalFilter, &status);
                            }
                            CHECK_STATUS_BREAK(status);

                            if ((strcmp(par.EnergyMethod,"PCA") != 0) || ((strcmp(par.EnergyMethod,"PCA") == 0) && lastRecord == 1))
                            {
                                    // In THREADING mode, saveEventListToFile is not called until finishing with calculus 
                                    // (ordering is necessary previously)  
                                    if(!is_threading()){    
                                            //printf("\n %p - %f", outfile, record_file->delta_t);
                                            //printf("\nRecord single");
                                            //printf("\n%f - %ld", record->time, record->pixid);
                                            saveEventListToFile(outfile,event_list,record->time,record_file->delta_t,record->pixid,&status);
                                            CHECK_STATUS_BREAK(status);
                                            //Reinitialize event list
                                            event_list->index=0;//////////////////////!!!!!!!!!!!!!!!!!!!!!OJO!!!!!!!!!!!!!!!!!!! Igual no hay que inicializarlo a 0
                                    }
                                    else
                                            printf("%s","Not prepared to run in THREADING mode with a input ASCII file (with several FITS files)\n");
                            }
                    } // while getNextRecord
                    
                    if ((!strcmp(par.Rcmethod,"SIRENA")) && (pulsesAll->ndetpulses == 0)) 
                            printf("%s %s %s","WARNING: no pulses have been detected in the current FITS file: ", filefits,"\n");
                    
                    if (numfits == 0)
                    {
                            // Copy trigger keywords to event file
                            copyTriggerKeywords(record_file->fptr,outfile->fptr,&status);
                            CHECK_STATUS_BREAK(status);
                            
                            // Messages providing info of some columns
                            //char keyword[9];
                            char keywordvalue[9];
                            char comment[MAXMSG];
                            //int keywordvalueint;
                            
                            fits_movnam_hdu(outfile->fptr, ANY_HDU,"EVENTS", 0, &status);
                            CHECK_STATUS_BREAK(status);
                            
                            fits_read_key(outfile->fptr, TSTRING, "TTYPE1", &keywordvalue, NULL, &status);
                            strcpy(comment, "Starting time");
                            fits_update_key(outfile->fptr, TSTRING, "TTYPE1", keywordvalue, comment, &status);
                            
                            fits_read_key(outfile->fptr, TSTRING, "TTYPE2", &keywordvalue, NULL, &status);
                            strcpy(comment, "Reconstructed-uncalibrated energy");
                            fits_update_key(outfile->fptr, TSTRING, "TTYPE2", keywordvalue, comment, &status);      
                            
                            fits_read_key(outfile->fptr, TSTRING, "TTYPE3", &keywordvalue, NULL, &status);
                            strcpy(comment, "Average first 4 samples (derivative)");
                            fits_update_key(outfile->fptr, TSTRING, "TTYPE3", keywordvalue, comment, &status);      
                            
                            fits_read_key(outfile->fptr, TSTRING, "TTYPE4", &keywordvalue, NULL, &status);
                            strcpy(comment, "Optimal filter length");
                            fits_update_key(outfile->fptr, TSTRING, "TTYPE4", keywordvalue, comment, &status);      
                            
                            fits_read_key(outfile->fptr, TSTRING, "TTYPE5", &keywordvalue, NULL, &status);
                            strcpy(comment, "Starting time-starting time previous event");
                            fits_update_key(outfile->fptr, TSTRING, "TTYPE5", keywordvalue, comment, &status);
                    }
                    
                    
                    freeTesTriggerFile(&record_file,&status);   // The record_file (every FITS file) is closed
                    
                    CHECK_STATUS_BREAK(status);
            
            }   // for every FITS file
            
            fclose(filetxt);
    }
    else
    {
            // Open record file
            // ----------------
            //TesTriggerFile* record_file = openexistingTesTriggerFile(par.RecordFile,keywords,&status);
            record_file = openexistingTesTriggerFile(par.RecordFile,keywords,&status);
            CHECK_STATUS_BREAK(status);

            if(!strcmp(par.Rcmethod,"PP")){
                initializeReconstruction(reconstruct_init,par.OptimalFilterFile,par.PulseLength,
                        par.PulseTemplateFile,par.Threshold,par.Calfac,par.NormalExclusion,
                        par.DerivateExclusion,par.SaturationValue,&status);
            }else{
                initializeReconstructionSIRENA(reconstruct_init_sirena, par.RecordFile, record_file->fptr, 
                        par.LibraryFile, par.TesEventFile, par.PulseLength, par.scaleFactor, par.samplesUp, 
                        par.samplesDown, par.nSgms, par.detectSP, par.opmode, par.detectionMode, par.LrsT, 
                        par.LbT, par.NoiseFile, par.FilterDomain, par.FilterMethod, par.EnergyMethod, 
                        par.filtEev, par.OFNoise, par.LagsOrNot, par.nLags, par.Fitting35, par.OFIter, 
                        par.OFLib, par.OFInterp, par.OFStrategy, par.OFLength, par.monoenergy, 
                        par.hduPRECALWN, par.hduPRCLOFWM, par.largeFilter, par.intermediate, par.detectFile, 
                        par.filterFile, par.clobber, par.EventListSize, par.SaturationValue, par.tstartPulse1, 
                        par.tstartPulse2, par.tstartPulse3, par.energyPCA1, par.energyPCA2, par.XMLFile, &status);
            }  
            CHECK_STATUS_BREAK(status);
            
            // Build up TesRecord to read the file
            //TesRecord* record = newTesRecord(&status);
            record = newTesRecord(&status);
            allocateTesRecord(record,record_file->trigger_size,record_file->delta_t,0,&status);
            CHECK_STATUS_BREAK(status);

            /*// Build up TesEventList to recover the results of the reconstruction
            TesEventList* event_list = newTesEventList(&status);
            allocateTesEventListTrigger(event_list,par.EventListSize,&status);
            CHECK_STATUS_BREAK(status);*/

            // Iterate of records and do the averageRecord
            //printf("%s %s", "averageRecord:","\n");
            //int lastRecord = 0, nrecord = 0;
            int nrecordOK = 0;
            TesTriggerFile* record_fileAux1 = openexistingTesTriggerFile(par.RecordFile,keywords,&status);
            gsl_vector * averageRecord = gsl_vector_alloc(record_fileAux1->trigger_size);
            gsl_vector_set_zero(averageRecord);
            CHECK_STATUS_BREAK(status);
            while(getNextRecord(record_fileAux1,record,&status))
            {
                nrecord = nrecord + 1;
                if(nrecord == record_file->nrows) lastRecord=1;
                
                //calculateAverageRecord(record,lastRecord,nrecord,&averageRecord,&status);
                calculateAverageRecord(record,lastRecord,&nrecordOK,&averageRecord,&status);
            }
            CHECK_STATUS_BREAK(status);
            freeTesTriggerFile(&record_fileAux1,&status);
            //printf("%s %d %s","recordsOK = ",nrecordOK,"\n");
            
            TesTriggerFile* record_fileAux2 = openexistingTesTriggerFile(par.RecordFile,keywords,&status);
            CHECK_STATUS_BREAK(status);
            nrecord = 0;
            while(getNextRecord(record_fileAux2,record,&status))
            {
                nrecord = nrecord + 1;
                calculateRecordsError(record,nrecord,averageRecord,&status);
            }
            CHECK_STATUS_BREAK(status);
            freeTesTriggerFile(&record_fileAux2,&status);
            
            // Iterate of records and do the reconstruction
            lastRecord = 0, nrecord = 0;    //last record required for SIRENA library creation
            while(getNextRecord(record_file,record,&status))
            {
                    if(!strcmp(par.Rcmethod,"PP"))
                    {
                            reconstructRecord(record,event_list,reconstruct_init,0,&status);
                    }
                    else
                    {
                            nrecord = nrecord + 1;
                            if(nrecord == record_file->nrows) lastRecord=1;
                            //if(nrecord < 10) 
                            //{
                            //  continue;
                            //}
                            //else if(nrecord > 10)
                            //{
                            //  status=1;
                            //  CHECK_STATUS_BREAK(status);
                            //}
                            //if(nrecord > 9)
                            //{
                            //	status=1;
                            //    CHECK_STATUS_BREAK(status);
                            //}
                            if ((strcmp(par.EnergyMethod,"I2R") == 0) || (strcmp(par.EnergyMethod,"I2RALL") == 0) 
                                || (strcmp(par.EnergyMethod,"I2RNOL") == 0) || (strcmp(par.EnergyMethod,"I2RFITTED") == 0))
                            {
                                strcpy(reconstruct_init_sirena->EnergyMethod,par.EnergyMethod);
                            }
                        
                            //printf("%s %d %s","**TESRECONSTRUCTION nrecord = ",nrecord,"\n");
                            reconstructRecordSIRENA(record,event_list,reconstruct_init_sirena,
                                                    lastRecord, nrecord, &pulsesAll, &optimalFilter, &status);
                    }
                    CHECK_STATUS_BREAK(status);

                    if ((strcmp(par.EnergyMethod,"PCA") != 0) || ((strcmp(par.EnergyMethod,"PCA") == 0) && lastRecord == 1))
                    {
                            // In THREADING mode, saveEventListToFile is not called until finishing with calculus 
                            // (ordering is necessary previously)  
                            if(!is_threading()){    
                                    //printf("\n %p - %f", outfile, record_file->delta_t);
                                    //printf("\nRecord single");
                                    //printf("\n%f - %ld", record->time, record->pixid);
                                    saveEventListToFile(outfile,event_list,record->time,record_file->delta_t,record->pixid,&status);
                                    CHECK_STATUS_BREAK(status);
                                    //Reinitialize event list
                                    event_list->index=0;
                            }
                    }
            }
            
            if(is_threading()) 
            {
                    th_end(&reconstruct_init_sirena, &pulsesAll, &optimalFilter);
                    int i = 1;
                    int aux = 1;
                    while((aux = th_get_event_list(&event_list, &record)) == 1)
                    {
                            saveEventListToFile(outfile,event_list,record->time,record_file->delta_t,record->pixid,&status);
                            CHECK_STATUS_BREAK(status);
                            ++i;
                    }
            }
            
            if ((!strcmp(par.Rcmethod,"SIRENA")) && (pulsesAll->ndetpulses == 0)) 
            printf("%s","WARNING: no pulses have been detected\n");
            
            // Copy trigger keywords to event file
            copyTriggerKeywords(record_file->fptr,outfile->fptr,&status);
            CHECK_STATUS_BREAK(status);
            
            // Messages providing info of some columns
            //char keyword[9];
            char keywordvalue[9];
            char comment[MAXMSG];
            //int keywordvalueint;
            
            fits_movnam_hdu(outfile->fptr, ANY_HDU,"EVENTS", 0, &status);
            CHECK_STATUS_BREAK(status);
            
            fits_read_key(outfile->fptr, TSTRING, "TTYPE1", &keywordvalue, NULL, &status);
            strcpy(comment, "Starting time");
            fits_update_key(outfile->fptr, TSTRING, "TTYPE1", keywordvalue, comment, &status);
            
            fits_read_key(outfile->fptr, TSTRING, "TTYPE2", &keywordvalue, NULL, &status);
            strcpy(comment, "Reconstructed-uncalibrated energy");
            fits_update_key(outfile->fptr, TSTRING, "TTYPE2", keywordvalue, comment, &status);      
            
            fits_read_key(outfile->fptr, TSTRING, "TTYPE3", &keywordvalue, NULL, &status);
            strcpy(comment, "Average first 4 samples (derivative)");
            fits_update_key(outfile->fptr, TSTRING, "TTYPE3", keywordvalue, comment, &status);      
            
            fits_read_key(outfile->fptr, TSTRING, "TTYPE4", &keywordvalue, NULL, &status);
            strcpy(comment, "Optimal filter length");
            fits_update_key(outfile->fptr, TSTRING, "TTYPE4", keywordvalue, comment, &status);      
            
            fits_read_key(outfile->fptr, TSTRING, "TTYPE5", &keywordvalue, NULL, &status);
            strcpy(comment, "Starting time-starting time previous event");
            fits_update_key(outfile->fptr, TSTRING, "TTYPE5", keywordvalue, comment, &status);
            
            freeTesTriggerFile(&record_file,&status);
    }
    
    // Save GTI extension to event file
    GTI* gti=getGTIFromFileOrContinuous("none",keywords->tstart, keywords->tstop,keywords->mjdref, &status);
    saveGTIExt(outfile->fptr, "STDGTI", gti, &status);    
    CHECK_STATUS_BREAK(status);
    
    //Free memory
    freeReconstructInit(reconstruct_init);
    freeReconstructInitSIRENA(reconstruct_init_sirena);
    freePulsesCollection(pulsesAll);
    freeOptimalFilterSIRENA(optimalFilter);
    //freeTesTriggerFile(&record_file,&status);
    freeTesEventFile(outfile,&status);
    freeTesEventList(event_list);
    freeTesRecord(&record);
    freeSixtStdKeywords(keywords);
    CHECK_STATUS_BREAK(status);
 
  } while(0); // END of the error handling loop.
  
  if (EXIT_SUCCESS==status) 
  {
	headas_chat(3, "finished successfully!\n\n");
	return(EXIT_SUCCESS);
  } 
  else 
  {
	return(status);
  }
}

int getpar(struct Parameters* const par)
{
  // String input buffer.
  char* sbuffer=NULL;

  // Error status.
  int status=EXIT_SUCCESS;

  status=ape_trad_query_string("Rcmethod", &sbuffer);
  if (EXIT_SUCCESS!=status) {
	  SIXT_ERROR("failed reading the reconstruction method");
	  return(status);
  }
  strcpy(par->Rcmethod, sbuffer);
  free(sbuffer);

  status=ape_trad_query_string("RecordFile", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the optimal filter file");
    return(status);
  }
  strcpy(par->RecordFile, sbuffer);
  free(sbuffer);

  status=ape_trad_query_string("TesEventFile", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the event file");
    return(status);
  }
  strcpy(par->TesEventFile, sbuffer);
  free(sbuffer);

  status=ape_trad_query_int("PulseLength", &par->PulseLength);
  if (EXIT_SUCCESS!=status) {
	  SIXT_ERROR("failed reading the PulseLength parameter");
	  return(status);
  }
  assert(par->PulseLength > 0);

  status=ape_trad_query_int("EventListSize", &par->EventListSize);
  if (EXIT_SUCCESS!=status) {
	  SIXT_ERROR("failed reading the EventListSize parameter");
	  return(status);
  }

  status=ape_trad_query_bool("clobber", &par->clobber);
  if (EXIT_SUCCESS!=status) {
	  SIXT_ERROR("failed reading the clobber parameter");
	  return(status);
  }

  status=ape_trad_query_bool("history", &par->history);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the history parameter");
    return(status);
  }

  status=ape_trad_query_double("SaturationValue", &par->SaturationValue);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the SaturationValue parameter");
    return(status);
  }

  if(strcmp(par->Rcmethod,"PP")==0){
	// PP  reconstruction method
	status=ape_trad_query_string("OptimalFilterFile", &sbuffer);
	if (EXIT_SUCCESS!=status) {
		SIXT_ERROR("failed reading the name of the optimal filter file");
		return(status);
	}
	strcpy(par->OptimalFilterFile, sbuffer);
	free(sbuffer);

	status=ape_trad_query_string("PulseTemplateFile", &sbuffer);
	if (EXIT_SUCCESS!=status) {
		SIXT_ERROR("failed reading the name of the pulse template file");
		return(status);
	}
	strcpy(par->PulseTemplateFile, sbuffer);
	free(sbuffer);

	status=ape_trad_query_double("Threshold", &par->Threshold);
	if (EXIT_SUCCESS!=status) {
		SIXT_ERROR("failed reading the Threshold parameter");
		return(status);
	}

	status=ape_trad_query_double("Calfac", &par->Calfac);
	if (EXIT_SUCCESS!=status) {
		SIXT_ERROR("failed reading the Calfac parameter");
		return(status);
	}

	status=ape_trad_query_int("NormalExclusion", &par->NormalExclusion);
	if (EXIT_SUCCESS!=status) {
		SIXT_ERROR("failed reading the NormalExclusion parameter");
		return(status);
	}

	status=ape_trad_query_int("DerivateExclusion", &par->DerivateExclusion);
	if (EXIT_SUCCESS!=status) {
		SIXT_ERROR("failed reading the DerivateExclusion parameter");
		return(status);
	}
  }else if(strcmp(par->Rcmethod,"SIRENA")==0){
	
	// SIRENA parameters
	status=ape_trad_query_string("LibraryFile", &sbuffer);
	strcpy(par->LibraryFile, sbuffer);
	free(sbuffer);

	status=ape_trad_query_double("scaleFactor", &par->scaleFactor);
    
	status=ape_trad_query_double("samplesUp", &par->samplesUp);
        
        status=ape_trad_query_double("samplesDown", &par->samplesDown);
  
	status=ape_trad_query_double("nSgms", &par->nSgms);
        
        status=ape_trad_query_int("detectSP", &par->detectSP);
  
	status=ape_trad_query_int("opmode", &par->opmode);
        
        status=ape_trad_query_string("detectionMode", &sbuffer);
	strcpy(par->detectionMode, sbuffer);
	free(sbuffer);
  
	status=ape_trad_query_double("LrsT", &par->LrsT);

	status=ape_trad_query_double("LbT", &par->LbT);

	status=ape_trad_query_int("intermediate", &par->intermediate);

	status=ape_trad_query_string("detectFile", &sbuffer);
	strcpy(par->detectFile, sbuffer);
	free(sbuffer);

	status=ape_trad_query_string("filterFile", &sbuffer);
	strcpy(par->filterFile, sbuffer);
	free(sbuffer);

	status=ape_trad_query_double("monoenergy", &par->monoenergy);
	
	status=ape_trad_query_bool("hduPRECALWN", &par->hduPRECALWN);
	status=ape_trad_query_bool("hduPRCLOFWM", &par->hduPRCLOFWM);
	
	status=ape_trad_query_int("largeFilter", &par->largeFilter);

	status=ape_trad_query_string("NoiseFile", &sbuffer);
	strcpy(par->NoiseFile, sbuffer);
	free(sbuffer);
	
	status=ape_trad_query_string("FilterDomain", &sbuffer);
	strcpy(par->FilterDomain, sbuffer);
	free(sbuffer);
	
	status=ape_trad_query_string("FilterMethod", &sbuffer);
	strcpy(par->FilterMethod, sbuffer);
	free(sbuffer);

	status=ape_trad_query_string("EnergyMethod", &sbuffer);
	strcpy(par->EnergyMethod, sbuffer);
	free(sbuffer);
        
        status=ape_trad_query_double("filtEev", &par->filtEev);

	status=ape_trad_query_string("OFNoise", &sbuffer);
	strcpy(par->OFNoise, sbuffer);
	free(sbuffer);
	
	status=ape_trad_query_int("LagsOrNot", &par->LagsOrNot);
        status=ape_trad_query_int("nLags", &par->nLags);
        status=ape_trad_query_int("Fitting35", &par->Fitting35);

	status=ape_trad_query_int("OFIter", &par->OFIter);

	status=ape_trad_query_bool("OFLib", &par->OFLib);
	
	strcpy(par->OFInterp, "DAB");
	
	status=ape_trad_query_string("OFStrategy", &sbuffer);
	strcpy(par->OFStrategy, sbuffer);
	free(sbuffer);
	
	status=ape_trad_query_int("OFLength", &par->OFLength);

	status=ape_trad_query_int("tstartPulse1", &par->tstartPulse1);
	
	status=ape_trad_query_int("tstartPulse2", &par->tstartPulse2);
	
	status=ape_trad_query_int("tstartPulse3", &par->tstartPulse3);
	
	status=ape_trad_query_double("energyPCA1", &par->energyPCA1);
	
	status=ape_trad_query_double("energyPCA2", &par->energyPCA2);
	
	status=ape_trad_query_string("XMLFile", &sbuffer);
	strcpy(par->XMLFile, sbuffer);
	free(sbuffer);
	
	if (EXIT_SUCCESS!=status) {
		SIXT_ERROR("failed reading some SIRENA parameter");
		return(status);
	}
	
	MyAssert((par->opmode == 0) || (par->opmode == 1), "opmode must be 0 or 1");
	  
	MyAssert((par->intermediate == 0) || (par->intermediate == 1), "intermediate must be 0 or 1");
	
        if (par->opmode == 0) MyAssert(par->monoenergy > 0, "monoenergy must be greater than 0");
	
	MyAssert((strcmp(par->FilterDomain,"T") == 0) || (strcmp(par->FilterDomain,"F") == 0), "FilterDomain must be T or F");
	
	MyAssert((strcmp(par->FilterMethod,"F0") == 0) || (strcmp(par->FilterMethod,"B0") == 0),"FilterMethod must be F0 or B0");
	
	MyAssert((strcmp(par->EnergyMethod,"OPTFILT") == 0) || (strcmp(par->EnergyMethod,"WEIGHT") == 0) || (strcmp(par->EnergyMethod,"WEIGHTN") == 0) ||
		(strcmp(par->EnergyMethod,"I2R") == 0) || (strcmp(par->EnergyMethod,"I2RALL") == 0) || (strcmp(par->EnergyMethod,"I2RNOL") == 0) || 
		(strcmp(par->EnergyMethod,"I2RFITTED") == 0) || (strcmp(par->EnergyMethod,"PCA") == 0), "EnergyMethod must be OPTFILT, WEIGHT, WEIGHTN, I2R, I2RALL, I2RNOL, I2RFITTED or PCA");
	
	MyAssert((strcmp(par->OFNoise,"NSD") == 0) || (strcmp(par->OFNoise,"WEIGHTM") == 0), "OFNoise must be NSD or WEIGHTM");
        
        MyAssert((strcmp(par->detectionMode,"AD") == 0) || (strcmp(par->detectionMode,"STC") == 0), "detectionMode must be AD or STC");
	
	MyAssert((par->LagsOrNot ==0) || (par->LagsOrNot ==1), "LagsOrNot must me 0 or 1");
        if ((par->nLags)%2 == 0)
	{
		SIXT_ERROR("parameter error: nLags must be odd");
		return(EXIT_FAILURE);
	}
	MyAssert((par->Fitting35 ==3) || (par->Fitting35 ==5), "Fitting35 must me 3 or 5");
        if ((par->Fitting35 ==3) && (par->nLags<3))
        {
                SIXT_ERROR("parameter error: nLags must be at least 3");
		return(EXIT_FAILURE);
        }
        if ((par->Fitting35 ==5) && (par->nLags<5))
        {
                SIXT_ERROR("parameter error: nLags must be at least 5");
		return(EXIT_FAILURE);
        }

	if (((strcmp(par->EnergyMethod,"WEIGHT") == 0) || (strcmp(par->EnergyMethod,"WEIGHTN") == 0)) && (par->LagsOrNot == 1))
	{
		SIXT_ERROR("parameter error: EnergyMethod=WEIGHT/WEIGHTN and Lags not implemented yet");
		return(EXIT_FAILURE);
	}
	
	MyAssert((par->OFIter ==0) || (par->OFIter ==1), "OFIter must be 0 or 1");
	
	if ((par->OFLib == 1) && (strcmp(par->FilterMethod,"F0") != 0))
	{
		SIXT_ERROR("parameter error: If OFLib=yes => FilterMethod must be F0");
		return(EXIT_FAILURE);
	}
	if ((strcmp(par->EnergyMethod,"WEIGHT") == 0) && (par->OFLib == 1))
	{
		SIXT_ERROR("parameter error: EnergyMethod=WEIGHT => OFLib should be 'no'");
		return(EXIT_FAILURE);
	}
	
	if ((strcmp(par->EnergyMethod,"OPTFILT") == 0) && (strcmp(par->OFNoise,"WEIGHTM") == 0) && (par->OFLib == 0))
	{
		SIXT_ERROR("parameter error: EnergyMethod=OPTFILT && OFNoise=WEIGHTM => OFLib should be 'yes'");
		return(EXIT_FAILURE);
	}
	
	MyAssert((strcmp(par->OFStrategy,"FREE") == 0) || (strcmp(par->OFStrategy,"BASE2") == 0) || (strcmp(par->OFStrategy,"BYGRADE") == 0) || (strcmp(par->OFStrategy,"FIXED") == 0), 
		 "OFStrategy must be FREE, BASE2, BYGRADE or FIXED");
	
        MyAssert(par->OFLength > 0, "OFLength must be greater than 0");
	
	MyAssert(par->energyPCA1 > 0, "energyPCA1 must be greater than 0");
        MyAssert(par->energyPCA2 > 0, "energyPCA2 must be greater than 0");
	
  } else {
	SIXT_ERROR("failed reading the Rcmethod parameter");
	return(EXIT_FAILURE);
  }
  return(status);
}

void MyAssert(int expr, char* msg)
{
    if (expr == 0)
    {
        printf("%s %s %s"," Assertion failure: ",msg,"\n");
        abort();
    }
}
