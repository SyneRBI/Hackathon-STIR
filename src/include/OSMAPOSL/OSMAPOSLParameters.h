//
// $Id$: $Date$
//
 
#ifndef __OSMAPOSLParameters_h__
#define __OSMAPOSLParameters_h__

/*!
  \file 
  \ingroup OSMAPOSL
 
  \brief declares the OSMAPOSLParameters class

  \author Kris Thielemans
  \author Matthew Jacobson
  \author PARAPET project

  \date $Date$
  \version $Revision$

  
*/


#include "LogLikBased/MAPParameters.h"


START_NAMESPACE_TOMO

/*! 

 \brief parameter class for OSMAPOSL

  This class is supposed to be the last in the Parameter hierarchy.
 */

class OSMAPOSLParameters : public MAPParameters
{

public:

  /*!
  \brief Constructor, initialises everything from parameter file, or (when
  parameter_filename == "") by calling ask_parameters().
  */
  explicit OSMAPOSLParameters(const string& parameter_filename = "");
 
  //! lists the parameter values
  virtual string parameter_info() const;

  //! prompts the user to enter parameter values manually
  virtual void ask_parameters();

  //! determines whether non-positive values in the initial image will be set to small positive ones
  int enforce_initial_positivity;

  //! subiteration interval at which to apply inter-update filters 
  int inter_update_filter_interval;

  //! inter-update filter type name
  string inter_update_filter_type;

  //! inter-update filter FWHM for transaxial direction filtering
  double inter_update_filter_fwhmxy_dir;

  //! inter-update filter FWHM for axial direction filtering
  double inter_update_filter_fwhmz_dir;

  //! inter-update filter Metz power for transaxial direction filtering
  double inter_update_filter_Nxy_dir;

  //! inter-update filter Metz power for axial direction filtering
  double inter_update_filter_Nz_dir;

  //! inter-update filter object
  ImageFilter inter_update_filter;

  // KT 17/08/2000 3 new parameters

  //! restrict updates (larger relative updates will be thresholded)
  double maximum_relative_change;

  //! restrict updates (smaller relative updates will be thresholded)
  double minimum_relative_change;
  
  //! boolean value to determine if the update images have to be written to disk
  int write_update_image;
private:

  //! used to check acceptable parameter ranges, etc...
  virtual bool post_processing();

};

END_NAMESPACE_TOMO

#endif // __OSMAPOSLParameters_h__
