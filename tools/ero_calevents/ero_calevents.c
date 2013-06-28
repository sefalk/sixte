#include "ero_calevents.h"


int ero_calevents_main() 
{
  // Containing all programm parameters read by PIL
  struct Parameters par; 

  // Input pattern file.
  PatternFile* plf=NULL;

  // File pointer to the output eROSITA event file. 
  fitsfile* fptr=NULL;

  // WCS data structure used for projection.
  struct wcsprm wcs={ .flag=-1 };
  // String buffer for FITS header.
  char* headerstr=NULL;

  GTI* gti=NULL;
  Attitude* ac=NULL;

  // Error status.
  int status=EXIT_SUCCESS; 


  // Register HEATOOL:
  set_toolname("ero_calevents");
  set_toolversion("0.11");


  do { // Beginning of the ERROR handling loop (will at most be run once).

    // --- Initialization ---

    headas_chat(3, "initialization ...\n");

    // Read parameters using PIL library:
    if ((status=getpar(&par))) break;

    // Check whether an appropriate WCS projection has been selected.
    if (strlen(par.Projection)!=3) {
      SIXT_ERROR("invalid WCS projection type");
      status=EXIT_FAILURE;
      break;
    }


    // Open the input pattern file.
    plf=openPatternFile(par.PatternList, READONLY, &status);
    CHECK_STATUS_BREAK(status);

    // Read keywords from the input file.
    char comment[MAXMSG];
    float timezero=0.0;
    fits_read_key(plf->fptr, TFLOAT, "TIMEZERO", &timezero, comment, &status);
    CHECK_STATUS_BREAK(status);

    char date_obs[MAXMSG];
    fits_read_key(plf->fptr, TSTRING, "DATE-OBS", date_obs, comment, &status);
    CHECK_STATUS_BREAK(status);

    char time_obs[MAXMSG];
    fits_read_key(plf->fptr, TSTRING, "TIME-OBS", time_obs, comment, &status);
    CHECK_STATUS_BREAK(status);

    char date_end[MAXMSG];
    fits_read_key(plf->fptr, TSTRING, "DATE-END", date_end, comment, &status);
    CHECK_STATUS_BREAK(status);

    char time_end[MAXMSG];
    fits_read_key(plf->fptr, TSTRING, "TIME-END", time_end, comment, &status);
    CHECK_STATUS_BREAK(status);

    double tstart=0.0;
    fits_read_key(plf->fptr, TDOUBLE, "TSTART", &tstart, comment, &status);
    CHECK_STATUS_BREAK(status);

    double tstop=0.0;
    fits_read_key(plf->fptr, TDOUBLE, "TSTOP", &tstop, comment, &status);
    CHECK_STATUS_BREAK(status);

    // Determine the file creation date for the header.
    char creation_date[MAXMSG];
    int timeref;
    fits_get_system_time(creation_date, &timeref, &status);
    CHECK_STATUS_BREAK(status);


    // Check if the output file already exists.
    int exists;
    fits_file_exists(par.eroEventList, &exists, &status);
    CHECK_STATUS_BREAK(status);
    if (0!=exists) {
      if (0!=par.clobber) {
	// Delete the file.
	remove(par.eroEventList);
      } else {
	// Throw an error.
	char msg[MAXMSG];
	sprintf(msg, "file '%s' already exists", par.eroEventList);
	SIXT_ERROR(msg);
	status=EXIT_FAILURE;
	break;
      }
    }

    // Create and open a new FITS file.
    headas_chat(3, "create new eROSITA event list file '%s' ...\n",
		par.eroEventList);
    fits_create_file(&fptr, par.eroEventList, &status);
    CHECK_STATUS_BREAK(status);

    // Create the event table.
    char *ttype[]={"TIME", "FRAME", "RAWX", "RAWY", "PHA", "ENERGY", 
		   "RA", "DEC", "X", "Y", "SUBX", "SUBY", "FLAG", 
		   "PAT_TYP", "PAT_INF", "EV_WEIGHT", "CCDNR"};
    char *tform[]={"D", "J", "I", "I", "I", "E", 
		   "J", "J", "J", "J", "B", "B", "J",
		   "I", "B", "E", "B"};
    char *tunit[]={"", "", "", "", "adu", "eV", 
		   "", "", "", "", "", "", "", "",
		   "", "", "", ""};
    fits_create_tbl(fptr, BINARY_TBL, 0, 17, ttype, tform, tunit, 
		    "EVENTS", &status);
    if (EXIT_SUCCESS!=status) {
      char msg[MAXMSG];
      sprintf(msg, "could not create binary table for events "
	      "in file '%s'", par.eroEventList);
      SIXT_ERROR(msg);
      break;
    }

    // Insert header keywords.
    char hduclass[MAXMSG]="OGIP";
    fits_update_key(fptr, TSTRING, "HDUCLASS", hduclass, "", &status);
    char hduclas1[MAXMSG]="EVENTS";
    fits_update_key(fptr, TSTRING, "HDUCLAS1", hduclas1, "", &status);
    CHECK_STATUS_BREAK(status);

    // Insert the standard eROSITA header keywords.
    sixt_add_fits_erostdkeywords(fptr, 1, creation_date, date_obs, time_obs,
				 date_end, time_end, tstart, tstop, 
				 timezero, &status);
    CHECK_STATUS_BREAK(status);
    sixt_add_fits_erostdkeywords(fptr, 2, creation_date, date_obs, time_obs,
				 date_end, time_end, tstart, tstop, 
				 timezero, &status);
    CHECK_STATUS_BREAK(status);

    // Determine the column numbers.
    int ctime, crawx, crawy, cframe, cpha, cenergy, cra, cdec, cx, cy, 
      csubx, csuby, cflag, cpat_typ, cpat_inf, cev_weight, cccdnr;
    fits_get_colnum(fptr, CASEINSEN, "TIME", &ctime, &status);
    fits_get_colnum(fptr, CASEINSEN, "FRAME", &cframe, &status);
    fits_get_colnum(fptr, CASEINSEN, "PHA", &cpha, &status);
    fits_get_colnum(fptr, CASEINSEN, "ENERGY", &cenergy, &status);
    fits_get_colnum(fptr, CASEINSEN, "RAWX", &crawx, &status);
    fits_get_colnum(fptr, CASEINSEN, "RAWY", &crawy, &status);
    fits_get_colnum(fptr, CASEINSEN, "RA", &cra, &status);
    fits_get_colnum(fptr, CASEINSEN, "DEC", &cdec, &status);
    fits_get_colnum(fptr, CASEINSEN, "X", &cx, &status);
    fits_get_colnum(fptr, CASEINSEN, "Y", &cy, &status);
    fits_get_colnum(fptr, CASEINSEN, "SUBX", &csubx, &status);
    fits_get_colnum(fptr, CASEINSEN, "SUBY", &csuby, &status);
    fits_get_colnum(fptr, CASEINSEN, "FLAG", &cflag, &status);
    fits_get_colnum(fptr, CASEINSEN, "PAT_TYP", &cpat_typ, &status);
    fits_get_colnum(fptr, CASEINSEN, "PAT_INF", &cpat_inf, &status);
    fits_get_colnum(fptr, CASEINSEN, "EV_WEIGHT", &cev_weight, &status);
    fits_get_colnum(fptr, CASEINSEN, "CCDNR", &cccdnr, &status);
    CHECK_STATUS_BREAK(status);

    // Set the TLMIN and TLMAX keywords.
    // For the PHA column.
    char keyword[MAXMSG];
    int tlmin_pha=0, tlmax_pha=4095;
    sprintf(keyword, "TLMIN%d", cpha);
    fits_update_key(fptr, TINT, keyword, &tlmin_pha, "", &status);
    sprintf(keyword, "TLMAX%d", cpha);
    fits_update_key(fptr, TINT, keyword, &tlmax_pha, "", &status);
    CHECK_STATUS_BREAK(status);

    // For the ENERGY column.
    float tlmin_energy=0.0, tlmax_energy=20.48;
    sprintf(keyword, "TLMIN%d", cenergy);
    fits_update_key(fptr, TFLOAT, keyword, &tlmin_energy, "", &status);
    sprintf(keyword, "TLMAX%d", cenergy);
    fits_update_key(fptr, TFLOAT, keyword, &tlmax_energy, "", &status);
    CHECK_STATUS_BREAK(status);

    // For the X and Y column.
    long tlmin_x=-12960000, tlmax_x=12960000;
    long tlmin_y= -6480000, tlmax_y= 6480000;
    sprintf(keyword, "TLMIN%d", cx);
    fits_update_key(fptr, TLONG, keyword, &tlmin_x, "", &status);
    fits_update_key(fptr, TLONG, "REFXLMIN", &tlmin_x, "", &status);
    sprintf(keyword, "TLMAX%d", cx);
    fits_update_key(fptr, TLONG, keyword, &tlmax_x, "", &status);
    fits_update_key(fptr, TLONG, "REFXLMAX", &tlmax_x, "", &status);
    sprintf(keyword, "TLMIN%d", cy);
    fits_update_key(fptr, TLONG, keyword, &tlmin_y, "", &status);
    fits_update_key(fptr, TLONG, "REFYLMIN", &tlmin_y, "", &status);
    sprintf(keyword, "TLMAX%d", cy);
    fits_update_key(fptr, TLONG, keyword, &tlmax_y, "", &status);
    fits_update_key(fptr, TLONG, "REFYLMAX", &tlmax_y, "", &status);
    CHECK_STATUS_BREAK(status);

    // Set up the WCS data structure.
    if (0!=wcsini(1, 2, &wcs)) {
      SIXT_ERROR("initalization of WCS data structure failed");
      status=EXIT_FAILURE;
      break;
    }
    wcs.naxis=2;
    wcs.crpix[0]=0.0;
    wcs.crpix[1]=0.0;
    wcs.crval[0]=par.RefRA;
    wcs.crval[1]=par.RefDec;    
    wcs.cdelt[0]=-0.05/3600.;
    wcs.cdelt[1]= 0.05/3600.;
    strcpy(wcs.cunit[0], "deg");
    strcpy(wcs.cunit[1], "deg");
    strcpy(wcs.ctype[0], "RA---");
    strcat(wcs.ctype[0], par.Projection);
    strcpy(wcs.ctype[1], "DEC--");
    strcat(wcs.ctype[1], par.Projection);

    // Update the WCS keywords in the output file.
    sprintf(keyword, "TCTYP%d", cx);
    fits_update_key(fptr, TSTRING, keyword, wcs.ctype[0], 
		    "projection type", &status);
    sprintf(keyword, "TCTYP%d", cy);
    fits_update_key(fptr, TSTRING, keyword, wcs.ctype[1], 
		    "projection type", &status);
    sprintf(keyword, "TCRVL%d", cx);
    fits_update_key(fptr, TDOUBLE, keyword, &wcs.crval[0], 
		    "reference value", &status);
    sprintf(keyword, "TCRVL%d", cy);
    fits_update_key(fptr, TDOUBLE, keyword, &wcs.crval[1], 
		    "reference value", &status);
    sprintf(keyword, "TCDLT%d", cx);
    fits_update_key(fptr, TDOUBLE, keyword, &wcs.cdelt[0], 
		    "pixel increment", &status);
    sprintf(keyword, "TCDLT%d", cy);
    fits_update_key(fptr, TDOUBLE, keyword, &wcs.cdelt[1], 
		    "pixel increment", &status);
    CHECK_STATUS_BREAK(status);

    fits_update_key(fptr, TSTRING, "REFXCTYP", wcs.ctype[0], 
		    "projection type", &status);
    fits_update_key(fptr, TSTRING, "REFYCTYP", wcs.ctype[1], 
		    "projection type", &status);
    fits_update_key(fptr, TSTRING, "REFXCUNI", "deg", "", &status);
    fits_update_key(fptr, TSTRING, "REFYCUNI", "deg", "", &status);
    float refxcrpx=0.0, refycrpx=0.0;
    fits_update_key(fptr, TFLOAT, "REFXCRPX", &refxcrpx, "", &status);
    fits_update_key(fptr, TFLOAT, "REFYCRPX", &refycrpx, "", &status);
    fits_update_key(fptr, TDOUBLE, "REFXCRVL", &wcs.crval[0], 
		    "reference value", &status);
    fits_update_key(fptr, TDOUBLE, "REFYCRVL", &wcs.crval[1], 
		    "reference value", &status);
    fits_update_key(fptr, TDOUBLE, "REFXCDLT", &wcs.cdelt[0], 
		    "pixel increment", &status);
    fits_update_key(fptr, TDOUBLE, "REFYCDLT", &wcs.cdelt[1], 
		    "pixel increment", &status);
    CHECK_STATUS_BREAK(status);

    // Set the TZERO keywords for the columns SUBX and SUBY. Note that
    // the TZERO values also have to be set with the routine 
    // fits_set_tscale(). Otherwise CFITSIO will access the raw values
    // in the file.
    int tzero_subx_suby=-127;
    sprintf(keyword, "TZERO%d", csubx);
    fits_update_key(fptr, TINT, keyword, &tzero_subx_suby, "", &status);
    fits_set_tscale(fptr, csubx, 1.0, (double)tzero_subx_suby, &status);
    sprintf(keyword, "TZERO%d", csuby);
    fits_update_key(fptr, TINT, keyword, &tzero_subx_suby, "", &status);
    fits_set_tscale(fptr, csuby, 1.0, (double)tzero_subx_suby, &status);
    CHECK_STATUS_BREAK(status);

    // TODO use keywords NLLRAT, SPLTTHR

    // --- END of initialization ---

    // --- Beginning of copy events ---

    headas_chat(3, "copy events ...\n");

    // Actual minimum and maximum values of X and Y.
    long refxdmin, refxdmax, refydmin, refydmax;
    double ra_min, ra_max, dec_min, dec_max;

    // Loop over all patterns in the FITS file. 
    long row;
    for (row=0; row<plf->nrows; row++) {
      
      // Read the next pattern from the input file.
      Pattern pattern;
      getPatternFromFile(plf, row+1, &pattern, &status);
      CHECK_STATUS_BREAK(status);

      // Determine the event data based on the pattern information.
      eroCalEvent ev;

      ev.time  =pattern.time;
      ev.frame =pattern.frame;
      ev.pha   =pattern.pi;
      ev.energy=pattern.signal*1000.; // [eV]
      ev.rawx  =pattern.rawx+1;
      ev.rawy  =pattern.rawy+1;

      // TODO Columns SUBX and SUBY should have real values, because the
      // center of a pixel is denoted by (-0.5,-0.5.).
      ev.subx=0;
      ev.suby=0;

      // TODO Not used in the current implementation.
      ev.pat_typ=0;

      // TODO In the current implementation PAT_INF is set to the default value of 5.
      ev.pat_inf=0;

      ev.ra=(long)(pattern.ra*180./M_PI/1.e-6);
      if (pattern.ra < 0.) {
	ev.ra--;
	SIXT_WARNING("value for right ascension <0.0deg");
      }
      ev.dec=(long)(pattern.dec*180./M_PI/1.e-6);
      if (pattern.dec < 0.) {
	ev.dec--;
      }

      // Determine the minimum and maximum values of RA and Dec in [rad].
      if (0==row) {
	ra_min =pattern.ra;
	ra_max =pattern.ra;
	dec_min=pattern.dec;
	dec_max=pattern.dec;
      }
      if (pattern.ra<ra_min) {
	ra_min=pattern.ra;
      }
      if (pattern.ra>ra_max) {
	ra_max=pattern.ra;
      }
      if (pattern.dec<dec_min) {
	dec_min=pattern.dec;
      }
      if (pattern.dec>dec_max) {
	dec_max=pattern.dec;
      }

      // Convert world coordinates to image coordinates X and Y.
      double world[2]={ pattern.ra*180./M_PI, pattern.dec*180./M_PI };
      double imgcrd[2], pixcrd[2];
      double phi, theta;
      int wcsstatus=0;
      wcss2p(&wcs, 1, 2, world, &phi, &theta, imgcrd, pixcrd, &wcsstatus);
      if (0!=wcsstatus) {
	char msg[MAXMSG];
	sprintf(msg, 
		"WCS coordinate conversion failed (RA=%lf, Dec=%lf, error code %d)", 
		world[0], world[1], wcsstatus);
	SIXT_ERROR(msg);
	status=EXIT_FAILURE;
	break;
      }
      ev.x=(long)pixcrd[0]; 
      if (pixcrd[0] < 0.) ev.x--;
      ev.y=(long)pixcrd[1]; 
      if (pixcrd[1] < 0.) ev.y--;

      // Determine the actual minimum and maximum values of X and Y.
      if (0==row) {
	refxdmin=ev.x;
	refxdmax=ev.x;
	refydmin=ev.y;
	refydmax=ev.y;
      }
      if (ev.x<refxdmin) {
	refxdmin=ev.x;
      }
      if (ev.x>refxdmax) {
	refxdmax=ev.x;
      }
      if (ev.y<refydmin) {
	refydmin=ev.y;
      }
      if (ev.y>refydmax) {
	refydmax=ev.y;
      }

      // TODO In the current implementation the value of FLAG is set to 
      // by default. This needs to be changed later.
      ev.flag=0xC0000000;

      // TODO Inverse vignetting correction factor is not used.
      ev.ev_weight=1.0; // Invers vignetting correction factor.

      // CCD number.
      ev.ccdnr=par.CCDNr;

      // Store the event in the output file.
      fits_write_col(fptr, TDOUBLE, ctime, row+1, 1, 1, &ev.time, &status);
      fits_write_col(fptr, TLONG, cframe, row+1, 1, 1, &ev.frame, &status);
      fits_write_col(fptr, TLONG, cpha, row+1, 1, 1, &ev.pha, &status);      
      fits_write_col(fptr, TFLOAT, cenergy, row+1, 1, 1, &ev.energy, &status);
      fits_write_col(fptr, TINT, crawx, row+1, 1, 1, &ev.rawx, &status);
      fits_write_col(fptr, TINT, crawy, row+1, 1, 1, &ev.rawy, &status);
      fits_write_col(fptr, TLONG, cra, row+1, 1, 1, &ev.ra, &status);
      fits_write_col(fptr, TLONG, cdec, row+1, 1, 1, &ev.dec, &status);
      fits_write_col(fptr, TLONG, cx, row+1, 1, 1, &ev.x, &status);
      fits_write_col(fptr, TLONG, cy, row+1, 1, 1, &ev.y, &status);
      fits_write_col(fptr, TBYTE, csubx, row+1, 1, 1, &ev.subx, &status);
      fits_write_col(fptr, TBYTE, csuby, row+1, 1, 1, &ev.suby, &status);
      fits_write_col(fptr, TLONG, cflag, row+1, 1, 1, &ev.flag, &status);
      fits_write_col(fptr, TUINT, cpat_typ, row+1, 1, 1, &ev.pat_typ, &status);
      fits_write_col(fptr, TBYTE, cpat_inf, row+1, 1, 1, &ev.pat_inf, &status);
      fits_write_col(fptr, TFLOAT, cev_weight, row+1, 1, 1, &ev.ev_weight, &status);
      fits_write_col(fptr, TINT, cccdnr, row+1, 1, 1, &ev.ccdnr, &status);
      CHECK_STATUS_BREAK(status);
    }
    CHECK_STATUS_BREAK(status);
    // END of loop over all patterns in the FITS file.

    // Set the RA_MIN, RA_MAX, DEC_MIN, DEC_MAX keywords (in [deg]).
    ra_min *=180./M_PI;
    ra_max *=180./M_PI;
    dec_min*=180./M_PI;
    dec_max*=180./M_PI;
    fits_update_key(fptr, TDOUBLE, "RA_MIN", &ra_min, "", &status);
    fits_update_key(fptr, TDOUBLE, "RA_MAX", &ra_max, "", &status);
    fits_update_key(fptr, TDOUBLE, "DEC_MIN", &dec_min, "", &status);
    fits_update_key(fptr, TDOUBLE, "DEC_MAX", &dec_max, "", &status);
    CHECK_STATUS_BREAK(status);

    // Set the number of unique events to the number of entries in the table.
    long uniq_evt;
    fits_get_num_rows(fptr, &uniq_evt, &status);
    CHECK_STATUS_BREAK(status);
    fits_update_key(fptr, TLONG, "UNIQ_EVT", &uniq_evt, 
		    "Number of unique events inside", &status);
    CHECK_STATUS_BREAK(status);

    // Set the REF?DMIN/MAX keywords.
    fits_update_key(fptr, TLONG, "REFXDMIN", &refxdmin, "", &status);
    fits_update_key(fptr, TLONG, "REFXDMAX", &refxdmax, "", &status);
    fits_update_key(fptr, TLONG, "REFYDMIN", &refydmin, "", &status);
    fits_update_key(fptr, TLONG, "REFYDMAX", &refydmax, "", &status);
    CHECK_STATUS_BREAK(status);

    // Determine the relative search threshold for split partners.
    fits_write_errmark();
    float spltthr;
    int opt_status=EXIT_SUCCESS;
    fits_read_key(plf->fptr, TFLOAT, "SPLTTHR", &spltthr, comment, &opt_status);
    if (EXIT_SUCCESS==opt_status) {
      fits_update_key(fptr, TFLOAT, "SPLTTHR", &spltthr, 
		      "Relative search level for split events", &status);      
      CHECK_STATUS_BREAK(status);
    }
    opt_status=EXIT_SUCCESS;
    fits_clear_errmark();

    // --- End of copy events ---

    // --- Beginning of append GTI extension ---

    headas_chat(3, "append GTI extension ...\n");

    // If available, load the specified GTI file.
    if (strlen(par.GTIFile)>0) {
      char ucase_buffer[MAXFILENAME];
      strcpy(ucase_buffer, par.GTIFile);
      strtoupper(ucase_buffer);
      if (0!=strcmp(ucase_buffer, "NONE")) {
	gti=loadGTI(par.GTIFile, &status);
	CHECK_STATUS_BREAK(status);
      }
    }

    // If not, create a dummy GTI from TSTART and TSTOP.
    if (NULL==gti) {
      gti=newGTI(&status);
      CHECK_STATUS_BREAK(status);
      appendGTI(gti, tstart, tstop, &status);
      CHECK_STATUS_BREAK(status);
    }

    // Store the GTI extension in the output file.
    char gti_extname[MAXMSG];
    sprintf(gti_extname, "GTI%d", par.CCDNr);
    saveGTIExt(fptr, gti_extname, gti, &status);
    CHECK_STATUS_BREAK(status);

    // --- End of append GTI extension ---

    // --- Beginning of append DEADCOR extension ---

    headas_chat(3, "append DEADCOR extension ...\n");

    // Create the DEADCOR table.
    char deadcor_extname[MAXMSG];
    sprintf(deadcor_extname, "DEADCOR%d", par.CCDNr);
    char *deadcor_ttype[]={"TIME", "DEADC"};
    char *deadcor_tform[]={"D", "E"};
    char *deadcor_tunit[]={"", ""};
    fits_create_tbl(fptr, BINARY_TBL, 0, 2, 
		    deadcor_ttype, deadcor_tform, deadcor_tunit, 
		    deadcor_extname, &status);
    if (EXIT_SUCCESS!=status) {
      SIXT_ERROR("could not create binary table for DEADCOR extension");
      break;
    }

    // Insert header keywords.
    fits_update_key(fptr, TSTRING, "HDUCLASS", "OGIP", "", &status);
    fits_update_key(fptr, TSTRING, "HDUCLAS1", "TEMPORALDATA", "", &status);
    fits_update_key(fptr, TSTRING, "HDUCLAS2", "TSI", "", &status);
    CHECK_STATUS_BREAK(status);

    // Determine the individual column numbers.
    int cdeadcor_time, cdeadc;
    fits_get_colnum(fptr, CASEINSEN, "TIME", &cdeadcor_time, &status);
    fits_get_colnum(fptr, CASEINSEN, "DEADC", &cdeadc, &status);
    CHECK_STATUS_BREAK(status);
  
    // Store the data in the table.
    double dbuffer=0.0;
    fits_write_col(fptr, TDOUBLE, cdeadcor_time, 1, 1, 1, &dbuffer, &status);
    float fbuffer=1.0;
    fits_write_col(fptr, TFLOAT, cdeadc, 1, 1, 1, &fbuffer, &status);
    CHECK_STATUS_BREAK(status);

    // --- End of append DEADCOR extension ---

    // --- Beginning of append BADPIX extension ---

    headas_chat(3, "append BADPIX extension ...\n");

    // Create the BADPIX table.
    char badpix_extname[MAXMSG];
    sprintf(badpix_extname, "BADPIX%d", par.CCDNr);
    char *badpix_ttype[]={"RAWX", "RAWY", "YEXTENT", "TYPE", "BADFLAG",
			  "TIMEMIN", "TIMEMAX", "PHAMIN", "PHAMAX", "PHAMED"};
    char *badpix_tform[]={"I", "I", "I", "I", "I", 
			  "D", "D", "I", "I", "E"};
    char *badpix_tunit[]={"", "", "", "", "",
			  "", "", "", "", ""};
    fits_create_tbl(fptr, BINARY_TBL, 0, 10,
		    badpix_ttype, badpix_tform, badpix_tunit, 
		    badpix_extname, &status);
    if (EXIT_SUCCESS!=status) {
      SIXT_ERROR("could not create binary table for BADPIX extension");
      break;
    }

    // Insert header keywords.
    fits_update_key(fptr, TSTRING, "HDUCLASS", "OGIP", "", &status);
    fits_update_key(fptr, TSTRING, "HDUCLAS1", "BADPIX", "", &status);
    fits_update_key(fptr, TSTRING, "HDUCLAS2", "STANDARD", "", &status);
    CHECK_STATUS_BREAK(status);

    // Determine the individual column numbers.
    int cbadpix_rawx, cbadpix_rawy, cbadpix_yextent, cbadpix_type, 
      cbadflag, ctimemin, ctimemax, cphamin, cphamax, cphamed;
    fits_get_colnum(fptr, CASEINSEN, "RAWX", &cbadpix_rawx, &status);
    fits_get_colnum(fptr, CASEINSEN, "RAWY", &cbadpix_rawy, &status);
    fits_get_colnum(fptr, CASEINSEN, "YEXTENT", &cbadpix_yextent, &status);
    fits_get_colnum(fptr, CASEINSEN, "TYPE", &cbadpix_type, &status);
    fits_get_colnum(fptr, CASEINSEN, "BADFLAG", &cbadflag, &status);
    fits_get_colnum(fptr, CASEINSEN, "TIMEMIN", &ctimemin, &status);
    fits_get_colnum(fptr, CASEINSEN, "TIMEMAX", &ctimemax, &status);
    fits_get_colnum(fptr, CASEINSEN, "PHAMIN", &cphamin, &status);
    fits_get_colnum(fptr, CASEINSEN, "PHAMAX", &cphamax, &status);
    fits_get_colnum(fptr, CASEINSEN, "PHAMED", &cphamed, &status);
    CHECK_STATUS_BREAK(status);

    // --- End of append BADPIX extension ---

    // --- Beginning of append CORRATT extension ---

    headas_chat(3, "append CORRATT extension ...\n");

    // If available, load the specified attitude file.
    if (strlen(par.Attitude)>0) {
      char ucase_buffer[MAXFILENAME];
      strcpy(ucase_buffer, par.Attitude);
      strtoupper(ucase_buffer);
      if (0!=strcmp(ucase_buffer, "NONE")) {
	ac=loadAttitude(par.Attitude, &status);
	CHECK_STATUS_BREAK(status);
      }
    }

    if (NULL!=ac) {
      // Create the CORRATT table.
      char corratt_extname[MAXMSG];
      sprintf(corratt_extname, "CORRATT%d", par.CCDNr);
      char *corratt_ttype[]={"TIME", "RA", "DEC", "ROLL"};
      char *corratt_tform[]={"D", "D", "D", "D"};
      char *corratt_tunit[]={"", "deg", "deg", "deg"};
      fits_create_tbl(fptr, BINARY_TBL, 0, 4,
		      corratt_ttype, corratt_tform, corratt_tunit, 
		      corratt_extname, &status);
      if (EXIT_SUCCESS!=status) {
	SIXT_ERROR("could not create binary table for CORRATT extension");
	break;
      }

      // Insert header keywords.
      fits_update_key(fptr, TSTRING, "HDUCLASS", "OGIP", "", &status);
      fits_update_key(fptr, TSTRING, "HDUCLAS1", "TEMPORALDATA", "", &status);
      fits_update_key(fptr, TSTRING, "HDUCLAS2", "ASPECT", "", &status);
      CHECK_STATUS_BREAK(status);

      // Determine the individual column numbers.
      int ccorratt_time, ccorratt_ra, ccorratt_dec, croll;
      fits_get_colnum(fptr, CASEINSEN, "TIME", &ccorratt_time, &status);
      fits_get_colnum(fptr, CASEINSEN, "RA", &ccorratt_ra, &status);
      fits_get_colnum(fptr, CASEINSEN, "DEC", &ccorratt_dec, &status);
      fits_get_colnum(fptr, CASEINSEN, "ROLL", &croll, &status);
      CHECK_STATUS_BREAK(status);

      // Determine the rotation of the CCD from the keyword in the event file.
      float ccdrotation;
      fits_read_key(plf->fptr, TFLOAT, "CCDROTA", &ccdrotation, comment, &status);
      if (EXIT_SUCCESS!=status) {
	SIXT_ERROR("failed reading keyword CCDROTA in input file");
	break;
      }

      // Insert the data.
      // Current bin in the GTI collection.
      unsigned long gtibin=0;
      // Loop over all intervals in the GTI collection.
      do {
	// Currently regarded interval.
	double t0, t1;
      
	// Determine the currently regarded interval.
	if (NULL==gti) {
	  t0=tstart;
	  t1=tstop;
	} else {
	  t0=gti->start[gtibin];
	  t1=gti->stop[gtibin];
	}
	
	// Note that the attitude is stored in steps of 1s 
	// according to the official event file format definition.
	long nrows=0;
	double currtime;
	for (currtime=t0; currtime<=t1; currtime+=1.0) {
	  Vector pointing=getTelescopeNz(ac, currtime, &status);
	  CHECK_STATUS_BREAK(status);	  
	  double ra, dec;
	  calculate_ra_dec(pointing, &ra, &dec);
	  ra *=180./M_PI;
	  dec*=180./M_PI;

	  float rollangle=getRollAngle(ac, currtime, &status);
	  CHECK_STATUS_BREAK(status);
	  rollangle*=180./M_PI;
	  
	  // Apply the rotation angle of the CCD.
	  rollangle+=ccdrotation;
	  
	  // Store the data in the file.
	  nrows++;
	  fits_write_col(fptr, TDOUBLE, ccorratt_time, nrows, 1, 1, &currtime, &status);
	  fits_write_col(fptr, TDOUBLE, ccorratt_ra, nrows, 1, 1, &ra, &status);
	  fits_write_col(fptr, TDOUBLE, ccorratt_dec, nrows, 1, 1, &dec, &status);
	  fits_write_col(fptr, TFLOAT, croll, nrows, 1, 1, &rollangle, &status);
	  CHECK_STATUS_BREAK(status);	  
	}
	CHECK_STATUS_BREAK(status);
	
	// Proceed to the next GTI interval.
	if (NULL!=gti) {
	  gtibin++;
	  if (gtibin>=gti->nentries) break;
	}
	
      } while (NULL!=gti);
      CHECK_STATUS_BREAK(status);
      // End of loop over the individual GTI intervals.
    }

    // --- End of append CORRATT extension ---

    // Append a check sum to the header of the event extension.
    int hdutype=0;
    fits_movabs_hdu(fptr, 2, &hdutype, &status);
    fits_write_chksum(fptr, &status);
    CHECK_STATUS_BREAK(status);

  } while(0); // END of the error handling loop.


  // --- Cleaning up ---
  headas_chat(3, "cleaning up ...\n");

  // Close the files.
  destroyPatternFile(&plf, &status);
  if (NULL!=fptr) fits_close_file(fptr, &status);
  
  // Release memory.
  wcsfree(&wcs);
  if (NULL!=headerstr) free(headerstr);
  freeGTI(&gti);
  freeAttitude(&ac);

  if (status==EXIT_SUCCESS) headas_chat(3, "finished successfully\n\n");
  return(status);
}


int getpar(struct Parameters* const par)
{
  // String input buffer.
  char* sbuffer=NULL;

  // Error status.
  int status=EXIT_SUCCESS;

  status=ape_trad_query_file_name("PatternList", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the input pattern list");
    return(status);
  } 
  strcpy(par->PatternList, sbuffer);
  free(sbuffer);

  status=ape_trad_query_file_name("eroEventList", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the output event list");
    return(status);
  } 
  strcpy(par->eroEventList, sbuffer);
  free(sbuffer);

  status=ape_trad_query_int("CCDNr", &par->CCDNr);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the CCDNr parameter");
    return(status);
  }

  status=ape_trad_query_string("Projection", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the projection type");
    return(status);
  }
  strcpy(par->Projection, sbuffer);
  free(sbuffer);

  status=ape_trad_query_float("RefRA", &par->RefRA);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading RefRA");
    return(status);
  }

  status=ape_trad_query_float("RefDec", &par->RefDec);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading RefDEC");
    return(status);
  }

  status=ape_trad_query_string("GTIFile", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the GTI file");
    return(status);
  }
  strcpy(par->GTIFile, sbuffer);
  free(sbuffer);

  status=ape_trad_query_string("Attitude", &sbuffer);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the name of the attitude file");
    return(status);
  }
  strcpy(par->Attitude, sbuffer);
  free(sbuffer);

  status=ape_trad_query_bool("clobber", &par->clobber);
  if (EXIT_SUCCESS!=status) {
    SIXT_ERROR("failed reading the clobber parameter");
    return(status);
  }

  return(status);
}