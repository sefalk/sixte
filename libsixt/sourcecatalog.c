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

#include "sourcecatalog.h"


SourceCatalog* newSourceCatalog(int* const status)
{
  SourceCatalog* cat=(SourceCatalog*)malloc(sizeof(SourceCatalog));
  CHECK_NULL(cat, *status,
	     "memory allocation for SourceCatalog failed");

  // Initialize pointers with NULL.
  cat->tree       =NULL;
  cat->extsources =NULL;
  cat->nextsources=0;
  cat->simput     =NULL;
  return(cat);
}


void freeSourceCatalog(SourceCatalog** const cat, int* const status)
{
  if (NULL!=*cat) {
    // Free the KD-Tree.
    if (NULL!=(*cat)->tree) {
      freeKDTreeElement(&((*cat)->tree));
    }
    // Free the array of extended sources.
    if (NULL!=(*cat)->extsources) {
      free((*cat)->extsources);
    }
    // Free the SIMPUT source catalog.
    if (NULL!=(*cat)->simput) {
      freeSimputCtlg(&((*cat)->simput), status);
    }
    free(*cat);
    *cat=NULL;
  }
}


SourceCatalog* loadSourceCatalog(const char* const filename,
				 struct ARF* const arf,
				 int* const status)
{
  headas_chat(3, "load source catalog from file '%s' ...\n", filename);

  SourceCatalog* cat=newSourceCatalog(status);
  CHECK_STATUS_RET(*status, cat);

  // Set refernce to the random number generator to be used by the
  // SIMPUT library routines.
  setSimputRndGen(sixt_get_random_number);

  // Use the routines from the SIMPUT library to load the catalog.
  cat->simput=openSimputCtlg(filename, READONLY, 0, 0, 0, 0, status);
  CHECK_STATUS_RET(*status, cat);

  // Set reference to ARF for SIMPUT library.
  setSimputARF(cat->simput, arf);

  // Determine the number of point-like and the number of
  // extended sources.
  unsigned long nextended =0;
  unsigned long npointlike=0;
  long ii;

  double ra_center_img=0.0;
  double dec_center_img=0.0;

  for (ii=0; ii<cat->simput->nentries; ii++) {
    // Get the source.
    SimputSrc* src=getSimputSrc(cat->simput, ii+1, status);
    CHECK_STATUS_BREAK(*status);

    // Determine the extension.
    float extension=getSimputSrcExt(cat->simput, src, &ra_center_img, &dec_center_img, 0., 0., status);
    CHECK_STATUS_BREAK(*status);
    if (extension>0.) {
      // This is an extended source.
      nextended++;
    } else {
      // This is a point-like source.
      npointlike++;
    }
  }
  CHECK_STATUS_RET(*status, cat);

  // Allocate memory for an array of the point-like sources,
  // which will be converted into a KDTree afterwards.
  Source* list=(Source*)malloc(npointlike*sizeof(Source));
  CHECK_NULL_RET(list, *status,
		 "memory allocation for source list failed", cat);

  // Allocate memory for the array of the extended sources.
  cat->extsources=(Source*)malloc(nextended*sizeof(Source));
  CHECK_NULL_RET(list, *status,
		 "memory allocation for source list failed", cat);

  // Empty template object.
  Source* templatesrc=newSource(status);
  CHECK_STATUS_RET(*status, cat);

  // Loop over all entries in the SIMPUT source catalog.
  unsigned long cpointlike=0;
  cat->nextsources=0;
  for (ii=0; ii<cat->simput->nentries; ii++) {
    // Get the source.
    SimputSrc* src=getSimputSrc(cat->simput, ii+1, status);
    CHECK_STATUS_BREAK(*status);

    float extension=getSimputSrcExt(cat->simput, src, &ra_center_img, &dec_center_img, 0., 0., status);
    CHECK_STATUS_BREAK(*status);

    if (extension>0.) {
      // This is an extended source.
      cat->nextsources++;
      // Start with an empty Source object for this entry.
      cat->extsources[cat->nextsources-1] = *templatesrc;
      // Set the properties from the SIMPUT catalog (position, extension, and row number in the catalog).

      // we need the center pixels here of the image, as this is what the extensions refers to)
      cat->extsources[cat->nextsources-1].ra  = ra_center_img;
      cat->extsources[cat->nextsources-1].dec = dec_center_img;
      cat->extsources[cat->nextsources-1].row = ii+1;
      cat->extsources[cat->nextsources-1].extension = extension;

    } else {
      // This is a point-like source.
      cpointlike++;
      list[cpointlike-1]     = *templatesrc;
      list[cpointlike-1].ra  = src->ra;
      list[cpointlike-1].dec = src->dec;
      list[cpointlike-1].row = ii+1;
    }
  }
  CHECK_STATUS_RET(*status, cat);
  // END of loop over all entries in the FITS table.

  // Build a KDTree from the source list (array of Source objects).
  cat->tree=buildKDTree2(list, npointlike, 0, status);
  CHECK_STATUS_RET(*status, cat);

  // In a later development stage this could be directly stored in
  // a memory-mapped space.

  // Load spectra into the internal cache used by the SIMPUT library.
  // This works only if all spectra are contained as mission-independent
  // spectra in a single binary-table FITS file HDU, which can be found
  // be tracing the location of the spectrum assigned to the first source
  // in the catalog.
  SimputSrc* src=getSimputSrc(cat->simput, 1, status);
  CHECK_STATUS_RET(*status, cat);

  char specref[MAXFILENAME];
  getSimputSrcSpecRef(cat->simput, src, 0., -1.0, specref, status);
  CHECK_STATUS_RET(*status, 0);

  char *search=strchr(specref, ']');
  if (NULL==search) {
    SIXT_WARNING("no valid reference to source spectrum");
  } else {
    *(search+1)='\0';
    headas_chat(3, "try to load all spectra ('%s') into cache ...\n", specref);
    loadCacheAllSimputMIdpSpec(cat->simput, specref, status);
    CHECK_STATUS_RET(*status, cat);
  }

  // Release memory.
  if (templatesrc) free(templatesrc);
  if (list) free(list);

  return(cat);
}


LinkedPhoListElement* genFoVXRayPhotons(SourceCatalog* const cat,
					const Vector* const pointing,
					const float fov,
					const double t0, const double t1,
					const double mjdref,
					int* const status)
{
  assert(NULL!=cat);

  // Minimum cos-value for point sources close to the FOV (in the direct
  // neighborhood).
  const double close_mult=1.5;
  const double close_fov_min_align=cos(close_mult*fov/2.);

  // Perform a range search over all sources in the KDTree and
  // generate new photons for the sources within the FoV.
  // The kdTree only contains point-like sources.
  LinkedPhoListElement* list=
    KDTreeRangeSearch(cat->tree, 0, pointing, close_fov_min_align,
		      t0, t1, mjdref, cat->simput, status);

  // Loop over all extended sources.
  long ii;
  for (ii=0; ii<cat->nextsources; ii++) {
	  // Check if at least a part of the source lies within the FoV.
	  Vector location=unit_vector(cat->extsources[ii].ra,
			  cat->extsources[ii].dec);
	  double min_align = close_mult*(fov*0.5) + cat->extsources[ii].extension;
	  if (min_align > M_PI){
		  min_align -= 2*M_PI;
	  }

	  if (0==check_fov(&location, pointing,cos(min_align))) {

		  // Generate photons for this particular source.
		  LinkedPhoListElement* newlist=
				  getXRayPhotons(&(cat->extsources[ii]), cat->simput,
						  t0, t1, mjdref, status);
		  CHECK_STATUS_RET(*status, list);

		  // Merge the new photons into the existing list.
		  list=mergeLinkedPhoLists(list, newlist);
	  }
  }

  return(list);
}
