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
#include <DICe_Image.h>

#include <random>

#include <Teuchos_RCP.hpp>
#include <Teuchos_oblackholestream.hpp>
#include <Teuchos_TimeMonitor.hpp>

#include <iostream>

using namespace DICe;

work_t intens(const work_t & x, const work_t & y){
  static work_t gamma = 0.2;
  return (255.0*0.5) + 0.5*255.0*std::sin(gamma*x)*std::cos(gamma*y);
}

int main(int argc, char *argv[]) {

  DICe::initialize(argc, argv);

  Teuchos::RCP<Teuchos::Time> bilinear_time  = Teuchos::TimeMonitor::getNewCounter("bilinear");
  Teuchos::RCP<Teuchos::Time> bicubic_time   = Teuchos::TimeMonitor::getNewCounter("bicubic");
  Teuchos::RCP<Teuchos::Time> keys_time      = Teuchos::TimeMonitor::getNewCounter("keys fourth");

  // only print output if args are given (for testing the output is quiet)
  int_t iprint     = argc - 1;
  int_t errorFlag  = 0;
  Teuchos::RCP<std::ostream> outStream;
  Teuchos::oblackholestream bhs; // outputs nothing
  if (iprint > 0)
    outStream = Teuchos::rcp(&std::cout, false);
  else
    outStream = Teuchos::rcp(&bhs, false);

  *outStream << "--- Begin test ---" << std::endl;

  // create a bi-linear image from an array
  *outStream << "creating an image from intensity function" << std::endl;
  const int_t w = 1000;
  const int_t h = 500;
  Teuchos::ArrayRCP<work_t> intensities(w*h,0.0);
  // populate the intensities with a sin/cos function
  for(int_t y=0;y<h;++y){
    for(int_t x=0;x<w;++x){
      intensities[y*w+x] = intens(x,y);
    }
  }
  Teuchos::RCP<Scalar_Image> img = Teuchos::rcp(new Scalar_Image(w,h,intensities));
  //array_img->write("opt_interp_ref.tif");

  // do tons of interpolations at random points
  std::default_random_engine generator;
  const work_t mean = 0.0;
  const work_t std_dev = 0.25;
  std::normal_distribution<work_t> distribution(mean,std_dev);
  work_t error = 0.0;
  *outStream << "running bilinear" << std::endl;
  {
    Teuchos::TimeMonitor bilinear_time_monitor(*bilinear_time);
    for(int_t it=0;it<100;++it){
      for(int_t j=10;j<h-10;++j){
        for(int_t i=10;i<h-10;++i){
          const work_t x = i + distribution(generator);
          const work_t y = j + distribution(generator);
          const work_t value = img->interpolate_bilinear(x,y);
          const work_t exact = intens(x,y);
          error += (value - exact)*(value - exact);
        }
      }
    }
    error = std::sqrt(error);
    *outStream << "error:        " << error << std::endl;
  }
  if(error > 2000.0){
    *outStream << "Error: bilinear interpolant accumulated error too high" << std::endl;
    errorFlag++;
  }
  error = 0.0;
  *outStream << "running bicubic" << std::endl;
  {
    Teuchos::TimeMonitor bicubic_time_monitor(*bicubic_time);
    for(int_t it=0;it<100;++it){
      for(int_t j=10;j<h-10;++j){
        for(int_t i=10;i<h-10;++i){
          const work_t x = i + distribution(generator);
          const work_t y = j + distribution(generator);
          const work_t value = img->interpolate_bicubic(x,y);
          const work_t exact = intens(x,y);
          error += (value - exact)*(value - exact);
        }
      }
    }
    error = std::sqrt(error);
    *outStream << "error:        " << error << std::endl;
  }
  if(error > 50.0){
    *outStream << "Error: bicubic interpolant accumulated error too high" << std::endl;
    errorFlag++;
  }
  error = 0.0;
  *outStream << "running keys-fourth" << std::endl;
  {
    Teuchos::TimeMonitor keys_time_monitor(*keys_time);
    for(int_t it=0;it<100;++it){
      for(int_t j=10;j<h-10;++j){
        for(int_t i=10;i<h-10;++i){
          const work_t x = i + distribution(generator);
          const work_t y = j + distribution(generator);
          const work_t value = img->interpolate_keys_fourth(x,y);
          const work_t exact = intens(x,y);
          error += (value - exact)*(value - exact);
        }
      }
    }
    error = std::sqrt(error);
    *outStream << "error:        " << error << std::endl;
  }
  if(error > 2.0){
    *outStream << "Error: keys_fourth interpolant accumulated error too high" << std::endl;
    errorFlag++;
  }
  Teuchos::TimeMonitor::summarize(*outStream,false,true,false/*zero timers*/);

  *outStream << "--- End test ---" << std::endl;

  DICe::finalize();

  if (errorFlag != 0)
    std::cout << "End Result: TEST FAILED\n";
  else
    std::cout << "End Result: TEST PASSED\n";

  return 0;

}

