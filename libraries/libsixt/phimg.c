#include "phimg.h"


void phimg(const GenDet* const det,
	   AttitudeCatalog* const ac,
	   PhotonListFile* const plf,
	   ImpactListFile* const ilf,
	   const double t0,
	   const double exposure,
	   int* const status)
{
  struct Telescope telescope;

  // Calculate the minimum cos-value for sources inside the FOV: 
  // (angle(x0,source) <= 1/2 * diameter)
  const double fov_min_align = cos(det->fov_diameter/2.); 

  // Scan the entire photon list.  
  while (plf->row < plf->nrows) {

    Photon photon={.time=0.};
      
    // Read an entry from the photon list:
    *status=PhotonListFile_getNextRow(plf, &photon);
    CHECK_STATUS_VOID(*status);

    // Check whether we are still within the requested time interval.
    if (photon.time < t0) continue;
    if (photon.time > t0+exposure) break;

    // Check whether the photon is inside the FOV.
    // First determine telescope pointing direction at the current time.
    telescope.nz = getTelescopeNz(ac, photon.time, status);
    CHECK_STATUS_VOID(*status);

    // Compare the photon direction to the direction of the telescope axis.
    Vector photon_direction = unit_vector(photon.ra, photon.dec);
    if (check_fov(&photon_direction, &telescope.nz, fov_min_align)==0) {
      // Photon is inside the FOV!
	
      // Determine telescope data like pointing direction (attitude) etc.
      // The telescope coordinate system consists of an x-, y-, and z-axis.
      getTelescopeAxes(ac, &telescope.nx, &telescope.ny, &telescope.nz, 
		       photon.time, status);
      CHECK_STATUS_BREAK(*status);

      // Determine the photon impact position on the detector (in [m]).

      // Convolution with PSF:
      // Function returns 0, if the photon does not fall on the detector. 
      // If it hits the detector, the return value is 1.
      struct Point2d position;
      if (get_psf_pos(&position, photon, telescope, det->focal_length, 
		      det->vignetting, det->psf)) {
	// Check whether the photon hits the detector within the FOV. 
	// (Due to the effects of the mirrors it might have been scattered over 
	// the edge of the FOV, although the source is inside the FOV.)
	if (sqrt(pow(position.x,2.)+pow(position.y,2.)) < 
	    tan(det->fov_diameter)*det->focal_length) {

	  // Insert the impact position with the photon data into the 
	  // impact list.
	  Impact impact = { 
	    .time   = photon.time,
	    .energy = photon.energy,
	    .position = position,
	    .ph_id  = photon.ph_id,
	    .src_id = photon.src_id
	  };
	  addImpact2File(ilf, &impact, status);	    
	  CHECK_STATUS_VOID(*status);
	  
	}
      } 
      // END get_psf_pos(...)
    } 
    // End of FOV check.
  }
  // END of scanning LOOP over the photon list.
}

