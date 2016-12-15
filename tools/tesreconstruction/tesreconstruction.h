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

#ifndef TESRECONSTRUCTION_H
#define TESRECONSTRUCTION_H 1

#include "optimalfilters.h"
#include "testriggerfile.h"
#include "teseventlist.h"
#include "tesproftemplates.h"
#include "testrigger.h"
#include "gti.h"
#include "integraSIRENA.h"

#define TOOLSUB tesreconstruction_main
#include "headas_main.c"

//typedef struct
//{
//	/** The grade values */
//        int value;
//
//	/** Size in samples before and after one pulse defining the grade */
//	long gradelim_pre;
//	long gradelim_post;
//
//} gradeData;

//typedef struct 
//{
//	// Number of grades 
//	int ngrades;
//
//	// Structure which contains the grading data
//	gradeData *gradeData;
//	
//} Grading;

struct Parameters {
	//File containing the optimal filter
	char OptimalFilterFile[MAXFILENAME];

	//File to reconstruct
	char RecordFile[MAXFILENAME];

	//Ouput event list
	char TesEventFile[MAXFILENAME];

	//File containing the pulse template
	char PulseTemplateFile[MAXFILENAME];

	//Pulse Length
	int PulseLength;

	//Threshold level
	double Threshold;

	//Calibration factor
	double Calfac;

	//Default size of the event list
	int EventListSize;

	//Minimal distance before using OFs after a mireconstruction
	int NormalExclusion;

	//Minimal distance before reconstructing any event after a mireconstruction
	int DerivateExclusion;

	//Saturation level of the ADC curves
	double SaturationValue;

	//Boolean to choose whether to erase an already existing event list
	char clobber;

	//Boolean to choose to save the run parameters in the output file
	char history;

	//Reconstruction Method (PP or SIRENA)
	char Rcmethod[7];
	
	//
	// SIRENA parameters
	//
	//File containing the library
	char LibraryFile[MAXFILENAME];
	
	//Scale Factor for initial filtering
	double scaleFactor;
	
	//Number of samples for threshold trespassing
	double samplesUp;
	
	//Number of standard deviations in the kappa-clipping process for threshold estimation
	double nSgms;

	//Calibration run (0) or energy reconstruction run (1)?
	int mode;

	/** Monochromatic energy for library creation **/
	double monoenergy;
	
	/** Length of the longest fixed filter for library creation **/
	int maxLengthFixedFilter;
	
	/** Running sum length for the RS raw energy estimation, in seconds (only in CALIBRATION) **/
	double LrsT;
	
	/** Baseline averaging length for the RS raw energy estimation, in seconds (only in CALIBRATION) **/
	double LbT;
	
	/** Baseline (in ADC units) **/
	double baseline;

	//Noise filename
	char NoiseFile[MAXFILENAME];
	
	//Pixel Type: SPA, LPA1, LPA2 or LPA3
	char PixelType[5];

	//Filtering Domain: Time(T) or Frequency(F)
	char FilterDomain[2];

	//Filtering Method: F0 (deleting the zero frequency bin) or F0 (deleting the baseline) **/
	char FilterMethod[3];
	
	//Energy Method: OPTFILT, WEIGHT, WEIGHTN, I2R, I2RBISALL, I2RBISNOL or PCA **/
	char EnergyMethod[8];

	//LagsOrNot: LAGS == 1 or NOLAGS == 0 **/
	int LagsOrNot;

	//OFIter: Iterate == 1 or NOTIterate == 0 **/
	int OFIter;

	//Boolean to choose whether to use a library with optimal filters or calculate the optimal filter to each pulse
	char OFLib;
	
	//Optimal Filter by using the Matched Filter (MF) or the DAB as matched filter (MF, DAB) **/
        char OFInterp[4];
	
	//Optimal Filter length Strategy: FREE, BASE2, BYGRADE or FIXED **/
	char OFStrategy[8];

	//Optimal Filter length (taken into account if OFStrategy=FIXED) **/
	int OFLength;
	
	//Write intermediate files (Yes:1, No:0)
	int intermediate;
	
	// File with the output detections 
	char detectFile[256];
	
	// File with the output filter (only in calibration)
	char filterFile[256];
	
	// Tstart of the pulses (to be used instead of calculating them if tstartPulse1 =! 0)
	int tstartPulse1;
	int tstartPulse2;
	int tstartPulse3;
	
	/** Energies for PCA **/
	double energyPCA1;
	double energyPCA2;
	
	// XML file with instrument definition
	char XMLFile[MAXFILENAME];

	// END SIRENA PARAMETERS
};

int getpar(struct Parameters* const par);

void MyAssert(int expr, char* msg);


#endif /* TESRECONSTRUCTION_H */
