/*---------------------------------------------------------------
 Programmer(s): Daniel R. Reynolds @ SMU
 ----------------------------------------------------------------
 Copyright (c) 2019, Southern Methodist University.
 All rights reserved.
 For details, see the LICENSE file.
 ----------------------------------------------------------------
 Implementation file for input/output utility routines.
 Input routines read problem and solver input parameters
 from specified files.  For solver parameters, this calls
 associated "set" routines to specify options to ARKode.
 Output routines compute/output shared diagnostics information,
 or write solution data to disk.
 ---------------------------------------------------------------*/

// Header files
#include <euler3D.hpp>
#include <string.h>
#include "hdf5.h"
#include "gopt.h"


#define MAX_LINE_LENGTH 512


// Load problem-defining parameters from file: root process
// reads parameters and broadcasts results to remaining
// processes
int load_inputs(int myid, int argc, char* argv[], UserData& udata,
                ARKodeParameters& opts, int& restart)
{
  int retval;
  double dbuff[23];
  long int ibuff[20];

  // disable 'restart' by default
  restart = -1;

  // root process handles command-line and file-based solver parameters, and packs send buffers
  if (myid == 0) {

    // use 'gopt' to handle parsing command-line; first define all available options
    const int nopt = 45;
    struct option options[nopt+1];
    enum iarg { ifname, ihelp, ixl, ixr, iyl, iyr, izl, izr, it0,
                itf, igam, inx, iny, inz, ixlb, ixrb, iylb,
                iyrb, izlb, izrb, icfl, inout, ishow,
                iord, idord, ibt, iadmth, imnef, imhnil, imaxst,
                isfty, ibias, igrow, ipq, ik1, ik2, ik3, iemx1,
                iemaf, ih0, ihmin, ihmax, irtol, iatol, irest};
    for (int i=0; i<nopt; i++) {
      options[i].short_name = '0';
      options[i].flags = GOPT_ARGUMENT_REQUIRED;
    }
    options[nopt].flags = GOPT_LAST;
    options[ifname].short_name = 'f';
    options[ifname].long_name = "infile";
    options[ihelp].short_name = 'h';
    options[ihelp].long_name = "help";
    options[ihelp].flags = GOPT_ARGUMENT_FORBIDDEN;
    options[ixl].long_name = "xl";
    options[ixr].long_name = "xr";
    options[iyl].long_name = "yl";
    options[iyr].long_name = "yr";
    options[izl].long_name = "zl";
    options[izr].long_name = "zr";
    options[it0].long_name = "t0";
    options[itf].long_name = "tf";
    options[igam].long_name = "gamma";
    options[inx].long_name = "nx";
    options[iny].long_name = "ny";
    options[inz].long_name = "nz";
    options[ixlb].long_name = "xlbc";
    options[ixrb].long_name = "xrbc";
    options[iylb].long_name = "ylbc";
    options[iyrb].long_name = "yrbc";
    options[izlb].long_name = "zlbc";
    options[izrb].long_name = "zrbc";
    options[icfl].long_name = "cfl";
    options[inout].long_name = "nout";
    options[ishow].long_name = "showstats";
    options[ishow].flags = GOPT_ARGUMENT_FORBIDDEN;
    options[iord].long_name = "order";
    options[idord].long_name = "dense_order";
    options[ibt].long_name = "btable";
    options[iadmth].long_name = "adapt_method";
    options[imnef].long_name = "maxnef";
    options[imhnil].long_name = "mxhnil";
    options[imaxst].long_name = "mxsteps";
    options[isfty].long_name = "safety";
    options[ibias].long_name = "bias";
    options[igrow].long_name = "growth";
    options[ipq].long_name = "pq";
    options[ik1].long_name = "k1";
    options[ik2].long_name = "k2";
    options[ik3].long_name = "k3";
    options[iemx1].long_name = "etamx1";
    options[iemaf].long_name = "etamxf";
    options[ih0].long_name = "h0";
    options[ihmin].long_name = "hmin";
    options[ihmax].long_name = "hmax";
    options[irtol].long_name = "rtol";
    options[iatol].long_name = "atol";
    options[irest].long_name = "restart";
    argc = gopt(argv, options);

    // handle help request
    if (options[ihelp].count) {
      cout << "\nEuler3D SUNDIALS ManyVector+Multirate demonstration code\n"
           << "\nUsage: " << argv[0] << " [options]\n"
           << "   -h or --help prints this message and exits the program\n"
           << "\nAvailable problem specification options (and the default if not provided):\n"
           << "   --xl=<float>         (" << udata.xl << ")\n"
           << "   --xr=<float>         (" << udata.xr << ")\n"
           << "   --yl=<float>         (" << udata.yl << ")\n"
           << "   --yr=<float>         (" << udata.yr << ")\n"
           << "   --zl=<float>         (" << udata.zl << ")\n"
           << "   --zr=<float>         (" << udata.zr << ")\n"
           << "   --t0=<float>         (" << udata.t0 << ")\n"
           << "   --tf=<float>         (" << udata.tf << ")\n"
           << "   --gamma=<float>      (" << udata.gamma << ")\n"
           << "   --nx=<int>           (" << udata.nx << ")\n"
           << "   --ny=<int>           (" << udata.ny << ")\n"
           << "   --nz=<int>           (" << udata.nz << ")\n"
           << "   --xlbc=<int>         (" << udata.xlbc << ")\n"
           << "   --xrbc=<int>         (" << udata.xrbc << ")\n"
           << "   --ylbc=<int>         (" << udata.ylbc << ")\n"
           << "   --yrbc=<int>         (" << udata.yrbc << ")\n"
           << "   --zlbc=<int>         (" << udata.zlbc << ")\n"
           << "   --zrbc=<int>         (" << udata.zrbc << ")\n"
           << "\nThe preceding 6 arguments allow any of the following boundary condition types:\n"
           << "   " << BC_PERIODIC <<" = periodic\n"
           << "   " << BC_NEUMANN <<" = homogeneous Neumann (zero gradient)\n"
           << "   " << BC_DIRICHLET <<" = homogeneous Dirichlet,\n"
           << "   " << BC_REFLECTING <<" = reflecting,\n"
           << "\nAvailable run options (and the default if not provided):\n"
           << "   --nout=<int>         (" << udata.nout << ")\n"
           << "   --showstats          to enable (disabled)\n"
           << "   --restart=<int>      output number to restart from: output-<num>.hdf5 (disabled)\n"
           << "\nAvailable time-stepping options (and the default if not provided):\n"
           << "   --cfl=<float>        (" << udata.cfl << ")\n"
           << "   --order=<int>        (" << opts.order << ")\n"
           << "   --dense_order=<int>  (" << opts.dense_order << ")\n"
           << "   --btable=<int>       (" << opts.btable << ")\n"
           << "   --adapt_method=<int> (" << opts.adapt_method << ")\n"
           << "   --maxnef=<int>       (" << opts.maxnef << ")\n"
           << "   --mxhnil=<int>       (" << opts.mxhnil << ")\n"
           << "   --mxsteps=<int>      (" << opts.mxsteps << ")\n"
           << "   --safety=<float>     (" << opts.safety << ")\n"
           << "   --bias=<float>       (" << opts.bias << ")\n"
           << "   --growth=<float>     (" << opts.growth << ")\n"
           << "   --pq=<int>           (" << opts.pq << ")\n"
           << "   --k1=<float>         (" << opts.k1 << ")\n"
           << "   --k2=<float>         (" << opts.k2 << ")\n"
           << "   --k3=<float>         (" << opts.k3 << ")\n"
           << "   --etamx1=<float>     (" << opts.etamx1 << ")\n"
           << "   --etamxf=<float>     (" << opts.etamxf << ")\n"
           << "   --h0=<float>         (" << opts.h0 << ")\n"
           << "   --hmin=<float>       (" << opts.hmin << ")\n"
           << "   --hmax=<float>       (" << opts.hmax << ")\n"
           << "   --rtol=<float>       (" << opts.rtol << ")\n"
           << "   --atol=<float>       (" << opts.atol << ")\n"
           << "\nAlternately, all of these options may be specified in a single\n"
           << "input file (with command-line arguments taking precedence if an\n"
           << "option is multiply-defined) via:"
           << "   -f <fname> or --infile=<fname>\n\n\n";
      return(1);
    }

    // if an input file was specified, read that here
    if (options[ifname].count) {
      char line[MAX_LINE_LENGTH];
      FILE *FID=NULL;
      FID = fopen(options[ifname].argument,"r");
      if (check_flag((void *) FID, "fopen (load_inputs)", 0)) return(-1);
      while (fgets(line, MAX_LINE_LENGTH, FID) != NULL) {

        /* initialize return flag for line */
        retval = 0;

        /* read parameters */
        retval += sscanf(line,"xl = %lf", &udata.xl);
        retval += sscanf(line,"xr = %lf", &udata.xr);
        retval += sscanf(line,"yl = %lf", &udata.yl);
        retval += sscanf(line,"yr = %lf", &udata.yr);
        retval += sscanf(line,"zl = %lf", &udata.zl);
        retval += sscanf(line,"zr = %lf", &udata.zr);
        retval += sscanf(line,"t0 = %lf", &udata.t0);
        retval += sscanf(line,"tf = %lf", &udata.tf);
        retval += sscanf(line,"gamma = %lf", &udata.gamma);
        retval += sscanf(line,"nx = %li", &udata.nx);
        retval += sscanf(line,"ny = %li", &udata.ny);
        retval += sscanf(line,"nz = %li", &udata.nz);
        retval += sscanf(line,"xlbc = %i", &udata.xlbc);
        retval += sscanf(line,"xrbc = %i", &udata.xrbc);
        retval += sscanf(line,"ylbc = %i", &udata.ylbc);
        retval += sscanf(line,"yrbc = %i", &udata.yrbc);
        retval += sscanf(line,"zlbc = %i", &udata.zlbc);
        retval += sscanf(line,"zrbc = %i", &udata.zrbc);
        retval += sscanf(line,"cfl = %lf", &udata.cfl);
        retval += sscanf(line,"nout = %i", &udata.nout);
        retval += sscanf(line,"showstats = %i", &udata.showstats);
        retval += sscanf(line,"order = %i", &opts.order);
        retval += sscanf(line,"dense_order = %i", &opts.dense_order);
        retval += sscanf(line,"btable = %i",  &opts.btable);
        retval += sscanf(line,"adapt_method = %i", &opts.adapt_method);
        retval += sscanf(line,"maxnef = %i", &opts.maxnef);
        retval += sscanf(line,"mxhnil = %i", &opts.mxhnil);
        retval += sscanf(line,"mxsteps = %i", &opts.mxsteps);
        retval += sscanf(line,"safety = %lf", &opts.safety);
        retval += sscanf(line,"bias = %lf", &opts.bias);
        retval += sscanf(line,"growth = %lf", &opts.growth);
        retval += sscanf(line,"pq = %i", &opts.pq);
        retval += sscanf(line,"k1 = %lf", &opts.k1);
        retval += sscanf(line,"k2 = %lf", &opts.k2);
        retval += sscanf(line,"k3 = %lf", &opts.k3);
        retval += sscanf(line,"etamx1 = %lf", &opts.etamx1);
        retval += sscanf(line,"etamxf = %lf", &opts.etamxf);
        retval += sscanf(line,"h0 = %lf", &opts.h0);
        retval += sscanf(line,"hmin = %lf", &opts.hmin);
        retval += sscanf(line,"hmax = %lf", &opts.hmax);
        retval += sscanf(line,"rtol = %lf", &opts.rtol);
        retval += sscanf(line,"atol = %lf", &opts.atol);
        retval += sscanf(line,"restart = %i", &restart);

        /* if unable to read the line (and it looks suspicious) issue a warning */
        if (retval == 0 && strstr(line, "=") != NULL && line[0] != '#')
          fprintf(stderr, "load_inputs Warning: parameter line was not interpreted:\n%s", line);
      }
      fclose(FID);

    }

    // replace any current option with a value specified on the command line
    if (options[ixl].count)    udata.xl          = atof(options[ixl].argument);
    if (options[ixr].count)    udata.xr          = atof(options[ixr].argument);
    if (options[iyl].count)    udata.yl          = atof(options[iyl].argument);
    if (options[iyr].count)    udata.yr          = atof(options[iyr].argument);
    if (options[izl].count)    udata.zl          = atof(options[izl].argument);
    if (options[izr].count)    udata.zr          = atof(options[izr].argument);
    if (options[it0].count)    udata.t0          = atof(options[it0].argument);
    if (options[itf].count)    udata.tf          = atof(options[itf].argument);
    if (options[igam].count)   udata.gamma       = atof(options[igam].argument);
    if (options[inx].count)    udata.nx          = atoi(options[inx].argument);
    if (options[iny].count)    udata.ny          = atoi(options[iny].argument);
    if (options[inz].count)    udata.nz          = atoi(options[inz].argument);
    if (options[ixlb].count)   udata.xlbc        = atoi(options[ixlb].argument);
    if (options[ixrb].count)   udata.xrbc        = atoi(options[ixrb].argument);
    if (options[iylb].count)   udata.ylbc        = atoi(options[iylb].argument);
    if (options[iyrb].count)   udata.yrbc        = atoi(options[iyrb].argument);
    if (options[izlb].count)   udata.zlbc        = atoi(options[izlb].argument);
    if (options[izrb].count)   udata.zrbc        = atoi(options[izrb].argument);
    if (options[icfl].count)   udata.cfl         = atof(options[icfl].argument);
    if (options[inout].count)  udata.nout        = atoi(options[inout].argument);
    if (options[ishow].count)  udata.showstats = 1;
    if (options[iord].count)   opts.order        = atoi(options[iord].argument);
    if (options[idord].count)  opts.dense_order  = atoi(options[idord].argument);
    if (options[ibt].count)    opts.btable       = atoi(options[ibt].argument);
    if (options[iadmth].count) opts.adapt_method = atoi(options[iadmth].argument);
    if (options[imnef].count)  opts.maxnef       = atoi(options[imnef].argument);
    if (options[imhnil].count) opts.mxhnil       = atoi(options[imhnil].argument);
    if (options[imaxst].count) opts.mxsteps      = atoi(options[imaxst].argument);
    if (options[isfty].count)  opts.safety       = atof(options[isfty].argument);
    if (options[ibias].count)  opts.bias         = atof(options[ibias].argument);
    if (options[igrow].count)  opts.growth       = atof(options[igrow].argument);
    if (options[ipq].count)    opts.pq           = atoi(options[ipq].argument);
    if (options[ik1].count)    opts.k1           = atof(options[ik1].argument);
    if (options[ik2].count)    opts.k2           = atof(options[ik2].argument);
    if (options[ik3].count)    opts.k3           = atof(options[ik3].argument);
    if (options[iemx1].count)  opts.etamx1       = atof(options[iemx1].argument);
    if (options[iemaf].count)  opts.etamxf       = atof(options[iemaf].argument);
    if (options[ih0].count)    opts.h0           = atof(options[ih0].argument);
    if (options[ihmin].count)  opts.hmin         = atof(options[ihmin].argument);
    if (options[ihmax].count)  opts.hmax         = atof(options[ihmax].argument);
    if (options[irtol].count)  opts.rtol         = atof(options[irtol].argument);
    if (options[iatol].count)  opts.atol         = atof(options[iatol].argument);
    if (options[irest].count)  restart           = atoi(options[irest].argument);

    // pack buffers with final parameter values
    ibuff[0]  = udata.nx;
    ibuff[1]  = udata.ny;
    ibuff[2]  = udata.nz;
    ibuff[3]  = udata.xlbc;
    ibuff[4]  = udata.xrbc;
    ibuff[5]  = udata.ylbc;
    ibuff[6]  = udata.yrbc;
    ibuff[7]  = udata.zlbc;
    ibuff[8]  = udata.zrbc;
    ibuff[9]  = udata.nout;
    ibuff[10] = udata.showstats;
    ibuff[11] = opts.order;
    ibuff[12] = opts.dense_order;
    ibuff[13] = opts.btable;
    ibuff[14] = opts.adapt_method;
    ibuff[15] = opts.maxnef;
    ibuff[16] = opts.mxhnil;
    ibuff[17] = opts.mxsteps;
    ibuff[18] = opts.pq;
    ibuff[19] = restart;

    dbuff[0]  = udata.xl;
    dbuff[1]  = udata.xr;
    dbuff[2]  = udata.yl;
    dbuff[3]  = udata.yr;
    dbuff[4]  = udata.zl;
    dbuff[5]  = udata.zr;
    dbuff[6]  = udata.t0;
    dbuff[7]  = udata.tf;
    dbuff[8]  = udata.gamma;
    dbuff[9]  = udata.cfl;
    dbuff[10] = opts.safety;
    dbuff[11] = opts.bias;
    dbuff[12] = opts.growth;
    dbuff[13] = opts.k1;
    dbuff[14] = opts.k2;
    dbuff[15] = opts.k3;
    dbuff[16] = opts.etamx1;
    dbuff[17] = opts.etamxf;
    dbuff[18] = opts.h0;
    dbuff[19] = opts.hmin;
    dbuff[20] = opts.hmax;
    dbuff[21] = opts.rtol;
    dbuff[22] = opts.atol;
  }

  // perform broadcast and unpack results
  retval = MPI_Bcast(dbuff, 23, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  if (check_flag(&retval, "MPI_Bcast (load_inputs)", 3)) return(-1);
  retval = MPI_Bcast(ibuff, 20, MPI_LONG, 0, MPI_COMM_WORLD);
  if (check_flag(&retval, "MPI_Bcast (load_inputs)", 3)) return(-1);

  // unpack buffers
  udata.nx = ibuff[0];
  udata.ny = ibuff[1];
  udata.nz = ibuff[2];
  udata.xlbc = ibuff[3];
  udata.xrbc = ibuff[4];
  udata.ylbc = ibuff[5];
  udata.yrbc = ibuff[6];
  udata.zlbc = ibuff[7];
  udata.zrbc = ibuff[8];
  udata.nout = ibuff[9];
  udata.showstats = ibuff[10];
  opts.order = ibuff[11];
  opts.dense_order = ibuff[12];
  opts.btable = ibuff[13];
  opts.adapt_method = ibuff[14];
  opts.maxnef = ibuff[15];
  opts.mxhnil = ibuff[16];
  opts.mxsteps = ibuff[17];
  opts.pq = ibuff[18];
  restart = ibuff[19];

  udata.xl = dbuff[0];
  udata.xr = dbuff[1];
  udata.yl = dbuff[2];
  udata.yr = dbuff[3];
  udata.zl = dbuff[4];
  udata.zr = dbuff[5];
  udata.t0 = dbuff[6];
  udata.tf = dbuff[7];
  udata.gamma = dbuff[8];
  udata.cfl = dbuff[9];
  opts.safety = dbuff[10];
  opts.bias = dbuff[11];
  opts.growth = dbuff[12];
  opts.k1 = dbuff[13];
  opts.k2 = dbuff[14];
  opts.k3 = dbuff[15];
  opts.etamx1 = dbuff[16];
  opts.etamxf = dbuff[17];
  opts.h0 = dbuff[18];
  opts.hmin = dbuff[19];
  opts.hmax = dbuff[20];
  opts.rtol = dbuff[21];
  opts.atol = dbuff[22];

  // return with success
  return(0);
}


// Computes the total of each conserved quantity;
// the root task then outputs these values to screen
int check_conservation(const realtype& t, const N_Vector w, const UserData& udata)
{
  realtype sumvals[] = {ZERO, ZERO};
  realtype totvals[] = {ZERO, ZERO};
  static realtype totsave[] = {-ONE, -ONE};
  bool outproc = (udata.myid == 0);
  long int i, j, k, idx;
  int retval;
  realtype *rho = N_VGetSubvectorArrayPointer_MPIManyVector(w,0);
  if (check_flag((void *) rho, "N_VGetSubvectorArrayPointer (check_conservation)", 0)) return -1;
  realtype *et = N_VGetSubvectorArrayPointer_MPIManyVector(w,4);
  if (check_flag((void *) et, "N_VGetSubvectorArrayPointer (check_conservation)", 0)) return -1;
  for (k=0; k<udata.nzl; k++)
    for (j=0; j<udata.nyl; j++)
      for (i=0; i<udata.nxl; i++) {
        idx = IDX(i,j,k,udata.nxl,udata.nyl,udata.nzl);
        sumvals[0] += rho[idx];
        sumvals[1] += et[idx];
      }
  sumvals[0] *= udata.dx*udata.dy*udata.dz;
  sumvals[1] *= udata.dx*udata.dy*udata.dz;
  retval = MPI_Reduce(sumvals, totvals, 2, MPI_SUNREALTYPE, MPI_SUM, 0, udata.comm);
  if (check_flag(&retval, "MPI_Reduce (check_conservation)", 3)) MPI_Abort(udata.comm, 1);
  if (!outproc)  return(0);
  if (totsave[0] == -ONE) {  // first time through; save/output the values
    printf("   Total mass   = %21.16e\n", totvals[0]);
    printf("   Total energy = %21.16e\n", totvals[1]);
    totsave[0] = totvals[0];
    totsave[1] = totvals[1];
  } else {
    printf("   Mass conservation relative change   = %7.2e\n",
           abs(totvals[0]-totsave[0])/totsave[0]);
    printf("   Energy conservation relative change = %7.2e\n",
           abs(totvals[1]-totsave[1])/totsave[1]);
  }
  return(0);
}


// Utility routine to print solution statistics
//    firstlast = 0 indicates the first output
//    firstlast = 1 indicates a normal output
//    firstlast = 2 indicates the lastoutput
int print_stats(const realtype& t, const N_Vector w, const int& firstlast,
                void *arkode_mem, const UserData& udata)
{
  realtype rmsvals[NVAR], totrms[NVAR];
  bool outproc = (udata.myid == 0);
  long int v, i, j, k, idx, nst;
  int retval;
  realtype *rho = N_VGetSubvectorArrayPointer_MPIManyVector(w,0);
  if (check_flag((void *) rho, "N_VGetSubvectorArrayPointer (print_stats)", 0)) return -1;
  realtype *mx = N_VGetSubvectorArrayPointer_MPIManyVector(w,1);
  if (check_flag((void *) mx, "N_VGetSubvectorArrayPointer (print_stats)", 0)) return -1;
  realtype *my = N_VGetSubvectorArrayPointer_MPIManyVector(w,2);
  if (check_flag((void *) my, "N_VGetSubvectorArrayPointer (print_stats)", 0)) return -1;
  realtype *mz = N_VGetSubvectorArrayPointer_MPIManyVector(w,3);
  if (check_flag((void *) mz, "N_VGetSubvectorArrayPointer (print_stats)", 0)) return -1;
  realtype *et = N_VGetSubvectorArrayPointer_MPIManyVector(w,4);
  if (check_flag((void *) et, "N_VGetSubvectorArrayPointer (print_stats)", 0)) return -1;
  retval = ARKStepGetNumSteps(arkode_mem, &nst);
  if (check_flag(&retval, "ARKStepGetNumSteps (main)", 1)) MPI_Abort(udata.comm, 1);
  if (firstlast < 2) {
    for (v=0; v<NVAR; v++)  rmsvals[v] = ZERO;
    for (k=0; k<udata.nzl; k++)
      for (j=0; j<udata.nyl; j++)
        for (i=0; i<udata.nxl; i++) {
          idx = IDX(i,j,k,udata.nxl,udata.nyl,udata.nzl);
          rmsvals[0] += pow(rho[idx], 2);
          rmsvals[1] += pow( mx[idx], 2);
          rmsvals[2] += pow( my[idx], 2);
          rmsvals[3] += pow( mz[idx], 2);
          rmsvals[4] += pow( et[idx], 2);
          if (udata.nchem > 0) {
            realtype *chem = N_VGetSubvectorArrayPointer_MPIManyVector(w,5+idx);
            if (check_flag((void *) chem, "N_VGetSubvectorArrayPointer (print_stats)", 0)) return -1;
            for (v=0; v<udata.nchem; v++)
              rmsvals[5+v] += SUNRpowerI( chem[v], 2);
          }
        }
    retval = MPI_Reduce(rmsvals, totrms, NVAR, MPI_SUNREALTYPE, MPI_SUM, 0, udata.comm);
    if (check_flag(&retval, "MPI_Reduce (print_stats)", 3)) MPI_Abort(udata.comm, 1);
    for (v=0; v<NVAR; v++)  totrms[v] = SUNRsqrt(totrms[v]/udata.nx/udata.ny/udata.nz);
  }
  if (!outproc)  return(0);
  if (firstlast == 0) {
    cout << "\n        t     ||rho||_rms  ||mx||_rms  ||my||_rms  ||mz||_rms  ||et||_rms";
    for (v=0; v<udata.nchem; v++)  cout << "  ||c" << v << "||_rms";
    cout << "     nst\n";
  }
  if (firstlast != 1) {
    cout << "   -----------------------------------------------------------------------";
    for (v=0; v<udata.nchem; v++)  cout << "------------";
    cout << "----------\n";
  }
  if (firstlast<2) {
    printf("  %10.6f  %10.6f  %10.6f  %10.6f  %10.6f  %10.6f", t,
           totrms[0], totrms[1], totrms[2], totrms[3], totrms[4]);
    for (v=0; v<udata.nchem; v++)  printf("  %10.6f", totrms[5+v]);
    printf("   %6li\n", nst);
  }
  return(0);
}


// Write problem-defining parameters to file
int write_parameters(const realtype& tcur, const int& iout,
                     const UserData& udata, const ARKodeParameters& opts)
{
  // root process creates restart file
  if (udata.myid == 0) {

    char outname[23];
    FILE *UFID = NULL;
    sprintf(outname, "restart_parameters.txt");
    UFID = fopen(outname,"w");
    if (check_flag((void*) UFID, "fopen (write_parameters)", 0)) return(1);
    fprintf(UFID, "# Euler3D restart file\n");
    fprintf(UFID, "xl = %lf\n", udata.xl);
    fprintf(UFID, "xr = %lf\n", udata.xr);
    fprintf(UFID, "yl = %lf\n", udata.yl);
    fprintf(UFID, "yr = %lf\n", udata.yr);
    fprintf(UFID, "zl = %lf\n", udata.zl);
    fprintf(UFID, "zr = %lf\n", udata.zr);
    fprintf(UFID, "t0 = %lf\n", tcur);
    fprintf(UFID, "tf = %lf\n", udata.tf);
    fprintf(UFID, "gamma = %lf\n", udata.gamma);
    fprintf(UFID, "nx = %li\n", udata.nx);
    fprintf(UFID, "ny = %li\n", udata.ny);
    fprintf(UFID, "nz = %li\n", udata.nz);
    fprintf(UFID, "xlbc = %i\n", udata.xlbc);
    fprintf(UFID, "xrbc = %i\n", udata.xrbc);
    fprintf(UFID, "ylbc = %i\n", udata.ylbc);
    fprintf(UFID, "yrbc = %i\n", udata.yrbc);
    fprintf(UFID, "zlbc = %i\n", udata.zlbc);
    fprintf(UFID, "zrbc = %i\n", udata.zrbc);
    fprintf(UFID, "cfl = %lf\n", udata.cfl);
    fprintf(UFID, "nout = %i\n", udata.nout);
    fprintf(UFID, "showstats = %i\n", udata.showstats);
    fprintf(UFID, "order = %i\n", opts.order);
    fprintf(UFID, "dense_order = %i\n", opts.dense_order);
    fprintf(UFID, "btable = %i\n",  opts.btable);
    fprintf(UFID, "adapt_method = %i\n", opts.adapt_method);
    fprintf(UFID, "maxnef = %i\n", opts.maxnef);
    fprintf(UFID, "mxhnil = %i\n", opts.mxhnil);
    fprintf(UFID, "mxsteps = %i\n", opts.mxsteps);
    fprintf(UFID, "safety = %lf\n", opts.safety);
    fprintf(UFID, "bias = %lf\n", opts.bias);
    fprintf(UFID, "growth = %lf\n", opts.growth);
    fprintf(UFID, "pq = %i\n", opts.pq);
    fprintf(UFID, "k1 = %lf\n", opts.k1);
    fprintf(UFID, "k2 = %lf\n", opts.k2);
    fprintf(UFID, "k3 = %lf\n", opts.k3);
    fprintf(UFID, "etamx1 = %lf\n", opts.etamx1);
    fprintf(UFID, "etamxf = %lf\n", opts.etamxf);
    fprintf(UFID, "h0 = %lf\n", opts.h0);
    fprintf(UFID, "hmin = %lf\n", opts.hmin);
    fprintf(UFID, "hmax = %lf\n", opts.hmax);
    fprintf(UFID, "rtol = %lf\n", opts.rtol);
    fprintf(UFID, "atol = %lf\n", opts.atol);
    fprintf(UFID, "restart = %i\n", iout);
    fclose(UFID);
  }

  // return with success
  return(0);
}


// Utility routine to output the current solution
//    iout should be an integer specifying which output to create
//
// Most of the contents of this routine follow from the hdf5_parallel.c
// example code available at:
// http://www.astro.sunysb.edu/mzingale/io_tutorial/HDF5_parallel/hdf5_parallel.c
int output_solution(const realtype& tcur, const N_Vector w, const int& iout,
                    const UserData& udata, const ARKodeParameters& opts)
{
  // reusable variables
  char outname[100];
  char chemname[13];
  realtype *W, **Wtmp;
  realtype domain[6];
  long int i, v;
  long int N = (udata.nzl)*(udata.nyl)*(udata.nxl);
  int retval;
  hid_t acc_template, file_identifier, filespace, memspace, dataset[NVAR], attspace, attr, h5_realtype;
  hsize_t dimens[3], stride[3], count[3], start[3];
  MPI_Info FILE_INFO_TEMPLATE;
  char comment[] = "This is an output/restart file from the 'Euler3D' SUNDIALS ManyVector+Multirate demonstration code";

  // Output restart parameter file
  retval = write_parameters(tcur, iout, udata, opts);
  if (check_flag(&retval, "write_parameters (output_solution)", 3)) return(-1);

  // Set string for output filename
  sprintf(outname, "output-%07i.hdf5", iout);

  // Determine HDF5 equivalent of 'realtype'
  if (sizeof(realtype) == sizeof(float)) {
    h5_realtype = H5T_NATIVE_FLOAT;
  } else if (sizeof(realtype) == sizeof(double)) {
    h5_realtype = H5T_NATIVE_DOUBLE;
  } else if (sizeof(realtype) == sizeof(long double)) {
    h5_realtype = H5T_NATIVE_LDOUBLE;
  } else {
    cerr << "Output_solution error: cannot map 'realtype' to HDF5 type\n";
    return(-1);
  }

  // set the file access template for parallel IO access
  acc_template = H5Pcreate(H5P_FILE_ACCESS);

  //-------------
  // platform dependent code goes here -- the access template should be
  // tuned for a particular filesystem blocksize.  some of these
  // numbers are guesses / experiments, others come from the file system
  // documentation
  //
  // The sieve_buf_size should be equal a multiple of the disk block size

  // create an MPI_INFO object -- on some platforms it is useful to
  // pass some information onto the underlying MPI_File_open call
  retval = MPI_Info_create(&FILE_INFO_TEMPLATE);
  if (check_flag(&retval, "MPI_Info_create (output_solution)", 3)) return(-1);
  retval = H5Pset_sieve_buf_size(acc_template, 262144);
  if (check_flag(&retval, "H5Pset_sieve_buf_size (output_solution)", 3)) return(-1);
  retval = H5Pset_alignment(acc_template, 524288, 262144);
  if (check_flag(&retval, "H5Pset_alignment (output_solution)", 3)) return(-1);
  retval = MPI_Info_set(FILE_INFO_TEMPLATE, "access_style", "write_once");
  if (check_flag(&retval, "MPI_Info_set (output_solution)", 3)) return(-1);
  retval = MPI_Info_set(FILE_INFO_TEMPLATE, "collective_buffering", "true");
  if (check_flag(&retval, "MPI_Info_set (output_solution)", 3)) return(-1);
  retval = MPI_Info_set(FILE_INFO_TEMPLATE, "cb_block_size", "1048576");
  if (check_flag(&retval, "MPI_Info_set (output_solution)", 3)) return(-1);
  retval = MPI_Info_set(FILE_INFO_TEMPLATE, "cb_buffer_size", "4194304");
  if (check_flag(&retval, "MPI_Info_set (output_solution)", 3)) return(-1);

  // tell HDF5 that we want to use MPI-IO to do the writing
  retval = H5Pset_fapl_mpio(acc_template, udata.comm, FILE_INFO_TEMPLATE);
  if (check_flag(&retval, "H5Pset_fapl_mpio (output_solution)", 3)) return(-1);

  // end of platform dependent properties
  //-------------

  // H5Fcreate takes several arguments in addition to the filename.  We
  // specify H5F_ACC_TRUNC to the second argument to tell it to overwrite
  // an existing file by the same name if it exists.  The next two
  // arguments are the file creation property list and the file access
  // property lists.  These are used to pass options to the library about
  // how to create the file, and how it will be accessed (ex. via mpi-io). */
  file_identifier = H5Fcreate(outname, H5F_ACC_TRUNC, H5P_DEFAULT, acc_template);

  // release the file access template
  retval = H5Pclose(acc_template);
  if (check_flag(&retval, "H5Pclose (output_solution)", 3)) return(-1);
  retval = MPI_Info_free(&FILE_INFO_TEMPLATE);
  if (check_flag(&retval, "MPI_Info_free (output_solution)", 3)) return(-1);


  //-------------
  // Now store some metadata for the output -- first some scalars
  attspace = H5Screate(H5S_SCALAR);
  //    current time
  attr = H5Dcreate(file_identifier, "time", h5_realtype, attspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  if (udata.myid == 0) {
    retval = H5Dwrite(attr, h5_realtype, H5S_ALL, attspace, H5P_DEFAULT, &tcur);
    if (check_flag(&retval, "H5Dwrite (output_solution)", 3)) return(-1);
  }
  H5Dclose(attr);
  //    number of chemical species
  attr = H5Dcreate(file_identifier, "nchem", H5T_NATIVE_INT, attspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  if (udata.myid == 0) {
    retval = H5Dwrite(attr, H5T_NATIVE_INT, H5S_ALL, attspace, H5P_DEFAULT, &udata.nchem);
    if (check_flag(&retval, "H5Dwrite (output_solution)", 3)) return(-1);
  }
  H5Dclose(attr);
  H5Sclose(attspace);

  // second, an array with the domain bounds
  domain[0] = udata.zl;  domain[1] = udata.zr;
  domain[2] = udata.yl;  domain[3] = udata.yr;
  domain[4] = udata.xl;  domain[5] = udata.xr;
  dimens[0] = 3;
  dimens[1] = 2;
  attspace = H5Screate_simple(2, dimens, NULL);
  attr = H5Dcreate(file_identifier, "domain", h5_realtype, attspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  if (udata.myid == 0) {
    retval = H5Dwrite(attr, h5_realtype, H5S_ALL, attspace, H5P_DEFAULT, domain);
    if (check_flag(&retval, "H5Dwrite (output_solution)", 3)) return(-1);
  }
  H5Dclose(attr);
  H5Sclose(attspace);


  //-------------
  // Now store the solution fields

  // create the filespace for the datasets (how each will be stored on disk)
  dimens[0] = udata.nz;
  dimens[1] = udata.ny;
  dimens[2] = udata.nx;
  filespace = H5Screate_simple(3, dimens, NULL);
  dimens[0] = udata.nzl;
  dimens[1] = udata.nyl;
  dimens[2] = udata.nxl;
  memspace = H5Screate_simple(3, dimens, NULL);

  // create the datasets (with default properties) and close filespace
  dataset[0] = H5Dcreate(file_identifier, "Density",     h5_realtype, filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  dataset[1] = H5Dcreate(file_identifier, "x-Momentum",  h5_realtype, filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  dataset[2] = H5Dcreate(file_identifier, "y-Momentum",  h5_realtype, filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  dataset[3] = H5Dcreate(file_identifier, "z-Momentum",  h5_realtype, filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  dataset[4] = H5Dcreate(file_identifier, "TotalEnergy", h5_realtype, filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  for (v=0; v<udata.nchem; v++) {
    sprintf(chemname, "Chemical-%03li", v);
    dataset[5+v] = H5Dcreate(file_identifier, chemname, h5_realtype, filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  }

  // Set this processor's offsets into the filespace
  start[0] = udata.ks;
  start[1] = udata.js;
  start[2] = udata.is;
  stride[0] = 1;
  stride[1] = 1;
  stride[2] = 1;
  count[0] = udata.nzl;
  count[1] = udata.nyl;
  count[2] = udata.nxl;
  retval = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, start, stride, count, NULL);
  if (check_flag(&retval, "H5Sselect_hyperslab (output_solution)", 3)) return(-1);

  // write each fluid field to disk, and close the associated dataset
  for (v=0; v<5; v++) {
    W = NULL;
    W = N_VGetSubvectorArrayPointer_MPIManyVector(w,v);  // data array
    retval = H5Dwrite(dataset[v], h5_realtype, memspace, filespace, H5P_DEFAULT, W);
    if (check_flag(&retval, "H5Dwrite (output_solution)", 3)) return(-1);
    H5Dclose(dataset[v]);
  }

  // write each chemical field to disk, and close the associated dataset
  // (note: we first copy these to be contiguous over this MPI task)
  if (udata.nchem > 0) {
    Wtmp = new realtype*[udata.nchem];
    for (v=0; v<udata.nchem; v++)
      Wtmp[v] = new realtype[N];
    for (i=0; i<N; i++) {                                // loop over subdomain
      W = NULL;
      W = N_VGetSubvectorArrayPointer_MPIManyVector(w,5+i);
      if (check_flag((void *) W, "N_VGetSubvectorArrayPointer (output_solution)", 0)) return -1;
      for (v=0; v<udata.nchem; v++)  Wtmp[v][i] = W[v];
    }
  }
  for (v=0; v<udata.nchem; v++) {
    retval = H5Dwrite(dataset[5+v], h5_realtype, memspace, filespace, H5P_DEFAULT, Wtmp[v]);
    if (check_flag(&retval, "H5Dwrite (output_solution)", 3)) return(-1);
    H5Dclose(dataset[5+v]);
  }

  // clean up and close the file
  if (udata.nchem > 0) {
    for (v=0; v<udata.nchem; v++)
      delete[] Wtmp[v];
    delete[] Wtmp;
  }
  H5Sclose(memspace);
  H5Sclose(filespace);
  H5Fclose(file_identifier);

  // return with success
  return(0);
}


//---- end of file ----
