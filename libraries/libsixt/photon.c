#include "photon.h"


float photon_energy(const Spectrum* const spectrum, 
		    const struct ARF* const arf)
{
  // Get a random PHA channel according to the given PHA distribution.
  float rand = (float)sixt_get_random_number();
  long upper = spectrum->NumberChannels-1, lower=0, mid;
  
  if(rand > spectrum->rate[spectrum->NumberChannels-1]) {
    printf("PHA sum: %f < RAND: %f\n", 
	   spectrum->rate[spectrum->NumberChannels-1], rand);
  }

  // Determine the energy of the photon (using binary search).
  while (upper>lower) {
    mid = (lower+upper)/2;
    if (spectrum->rate[mid] < rand) {
      lower = mid+1;
    } else {
      upper = mid;
    }
  }

  // Return an energy chosen randomly out of the determined PHA bin:
  return(arf->LowEnergy[lower] + 
	 sixt_get_random_number()*(arf->HighEnergy[lower]-
				   arf->LowEnergy[lower]));
}



int create_PointSourcePhotons(/** Source data. */
			      PointSource* ps,
			      /** Current time. */
			      double time, 
			      /** Time interval for photon generation. */
			      double dt,
			      /** Address of pointer to time-ordered photon list.*/
			      struct PhotonOrderedListEntry** list_first,
			      const struct ARF* const arf)
{
  // Second pointer to photon list, that can be moved along the list,   
  // without loosing the first entry.
  struct PhotonOrderedListEntry* list_current = *list_first;

  int status=EXIT_SUCCESS;

  // Check if the photon rate of the source is positive.
  if (ps->rate<=0.) return(status);

  // If there is no photon time stored so far, set the current time.
  if (ps->t_last_photon <= time) {
    ps->t_last_photon = time;
  }


  // Create photons and insert them into the given time-ordered list:
  while (ps->t_last_photon < time+dt) {
    Photon new_photon;
    new_photon.ra  = ps->ra;
    new_photon.dec = ps->dec; 

    // Determine the energy of the new photon
    new_photon.energy = photon_energy(ps->spectrum, arf);

    // Determine the impact time of the photon according to the light
    // curve of the source.
    if (NULL==ps->lc) {
      if (T_LC_CONSTANT==ps->lc_type) {
	ps->lc = getLinLightCurve(1, &status);
	if (EXIT_SUCCESS!=status) return(status);
      } else if (T_LC_TIMMER_KOENIG==ps->lc_type) {
	ps->lc = getLinLightCurve(TK_LC_LENGTH, &status);
	if (EXIT_SUCCESS!=status) return(status);
      } else {
	status=EXIT_FAILURE;
	HD_ERROR_THROW("Error: Unknown light curve type!\n", EXIT_FAILURE);
	return(status);
      }
    }

    // Determine the arrival time of the new photon with the
    // appropriate random number generator. If the light curve
    // is not initialized, the function returns a negative time 
    // (-1).
    new_photon.time = getPhotonTime(ps->lc, ps->t_last_photon);

    // If the new time is negative, the light curve did not cover the
    // appropriate interval or is empty.
    if (new_photon.time < 0.) {
      // The current point of time is not covered by the light curve.
      // Therefore we have to generate a new one.
      if (T_LC_CONSTANT==ps->lc_type) {
	status = initConstantLinLightCurve(ps->lc, 
					   ps->t_last_photon, 1.e9, 
					   ps->rate);
	if (EXIT_SUCCESS!=status) return(status);
      } else if (T_LC_TIMMER_KOENIG==ps->lc_type) {
	status = initTimmerKoenigLinLightCurve(ps->lc, ps->t_last_photon, 
					       TK_LC_STEP_WIDTH, 
					       ps->rate, ps->rate/3.);
	if (EXIT_SUCCESS!=status) return(status);
      }
      // Repeat the determination of the photon arrival time,
      // now with the proper light curve.
      new_photon.time = getPhotonTime(ps->lc, ps->t_last_photon);
    } 
    // END if (new_photon.time < 0.)
        
    assert(new_photon.time>=0.);
    assert(new_photon.time>=ps->t_last_photon);
    ps->t_last_photon = new_photon.time;

    // Insert photon to the global photon list:
    status=insert_Photon2TimeOrderedList(list_first, &list_current, 
					 &new_photon);
    if (EXIT_SUCCESS!=status) return(status);  
    
  } // END of loop 'while(t_last_photon < time+dt)'

  return(status);
}



int create_ExtendedSourcePhotons(/** Source data. */
				 ExtendedSource* es,
				 /** Current time. */
				 double time, 
				 /** Time interval for photon generation. */
				 double dt,
				 /** Address of pointer to time-ordered photon list.*/
				 struct PhotonOrderedListEntry** list_first,
				 const struct ARF* const arf)
{
  // Second pointer to photon list, that can be moved along the list,   
  // without loosing the first entry.
  struct PhotonOrderedListEntry* list_current = *list_first;

  int status=EXIT_SUCCESS;

  // If there is no photon time stored so far, set the current time.
  if (es->t_last_photon < 0.) {
    es->t_last_photon = time;
  }

  // Buffer for new Photon.
  Photon new_photon;
  // Center of the extended source.
  Vector center_direction; 
  // Normalized vectors perpendicular to the source vector / 
  // center direction.
  Vector a1, a2; 
  Vector b; // Auxiliary vector.
  double alpha, radius;
  // Create photons and insert them into the given time-ordered list:
  while (es->t_last_photon < time+dt) {

    // Determine the photon direction of origin.
    // Center of the extended source:
    center_direction = unit_vector(es->ra, es->dec);

    // Determine auxiliary vector b.
    if (center_direction.x > center_direction.y) {
      b.y = 1.;
      if (center_direction.x > center_direction.z) {
	b.x = 0.;
	b.z = 1.;
      } else {
	b.x = 1.;
	b.z = 0.;
      }
    } else {
      b.x = 1.;
      if (center_direction.y > center_direction.z) {
	b.y = 0.;
	b.z = 1.;
      } else {
	b.y = 1.;
	b.z = 0.;
      }
    }
    
    // Determine the two vectors perpendicular to the source direction
    // spanning the local reference coordinate system for the source
    // extension.
    a1 = normalize_vector(vector_product(b , center_direction));
    a2 = normalize_vector(vector_product(a1, center_direction));
    
    // Determine an azimuthal angle and an radius [rad]:
    alpha  = sixt_get_random_number()*2*M_PI;
    radius = sixt_get_random_number()*es->radius;
    
    // Photon direction is composition of these vectors:
    Vector new_photon_direction;
    new_photon_direction.x = center_direction.x + 
      radius*(cos(alpha)*a1.x + sin(alpha)*a2.x);
    new_photon_direction.y = center_direction.y + 
      radius*(cos(alpha)*a1.y + sin(alpha)*a2.y);
    new_photon_direction.z = center_direction.z + 
      radius*(cos(alpha)*a1.z + sin(alpha)*a2.z);
    normalize_vector_fast(&new_photon_direction);
    calculate_ra_dec(new_photon_direction, &new_photon.ra, &new_photon.dec);

    // Determine the energy of the new photon
    new_photon.energy = photon_energy(es->spectrum, arf);

    // Determine the impact time of the photon according to the light
    // curve of the source.
    if (NULL==es->lc) {
      if (T_LC_CONSTANT==es->lc_type) {
	es->lc = getLinLightCurve(1, &status);
	if (EXIT_SUCCESS!=status) break;
	status = initConstantLinLightCurve(es->lc, time, 1.e6, es->rate);
	if (EXIT_SUCCESS!=status) break;
      } else if (T_LC_TIMMER_KOENIG==es->lc_type) {
	es->lc = getLinLightCurve(131072, &status);
	if (EXIT_SUCCESS!=status) break;
	status = initTimmerKoenigLinLightCurve(es->lc, time, TK_LC_STEP_WIDTH, 
					       es->rate, es->rate/3.);
	if (EXIT_SUCCESS!=status) break;
      } else {
	status=EXIT_FAILURE;
	HD_ERROR_THROW("Error: Unknown light curve type!\n", EXIT_FAILURE);
	break;
      }
    }

    if (time > es->lc->t0 + es->lc->nvalues*es->lc->step_width) {
      if (T_LC_CONSTANT==es->lc_type) {
	status = initConstantLinLightCurve(es->lc, time, 1.e6, es->rate);
	if (EXIT_SUCCESS!=status) break;
      } else if (T_LC_TIMMER_KOENIG==es->lc_type) {
	status = initTimmerKoenigLinLightCurve(es->lc, time, TK_LC_STEP_WIDTH, es->rate,
					       es->rate/3.);
	if (EXIT_SUCCESS!=status) break;
      }
    }

    new_photon.time = getPhotonTime(es->lc, es->t_last_photon);
    assert(new_photon.time>=es->t_last_photon);
    es->t_last_photon = new_photon.time;

    // Insert photon to the global photon list:
    if ((status=insert_Photon2TimeOrderedList(list_first, &list_current, &new_photon)) 
	!= EXIT_SUCCESS) break;  
    
  } // END of loop 'while(t_last_photon < time+dt)'

  return(status);
}



void clear_PhotonList(struct PhotonOrderedListEntry** pole)
{
  struct PhotonOrderedListEntry* buffer;

  while (NULL!=*pole) {
    buffer = (*pole)->next;
    free(*pole);
    *pole = buffer;
  }
}



int insert_Photon2TimeOrderedList(struct PhotonOrderedListEntry** first,
				  struct PhotonOrderedListEntry** current, 
				  Photon* ph) 
{
  // The iterator is shifted over the time-ordered list to find the right entry.
  struct PhotonOrderedListEntry** iterator = current;

  // Find the right entry where to insert the new photon.
  while ((NULL!=*iterator) && ((*iterator)->photon.time <= ph->time)) {
    iterator = &((*iterator)->next);
  }
  // Now '*iterator' points to the entry before which the new photon has to be inserted.
  // I.e. 'iterator' is equivalent to '&[fromer_entry]->next'. 
  // Therefore changeing '*iterator' means redirecting the chain.
  // '*iterator' has to be redirected to the new entry.
    
  // Create a new PhotonOrderedListEntry and insert it before '**iterator'.
  struct PhotonOrderedListEntry* new_entry = 
    (struct PhotonOrderedListEntry*)malloc(sizeof(struct PhotonOrderedListEntry));
  if (NULL==new_entry) {
    HD_ERROR_THROW("Error: Could not allocate memory for new photon entry!\n", EXIT_FAILURE);
    return(EXIT_FAILURE);
  }

  // Set the values of the new entry:
  new_entry->photon = *ph;
  new_entry->next = *iterator;
  if (*iterator == *first) {
    // If the new photon was inserted as first entry of the list, the pointer to this 
    // first entry has to be redirected.
    *first = new_entry;
  }
  *iterator = new_entry;

  // The pointer '*current' should point to the new entry in the time-ordered list.
  *current = new_entry;
  
  return(EXIT_SUCCESS);
}



int insert_Photon2BinaryTree(struct PhotonBinaryTreeEntry** ptr, Photon* ph)
{
  int status = EXIT_SUCCESS;

  if (NULL==*ptr) {
    // The tree is completely empty so far. Therefore create the first
    // tree element and redirect the pointer from the calling routine to it.
    *ptr = (struct PhotonBinaryTreeEntry*)malloc(sizeof(struct PhotonBinaryTreeEntry));
    if (NULL==*ptr) { // Check if memory allocation was successfull.
      status = EXIT_FAILURE;
      HD_ERROR_THROW("Error: memory allocation for binary tree failed!\n", status);
      return(status);
    }
    (*ptr)->photon = *ph;
    (*ptr)->sptr = NULL;
    (*ptr)->gptr = NULL;

  } else { 
    // There is a first element (root) of the tree. So we have to find
    // the position where to insert the new element.
    struct PhotonBinaryTreeEntry** next=NULL;
    if ((*ptr)->photon.time > ph->time) {
      next = &(*ptr)->sptr;
    } else {
      next = &(*ptr)->gptr;
    }
    
    while (NULL!=*next) {
      // We have to go deeper into the tree. So decide whether the new photon
      // is earlier or later than the current entry.
      if ((*next)->photon.time > ph->time) {
	next=&(*next)->sptr;
      } else {
	next=&(*next)->gptr;
      }
    }

    // We have reached one of the ends of the tree. Insert the photon here.
    *next = (struct PhotonBinaryTreeEntry*)malloc(sizeof(struct PhotonBinaryTreeEntry));
    if (NULL==*next) { // Check if memory allocation was successfull.
      status = EXIT_FAILURE;
      HD_ERROR_THROW("Error: memory allocation for binary tree failed!\n", status);
      return(status);
    }
    (*next)->photon = *ph;
    (*next)->sptr = NULL;
    (*next)->gptr = NULL;

  }

  return(status);
}



int CreateOrderedPhotonList(struct PhotonBinaryTreeEntry** tree_ptr,
			    struct PhotonOrderedListEntry** list_first,
			    struct PhotonOrderedListEntry** list_current)
{
  int status=EXIT_SUCCESS;

  // Check if the current tree entry exists.
  if (NULL != *tree_ptr) {

    // Add the entries before the current one to the time-ordered list.
    status = CreateOrderedPhotonList(&(*tree_ptr)->sptr, list_first, list_current);
    if (EXIT_SUCCESS != status) return(status);

    // Insert the current entry into the time-ordered list at the right position.
    status = insert_Photon2TimeOrderedList(list_first, list_current, &(*tree_ptr)->photon);
    if (EXIT_SUCCESS != status) return(status);
    
    // Add the entries after the current one to the time-ordered list.
    status = CreateOrderedPhotonList(&(*tree_ptr)->gptr, list_first, list_current);
    if (EXIT_SUCCESS != status) return(status);

    // Delete the binary tree entry, as it is not required any more.
    free(*tree_ptr);
    *tree_ptr=NULL;

  } // END if current tree entry exists.

  return(status);
}



Photon* newPhoton(int* const status)
{
  Photon* ph = (Photon*)malloc(sizeof(Photon));
  CHECK_NULL(ph, *status, "memory allocation for Photon failed");

  // Set initial values.
  ph->ra     = 0.;
  ph->dec    = 0.;
  ph->time   = 0.;
  ph->energy = 0.;

  return(ph);
}

