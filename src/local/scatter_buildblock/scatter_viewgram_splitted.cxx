//
// $Id$
//

/*
Copyright (C) 2004 - $Date$, Hammersmith Imanet Ltd
This file is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

  This file is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details. 
  
	See STIR/LICENSE.txt for details
*/
/*!
\file
\ingroup scatter
\brief Implementations of functions defined in Scatter.

  \author Charalampos Tsoumpas
  \author Pablo Aguiar
  \author Kris Thielemans
  
	$Date$
	$Revision$
*/
#include "local/stir/Scatter.h"
#include "stir/ProjData.h"
#include "stir/ProjDataInfo.h"
#include "stir/ProjDataInfoCylindricalNoArcCorr.h" 
#include "stir/Bin.h"
#include "stir/stream.h"
#include "stir/CPUTimer.h"
#include "stir/Viewgram.h"
#include <fstream>
#include <cstdio>
using namespace std;

START_NAMESPACE_STIR
std::vector< ScatterPoint> scatt_points_vector;
std::vector<CartesianCoordinate3D<float> > detection_points_vector;
int total_detectors;
static
unsigned 
find_in_detection_points_vector(const CartesianCoordinate3D<float>& coord)
{
	std::vector<CartesianCoordinate3D<float> >::const_iterator iter=
	std::find(detection_points_vector.begin(),
		      detection_points_vector.end(),
			  coord);
	if (iter != detection_points_vector.end())
	{
		return iter-detection_points_vector.begin();
	}
	else
	{
	  if (detection_points_vector.size()==static_cast<std::size_t>(total_detectors))
	    error("More detection points than we think there are!\n");

	  detection_points_vector.push_back(coord);
	  return detection_points_vector.size()-1;
	}
}
void scatter_viewgram_splitted( 
					  ProjData& proj_data,
					  ProjData& proj_data2,                    
					  const DiscretisedDensityOnCartesianGrid<3,float>& image_as_activity,
					  const DiscretisedDensityOnCartesianGrid<3,float>& image_as_density,
					  const DiscretisedDensityOnCartesianGrid<3,float>& smooth_image_as_density,
					  int& scatt_points, const float att_threshold, 
					  const float lower_energy_threshold, const float upper_energy_threshold, const float resolution,
					  const bool use_cache,
					  const bool use_sintheta, 
					  const bool use_polarization,  const int scatter_level, const bool random)
{		
	const ProjDataInfoCylindricalNoArcCorr &proj_data_info = 
		dynamic_cast<const ProjDataInfoCylindricalNoArcCorr&> 
		(*proj_data.get_proj_data_info_ptr());

	// fill in scatt_points_vector
	sample_scatter_points(smooth_image_as_density,scatt_points,att_threshold,random);
	// find final size of detection_points_vector
	total_detectors = 
	   proj_data_info.get_scanner_ptr()->get_num_rings()*
	   proj_data_info.get_scanner_ptr()->get_num_detectors_per_ring ();
	// reserve space to avoid reallocation, but the actual size will grow dynamically
	detection_points_vector.reserve(total_detectors);
#if 0
	{
		fstream scatter_points_file("scatter_points.txt", ios::out); //output file //
		if(!scatter_points_file)    
		  warning("Cannot open scatter_points file.\n") ;	              
		else
		  scatter_points_file << scatt_points_vector;
		cerr << scatt_points_vector.size() << " scatter points selected!" << endl;				
	}
#endif
	CartesianCoordinate3D<float> detector_coord_A, detector_coord_B;
    Bin bin;
    Bin bin2;
	
	/////////////////// SCATTER ESTIMATION TIME /////////////////	
	CPUTimer bin_timer;
	int bin_counter = 0;
	bin_timer.start();
	int axial_bins = 0 ;
	for (bin.segment_num()=proj_data_info.get_min_segment_num();
	bin.segment_num()<=proj_data_info.get_max_segment_num();
	++bin.segment_num())	
		axial_bins += proj_data_info.get_num_axial_poss(bin.segment_num());	
    const int total_bins = proj_data_info.get_num_views() * axial_bins *
		proj_data_info.get_num_tangential_poss()	;
	/////////////////// end SCATTER ESTIMATION TIME /////////////////

	/* Currently, proj_data_info.find_cartesian_coordinates_of_detection() returns
	   coordinate in a coordinate system where z=0 in the first ring of the scanner.
	   We want to shift this to a coordinate system where z=0 in the middle 
	   of the scanner.
	   We can use get_m() as that uses the 'middle of the scanner' system.
	   (sorry)
	   */
#ifndef NDEBUG
	// check above statement
	proj_data_info.find_cartesian_coordinates_of_detection(
						detector_coord_A,detector_coord_B,Bin(0,0,0,0));
	assert(detector_coord_A.z()==0);
	assert(detector_coord_B.z()==0);
	// check that get_m refers to the middle of the scanner
	const float m_first =proj_data_info.get_m(Bin(0,0,proj_data_info.get_min_axial_pos_num(0),0));
	const float m_last =proj_data_info.get_m(Bin(0,0,proj_data_info.get_max_axial_pos_num(0),0));
    assert(fabs(m_last + m_first)<m_last*10E-4);
#endif
	const CartesianCoordinate3D<float>  
			shift_detector_coordinates_to_origin(proj_data_info.get_m(Bin(0,0,0,0)),0, 0);
	
        float total_scatter = 0 ;
        double double_scatter_ratio1 = 0;
        double double_scatter_ratio2 = 0;

	for (bin.segment_num()=proj_data_info.get_min_segment_num(),bin2.segment_num()=proj_data_info.get_min_segment_num();
	bin.segment_num()<=proj_data_info.get_max_segment_num();
	++bin.segment_num(),++bin2.segment_num())

		for (bin.view_num()=proj_data_info.get_min_view_num(),bin2.view_num()=proj_data_info.get_min_view_num();
		bin.view_num()<=proj_data_info.get_max_view_num();
		++bin.view_num(),++bin2.view_num())
		{
			Viewgram<float> viewgram =
				proj_data.get_empty_viewgram(bin.view_num(), bin.segment_num());
			Viewgram<float> viewgram2 =
				proj_data2.get_empty_viewgram(bin2.view_num(), bin2.segment_num());
			
			for (bin.axial_pos_num()=proj_data_info.get_min_axial_pos_num(bin.segment_num()),
			bin2.axial_pos_num()=proj_data_info.get_min_axial_pos_num(bin.segment_num());
			bin.axial_pos_num()<=proj_data_info.get_max_axial_pos_num(bin.segment_num());
			++bin.axial_pos_num(),++bin2.axial_pos_num())

				for (bin.tangential_pos_num()=proj_data_info.get_min_tangential_pos_num(),
                                bin2.tangential_pos_num()=proj_data_info.get_min_tangential_pos_num();
				bin.tangential_pos_num()<=proj_data_info.get_max_tangential_pos_num();
				++bin.tangential_pos_num(),++bin2.tangential_pos_num())
				{  
					// have now all bin coordinates
					proj_data_info.find_cartesian_coordinates_of_detection(
						detector_coord_A,detector_coord_B,bin);
					const unsigned det_num_A =
					  find_in_detection_points_vector(detector_coord_A + 
					                                  shift_detector_coordinates_to_origin);
					const unsigned det_num_B =
					  find_in_detection_points_vector(detector_coord_B + 
					                                  shift_detector_coordinates_to_origin);
				
					//Temporary save in the scatter sinogram
					if(scatter_level%10==0)
					bin.set_bin_value(scatter_estimate_for_none_scatter_point(
						image_as_activity, image_as_density, 
						det_num_A, det_num_B,
						lower_energy_threshold, 
						upper_energy_threshold,resolution));    
//It is not devided by the prompting anlge. This is expected to take place in the normalisation procedure.	
					else if(scatter_level==120)
					bin.set_bin_value(scatter_estimate_for_none_scatter_point(
						image_as_activity, image_as_density, 
						det_num_A, det_num_B,
						lower_energy_threshold, 
						upper_energy_threshold,resolution)+
						scatter_estimate_for_all_scatter_points(
						image_as_activity,
						image_as_density,
						det_num_A, 
						det_num_B,
						lower_energy_threshold,
						upper_energy_threshold,
						resolution,
						use_cache,scatter_level));	
					else {
					 bin.set_bin_value(scatter_estimate_for_all_scatter_points_splitted(
						image_as_activity,
						image_as_density,
						smooth_image_as_density,
						det_num_A, 
						det_num_B,
						lower_energy_threshold,
						upper_energy_threshold,
						resolution,
						use_cache,use_sintheta, use_polarization,scatter_level,1));
                                         bin2.set_bin_value(scatter_estimate_for_all_scatter_points_splitted(
						image_as_activity,
						image_as_density,
						smooth_image_as_density,
						det_num_A, 
						det_num_B,
						lower_energy_threshold,
						upper_energy_threshold,
						resolution,
						use_cache,use_sintheta, use_polarization,scatter_level,2));
                                        }

					viewgram[bin.axial_pos_num()][bin.tangential_pos_num()] =
						bin.get_bin_value();
					viewgram2[bin2.axial_pos_num()][bin2.tangential_pos_num()] =
						bin2.get_bin_value();
					total_scatter = total_scatter + 
                                                        bin.get_bin_value() +
                                                        bin2.get_bin_value();
					
					++bin_counter;
				}			    	
				proj_data.set_viewgram(viewgram);
				proj_data2.set_viewgram(viewgram2);
				/////////////////// SCATTER ESTIMATION TIME /////////////////												 						
				{				
					static double previous_timer = 0 ;		
					static int previous_bin_count = 0 ;
					cerr << bin_counter << " bins  Total time elapsed "
						 << bin_timer.value() << " sec \tTime remaining about "
						 << (bin_timer.value()-previous_timer)*(total_bins-bin_counter)
						/(bin_counter-previous_bin_count)/60						
						<< " minutes\n";				
					previous_timer = bin_timer.value() ;
					previous_bin_count = bin_counter ;
				}						
				/////////////////// end SCATTER ESTIMATION TIME /////////////////
		}
		bin_timer.stop();		
		writing_time(bin_timer.value(), scatt_points_vector.size(),	scatter_level, total_scatter);

		if (detection_points_vector.size() != static_cast<unsigned int>(total_detectors))
		  warning("Expected num detectors: %d, but found %d\n",
			  total_detectors, detection_points_vector.size());
}
	END_NAMESPACE_STIR
		
