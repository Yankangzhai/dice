// @HEADER
// ************************************************************************
//
//               Digital Image Correlation Engine (DICe)
//                 Copyright 2015 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY NTESS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NTESS OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact: Dan Turner (dzturne@sandia.gov)
//
// ************************************************************************
// @HEADER
#include <DICe.h>
#include <DICe_NetCDF.h>
#include <DICe_Image.h>

#include <Teuchos_RCP.hpp>
#include <Teuchos_oblackholestream.hpp>
#include <Teuchos_ParameterList.hpp>

#include <iostream>

#include "netcdf.h"

using namespace DICe;

int main(int argc, char *argv[]) {

  DICe::initialize(argc, argv);

  // only print output if args are given (for testing the output is quiet)
  int_t iprint     = argc - 1;
  int_t errorFlag  = 0;
  work_t errorTol = 0.001;
  Teuchos::RCP<std::ostream> outStream;
  Teuchos::oblackholestream bhs; // outputs nothing
  if (iprint > 0)
    outStream = Teuchos::rcp(&std::cout, false);
  else
    outStream = Teuchos::rcp(&bhs, false);

  *outStream << "--- Begin test ---" << std::endl;

  // create a netcdf reader:
  Teuchos::RCP<Scalar_Image> img = Teuchos::rcp(new Scalar_Image("../images/goes14.2016.222.215844.BAND_01.nc"));
  //img->write("netcdf_image.tif");
  //img->write("netcdf.rawi");

  // read the gold version of this image
  Teuchos::RCP<Scalar_Image> gold_img = Teuchos::rcp(new Scalar_Image("../images/netcdf.rawi"));
  const work_t diff = img->diff(gold_img);
  *outStream << "gold diff " << diff << std::endl;
  if(std::abs(diff) > errorTol){
    errorFlag++;
    *outStream << "Error, the NetCDF image was not read correctly" << std::endl;
  }
  Teuchos::RCP<Teuchos::ParameterList> imgParams = Teuchos::rcp(new Teuchos::ParameterList());
  imgParams->set(DICe::subimage_width,496);
  imgParams->set(DICe::subimage_height,370);
  imgParams->set(DICe::subimage_offset_x,1235);
  imgParams->set(DICe::subimage_offset_y,491);
  Scalar_Image subImg("../images/goes14.2016.222.215844.BAND_01.nc",imgParams);
  subImg.write("netcdf_sub_image.tif");
  //subImg.write("netcdf_sub_image.rawi");
  Teuchos::RCP<Scalar_Image> gold_sub_img = Teuchos::rcp(new Scalar_Image("../images/netcdf_sub_image.rawi"));
  const work_t sub_diff = subImg.diff(gold_sub_img);
  //gold_sub_img->write("gold.tif");
  *outStream << "sub gold diff " << sub_diff << std::endl;
  if(std::abs(sub_diff) > errorTol){
    errorFlag++;
    *outStream << "Error, the NetCDF sub image was not read correctly" << std::endl;
  }

  // test writing fields out to a netcdf file:
  int_t img_w = gold_sub_img->width();
  int_t img_h = gold_sub_img->height();
  // create an array that's of the dims of the image ...
  std::vector<float> data_vec(img_h*img_w);
  for(int_t j=0;j<img_h;++j){
    for(int_t i=0;i<img_w;++i){
      data_vec[j*img_w+i] = (*gold_sub_img)(i,j);
    }
  }
  std::vector<std::string> var_names;
  var_names.push_back("data");
  Teuchos::RCP<DICe::netcdf::NetCDF_Writer> netcdf_writer = Teuchos::rcp(new DICe::netcdf::NetCDF_Writer("test.nc",img_w,img_h,1,var_names));
  *outStream << "created output netcdf file " << std::endl;
  netcdf_writer->write_float_array("data",0,data_vec);

  Teuchos::RCP<Scalar_Image> test_img = Teuchos::rcp(new Scalar_Image("./test.nc"));
  test_img->write("test_img.tif");
  const work_t test_diff = test_img->diff(gold_sub_img);
  *outStream << "test diff " << test_diff << std::endl;
  if(std::abs(test_diff) > errorTol){
    errorFlag++;
    *outStream << "Error, the NetCDF image created from scratch was not read correctly" << std::endl;
  }

//  Teuchos::RCP<Teuchos::ParameterList> params = Teuchos::rcp(new Teuchos::ParameterList());
//  params->set(remove_outlier_pixels,true);
//  params->set(outlier_replacement_value,0.0);
//  params->set(spread_intensity_histogram,true);
//  Image goes16("PATH/goes16.nc",params);
//  goes16.write("goes_16.tif");
//  Image goes17("PATH/goes17.nc");
//  goes17.write("goes_17.tif");

  *outStream << "--- End test ---" << std::endl;

  DICe::finalize();

  if (errorFlag != 0)
    std::cout << "End Result: TEST FAILED\n";
  else
    std::cout << "End Result: TEST PASSED\n";

  return 0;

}

