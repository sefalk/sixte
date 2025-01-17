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


 Copyright 2007-2014 Christian Schmid, FAU
 Copyright 2015-2019 Remeis-Sternwarte, Friedrich-Alexander-Universitaet
 Erlangen-Nuernberg
 */

#include "pha2pilib.h"

Pha2Pi* getPha2Pi(int* const status) {
	// Allocate memory.
	Pha2Pi* p2p = (Pha2Pi*) malloc(sizeof(Pha2Pi));
	CHECK_NULL_RET(p2p, *status, "memory allocation for Pha2Pi failed", p2p);

	// Initialize.
	p2p->pha2pi_filename = NULL;
	p2p->pirmf_filename = NULL;
	p2p->specarf_filename = NULL;
	p2p->rmffile = NULL;
	p2p->seed = -1;
	p2p->nrows = 0;
	p2p->ngrades = 0;
	p2p->pha = NULL;
	p2p->pien = NULL;
	p2p->pilow = NULL;
	p2p->pihigh = NULL;
	p2p->nulval = -255;

	return (p2p);
}

void freePha2Pi(Pha2Pi** const p2p) {
	if (NULL != *p2p) {
		if (NULL != (*p2p)->pha2pi_filename) {
			free((*p2p)->pha2pi_filename);
		}
		if (NULL != (*p2p)->pirmf_filename) {
			free((*p2p)->pirmf_filename);
		}
		if (NULL != (*p2p)->specarf_filename) {
			free((*p2p)->specarf_filename);
		}
		if (NULL != (*p2p)->rmffile) {
			free((*p2p)->rmffile);
		}
		if (NULL != (*p2p)->pha) {
			free((*p2p)->pha);
		}
		if (NULL != (*p2p)->pien) {
			for (int ii = 0; ii < (*p2p)->nrows; ++ii) {
				free((*p2p)->pien[ii]);
			}
			free((*p2p)->pien);
		}
		if (NULL != (*p2p)->pilow) {
			for (int ii = 0; ii < (*p2p)->nrows; ++ii) {
				free((*p2p)->pilow[ii]);
			}
			free((*p2p)->pilow);
		}
		if (NULL != (*p2p)->pihigh) {
			for (int ii = 0; ii < (*p2p)->nrows; ++ii) {
				free((*p2p)->pihigh[ii]);
			}
			free((*p2p)->pihigh);
		}
		free(*p2p);
		*p2p = NULL;
	}

	sixt_destroy_rng();
}

Pha2Pi* initPha2Pi(const char* const filename, const char* const pirmf_filename,
		const char* const specarf_filename, const unsigned int seed,
		int* const status) {

	/** CHECK if Pha2Pi file is accessible */
	if (filename == NULL || strlen(filename) == 0) {
		headas_chat(5,
				"No Pha2Pi correction file given ... skipping correction!\n");
		return NULL;
	}
	if (access(filename, F_OK) != 0) {
		char msg[MAXMSG];
		sprintf(msg, "Pha2Pi correction File '%s' not accessible!", filename);
		SIXT_ERROR(msg);
		*status = EXIT_FAILURE;
		return NULL;
	}

	int colnum, typecode;
	long width;

	Pha2Pi* p2p = getPha2Pi(status);
	CHECK_STATUS_RET(*status, p2p);

	/** Store filename */
	p2p->pha2pi_filename = (char*) malloc(
			(strlen(filename) + 1) * sizeof(char));
	CHECK_NULL_RET(p2p->pha2pi_filename, *status,
			"memory allocation for p2p_filename failed", p2p);
	strcpy(p2p->pha2pi_filename, filename);

	/** INITIALIZE RANDOM NUMBER GENERATOR */
	p2p->seed = seed;
	sixt_init_rng(seed, status);
	CHECK_STATUS_RET(*status, p2p);

	/** LOAD FILE */
	headas_chat(3, "open Pha2Pi file '%s' ...\n", filename);
	fitsfile* fptr;
	fits_open_table(&fptr, filename, READONLY, status);
	CHECK_STATUS_RET(*status, p2p);

	// Read the RESPFILE keyword.
	char comment[MAXMSG];
	char rmffile[MAXMSG];
	fits_read_key(fptr, TSTRING, "RESPFILE", rmffile, comment, status);

	CHECK_STATUS_RET(*status, p2p);
	p2p->rmffile = (char*) malloc((strlen(rmffile) + 1) * sizeof(char));
	CHECK_NULL_RET(p2p->rmffile, *status,
			"memory allocation for p2p->rmffile failed", p2p);
	strcpy(p2p->rmffile, rmffile);

	// Check if PIRMF filename is given (defined in XML).
	if (pirmf_filename != NULL) {
		p2p->pirmf_filename = (char*) malloc(
				(strlen(pirmf_filename) + 1) * sizeof(char));
		CHECK_NULL_RET(p2p->pirmf_filename, *status,
				"memory allocation for p2p->pirmf_filename failed", p2p);
		strcpy(p2p->pirmf_filename, pirmf_filename);
	}
	// Check if SPECARF filename is given (defined in XML).
	if (specarf_filename != NULL) {
		p2p->specarf_filename = (char*) malloc(
				(strlen(specarf_filename) + 1) * sizeof(char));
		CHECK_NULL_RET(p2p->specarf_filename, *status,
				"memory allocation for p2p->specarf_filename failed", p2p);
		strcpy(p2p->specarf_filename, specarf_filename);
	}

	// Determine the number of rows.
	fits_get_num_rows(fptr, &p2p->nrows, status);

	// Load PHA column
	p2p->pha = (long *) malloc(p2p->nrows * sizeof(long));
	fits_get_colnum(fptr, CASEINSEN, "PHA", &colnum, status);
	fits_read_col(fptr, TINT32BIT, colnum, 1, 1, p2p->nrows, NULL, p2p->pha,
			&p2p->nulval, status);
	CHECK_STATUS_RET(*status, p2p);

	// CHECK CONSISTENCY: PHA has to start at zero and must be continues (assuming sorted PHAs)
	if (p2p->pha[0] != 0 || p2p->pha[p2p->nrows - 1] != p2p->nrows - 1) {
		char msg[MAXMSG];
		sprintf(msg,
				"Pha2Pi correction File is inconsistent! PHA values must be continues starting with 0!");
		SIXT_ERROR(msg);
		*status = EXIT_FAILURE;
		return NULL;
	}

	// Determine the number of grades element dimension.
	fits_get_colnum(fptr, CASEINSEN, "PIEN", &colnum, status);
	fits_get_coltype(fptr, colnum, &typecode, &p2p->ngrades, &width, status);
	CHECK_STATUS_RET(*status, p2p);

	// Load PIEN, PILOW, PIHIGH columns
	p2p->pien = (double **) malloc(p2p->nrows * sizeof(double*));
	for (int ii = 0; ii < p2p->nrows; ++ii) {
		p2p->pien[ii] = (double *) malloc(p2p->ngrades * sizeof(double));
		fits_read_col(fptr, TDOUBLE, colnum, ii + 1, 1, p2p->ngrades, NULL,
				p2p->pien[ii], &p2p->nulval, status);
		CHECK_STATUS_RET(*status, p2p);
	}
	fits_get_colnum(fptr, CASEINSEN, "PILOW", &colnum, status);
	p2p->pilow = (double **) malloc(p2p->nrows * sizeof(double*));
	for (int ii = 0; ii < p2p->nrows; ++ii) {
		p2p->pilow[ii] = (double *) malloc(p2p->ngrades * sizeof(double));
		fits_read_col(fptr, TDOUBLE, colnum, ii + 1, 1, p2p->ngrades, NULL,
				p2p->pilow[ii], &p2p->nulval, status);
		CHECK_STATUS_RET(*status, p2p);
	}
	fits_get_colnum(fptr, CASEINSEN, "PIHIGH", &colnum, status);
	p2p->pihigh = (double **) malloc(p2p->nrows * sizeof(double*));
	for (int ii = 0; ii < p2p->nrows; ++ii) {
		p2p->pihigh[ii] = (double *) malloc(p2p->ngrades * sizeof(double));
		fits_read_col(fptr, TDOUBLE, colnum, ii + 1, 1, p2p->ngrades, NULL,
				p2p->pihigh[ii], &p2p->nulval, status);
		CHECK_STATUS_RET(*status, p2p);
	}

	fits_close_file(fptr, status);

	return (p2p);
}

Pha2Pi* initPha2Pi_from_GenInst(GenInst* const inst, const unsigned int seed,
		int* const status) {

	// Initialize & load Pha2Pi File (NULL if not set)
	char pha2pi_filename[MAXFILENAME];
	if (inst->det->pha2pi_filename == NULL) {
		*pha2pi_filename = '\0';
	} else {
		strcpy(pha2pi_filename, inst->filepath);
		strcat(pha2pi_filename, inst->det->pha2pi_filename);
	}

	char pirmf_filename[MAXFILENAME];
	if (inst->det->pirmf_filename == NULL) {
		*pirmf_filename = '\0';
	} else {
		strcpy(pirmf_filename, inst->det->pirmf_filename);
	}

	char specarf_filename[MAXFILENAME];
	if (inst->det->specarf_filename == NULL) {
		*specarf_filename = '\0';
	} else {
		strcpy(specarf_filename, inst->det->specarf_filename);
	}

	// Pha2Pi correction file
	Pha2Pi* p2p = initPha2Pi(pha2pi_filename, pirmf_filename,specarf_filename, seed, status);

	return (p2p);
}

void pha2pi_correct_event(Event* const evt, const Pha2Pi* const p2p,
		const struct RMF* const rmf, int* const status) {

	// Do nothing if the Pha2Pi structure is uninitialized
	if (p2p == NULL) {
		return;
	}

	// Do nothing if event is invalid => pi = -1.
	if (evt->type == -1 || evt->pha < 0
			|| evt->pha > p2p->pha[p2p->nrows - 1]) {
		return;
	}
	// Make sure the requested evt->type is tabulated
	else if (evt->type < 0 || evt->type > p2p->ngrades) {
		char msg[MAXMSG];
		sprintf(msg, "Pha2Pi correction event type '%d' not tabulated!",
				evt->type);
		SIXT_WARNING(msg);
		return;
	}
	// Determine a PI value for the event's PHA value
	else {

		// Consistency check: Does index point to correct pha channel?
		if (evt->pha != p2p->pha[evt->pha]) {
			char msg[MAXMSG];
			sprintf(msg,
					"pha2pi: Event PHA (%ld) not equal to PHA (%ld) in row [%ld] in Pha2Pi file '%s' ... aborting!\n",
					evt->pha, p2p->pha[evt->pha], evt->pha,
					p2p->pha2pi_filename);
			SIXT_ERROR(msg);
			*status = EXIT_FAILURE;
			return;
		}

		// Find energy range [emin,emax] corresponding to event's pha
		const double emin = p2p->pilow[evt->pha][evt->type];
		const double emax = p2p->pihigh[evt->pha][evt->type];
		/* Consistency check:
		 * Pha2Pi File might contain NaN entries for requested
		 * PHA-TYPE combination -> Pha2Pi File is invalid!
		 * */
		if (isnan(emin) || isnan(emax)) {
			// IF this case occurs the Pha2Pi File is not valid!
			char msg[MAXMSG];
			sprintf(msg,
					"pha2pi: Pha2Pi file '%s' contains invalid NULL entries for PHA=%ld and TYPE=%d! Aborting ...\n",
					p2p->pha2pi_filename, evt->pha, evt->type);
			SIXT_ERROR(msg);
			*status = EXIT_FAILURE;
			// HOTFIX:
			//evt->pi = evt->pha;
			return;
		}
		// PI value in keV randomly picked within
		double ran = sixt_get_random_number(status);
		CHECK_STATUS_VOID(*status);
		const double pi = emin + ran * (emax - emin);

		// PI value in ADU based on given RMF's EBOUNDS
		evt->pi = getEBOUNDSChannel((float) pi, rmf);
	}
}

void pha2pi_correct_eventfile(EventFile* const evtfile, const Pha2Pi* const p2p,
		const char* RSPPath, const char* RESPfile, int* const status) {

	// Do nothing if the Pha2Pi structure is uninitialized
	if (p2p == NULL) {
		return;
	}

	headas_chat(3, "run pha2pi correction on event file ...\n");

	// Read the eventfile RESPFILE keyword.
	char comment[MAXMSG];
	char evtrmf[MAXMSG];
	fits_movnam_hdu(evtfile->fptr, BINARY_TBL, "EVENTS", 0, status);
	CHECK_STATUS_VOID(*status);
	fits_read_key(evtfile->fptr, TSTRING, "RESPFILE", &evtrmf, comment, status);
	CHECK_STATUS_VOID(*status);

	// CHECK if eventfile & Pha2Pi file were created with the same RMF
	if (strcmp(evtrmf, p2p->rmffile) != 0) {
		*status = EXIT_FAILURE;
		SIXT_ERROR(
				"RESPfile keyword of EventFile and Pha2Pi are different, but must be the same!");
		return;
	}

	// CHECK whether the user demands a different RMF:
	char respfile[MAXFILENAME];
	if (strlen(RESPfile) > 0) { // User demands a different rmf
		strcpy(respfile, RESPfile);
	} else {          	          // We use the same rmf as in the simulation
		strcpy(respfile, p2p->rmffile);
	}

	// We put the paths to the rmf into resppathname:
	char resppathname[2 * MAXFILENAME];
	if (strlen(RSPPath) > 0) {
		strcpy(resppathname, RSPPath);
		strcat(resppathname, "/");
		strcat(resppathname, respfile);
	} else {      // The file should be located in the working directory.
		strcpy(resppathname, respfile);
	}

	if (access(resppathname, F_OK) != 0) {
		char msg[MAXMSG];
		sprintf(msg, "RESPfile '%s' not accessible!", resppathname);
		SIXT_ERROR(msg);
		*status = EXIT_FAILURE;
		return;
	} else if (strcmp(respfile, p2p->rmffile) != 0) {
		char msg[MAXMSG];
		sprintf(msg, "pha2pi: Using RMF='%s' for PI binning instead of '%s'!\n",
				respfile, p2p->rmffile);
		SIXT_WARNING(msg);
	}

	// Load the EBOUNDS of the RMF that will be used in the pi correction.
	// Some fields, e.g., FirstChannel, will not be loaded!
	struct RMF* rmf = getRMF(status);
	loadEbounds(rmf, resppathname, status);
	CHECK_STATUS_VOID(*status);

	// Add 'PI' column to evtfile if necessary
	if (evtfile->cpi == 0) {
		evtfile->cpi = evtfile->cpha + 1;
		addCol2EventFile(evtfile, &evtfile->cpi, "PI", "J", "ADU", status);
		CHECK_STATUS_VOID(*status);
	}

	// Loop over all events in the input list.
	long ii;
	for (ii = 1; ii <= evtfile->nrows; ii++) {
		// Get event
		Event* event = getEvent(status);
		CHECK_STATUS_BREAK(*status);
		getEventFromFile(evtfile, ii, event, status);
		CHECK_STATUS_BREAK(*status);

		// run pi correction on event
		pha2pi_correct_event(event, p2p, rmf, status);

		// Save changes to eventfile
		updateEventInFile(evtfile, ii, event, status);
		CHECK_STATUS_BREAK(*status);

		// Release memory.
		freeEvent(&event);
	}
	if (*status == EXIT_SUCCESS) {
		fits_update_key(evtfile->fptr, TSTRING, "PHA2PI", p2p->pha2pi_filename,
				"Pha2Pi correction file", status);
		if (p2p->pirmf_filename != NULL && strlen(p2p->pirmf_filename) > 0) {
			fits_update_key(evtfile->fptr, TSTRING, "PIRMF",
					p2p->pirmf_filename, "PI-RMF needed for PI values", status);
		} else {
			headas_chat(5,
					" 'PIRMF' Key not written to eventfile as not given!\n");
		}
		if (p2p->specarf_filename != NULL
				&& strlen(p2p->specarf_filename) > 0) {
			fits_update_key(evtfile->fptr, TSTRING, "SPECARF",
					p2p->specarf_filename, "calibrated ARF for analysis",
					status);
		} else {
			headas_chat(5,
					" 'SPECARF' Key not written to eventfile as not given!\n");
		}
		headas_chat(5, " ... Pha2PI correction successful!\n");
		return;
	} else {
		char msg[MAXMSG];
		sprintf(msg,
				"*** ERROR occurred while Pha2Pi correcting using Pha2PiFile='%s'!",
				p2p->pha2pi_filename);
		SIXT_ERROR(msg);
		*status = EXIT_FAILURE;
		return;
	}
}
