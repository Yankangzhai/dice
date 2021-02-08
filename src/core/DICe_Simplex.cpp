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

#include <DICe_Simplex.h>
#include <DICe_Triangulation.h>

#include <cassert>

namespace DICe {

Simplex::Simplex(const Teuchos::RCP<Teuchos::ParameterList> & params):
  max_iterations_(1000),
  tolerance_(1.0E-8),
  tiny_(1.0E-10)
{
  if(params!=Teuchos::null){
    if(params->isParameter(DICe::max_iterations)) max_iterations_ = params->get<int_t>(DICe::max_iterations);
    if(params->isParameter(DICe::tolerance)) tolerance_ = params->get<double>(DICe::tolerance);
  }
}

Status_Flag
Simplex::minimize(Teuchos::RCP<std::vector<work_t> > variables,
  Teuchos::RCP<std::vector<work_t> > deltas,
  int_t & num_iterations,
  const work_t & threshold){
  const int_t num_dofs = variables->size();
  assert(num_dofs>0);
  assert((int_t)deltas->size()==num_dofs);

  DEBUG_MSG("Conducting multidimensional simplex minimization");
  DEBUG_MSG("Max iterations: " << max_iterations_ << " tolerance: " << tolerance_);
  DEBUG_MSG("Initial guess: ");
#ifdef DICE_DEBUG_MSG
  std::cout << " POINT 0: ";
  for(int_t j=0;j<num_dofs;++j) std::cout << " " << (*variables)[j];
  std::cout << std::endl;
#endif

  // allocate temp storage for routine and initialize the simplex vertices
  std::vector<work_t> init_variables(num_dofs,0.0);
  for(int_t i=0;i<num_dofs;++i)
    init_variables[i] = (*variables)[i];

  const int_t mpts = num_dofs + 1;
  work_t * gamma_values = new work_t[mpts];
  std::vector< Teuchos::RCP<std::vector<work_t> > > points(mpts);
  for(int_t i=0;i<mpts;++i){
    points[i] = Teuchos::rcp(new std::vector<work_t>(num_dofs,0.0));
    // set the points for the initial bounding simplex
    for(int_t j=0;j<num_dofs;++j){
      (*points[i])[j] = (*variables)[j];
    }
    if(i>0)
      (*points[i])[i-1] += (*deltas)[i-1];
#ifdef DICE_DEBUG_MSG
    std::cout << " SIMPLEX POINT: ";
    for(int_t j=0;j<num_dofs;++j) std::cout << " " << (*points[i])[j];
    std::cout << std::endl;
#endif
    // evaluate gamma at these points
    for(int_t j=0;j<num_dofs;++j)
       (*variables)[j] = (*points[i])[j];
    gamma_values[i] = objective(variables);
    DEBUG_MSG("Gamma value for this point: " << gamma_values[i]);
    if(i==0&&gamma_values[i]<threshold&&gamma_values[i]>=0.0){
      num_iterations = 0;
      DEBUG_MSG("Initial variables guess is good enough (gamma < " << threshold << " for this guess)");
      return CORRELATION_SUCCESSFUL;
    }
    for(int_t j=0;j<num_dofs;++j)
       (*variables)[j] = init_variables[j];
  }

  // work variables

  int_t inhi;
  work_t ysave;
  int_t nfunk = 0;
  work_t * points_column_sums = new work_t[num_dofs];
  work_t * ptry = new work_t[num_dofs];

  // sum up the columns of the simplex vertices

  for (int_t j = 0; j < num_dofs; j++) {
    work_t sum = 0.0;
    for (int_t i = 0; i < mpts; i++) {
      sum += (*points[i])[j];
    }
    points_column_sums[j] = sum;
  }

  // simplex minimization routine
  work_t old_rtol = 0.0;
  work_t gamma_new = 0.0;
  work_t gamma_old = -1.0;
  int_t gamma_repeats = 0;
  const int_t gamma_repeats_allowed = 15;
  const work_t gamma_tol = 1.0E-8;
  int_t iteration = 0;
  for (iteration=0; iteration < max_iterations_; iteration++) {
    if( iteration >= max_iterations_-1){
      DEBUG_MSG("Simplex method max iterations exceeded");
      delete [] gamma_values;
      delete [] points_column_sums;
      delete [] ptry;
      num_iterations = iteration;
      return MAX_ITERATIONS_REACHED;
    }

    int_t ilo = 0;
    int_t ihi = gamma_values[0]>gamma_values[1] ? (inhi=1,0) : (inhi=0,1);
    for (int_t i = 0; i < mpts; i++) {
      if (gamma_values[i] <= gamma_values[ilo]) ilo = i;
        if (gamma_values[i] > gamma_values[ihi]) {
          inhi = ihi;
          ihi = i;
        } else if (gamma_values[i] > gamma_values[inhi] && i != ihi) inhi = i;
    }
    work_t rtol = 2.0*std::abs(gamma_values[ihi]-gamma_values[ilo])/(std::abs(gamma_values[ihi]) + std::abs(gamma_values[ilo]) + tiny_);
    work_t rtol_diff = std::abs(rtol - old_rtol);
    //DEBUG_MSG("rtol old: " << old_rtol << " rtol " << rtol <<  " rtol diff: " << rtol_diff );
    old_rtol = rtol;
    if(rtol_diff < tiny_){
      DEBUG_MSG("Warning: simplex optimization exiting due to tolerance stagnation");
    }
    if(std::abs(gamma_new-gamma_old) < gamma_tol){
      gamma_repeats ++;
      DEBUG_MSG("Warning: simplex optimization gamma is repeating (leads to poor convergence)");
    }
    gamma_old = gamma_new;
    if(gamma_repeats > gamma_repeats_allowed){
      DEBUG_MSG("*** Warning: simplex optimization exiting due to repeating gamma (loss of accuracy may occur)");
    }
    if (rtol < tolerance_ || rtol_diff < tolerance_ || gamma_repeats > gamma_repeats_allowed) {
      work_t dum = gamma_values[0];
      gamma_values[0] = gamma_values[ilo];
      gamma_values[ilo] = dum;
      for (int_t i = 0; i < num_dofs; i++) {
        work_t dum2 = (*points[0])[i];
        (*points[0])[i] = (*points[ilo])[i];
        (*points[ilo])[i] = dum2;
      }
      break;
    }
    nfunk += 2;

    work_t ytry,fac,fac1,fac2;

    fac = -1.0;
    fac1 = (1.0 - fac)/num_dofs;
    fac2 = fac1 - fac;

    for (int_t j = 0; j < num_dofs; j++)
      ptry[j] = points_column_sums[j]*fac1 - (*points[ihi])[j]*fac2;

    for(int_t n=0;n<num_dofs;++n)
      (*variables)[n] = ptry[n];
    ytry = objective(variables);

    if (ytry < gamma_values[ihi]) {
      gamma_values[ihi] = ytry;
      for (int_t j = 0; j < num_dofs; j++) {
        points_column_sums[j] += ptry[j] - (*points[ihi])[j];
        (*points[ihi])[j] = ptry[j];
      }
    }
    if (ytry <= gamma_values[ilo]) {
      fac = 2.0;
      fac1 = (1.0 - fac)/num_dofs;
      fac2 = fac1 - fac;
      for (int_t j = 0; j < num_dofs; j++)
        ptry[j] = points_column_sums[j]*fac1 - (*points[ihi])[j]*fac2;

      for(int_t n=0;n<num_dofs;++n)
        (*variables)[n] = ptry[n];
      ytry = objective(variables);

      if (ytry < gamma_values[ihi]) {
        gamma_values[ihi] = ytry;
        for (int_t j = 0; j < num_dofs; j++) {
          points_column_sums[j] += ptry[j] - (*points[ihi])[j];
          (*points[ihi])[j] = ptry[j];
        }
      }
    } else if (ytry >= gamma_values[inhi]) {
      ysave = gamma_values[ihi];
      fac = 0.5;
      fac1 = (1.0 - fac)/num_dofs;
      fac2 = fac1 - fac;
      for (int_t j = 0; j < num_dofs; j++)
        ptry[j] = points_column_sums[j]*fac1 - (*points[ihi])[j]*fac2;

      for(int_t n=0;n<num_dofs;++n)
        (*variables)[n] = ptry[n];
      ytry = objective(variables);

      if (ytry < gamma_values[ihi]) {
        gamma_values[ihi] = ytry;
        for (int_t j = 0; j < num_dofs; j++) {
          points_column_sums[j] += ptry[j] - (*points[ihi])[j];
          (*points[ihi])[j] = ptry[j];
        }
      }
      if (ytry >= ysave) {
        for (int_t i = 0; i < mpts; i++) {
          if (i != ilo) {
            for (int_t j = 0; j < num_dofs; j++)
              (*points[i])[j] = points_column_sums[j] = 0.5*((*points[i])[j] + (*points[ilo])[j]);
            for(int_t n=0;n<num_dofs;++n)
              (*variables)[n] = points_column_sums[n];
            gamma_values[i] = objective(variables);
          }
        }
        nfunk += num_dofs;

        for (int_t j = 0; j < num_dofs; j++) {
          work_t sum = 0.0;
          for (int_t i = 0; i < mpts; i++)
            sum += (*points[i])[j];
          points_column_sums[j] = sum;
        }
      }
    } else --nfunk;

    gamma_new = objective(variables);
#ifdef DICE_DEBUG_MSG
    std::cout << "Iteration " << iteration;
    for(int_t i=0;i<num_dofs;++i) std::cout << " " << (*variables)[i];
    std::cout << " nfunk: " << nfunk << " gamma: " << gamma_new << " rtol: " << rtol << " tol: " << tolerance_ << std::endl;
#endif
  }

  delete [] gamma_values;
  delete [] points_column_sums;
  delete [] ptry;
  num_iterations = iteration;
  return CORRELATION_SUCCESSFUL;
}

Status_Flag
Subset_Simplex::minimize(Teuchos::RCP<Local_Shape_Function> shape_function,
  int_t & num_iterations,
  const work_t & threshold){
  if(shape_function_==Teuchos::null)
    shape_function_=Teuchos::RCP<Local_Shape_Function>(shape_function);
  assert(shape_function_!=Teuchos::null);
  return Simplex::minimize(shape_function_->rcp(),shape_function_->deltas(),num_iterations,threshold);
}

Subset_Simplex::Subset_Simplex(const DICe::Objective * const obj,
  const Teuchos::RCP<Teuchos::ParameterList> & params):
  Simplex(params),
  obj_(obj)
{
  // get num dims from the objective
  assert(obj_);
}

work_t
Subset_Simplex::objective(Teuchos::RCP<std::vector<work_t> > variables){
  assert(shape_function_->num_params()==(int_t)variables->size());
  return obj_->gamma(shape_function_);
}

Homography_Simplex::Homography_Simplex(Teuchos::RCP<Image> left_img,
  Teuchos::RCP<Image> right_img,
  Triangulation * tri,
  const Teuchos::RCP<Teuchos::ParameterList> & params):
  Simplex(params),
  tri_(tri){

//  // median filter the images:
//  const intensity_t avg_left = left_img->mean();
//  const intensity_t avg_right = right_img->mean();
//  const int_t wl = left_img->width();
//  const int_t hl = left_img->height();
//  const int_t num_px_left = wl*hl;
//  const int_t wr = right_img->width();
//  const int_t hr = right_img->height();
//  const int_t num_px_right = wr*hr;
//  Teuchos::RCP<Image> mf_left = Teuchos::rcp(new Image(left_img));
//  Teuchos::ArrayRCP<intensity_t> mf_left_intens = mf_left->intensities();
//  Teuchos::RCP<Image> mf_right = Teuchos::rcp(new Image(right_img));
//  Teuchos::ArrayRCP<intensity_t> mf_right_intens = mf_right->intensities();
//  for(int_t i=0; i<num_px_left;++i){
//    if(mf_left_intens[i]<avg_left){
//      mf_left_intens[i] = 0.0;
//    }else{
//      mf_left_intens[i] = 1.0;
//    }
//  }
//  for(int_t i=0; i<num_px_right;++i){
//    if(mf_right_intens[i]<avg_right){
//      mf_right_intens[i] = 0.0;
//    }else{
//      mf_right_intens[i] = 1.0;
//    }
//  }
//  mf_left->write("median_filter_left.tif");
//  mf_right->write("median_filter_right.tif");
//  left_img_ = mf_left;
//  right_img_ = mf_right;
  left_img_ = left_img;
  right_img_ = right_img;
}

work_t
Homography_Simplex::objective(Teuchos::RCP<std::vector<work_t> > variables){

  const int_t w = left_img_->width();
  const int_t h = left_img_->height();

  tri_->set_projective_params(variables);

  work_t value = 0.0;
  work_t xr = 0.0;
  work_t yr = 0.0;
  work_t left_intens = 0.0;
  work_t right_intens = 0.0;
  for(int_t j=0.1*h;j<0.9*h;++j){
    for(int_t i=0.1*w;i<0.9*w;++i){
      tri_->project_left_to_right_sensor_coords(i,j,xr,yr);
      left_intens = (*left_img_)(i,j);
      right_intens = right_img_->interpolate_keys_fourth(xr,yr);
      //intens[j*w+i] = right_intens;
      value += (left_intens - right_intens)*(left_intens - right_intens);
    }
  }
  return value;
}

Affine_Homography_Simplex::Affine_Homography_Simplex(Teuchos::RCP<Image> left_img,
  Teuchos::RCP<Image> right_img,
  Triangulation * tri,
  const Teuchos::RCP<Teuchos::ParameterList> & params):
  Simplex(params),
  tri_(tri){
  left_img_ = left_img;
  right_img_ = right_img;
}

work_t
Affine_Homography_Simplex::objective(Teuchos::RCP<std::vector<work_t> > variables){

  const int_t w = left_img_->width();
  const int_t h = left_img_->height();

  assert(variables->size()==6);
  Teuchos::RCP<std::vector<work_t> > proj_vars = Teuchos::rcp(new std::vector<work_t>(9,0.0));
  for(size_t i=0;i<variables->size();++i)
  (*proj_vars)[i] = (*variables)[i];
  (*proj_vars)[8] = 1.0;

  tri_->set_projective_params(proj_vars);
  work_t value = 0.0;
  work_t xr = 0.0;
  work_t yr = 0.0;
  work_t left_intens = 0.0;
  work_t right_intens = 0.0;
  for(int_t j=0.1*h;j<0.9*h;++j){
    for(int_t i=0.1*w;i<0.9*w;++i){
      tri_->project_left_to_right_sensor_coords(i,j,xr,yr);
      left_intens = (*left_img_)(i,j);
      right_intens = right_img_->interpolate_keys_fourth(xr,yr);
      //intens[j*w+i] = right_intens;
      value += (left_intens - right_intens)*(left_intens - right_intens);
    }
  }
  return value;
}


Quadratic_Homography_Simplex::Quadratic_Homography_Simplex(Teuchos::RCP<Image> left_img,
  Teuchos::RCP<Image> right_img,
  const int_t & ulx,
  const int_t & uly,
  const int_t & lrx,
  const int_t & lry,
  const Teuchos::RCP<Teuchos::ParameterList> & params):
  Simplex(params),
  ulx_(ulx),
  uly_(uly),
  lrx_(lrx),
  lry_(lry){
  left_img_ = left_img;
  right_img_ = right_img;
}

work_t
Quadratic_Homography_Simplex::objective(Teuchos::RCP<std::vector<work_t> > variables){
  assert(variables->size()==12);
  assert(ulx_<lrx_);
  assert(uly_<lry_);
  assert(ulx_>=0&&ulx_<left_img_->width());
  assert(uly_>=0&&uly_<left_img_->height());
  assert(lrx_>=0&&lrx_<left_img_->width());
  assert(lry_>=0&&lry_<left_img_->height());

  work_t value = 0.0;
  work_t xr=0.0,yr=0.0,xl=0.0,yl=0.0;
  work_t left_intens = 0.0;
  work_t right_intens = 0.0;
  for(int_t j=uly_;j<=lry_;++j){
    yl = (work_t)j;
    for(int_t i=ulx_;i<=lrx_;++i){
      left_intens = (*left_img_)(i,j);
      xl = (work_t)i;
      // project the point to the right image using a quadratic interpolant
      // TODO move this to a new local shape function...
      xr = (*variables)[0]*xl + (*variables)[1]*yl + (*variables)[2]*xl*yl + (*variables)[3]*xl*xl + (*variables)[4]*yl*yl  + (*variables)[5];
      yr = (*variables)[6]*xl + (*variables)[7]*yl + (*variables)[8]*xl*yl + (*variables)[9]*xl*xl + (*variables)[10]*yl*yl + (*variables)[11];
      right_intens = right_img_->interpolate_keys_fourth(xr,yr);
      value += (left_intens - right_intens)*(left_intens - right_intens);
    }
  }
  return value;
}


Warp_Simplex::Warp_Simplex(Teuchos::RCP<Image> left_img,
  Teuchos::RCP<Image> right_img,
  Triangulation * tri,
  const Teuchos::RCP<Teuchos::ParameterList> & params):
  Simplex(params),
  tri_(tri){

//  // median filter the images:
//  const intensity_t avg_left = left_img->mean();
//  const intensity_t avg_right = right_img->mean();
//  const int_t wl = left_img->width();
//  const int_t hl = left_img->height();
//  const int_t num_px_left = wl*hl;
//  const int_t wr = right_img->width();
//  const int_t hr = right_img->height();
//  const int_t num_px_right = wr*hr;
//  Teuchos::RCP<Image> mf_left = Teuchos::rcp(new Image(left_img));
//  Teuchos::ArrayRCP<intensity_t> mf_left_intens = mf_left->intensities();
//  Teuchos::RCP<Image> mf_right = Teuchos::rcp(new Image(right_img));
//  Teuchos::ArrayRCP<intensity_t> mf_right_intens = mf_right->intensities();
//  for(int_t i=0; i<num_px_left;++i){
//    if(mf_left_intens[i]<avg_left){
//      mf_left_intens[i] = 0.0;
//    }else{
//      mf_left_intens[i] = 1.0;
//    }
//  }
//  for(int_t i=0; i<num_px_right;++i){
//    if(mf_right_intens[i]<avg_right){
//      mf_right_intens[i] = 0.0;
//    }else{
//      mf_right_intens[i] = 1.0;
//    }
//  }
//  mf_left->write("median_filter_left.tif");
//  mf_right->write("median_filter_right.tif");
//  left_img_ = mf_left;
//  right_img_ = mf_right;
  left_img_ = left_img;
  right_img_ = right_img;
}

work_t
Warp_Simplex::objective(Teuchos::RCP<std::vector<work_t> > variables){

  const int_t w = left_img_->width();
  const int_t h = left_img_->height();

  tri_->set_warp_params(variables);

  work_t value = 0.0;
  work_t xr = 0.0;
  work_t yr = 0.0;
  work_t left_intens = 0.0;
  work_t right_intens = 0.0;
  for(int_t j=0.1*h;j<0.9*h;++j){
    for(int_t i=0.1*w;i<0.9*w;++i){
      tri_->project_left_to_right_sensor_coords(i,j,xr,yr);
      left_intens = (*left_img_)(i,j);
      right_intens = right_img_->interpolate_keys_fourth(xr,yr);
      //intens[j*w+i] = right_intens;
      value += (left_intens - right_intens)*(left_intens - right_intens);
    }
  }
  return value;
}


}// End DICe Namespace
