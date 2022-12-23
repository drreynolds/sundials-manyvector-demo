/*---------------------------------------------------------------
 Programmer(s): Daniel R. Reynolds @ SMU
 ----------------------------------------------------------------
 Copyright (c) 2019, Southern Methodist University.
 All rights reserved.
 For details, see the LICENSE file.
 ----------------------------------------------------------------
 Simple 'smoke test' problem to ensure that things run and a
 constant-valued state is retained.
---------------------------------------------------------------*/

// Header files
#include <euler3D.hpp>

// Initial conditions
int initial_conditions(const realtype& t, N_Vector w, const EulerData& udata)
{
  // iterate over subdomain, setting initial condition
  long int v, i, j, k, idx;
  realtype xloc, yloc, zloc;
  realtype *rho = N_VGetSubvectorArrayPointer_MPIManyVector(w,0);
  if (check_flag((void *) rho, "N_VGetSubvectorArrayPointer (initial_conditions)", 0)) return -1;
  realtype *mx = N_VGetSubvectorArrayPointer_MPIManyVector(w,1);
  if (check_flag((void *) mx, "N_VGetSubvectorArrayPointer (initial_conditions)", 0)) return -1;
  realtype *my = N_VGetSubvectorArrayPointer_MPIManyVector(w,2);
  if (check_flag((void *) my, "N_VGetSubvectorArrayPointer (initial_conditions)", 0)) return -1;
  realtype *mz = N_VGetSubvectorArrayPointer_MPIManyVector(w,3);
  if (check_flag((void *) mz, "N_VGetSubvectorArrayPointer (initial_conditions)", 0)) return -1;
  realtype *et = N_VGetSubvectorArrayPointer_MPIManyVector(w,4);
  if (check_flag((void *) et, "N_VGetSubvectorArrayPointer (initial_conditions)", 0)) return -1;
  realtype *chem = NULL;
  if (udata.nchem > 0) {
    chem = N_VGetSubvectorArrayPointer_MPIManyVector(w,5);
    if (check_flag((void *) chem, "N_VGetSubvectorArrayPointer (initial_conditions)", 0)) return -1;
  }
  for (k=0; k<udata.nzl; k++)
    for (j=0; j<udata.nyl; j++)
      for (i=0; i<udata.nxl; i++) {
        xloc = (udata.is+i+HALF)*udata.dx + udata.xl;
        yloc = (udata.js+j+HALF)*udata.dy + udata.yl;
        zloc = (udata.ks+k+HALF)*udata.dz + udata.zl;

        // fluid initial conditions
        idx = INDX(i,j,k,udata.nxl,udata.nyl,udata.nzl);
        rho[idx] = RCONST(4.0);
        mx[idx]  = RCONST(0.5);
        my[idx]  = RCONST(0.3);
        mz[idx]  = RCONST(0.1);
        et[idx]  = RCONST(2.0);

        // tracer initial conditions
        if (udata.nchem > 0) {
          for (v=0; v<udata.nchem; v++) {
            idx = BUFINDX(v,i,j,k,udata.nchem,udata.nxl,udata.nyl,udata.nzl);
            chem[idx] = ONE*(v+1)/udata.nchem;
          }
        }
      }

  return 0;
}

// External forcing terms -- note that G was previously zeroed out,
// so only nonzero terms need to be added
int external_forces(const realtype& t, N_Vector G, const EulerData& udata)
{
  // iterate over subdomain, applying nonzero external forces
  long int i, j, k, idx;
  realtype xloc, yloc, zloc;
  realtype *Grho = N_VGetSubvectorArrayPointer_MPIManyVector(G,0);
  if (check_flag((void *) Grho, "N_VGetSubvectorArrayPointer (external_forces)", 0)) return -1;
  realtype *Gmx = N_VGetSubvectorArrayPointer_MPIManyVector(G,1);
  if (check_flag((void *) Gmx, "N_VGetSubvectorArrayPointer (external_forces)", 0)) return -1;
  realtype *Gmy = N_VGetSubvectorArrayPointer_MPIManyVector(G,2);
  if (check_flag((void *) Gmy, "N_VGetSubvectorArrayPointer (external_forces)", 0)) return -1;
  realtype *Gmz = N_VGetSubvectorArrayPointer_MPIManyVector(G,3);
  if (check_flag((void *) Gmz, "N_VGetSubvectorArrayPointer (external_forces)", 0)) return -1;
  realtype *Get = N_VGetSubvectorArrayPointer_MPIManyVector(G,4);
  if (check_flag((void *) Get, "N_VGetSubvectorArrayPointer (external_forces)", 0)) return -1;
  for (k=0; k<udata.nzl; k++)
    for (j=0; j<udata.nyl; j++)
      for (i=0; i<udata.nxl; i++) {
        xloc = (udata.is+i+HALF)*udata.dx + udata.xl;
        yloc = (udata.js+j+HALF)*udata.dy + udata.yl;
        zloc = (udata.ks+k+HALF)*udata.dz + udata.zl;
        idx = INDX(i,j,k,udata.nxl,udata.nyl,udata.nzl);
        Grho[idx] = ZERO;
        Gmx[idx]  = ZERO;
        Gmy[idx]  = ZERO;
        Gmz[idx]  = ZERO;
        Get[idx]  = ZERO;
      }
  return 0;
}

// Diagnostics output for this test
int output_diagnostics(const realtype& t, const N_Vector w, const EulerData& udata)
{
  return(0);
}

//---- end of file ----
