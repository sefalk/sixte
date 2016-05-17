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


   Copyright 2016 Philippe Peille, IRAP; Thomas Dauser, ECAP
*/

#include "crosstalk.h"

int num_imod = 6;
char* imod_xt_names[] = { "|f2 - f1|", " f2 + f1 ","2*f2 - f1","2*f1 - f2", "2*f2 + f1", "2*f1 + f2" };
int counting_imod_xt[] = {0, 0, 0, 0, 0, 0 };


/** Calculates distance between two pixels */
static double distance_two_pixels(AdvPix* pix1,AdvPix*pix2){
	// Note: this assumes no pixel overlap (this was ensured at detector loading stage)

	// pix1 is on the right of pix2
	if(pix1->sx>pix2->sx + .5*pix2->width){
		// pix1 is above pix2
		if (pix1->sy > pix2->sy + .5*pix2->height){
			// distance is distance between bottom left corner of pix1 and top right corner of pix2
			return sqrt(pow(pix1->sx - .5*pix1->width - (pix2->sx + .5*pix2->width),2) +
					pow(pix1->sy - .5*pix1->height - (pix2->sy + .5*pix2->height),2));
		}
		// pix1 is below pix2
		if (pix1->sy < pix2->sy - .5*pix2->height){
			// distance is distance between top left corner of pix1 and bottom right corner of pix2
			return sqrt(pow(pix1->sx - .5*pix1->width - (pix2->sx + .5*pix2->width),2) +
					pow(pix1->sy + .5*pix1->height - (pix2->sy - .5*pix2->height),2));
		}
		// pix1 is at the same level as pix2
		// distance is distance between left edge of pix1 and right edge of pix2
		return pix1->sx - .5*pix1->width - (pix2->sx + .5*pix2->width);
	}

	// pix1 is on the left of pix2
	if(pix1->sx<pix2->sx - .5*pix2->width){
		// pix1 is above pix2
		if (pix1->sy > pix2->sy + .5*pix2->height){
			// distance is distance between bottom right corner of pix1 and top left corner of pix2
			return sqrt(pow(pix1->sx + .5*pix1->width - (pix2->sx - .5*pix2->width),2) +
					pow(pix1->sy - .5*pix1->height - (pix2->sy + .5*pix2->height),2));
		}
		// pix1 is below pix2
		if (pix1->sy < pix2->sy - .5*pix2->height){
			// distance is distance between top right corner of pix1 and bottom left corner of pix2
			return sqrt(pow(pix1->sx + .5*pix1->width - (pix2->sx - .5*pix2->width),2) +
					pow(pix1->sy + .5*pix1->height - (pix2->sy - .5*pix2->height),2));
		}
		// pix1 is at the same level as pix2
		// distance is distance between right edge of pix1 and left edge of pix2
		return (pix2->sx - .5*pix2->width) - (pix1->sx + .5*pix1->width);

	}

	// pix1 is at the same left/right position as pix2

	// pix1 is above pix2
	if (pix1->sy > pix2->sy){
		// distance is distance between bottom edge of pix1 and top edge of pix2
		return (pix1->sy - .5*pix1->height - (pix2->sy + .5*pix2->height));
	}
	// pix1 is below pix2
	// distance is distance between top edge of pix1 and bottom edge of pix2
	return (pix2->sy - .5*pix2->height - (pix1->sy + .5*pix1->height));
}

/** Adds a cross talk pixel to the matrix */
static void add_xt_pixel(MatrixCrossTalk** matrix,AdvPix* pixel,double xt_weigth,int* const status){
	CHECK_STATUS_VOID(*status);

	// Allocate matrix if necessary
	if(*matrix==NULL){
		*matrix = newMatrixCrossTalk(status);
		CHECK_MALLOC_VOID_STATUS(*matrix,*status);
	}

	// Increase matrix size
	(*matrix)->cross_talk_pixels = realloc((*matrix)->cross_talk_pixels,((*matrix)->num_cross_talk_pixels+1)*sizeof(*((*matrix)->cross_talk_pixels)));
	CHECK_MALLOC_VOID_STATUS((*matrix)->cross_talk_pixels,*status);
	(*matrix)->cross_talk_weights = realloc((*matrix)->cross_talk_weights,((*matrix)->num_cross_talk_pixels+1)*sizeof(*((*matrix)->cross_talk_weights)));
	CHECK_MALLOC_VOID_STATUS((*matrix)->cross_talk_weights,*status);

	// Affect new values
	(*matrix)->cross_talk_pixels[(*matrix)->num_cross_talk_pixels] = pixel;
	(*matrix)->cross_talk_weights[(*matrix)->num_cross_talk_pixels] = xt_weigth;

	// Now, we can say that the matrix is effectively bigger
	(*matrix)->num_cross_talk_pixels++;
}

// Loads thermal cross-talk for requested pixel
// Concretely, iterates over all the pixels to find neighbours
static void load_thermal_cross_talk(AdvDet* det,int pixid,int* const status){
	double max_cross_talk_dist = det->xt_dist_thermal[det->xt_num_thermal-1];
	AdvPix* concerned_pixel = &(det->pix[pixid]);
	AdvPix* current_pixel = NULL;
	for (int i=0;i<det->npix;i++){
		// Cross-talk is not with yourself ;)
		if (i==pixid){
			continue;
		}
		current_pixel = &(det->pix[i]);

		// Initial quick distance check to avoid spending time on useless pixels
		if((fabs(current_pixel->sx-concerned_pixel->sx) > .5*(current_pixel->width+concerned_pixel->width)+max_cross_talk_dist) ||
				(fabs(current_pixel->sy-concerned_pixel->sy) > .5*(current_pixel->height+concerned_pixel->height)+max_cross_talk_dist)){
			continue;
		}

		// Get distance between two pixels
		double pixel_distance = distance_two_pixels(current_pixel,concerned_pixel);
		if (pixel_distance<0){ // distance should be positive
			*status = EXIT_FAILURE;
			printf("*** error: Distance between pixels %d and %d is negative\n",current_pixel->pindex,concerned_pixel->pindex);
			return;
		}
		//printf("%d - %d : %f\n",pixid,i,pixel_distance*1e6);

		// Iterate over cross-talk values and look for the first matching one
		for (int xt_index=0;xt_index<det->xt_num_thermal;xt_index++){
			if (pixel_distance<det->xt_dist_thermal[xt_index]){
				add_xt_pixel(&concerned_pixel->thermal_cross_talk,current_pixel,det->xt_weight_thermal[xt_index],status);
				CHECK_STATUS_VOID(*status);
				// If one cross talk was identified, go to next pixel (the future cross-talks should be lower order cases)
				break;
			}
		}
	}
}

// Loads electrical cross-talk for requested pixel
// Concretely, iterates over all the pixels of the channel
static void load_electrical_cross_talk(AdvDet* det,int pixid,int* const status){
	CHECK_STATUS_VOID(*status);
	if (det->elec_xt_par==NULL){
		*status = EXIT_FAILURE;
		SIXT_ERROR("Tried to load electrical crosstalk with no corresponding information available at detector level");
		return;
	}

	AdvPix* concerned_pixel = &(det->pix[pixid]);
	AdvPix* current_pixel = NULL;
	double weight=0.;


	// Iterate over the channel
	for (int i=0;i<concerned_pixel->channel->num_pixels;i++){
		current_pixel = concerned_pixel->channel->pixels[i];

		// Cross-talk is not with yourself ;)
		if (current_pixel==concerned_pixel) continue;

		// Carrier overlap
		weight = pow(det->elec_xt_par->R0/(4*M_PI*(concerned_pixel->freq - current_pixel->freq)*det->elec_xt_par->Lfprim),2);

		// Common impedence
		weight+=pow(concerned_pixel->freq*det->elec_xt_par->Lcommon/(2*(concerned_pixel->freq - current_pixel->freq)*det->elec_xt_par->Lfsec),2);

		// Add cross-talk pixel
		add_xt_pixel(&concerned_pixel->electrical_cross_talk,current_pixel,weight,status);
		CHECK_STATUS_VOID(*status);

	}

}

/** Load crosstalk timedependence information */
static void load_crosstalk_timedep(AdvDet* det,int* const status){
	if(det->crosstalk_timedep_file==NULL){
		*status=EXIT_FAILURE;
		SIXT_ERROR("Tried to load crosstalk without timedependence information");
		return;
	}

	char fullfilename[MAXFILENAME];
	strcpy(fullfilename,det->filepath);
	strcat(fullfilename,det->crosstalk_timedep_file);

	// open the file
	FILE *file=NULL;
	file=fopen(fullfilename, "r");
	if (file == NULL){
		printf("*** error reading file %s \n",fullfilename);
		*status=EXIT_FAILURE;
		return;
	}

	// initialize crosstalk_timedep structure
	det->crosstalk_timedep = newCrossTalkTimedep(status);
	CHECK_STATUS_VOID(*status);

	double* time=NULL;
	double* weight=NULL;

	// Ignore first line
	char buffer[1000];
	if(fgets(buffer, 1000, file)==NULL){
		printf(" *** error: reading of the time dependence crosstalk file %s failed\n",fullfilename);
		*status=EXIT_FAILURE;
		return;
	}

	// Read time dep info
	while(!feof(file)){
		time = realloc(det->crosstalk_timedep->time, (det->crosstalk_timedep->length+1)*sizeof(double));
		weight = realloc(det->crosstalk_timedep->weight, (det->crosstalk_timedep->length+1)*sizeof(double));

		if ((time==NULL)||(weight==NULL)){
			freeCrosstalkTimedep(&det->crosstalk_timedep);
			SIXT_ERROR("error when reallocating arrays in crosstalk timedep loading");
			*status=EXIT_FAILURE;
			return;
		} else {
			det->crosstalk_timedep->time = time;
			det->crosstalk_timedep->weight = weight;
		}
		// check that always all three numbers are matched
		if ( (fscanf(file, "%lg %lg\n",&(det->crosstalk_timedep->time[det->crosstalk_timedep->length]),&(det->crosstalk_timedep->weight[det->crosstalk_timedep->length]))) != 2){
			printf("*** formatting error in line %i of the channel file %s: check your input file\n",det->crosstalk_timedep->length+1,fullfilename);
			*status=EXIT_FAILURE;
			return;
		}
		// printf("reading channel list (line %i): %i %i %lf\n",n+1,chans->pixid[n],chans->chan[n],chans->freq[n]);
		det->crosstalk_timedep->length++;
	}
	fclose(file);

}

static void initImodTab(ImodTab** tab, int n_ampl, int n_dt, int n_freq,
		double* ampl, double* dt, double* freq, int* status){

	(*tab) = (ImodTab*) malloc (sizeof (ImodTab));
	CHECK_MALLOC_VOID_STATUS(tab,*status);

	// make a short pointer
	ImodTab* t = (*tab);

	t->n_ampl = n_ampl;
	t->n_dt   = n_dt;
	t->n_freq = n_freq;

	t->ampl = (double*) malloc(n_ampl * sizeof(double));
	CHECK_MALLOC_VOID_STATUS(t->ampl,*status);
	for (int ii=0; ii<n_ampl; ii++){
		t->ampl[ii] = ampl[ii];
	}


	t->dt = (double*) malloc(n_dt * sizeof(double));
	CHECK_MALLOC_VOID_STATUS(t->dt,*status);

	for (int ii=0; ii<n_dt; ii++){
		t->dt[ii] = dt[ii];
	}

	t->freq = (double*) malloc(n_freq * sizeof(double));
	CHECK_MALLOC_VOID_STATUS(t->freq,*status);

	for (int ii=0; ii<n_freq; ii++){
		t->freq[ii] = freq[ii];
	}


	// allocate the 4d matrix (n_freq x n_dt x n_ampl x nampl)
	t->matrix = (double****) malloc (n_freq*sizeof(double***));
	CHECK_MALLOC_VOID_STATUS(t->matrix,*status);

	for (int ll=0; ll<n_freq; ll++){               // FREQ-LOOP
		t->matrix[ll] = (double***) malloc (n_dt*sizeof(double**));
		CHECK_MALLOC_VOID_STATUS(t->matrix[ll],*status);

		for (int ii=0; ii<n_dt; ii++){             // DT-LOOP
			t->matrix[ll][ii] = (double**) malloc (n_ampl*sizeof(double*));
			CHECK_MALLOC_VOID_STATUS(t->matrix[ll][ii],*status);

			for (int jj=0; jj<n_ampl; jj++){      // AMPL1-LOOP
				t->matrix[ll][ii][jj] = (double*) malloc (n_ampl*sizeof(double));
				CHECK_MALLOC_VOID_STATUS(t->matrix[ll][ii][jj],*status);

				for (int kk=0; kk<n_ampl; kk++){  // AMPL2-LOOP
					t->matrix[ll][ii][jj][kk] = 0.0;
				}
			}
		}
	}
	return;
}

static void get_imodtable_axis(int* nrows, double** val, char* extname, char* colname, fitsfile* fptr, int* status){

	int extver = 0;
	fits_movnam_hdu(fptr, BINARY_TBL, extname, extver ,status);
	if (*status!=EXIT_SUCCESS){
		printf(" *** error moving to extension %s\n",extname);
		return;
	}

	// get the column id-number
	int colnum;
	if(fits_get_colnum(fptr, CASEINSEN, colname, &colnum, status)) return;

	// get the number of rows
	long n;
	if (fits_get_num_rows(fptr, &n, status)) return;

	// allocate memory for the array
	*val=(double*)malloc(n*sizeof(double));
	CHECK_MALLOC_VOID_STATUS(*val,*status);

    int anynul=0;
    double nullval=0.0;
    LONGLONG nelem = (LONGLONG) n;
    fits_read_col(fptr, TDOUBLE, colnum, 1, 1, nelem ,&nullval,*val, &anynul, status);

	(*nrows) = (int) n;

	return;
}

/** read one intermodulation matrix from a fits file and store it in the structure */
static void read_intermod_matrix(fitsfile* fptr,int n_ampl, int n_dt, int n_freq,
		ImodTab* tab, char* extname, int* status){

	// ampl and dt arrays should be initialized before
	assert(tab->ampl!=NULL);
	assert(tab->dt!=NULL);
	assert(tab->freq!=NULL);

	// move to the extension containing the data
	int extver = 0;
//	if (fits_movnam_hdu(fptr, BINARY_TBL, extname, extver ,status)){
	if (fits_movnam_hdu(fptr, IMAGE_HDU, extname, extver ,status)){
		printf(" error: moving to extension %s failed \n",extname);
		return;
	}

	// get number of rows
//	int colnum;
//	if(fits_get_colnum(fptr, CASEINSEN,colname, &colnum, status)) return;
	int naxis;
	if (fits_get_img_dim(fptr, &naxis, status) && (naxis!=4)){
		printf(" error: getting dimensions of intermodulation data array failed \n");
		return;
	}
	long naxes[naxis];


	if (fits_get_img_size(fptr, naxis, naxes, status) ){
		printf(" error: getting dimensions of intermodulation data array failed \n");
		return;
	}
	// check dimensions
	if (naxes[0]!=n_freq || naxes[1]!=n_dt || naxes[2]!=n_ampl || naxes[3]!=n_ampl ){
		*status=EXIT_FAILURE;
		printf(" error: wrong dimensions of the intermodulation data array [%ld %ld %ld %ld] \n",
				naxes[0],naxes[1],naxes[2],naxes[3]);
	}



	// actually read the table now
	int anynul=0;
	double* buf;
	buf = (double*)malloc(n_freq*n_dt*n_ampl*n_ampl*sizeof(double));
	CHECK_MALLOC_VOID(buf);

	double nullval=0.0;
	long nelem = n_freq*n_dt*n_ampl*n_ampl;
	long fpixel[naxis];
	for (int ii=0;ii<naxis;ii++){
		fpixel[ii]=1;
	}

	fits_read_pix(fptr, TDOUBLE, fpixel,nelem, &nullval,buf, &anynul, status);
	fits_report_error(stdout, *status);
	CHECK_STATUS_VOID(*status);

	printf(" start reading the matrix \n");
	for (int ii=0; ii<n_freq ; ii++){  // freq loop
		for (int jj=0; jj<n_dt ; jj++){
			for (int kk=0; kk<n_ampl ; kk++){
				for (int ll=0; ll<n_ampl ; ll++){
					int ind = 	(( ll*n_ampl + kk ) * n_dt + jj ) * n_freq + ii;
					assert(ind < n_dt*n_ampl*n_ampl*n_freq);
					tab->matrix[ii][jj][kk][ll] = buf[ind];
				}
			}
		}
	} // ------------------------  //  end (freq loop)

	free(buf);
	free(fpixel);
	free(naxes);

	return;
}

static void print_imod_matrix(ImodTab* tab){

	FILE * fp;

	fp = fopen ("output_tab.txt", "w+");

	for (int ll=0; ll<tab->n_ampl ; ll++){
		for (int kk=0; kk<tab->n_ampl ; kk++){
			for (int jj=0; jj<tab->n_dt ; jj++){
				for (int ii=0; ii<tab->n_freq ; ii++){  // freq loop
					fprintf(fp, "%i \t %i \t %i \t %i \t %e \t %e \t %e \n",
							ii,jj,kk,ll,tab->freq[ii],tab->dt[jj],
							tab->matrix[ii][jj][kk][ll]);

				}
			}
		}
	} // ------------------------  //  end (freq loop)

	fclose(fp);

}

/** load the intermodulation frequency table */
static void load_intermod_freq_table(AdvDet* det, int* status){

	// check if the table exists
	CHECK_NULL_VOID(det->crosstalk_intermod_file,*status,"no file for the intermodulation table specified");

	char* EXTNAME_AMPLITUDE = "amplitude";
	char* EXTNAME_TIMEOFFSET = "time_offset";
	char* EXTNAME_FREQOFFSET = "frequency_offset";
	char* COLNAME_AMPLITUDE = "AMPLITUDE";
	char* COLNAME_TIMEOFFSET = "DT_SEC";
	char* COLNAME_FREQOFFSET = "D_FREQ";
	char* EXTNAME_CROSSTALK = "crosstalk";
//	char* COLNAME_CROSSTALK = "CROSSTALK";


	fitsfile *fptr=NULL;

	do {

		char fullfilename[MAXFILENAME];
		strcpy(fullfilename,det->filepath);
		strcat(fullfilename,det->crosstalk_intermod_file);

		// open the file
		if (fits_open_table(&fptr, fullfilename, READONLY, status)) break;
		headas_chat(5, "\n[crosstalk] reading the intermodulation table %s \n",fullfilename);

		// read the extensions specifying the axes of the 3d matrix
		int n_ampl,n_dt,n_freq;
		double* ampl;
		double* dt;
		double* freq;
		get_imodtable_axis(&n_ampl,&ampl,EXTNAME_AMPLITUDE,COLNAME_AMPLITUDE,fptr,status);
		CHECK_STATUS_BREAK(*status);
		get_imodtable_axis(&n_dt,&dt,EXTNAME_TIMEOFFSET,COLNAME_TIMEOFFSET,fptr,status);
		CHECK_STATUS_BREAK(*status);
		get_imodtable_axis(&n_freq,&freq,EXTNAME_FREQOFFSET,COLNAME_FREQOFFSET,fptr,status);
		CHECK_STATUS_BREAK(*status);

		// initialize the intermodulation table
		initImodTab(&(det->crosstalk_intermod_table), n_ampl, n_dt, n_freq, ampl, dt, freq, status);
		if (*status!=EXIT_SUCCESS){
			SIXT_ERROR("initializing intermodulation table in memory failed");
			break;
		}

		read_intermod_matrix(fptr,n_ampl,n_dt, n_freq, det->crosstalk_intermod_table,
				EXTNAME_CROSSTALK,status);
		if (*status != EXIT_SUCCESS){
			printf(" *** error: reading intermodulation table %s  failed\n", fullfilename);
			break;
		}

		print_imod_matrix(det->crosstalk_intermod_table);

	} while(0); // END of Error handling loop

	if (fptr!=NULL) {fits_close_file(fptr,status);}

	return;
}

/** check if frequencies cause crosstalk */
static int check_frequencies(double f, double f1, double f2,double prec){

	prec= prec*1.001; // give a small error margin

	if   ( fabs( fabs(f1-f2) - f )     <= prec ){
		return 0;
	} else if  ( fabs( (f1+f2) - f ) <= prec ){
		return 1;
	} else if ( fabs( (2*f2-f1) - f )   <= prec ) {
		return 2;
	} else if ( fabs( (2*f1-f2) - f )   <= prec ) {
		return 3;
	} else if ( fabs( (2*f2+f1) - f )   <= prec ) {
		return 4;
	} else if ( fabs( (2*f1+f2) - f )   <= prec ) {
		return 5;
	} else {
		return -1;
	}

}

static void add_to_intermod_channel_matrix(AdvDet* det, AdvPix* pix_inp, AdvPix* pix1, AdvPix* pix_xt, int freq_num, int* status){
//static IntermodulationCrossTalk* add_to_intermod_channel_matrix(AdvDet* det, AdvPix* pix, AdvPix* pix1, AdvPix* pix2, int freq_num, int* status){


	// Important: Needs to exactly fit to the output of freq_num
/*	ImodTab* itab[]  = {
			(det->crosstalk_intermod_table->w_f2mf1),
			(det->crosstalk_intermod_table->w_f2pf1),
			(det->crosstalk_intermod_table->w_2f2mf1),
			(det->crosstalk_intermod_table->w_2f1mf2),
			(det->crosstalk_intermod_table->w_2f2pf1),
			(det->crosstalk_intermod_table->w_2f1pf2)
	}; */
	ImodTab **itab=NULL;


	if (pix_inp->intermodulation_cross_talk==NULL){
		pix_inp->intermodulation_cross_talk = newImodCrossTalk(status);
		CHECK_STATUS_VOID(*status);
	}

	IntermodulationCrossTalk** imod_xt = &(pix_inp->intermodulation_cross_talk);


	// Increase matrix size
	(*imod_xt)->cross_talk_pixels = (AdvPix***) realloc((*imod_xt)->cross_talk_pixels,((*imod_xt)->num_cross_talk_pixels+1)*sizeof(AdvPix**));
	CHECK_MALLOC_VOID_STATUS((*imod_xt)->cross_talk_pixels,*status);

	// crosstalk produced by two pixels
	(*imod_xt)->cross_talk_pixels[(*imod_xt)->num_cross_talk_pixels] = NULL;
	(*imod_xt)->cross_talk_pixels[(*imod_xt)->num_cross_talk_pixels] =
			(AdvPix**) realloc((*imod_xt)->cross_talk_pixels[(*imod_xt)->num_cross_talk_pixels],(2*sizeof(AdvPix*)));
	CHECK_MALLOC_VOID_STATUS((*imod_xt)->cross_talk_pixels[(*imod_xt)->num_cross_talk_pixels],*status);

	(*imod_xt)->cross_talk_info = (ImodTab**) realloc((*imod_xt)->cross_talk_info,((*imod_xt)->num_cross_talk_pixels+1)*sizeof(ImodTab*));
	CHECK_MALLOC_VOID_STATUS((*imod_xt)->cross_talk_pixels,*status);


	// Affect new values
	(*imod_xt)->cross_talk_pixels[(*imod_xt)->num_cross_talk_pixels][0] = pix1;
	(*imod_xt)->cross_talk_pixels[(*imod_xt)->num_cross_talk_pixels][1] = pix_xt;
	(*imod_xt)->cross_talk_info[(*imod_xt)->num_cross_talk_pixels] = itab[freq_num];

	// Now, we can say that the matrix is effectively bigger
	(*imod_xt)->num_cross_talk_pixels++;

	// check that now the intermodulation table does exist
	assert(pix_inp->intermodulation_cross_talk != NULL);
}

/** set for one pixels the pixel-combinations for the intermodulation crosstalk */
static void set_advpix_intermod_cross_talk(AdvDet* det, AdvPix* pix, int* status){

	double f1 = 0.0;
	double f_xt = 0.0;

	int nchanpix = pix->channel->num_pixels;


	/** loop over the all pixels in the channel */
	for (int ii=0; ii< nchanpix; ii++){
		f1 = pix->channel->pixels[ii]->freq;

		// do not allow intermodulation cross talk with the pixel itself
		if (fabs(f1-pix->freq) > 1e-3){

			// starting at jj=ii+1 removes the diagonal and duplicated entries
			for (int jj=0; jj< nchanpix; jj++){

				f_xt = pix->channel->pixels[jj]->freq;

				// check if input-freq and f1 can be somehow combined to produce f_xt
				double df_band = det->readout_channels->df_information_band[pix->channel->channel_id-1];  // channelID starts at 1
				int freq_num = check_frequencies(f_xt,pix->freq, f1,df_band);

				if (freq_num >= 0){
					add_to_intermod_channel_matrix(det,pix,pix->channel->pixels[ii],pix->channel->pixels[jj],freq_num,status);
					CHECK_STATUS_VOID(*status);

					// count each pair only once (definition to match Roland's files)
					if (f1 > pix->freq){
						counting_imod_xt[freq_num]++;
						headas_chat(7,"Pixel %03i is influenced by (%03i %03i) -> crosstalk type %i (%.3f %.3f -> %.3f) \n",
							pix->channel->pixels[jj]->pindex+1, pix->pindex+1,pix->channel->pixels[ii]->pindex+1,freq_num,
							pix->freq*1e-3,pix->channel->pixels[ii]->freq*1e-3,pix->channel->pixels[jj]->freq*1e-3);
					}

				}
			} // ----- end jj-loop

		}

	}  // ----- end ii-loop

}

static void get_sorted_list(double* freq, int nfreq){

	for (int jj=0; jj<nfreq; jj++){
		for (int kk=jj+1; kk<nfreq; kk++){
			if (freq[jj]>freq[kk]){
				double dummy=freq[jj];
				freq[jj]=freq[kk];
				freq[kk]=dummy;
			}
		}
	}


}

/**static double get_min_carrier_spacing(double* freq, int nfreq){

	get_sorted_list(freq,nfreq);

	double retval=-1.0;
	double val;
	for (int ii=0;ii<nfreq-1;ii++){
		val = freq[ii+1]-freq[ii];
		if (ii>0){
			retval = MIN(retval,val);
		} else {
			retval = val;
		}
	}
	return retval;
} */

/** get the information bandwidth form the carrier spacing in each channel */
/**static void set_df_information_band(ReadoutChannels* ro, int* status){

	// make sure the information is loaded in the table
	if (ro==NULL){
		printf(" ** error: readout channels have to be set before calculating the information band\n");
		*status=EXIT_FAILURE;
		return;
	}

	double* freq=NULL;
	int nfreq=0;

	headas_chat(5,"\n[crosstalk] determining the bandwidth of a carrier:\n");

	for (int ii=0; ii<ro->num_channels;ii++){

		nfreq = ro->channels[ii].num_pixels;
		freq = (double*) realloc(freq,nfreq*sizeof(double));
		CHECK_MALLOC_VOID(freq);

		for (int jj=0; jj<nfreq ; jj++){
			freq[jj]=ro->channels[ii].pixels[jj]->freq;
		}

		// Roland's worst case assumption
		ro->df_information_band[ii] = get_min_carrier_spacing(freq,nfreq)/6.0;

		if (ro->df_information_band[ii] <= 0){
			printf(" ** error: information band width could not be determined. Check your list of frequencies. \n");
			*status=EXIT_FAILURE;
			return ;
		}

		headas_chat(5," - chan %02i = %.3f [kHz] \n",
				ro->channels[ii].channel_id,ro->df_information_band[ii]*1e-3);

	}

	return;
} */

/** Load intermodulation cross talk information into a single AdvPix*/
static void load_intermod_cross_talk(AdvDet* det, int pixid, int* status){


	/** we only need to set the intermod for the pixel, if it wasn't already done */
	if (det->pix[pixid].intermodulation_cross_talk==NULL){

		/** make sure the information is loaded in the table */
		if (det->crosstalk_intermod_table==NULL){
			load_intermod_freq_table(det,status);
			CHECK_STATUS_VOID(*status);
		}

//		set_advpix_intermod_cross_talk(det, &(det->pix[pixid]), status);

	}

	return;

}

static void print_imod_statistics(AdvDet* det){

	int nchan = det->readout_channels->num_channels;

	headas_chat(5,"\n[crosstalk] statistics on the loaded Intermodulation Crosstalk:  \n" );
	for (int ii=0; ii<num_imod; ii++){
		headas_chat(3,"%s: %03i \n",imod_xt_names[ii],counting_imod_xt[ii]/nchan);
	}

}

static int doCrosstalk(int id, AdvDet* det){
	if ( (id == det->crosstalk_id) || det->crosstalk_id == CROSSTALK_ID_ALL){
		return 1;
	} else {
		return 0;
	}
}

void init_crosstalk(AdvDet* det, int* const status){
	// load time dependence
	load_crosstalk_timedep(det,status);
	CHECK_STATUS_VOID(*status);

	// load the channel list
	if (det->readout_channels==NULL){
		det->readout_channels = get_readout_channels(det,status);
		CHECK_STATUS_VOID(*status);
	}

	headas_chat(5,"\n[crosstalk] the modes are switched on (%i): \n",det->crosstalk_id);

	// load thermal cross talk
	if (det->xt_num_thermal>0 && doCrosstalk(CROSSTALK_ID_THERM,det)){
		headas_chat(5," - thermal crosstalk\n");
		for (int ii=0;ii<det->npix;ii++){
			load_thermal_cross_talk(det,ii,status);
			CHECK_STATUS_VOID(*status);
		}
	}

	// load electrical cross talk
	if (det->elec_xt_par!=NULL && doCrosstalk(CROSSTALK_ID_ELEC,det)){
		headas_chat(5," - electrical crosstalk\n");
		for (int ii=0;ii<det->npix;ii++){
			load_electrical_cross_talk(det,ii,status);
			CHECK_STATUS_VOID(*status);
		}
	}

	// load intermodulation cross talk
	if (det->crosstalk_intermod_file!=NULL && doCrosstalk(CROSSTALK_ID_IMOD,det)){
		headas_chat(5," - intermodulation crosstalk\n");
		// count the number of combinations: set everything to zero here
		for (int ii=0; ii<num_imod; ii++){
			counting_imod_xt[ii] = 0;
		}
		// loop through all pixels
		for (int ii=0;ii<det->npix;ii++){
			load_intermod_cross_talk(det,ii,status);
			CHECK_STATUS_VOID(*status);
		}
//		print_imod_statistics(det);
	}

}

static void add_pixel_to_readout(ReadoutChannels* read_chan, AdvPix* pix, int ic, int* status){

	// check if PixID already belongs to a Channel (duplicated naming not allowed)
	if (pix->channel != NULL){
		printf("*** error: Pixel with ID %i already belongs to a Channel (check channel_file)\n",pix->pindex);
		*status = EXIT_FAILURE;
		return;
	}

	if ( (ic <= 0) || ic > read_chan->num_channels ){
		printf("*** error: Channel with ID %i not a valid channel number\n",ic);
		*status = EXIT_FAILURE;
		return;
	}

	// add another pixel to the channel (ID starts at 1)
	// (note that we use here that realloc behaves like malloc for a NULL pointer)
	read_chan->channels[ic-1].pixels = (AdvPix**)
			realloc( read_chan->channels[ic-1].pixels,
					(read_chan->channels[ic-1].num_pixels+1) * sizeof(AdvPix*)
					);
	CHECK_MALLOC_VOID_STATUS(read_chan->channels[ic-1].pixels,*status);

	read_chan->channels[ic-1].pixels[read_chan->channels[ic-1].num_pixels] = pix;

	read_chan->channels[ic-1].num_pixels++;

	return;
}

static int get_num_chans(channel_list* cl, int* status){

	int num_chans = -1;

	for(int ii=0; ii<cl->len; ii++){
		num_chans = MAX(num_chans,cl->chan[ii]);
	}

	if (num_chans <= 0 ){
		CHECK_STATUS_RET(*status,0);
	}

	return num_chans;
}

static ReadoutChannels* init_newReadout(int num_chan, int* status){

	ReadoutChannels* read_chan = (ReadoutChannels*) malloc(sizeof (ReadoutChannels) );
	CHECK_MALLOC_RET_NULL_STATUS(read_chan,*status);

	read_chan->num_channels = num_chan;

	read_chan->channels = (Channel*) malloc (num_chan*sizeof(Channel));
	CHECK_MALLOC_RET_NULL_STATUS(read_chan->channels,*status);

	read_chan->df_information_band = (double*) malloc (num_chan*sizeof(double));
	CHECK_MALLOC_RET_NULL_STATUS(read_chan->df_information_band,*status);

	for (int ii=0; ii<num_chan; ii++){
		read_chan->channels[ii].channel_id = ii+1; // channel IDs go from 1 to num_chan
		read_chan->channels[ii].num_pixels = 0;
		read_chan->channels[ii].pixels = NULL;
		read_chan->df_information_band[ii] = 0.0;
	}

	return read_chan;
}

ReadoutChannels* get_readout_channels(AdvDet* det, int* status){

	// get the complete file name
	char fullfilename[MAXFILENAME];
	strcpy(fullfilename,det->filepath);
	strcat(fullfilename,det->channel_file);

	// get the channel list
	channel_list* chans = load_channel_list(fullfilename, status);
	CHECK_STATUS_RET(*status,NULL);

	// check if the file agrees with the number of pixels
	if (chans->len != det->npix){
		printf("*** error: number of pixels from channel_file %s (%i) is not equal total number of pixels (%i)!\n",
				fullfilename,chans->len,det->npix);
		*status=EXIT_FAILURE;
		return NULL;
	}


	int num_chan = get_num_chans(chans,status);
	CHECK_STATUS_RET(*status,NULL);
	ReadoutChannels* read_chan = init_newReadout(num_chan, status);

	// sort pixels in the channel array
	AdvPix* pix=NULL;
	for (int ii=0; ii < chans->len; ii++ ){

		// check if PixID makes sense (PixID starts at 0)
		if ( (chans->pixid[ii] < 0) || (chans->pixid[ii] > det->npix-1) ){
			printf("*** error: Pixel-ID %i does not belong to the detector \n",chans->pixid[ii]);
			*status = EXIT_FAILURE;
			return NULL;
		}

		pix = &(det->pix[chans->pixid[ii]]);

		// set frequency of the pixel
		pix->freq = chans->freq[ii];
		if (pix->freq <= 0.0){
			printf("*** warning: assiging unrealistic frequency (%.3e) to pixel %i\n",pix->freq,pix->pindex);
		}

		// assign pixel to readout channel
		add_pixel_to_readout(read_chan, pix, chans->chan[ii], status);
		// make sure the pixel knows its channel (Channel ID starts at 1)
		pix->channel = &(read_chan->channels[chans->chan[ii]-1]);

//		printf("Readout Channel: Assigne PixID %i to Channel %i with Freq %.3e\n",
//				pix->pindex,pix->channel->channel_id,pix->freq);
	}

	// free the channel list
	free_channel_list(&chans);

	// set the information band for each channel now
//	set_df_information_band(read_chan,status);

	return read_chan;
}

void free_channel_list(channel_list** chans){
	if (*(chans)!=NULL){
		free((*chans)->chan);
		free((*chans)->pixid);
		free((*chans)->freq);
		free(*chans);
	}
}

channel_list* load_channel_list(char* fname, int* status){

	channel_list* chans = (channel_list*)malloc(sizeof(channel_list));
	CHECK_MALLOC_RET_NULL(chans);

	// init parameters
	chans->len=0;
	chans->chan=NULL;
	chans->freq=NULL;
	chans->pixid=NULL;

	// open the file
	FILE *file=NULL;
	file=fopen(fname, "r");

	if (file == NULL){
		printf("*** error reading file %s \n",fname);
	 	 *status=EXIT_FAILURE;
	 	 return NULL;
	}

	int n=0;
	int* cha;
	int* pix;
	double* freq;
	while(!feof(file)){

	     pix = realloc(chans->pixid, (n+1)*sizeof(int));
	     cha = realloc(chans->chan,  (n+1)*sizeof(int));
	     freq = realloc(chans->freq,  (n+1)*sizeof(double));

	     if ((cha==NULL)||(pix==NULL)||(freq==NULL)){
	    	 	 free_channel_list(&chans);
	    	 	 SIXT_ERROR("error when reallocating arrays");
	    	 	 *status=EXIT_FAILURE;
	    	 	 return NULL;
	     } else {
	    	 chans->pixid = pix;
	    	 chans->chan  = cha;
	    	 chans->freq  = freq;
	     }
	     // check that always all three numbers are matched
	     if ( (fscanf(file, "%i %i %lf\n",&(chans->pixid[n]),&(chans->chan[n]),&(chans->freq[n]))) != 3){
	    	 printf("*** formatting error in line %i of the channel file %s: check your input file\n",n+1,fname);
	    	 *status=EXIT_FAILURE;
	    	 return NULL;
	     }
	      // printf("reading channel list (line %i): %i %i %lf\n",n+1,chans->pixid[n],chans->chan[n],chans->freq[n]);
	     n++;
	}
	fclose(file);

    chans->len = n;

	return chans;
}

/** Compute influence of the crosstalk event on an impact using the timedependence table */
int computeCrosstalkInfluence(AdvDet* det,PixImpact* impact,PixImpact* crosstalk,double* influence){
	assert(impact->pixID == crosstalk->pixID);
	// For the moment, the crosstalk event is supposed to happen afterwards, but this could be modified if needed
	double time_difference = crosstalk->time - impact->time;
	double energy_influence = 0.;
	// if impact is close enough to have an influence
	if ((time_difference>=0) && (time_difference<det->crosstalk_timedep->time[det->crosstalk_timedep->length-1])){
		// Binary search for to find interpolation interval
		int high=det->crosstalk_timedep->length-1;
		int low=0;
		int mid;
		while (high > low) {
			mid=(low+high)/2;
			if (det->crosstalk_timedep->time[mid] < time_difference) {
				low=mid+1;
			} else {
				high=mid;
			}
		}
		int timedep_index = low-1;
		assert(timedep_index<det->crosstalk_timedep->length-1);

		// influence previous impact
		energy_influence= crosstalk->energy*(det->crosstalk_timedep->weight[timedep_index]+
				(det->crosstalk_timedep->weight[timedep_index+1]-det->crosstalk_timedep->weight[timedep_index])/(det->crosstalk_timedep->time[timedep_index+1]-det->crosstalk_timedep->time[timedep_index])*(time_difference-det->crosstalk_timedep->time[timedep_index]));
		impact->energy+=energy_influence;
		*influence+=energy_influence;
		return 1;
	}
	return 0;
}