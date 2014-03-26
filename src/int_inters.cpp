/*!
 * \file int_inters.cpp
 * \brief _____________________________
 * \author - Original code: SD++ developed by Patrice Castonguay, Antony Jameson,
 *                          Peter Vincent, David Williams (alphabetical by surname).
 *         - Current development: Aerospace Computing Laboratory (ACL) directed
 *                                by Prof. Jameson. (Aero/Astro Dept. Stanford University).
 * \version 1.0.0
 *
 * HiFiLES (High Fidelity Large Eddy Simulation).
 * Copyright (C) 2013 Aerospace Computing Laboratory.
 */

#include <iostream>
#include <cmath>

#include "../include/global.h"
#include "../include/array.h"
#include "../include/inters.h"
#include "../include/int_inters.h"
#include "../include/geometry.h"
#include "../include/solver.h"
#include "../include/output.h"
#include "../include/flux.h"
#include "../include/error.h"

#if defined _GPU
#include "../include/cuda_kernels.h"
#endif

#ifdef _MPI
#include "mpi.h"
#endif

using namespace std;

// #### constructors ####

// default constructor

int_inters::int_inters()
{
  order=run_input.order;
  viscous=run_input.viscous;
  LES=run_input.LES;
}

int_inters::~int_inters() { }

// #### methods ####

// setup inters

void int_inters::setup(int in_n_inters,int in_inter_type,int in_run_type)
{

  (*this).setup_inters(in_n_inters,in_inter_type,in_run_type);

  if (in_run_type==0)
    {
      disu_fpts_r.setup(n_fpts_per_inter,n_inters,n_fields);
      norm_tconf_fpts_r.setup(n_fpts_per_inter,n_inters,n_fields);
      detjac_fpts_r.setup(n_fpts_per_inter,n_inters);
      mag_tnorm_dot_inv_detjac_mul_jac_fpts_r.setup(n_fpts_per_inter,n_inters);

      delta_disu_fpts_r.setup(n_fpts_per_inter,n_inters,n_fields);

      if(viscous)
        {
          grad_disu_fpts_r.setup(n_fpts_per_inter,n_inters,n_fields,n_dims);
        }
    }

}

// set interior interface
void int_inters::set_interior(int in_inter, int in_ele_type_l, int in_ele_type_r, int in_ele_l, int in_ele_r, int in_local_inter_l, int in_local_inter_r, int rot_tag, int in_run_type, struct solution* FlowSol)
{
  int i,j,k;
  int i_rhs,j_rhs;

  if (in_run_type==0)
    {
      get_lut(rot_tag);

      for(i=0;i<n_fields;i++)
        {
          for(j=0;j<n_fpts_per_inter;j++)
            {
              j_rhs=lut(j);

              disu_fpts_l(j,in_inter,i)=get_disu_fpts_ptr(in_ele_type_l,in_ele_l,i,in_local_inter_l,j,FlowSol);
              disu_fpts_r(j,in_inter,i)=get_disu_fpts_ptr(in_ele_type_r,in_ele_r,i,in_local_inter_r,j_rhs,FlowSol);

              norm_tconf_fpts_l(j,in_inter,i)=get_norm_tconf_fpts_ptr(in_ele_type_l,in_ele_l,i,in_local_inter_l,j,FlowSol);
              norm_tconf_fpts_r(j,in_inter,i)=get_norm_tconf_fpts_ptr(in_ele_type_r,in_ele_r,i,in_local_inter_r,j_rhs,FlowSol);

              if(viscous)
                {

                  delta_disu_fpts_l(j,in_inter,i)=get_delta_disu_fpts_ptr(in_ele_type_l,in_ele_l,i,in_local_inter_l,j,FlowSol);
                  delta_disu_fpts_r(j,in_inter,i)=get_delta_disu_fpts_ptr(in_ele_type_r,in_ele_r,i,in_local_inter_r,j_rhs,FlowSol);

                  for (int k=0;k<n_dims;k++) {
                      grad_disu_fpts_l(j,in_inter,i,k) = get_grad_disu_fpts_ptr(in_ele_type_l,in_ele_l,in_local_inter_l,i,k,j,FlowSol);
                      grad_disu_fpts_r(j,in_inter,i,k) = get_grad_disu_fpts_ptr(in_ele_type_r,in_ele_r,in_local_inter_r,i,k,j_rhs,FlowSol);
                    }

                }

              // Subgrid-scale flux
              if(LES) {
                for (int k=0;k<n_dims;k++) {
                  sgsf_fpts_l(j,in_inter,i,k) = get_sgsf_fpts_ptr(in_ele_type_l,in_ele_l,in_local_inter_l,i,k,j,FlowSol);
                  sgsf_fpts_r(j,in_inter,i,k) = get_sgsf_fpts_ptr(in_ele_type_r,in_ele_r,in_local_inter_r,i,k,j_rhs,FlowSol);
                }
              }
            }
        }

      for(i=0;i<n_fpts_per_inter;i++)
        {
          i_rhs=lut(i);

          mag_tnorm_dot_inv_detjac_mul_jac_fpts_l(i,in_inter)=get_mag_tnorm_dot_inv_detjac_mul_jac_fpts_ptr(in_ele_type_l,in_ele_l,in_local_inter_l,i,FlowSol);
          mag_tnorm_dot_inv_detjac_mul_jac_fpts_r(i,in_inter)=get_mag_tnorm_dot_inv_detjac_mul_jac_fpts_ptr(in_ele_type_r,in_ele_r,in_local_inter_r,i_rhs,FlowSol);

          for(j=0;j<n_dims;j++)
            {
              norm_fpts(i,in_inter,j)=get_norm_fpts_ptr(in_ele_type_l,in_ele_l,in_local_inter_l,i,j,FlowSol);
            }
        }
    }
}

// move all from cpu to gpu

void int_inters::mv_all_cpu_gpu(void)
{
#ifdef _GPU

  disu_fpts_l.mv_cpu_gpu();
  norm_tconf_fpts_l.mv_cpu_gpu();
  mag_tnorm_dot_inv_detjac_mul_jac_fpts_l.mv_cpu_gpu();
  norm_fpts.mv_cpu_gpu();

  disu_fpts_r.mv_cpu_gpu();
  norm_tconf_fpts_r.mv_cpu_gpu();
  mag_tnorm_dot_inv_detjac_mul_jac_fpts_r.mv_cpu_gpu();

  //detjac_fpts_r.mv_cpu_gpu();
  //detjac_fpts_l.mv_cpu_gpu();

  delta_disu_fpts_l.mv_cpu_gpu();
  delta_disu_fpts_r.mv_cpu_gpu();

  if(viscous)
    {
      grad_disu_fpts_l.mv_cpu_gpu();
      //norm_tconvisf_fpts_l.mv_cpu_gpu();

      grad_disu_fpts_r.mv_cpu_gpu();
      //norm_tconvisf_fpts_r.mv_cpu_gpu();
    }

  sgsf_fpts_l.mv_cpu_gpu();
  sgsf_fpts_r.mv_cpu_gpu();

#endif
}

// calculate normal transformed continuous inviscid flux at the flux points
void int_inters::calc_norm_tconinvf_fpts(void)
{

#ifdef _CPU
  array<double> norm(n_dims), fn(n_fields);

  //viscous
  array<double> u_c(n_fields);

  for(int i=0;i<n_inters;i++)
    {
      for(int j=0;j<n_fpts_per_inter;j++)
        {

          // calculate discontinuous solution at flux points
          for(int k=0;k<n_fields;k++) {
              temp_u_l(k)=(*disu_fpts_l(j,i,k));
              temp_u_r(k)=(*disu_fpts_r(j,i,k));
            }

          // storing normal components
          for (int m=0;m<n_dims;m++)
            {
              norm(m) = *norm_fpts(j,i,m);
            }


          // Calling Riemann solver
          if (run_input.riemann_solve_type==0) // Rusanov
            {
              // calculate flux from discontinuous solution at flux points
              if(n_dims==2) {
                  calc_invf_2d(temp_u_l,temp_f_l);
                  calc_invf_2d(temp_u_r,temp_f_r);
                }
              else if(n_dims==3) {
                  calc_invf_3d(temp_u_l,temp_f_l);
                  calc_invf_3d(temp_u_r,temp_f_r);
                }
              else
                FatalError("ERROR: Invalid number of dimensions ... ");

              rusanov_flux(temp_u_l,temp_u_r,temp_f_l,temp_f_r,norm,fn,n_dims,n_fields,run_input.gamma);
            }
          else if (run_input.riemann_solve_type==1) { // Lax-Friedrich
              lax_friedrich(temp_u_l,temp_u_r,norm,fn,n_dims,n_fields,run_input.lambda,run_input.wave_speed);
            }
          else if (run_input.riemann_solve_type==2) { // ROE
              roe_flux(temp_u_l,temp_u_r,norm,fn,n_dims,n_fields,run_input.gamma);
            }
          else
            FatalError("Riemann solver not implemented");

          // Transform back to reference space
          for(int k=0;k<n_fields;k++) {
              (*norm_tconf_fpts_l(j,i,k))=fn(k)*(*mag_tnorm_dot_inv_detjac_mul_jac_fpts_l(j,i));
              (*norm_tconf_fpts_r(j,i,k))=-fn(k)*(*mag_tnorm_dot_inv_detjac_mul_jac_fpts_r(j,i));
            }

          if(viscous)
            {
              // Calling viscous riemann solver
              if (run_input.vis_riemann_solve_type==0)
                ldg_solution(0,temp_u_l,temp_u_r,u_c,run_input.pen_fact,norm);
              else
                FatalError("Viscous Riemann solver not implemented");

              for(int k=0;k<n_fields;k++) {
                  *delta_disu_fpts_l(j,i,k) = (u_c(k) - temp_u_l(k));
                  *delta_disu_fpts_r(j,i,k) = (u_c(k) - temp_u_r(k));
                }
            }

        }
    }
#endif

#ifdef _GPU
  if (n_inters!=0)
    {
      calc_norm_tconinvf_fpts_gpu_kernel_wrapper(n_fpts_per_inter,n_dims,n_fields,n_inters,disu_fpts_l.get_ptr_gpu(),disu_fpts_r.get_ptr_gpu(),norm_tconf_fpts_l.get_ptr_gpu(),norm_tconf_fpts_r.get_ptr_gpu(),mag_tnorm_dot_inv_detjac_mul_jac_fpts_l.get_ptr_gpu(),mag_tnorm_dot_inv_detjac_mul_jac_fpts_r.get_ptr_gpu(),norm_fpts.get_ptr_gpu(),run_input.riemann_solve_type,delta_disu_fpts_l.get_ptr_gpu(),delta_disu_fpts_r.get_ptr_gpu(),run_input.gamma,run_input.pen_fact,viscous,run_input.vis_riemann_solve_type,run_input.wave_speed(0),run_input.wave_speed(1),run_input.wave_speed(2),run_input.lambda);
    }

#endif





}


// calculate normal transformed continuous viscous flux at the flux points

void int_inters::calc_norm_tconvisf_fpts(void)
{

#ifdef _CPU
  array<double> norm(n_dims), fn(n_fields);

  for(int i=0;i<n_inters;i++)
    {
      for(int j=0;j<n_fpts_per_inter;j++)
        {
          // obtain discontinuous solution at flux points

          for(int k=0;k<n_fields;k++)
            {
              temp_u_l(k)=(*disu_fpts_l(j,i,k));
              temp_u_r(k)=(*disu_fpts_r(j,i,k));
            }

          // obtain gradient of discontinuous solution at flux points

          for(int k=0;k<n_dims;k++)
            {
              for(int l=0;l<n_fields;l++)
                {
                  temp_grad_u_l(l,k) = *grad_disu_fpts_l(j,i,l,k);
                  temp_grad_u_r(l,k) = *grad_disu_fpts_r(j,i,l,k);
                }
            }

          // calculate flux from discontinuous solution at flux points

          if(n_dims==2)
            {
              calc_visf_2d(temp_u_l,temp_grad_u_l,temp_f_l);
              calc_visf_2d(temp_u_r,temp_grad_u_r,temp_f_r);
            }
          else if(n_dims==3)
            {
              calc_visf_3d(temp_u_l,temp_grad_u_l,temp_f_l);
              calc_visf_3d(temp_u_r,temp_grad_u_r,temp_f_r);
            }
          else
            FatalError("ERROR: Invalid number of dimensions ... ");

          // If LES, get SGS flux and add to viscous flux
          if(LES) {
            for(int k=0;k<n_dims;k++) {
              for(int l=0;l<n_fields;l++) {
                // pointer to subgrid-scale interface fluxes
                temp_sgsf_l(l,k) = *sgsf_fpts_l(j,i,l,k);
                temp_sgsf_r(l,k) = *sgsf_fpts_r(j,i,l,k);

                // Add SGS fluxes to viscous fluxes
                temp_f_l(l,k) += temp_sgsf_l(l,k);
                temp_f_r(l,k) += temp_sgsf_r(l,k);
              }
            }
          }

          // storing normal components
          for (int m=0;m<n_dims;m++)
            norm(m) = *norm_fpts(j,i,m);

          // Calling viscous riemann solver
          if (run_input.vis_riemann_solve_type==0)
            ldg_flux(0,temp_u_l,temp_u_r,temp_f_l,temp_f_r,norm,fn,n_dims,n_fields,run_input.tau,run_input.pen_fact);
          else
            FatalError("Viscous Riemann solver not implemented");

          // Transform back to reference space
          for(int k=0;k<n_fields;k++) {
              (*norm_tconf_fpts_l(j,i,k))+=  fn(k)*(*mag_tnorm_dot_inv_detjac_mul_jac_fpts_l(j,i));
              (*norm_tconf_fpts_r(j,i,k))+= -fn(k)*(*mag_tnorm_dot_inv_detjac_mul_jac_fpts_r(j,i));
            }

        }
    }

#endif

#ifdef _GPU
  if (n_inters!=0)
    calc_norm_tconvisf_fpts_gpu_kernel_wrapper(n_fpts_per_inter,n_dims,n_fields,n_inters,disu_fpts_l.get_ptr_gpu(),disu_fpts_r.get_ptr_gpu(),grad_disu_fpts_l.get_ptr_gpu(),grad_disu_fpts_r.get_ptr_gpu(),norm_tconf_fpts_l.get_ptr_gpu(),norm_tconf_fpts_r.get_ptr_gpu(),mag_tnorm_dot_inv_detjac_mul_jac_fpts_l.get_ptr_gpu(),mag_tnorm_dot_inv_detjac_mul_jac_fpts_r.get_ptr_gpu(),norm_fpts.get_ptr_gpu(),sgsf_fpts_l.get_ptr_gpu(),run_input.riemann_solve_type,run_input.vis_riemann_solve_type,run_input.pen_fact,run_input.tau,run_input.gamma,run_input.prandtl,run_input.rt_inf,run_input.mu_inf,run_input.c_sth,run_input.fix_vis,run_input.equation,run_input.diff_coeff);
#endif
}

