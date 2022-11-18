/*---------------------------------------------------------------
 Programmer(s): Daniel R. Reynolds @ SMU
                David J. Gardner @ LLNL
 ----------------------------------------------------------------
 Copyright (c) 2022, Southern Methodist University.
 All rights reserved.
 For details, see the LICENSE file.
 ----------------------------------------------------------------
 Implementation file for input/output utility routines.
 Input routines read problem and solver input parameters
 from specified files.  For solver parameters, this calls
 associated "set" routines to specify options to ARKODE.
 Output routines compute/output shared diagnostics information,
 or write solution data to disk.
 ---------------------------------------------------------------*/

// Header files
#include <euler3D.hpp>
#include <arkode/arkode_arkstep.h>
#include <string.h>
#ifdef USEHDF5
#include "hdf5.h"
#endif
#include "gopt.hpp"


#define MAX_LINE_LENGTH 512


#if defined(SUNDIALS_SINGLE_PRECISION)
#define ESYM "%.8e"
#elif defined(SUNDIALS_DOUBLE_PRECISION)
#define ESYM "%.16e"
#else
#define ESYM "%.29e"
#endif


// Load problem-defining parameters from file: root process
// reads parameters and broadcasts results to remaining
// processes
int load_inputs(int myid, int argc, char* argv[], EulerData& udata,
                ARKODEParameters& opts, int& restart)
{
  int retval;
  double dbuff[28];
  long int ibuff[28];

  // disable 'restart' by default
  restart = -1;

  // root process handles command-line and file-based solver parameters, and packs send buffers
  if (myid == 0) {

    cout << "Reading command-line options\n";

    // use 'gopt' to handle parsing command-line; first define all available options
    const int nopt = 57;
    struct option options[nopt+1];
    enum iarg { ifname, ihelp, ixl, ixr, iyl, iyr, izl, izr, it0,
                itf, igam, imun, ilun, itun, inx, iny, inz, ixlb,
                ixrb, iylb, iyrb, izlb, izrb, icfl, inout, ishow,
                iord, idord, iebt, iibt, imbt, iadmth, imnef, imhnil,
                imaxst, isfty, ibias, igrow, ipq, ik1, ik2, ik3,
                iemx1, iemaf, ih0, ihmin, ihmax, ifixed, ihtrans,
                irtol, iatol, irest, ipred, imxnit, inlcoef, ifk,
                ilr };
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
    options[imun].long_name = "MassUnits";
    options[ilun].long_name = "LengthUnits";
    options[itun].long_name = "TimeUnits";
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
    options[iebt].long_name = "etable";
    options[iibt].long_name = "itable";
    options[imbt].long_name = "mtable";
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
    options[ifixed].long_name = "fixedstep";
    options[ihtrans].long_name = "htrans";
    options[irtol].long_name = "rtol";
    options[iatol].long_name = "atol";
    options[irest].long_name = "restart";
    options[ipred].long_name = "predictor";
    options[imxnit].long_name = "maxniters";
    options[inlcoef].long_name = "nlconvcoef";
    options[ifk].long_name = "fusedkernels";
    options[ilr].long_name = "localreduce";
    argc = gopt(argv, options);

    // handle help request
    if (options[ihelp].count) {
      cerr << "\nEuler3D SUNDIALS ManyVector+Multirate demonstration code\n"
           << "\nUsage: " << argv[0] << " [options]\n"
           << "   -h or --help prints this message and exits the program\n"
           << "\nAvailable problem specification options (and the default if not provided):\n"
           << "   --xl=<float>           (" << udata.xl << ")\n"
           << "   --xr=<float>           (" << udata.xr << ")\n"
           << "   --yl=<float>           (" << udata.yl << ")\n"
           << "   --yr=<float>           (" << udata.yr << ")\n"
           << "   --zl=<float>           (" << udata.zl << ")\n"
           << "   --zr=<float>           (" << udata.zr << ")\n"
           << "   --t0=<float>           (" << udata.t0 << ")\n"
           << "   --tf=<float>           (" << udata.tf << ")\n"
           << "   --gamma=<float>        (" << udata.gamma << ")\n"
           << "   --MassUnits=<float>    (" << udata.MassUnits << ")\n"
           << "   --LengthUnits=<float>  (" << udata.LengthUnits << ")\n"
           << "   --TimeUnits=<float>    (" << udata.TimeUnits << ")\n"
           << "   --nx=<int>             (" << udata.nx << ")\n"
           << "   --ny=<int>             (" << udata.ny << ")\n"
           << "   --nz=<int>             (" << udata.nz << ")\n"
           << "   --xlbc=<int>           (" << udata.xlbc << ")\n"
           << "   --xrbc=<int>           (" << udata.xrbc << ")\n"
           << "   --ylbc=<int>           (" << udata.ylbc << ")\n"
           << "   --yrbc=<int>           (" << udata.yrbc << ")\n"
           << "   --zlbc=<int>           (" << udata.zlbc << ")\n"
           << "   --zrbc=<int>           (" << udata.zrbc << ")\n"
           << "\nThe preceding 6 arguments allow any of the following boundary condition types:\n"
           << "   " << BC_PERIODIC <<" = periodic\n"
           << "   " << BC_NEUMANN <<" = homogeneous Neumann (zero gradient)\n"
           << "   " << BC_DIRICHLET <<" = homogeneous Dirichlet,\n"
           << "   " << BC_REFLECTING <<" = reflecting,\n"
           << "\nAvailable run options (and the default if not provided):\n"
           << "   --nout=<int>           (" << udata.nout << ")\n"
           << "   --showstats            to enable (disabled)\n"
           << "   --restart=<int>        output number to restart from: output-<num>.hdf5 (disabled)\n"
           << "\nAvailable time-stepping options (and the default if not provided):\n"
           << "   --cfl=<float>          (" << udata.cfl << ")\n"
           << "   --order=<int>          (" << opts.order << ")\n"
           << "   --dense_order=<int>    (" << opts.dense_order << ")\n"
           << "   --etable=<int>         (" << -1 << ")\n"
           << "   --itable=<int>         (" << -1 << ")\n"
           << "   --mtable=<int>         (" << -1 << ")\n"
           << "   --adapt_method=<int>   (" << opts.adapt_method << ")\n"
           << "   --maxnef=<int>         (" << opts.maxnef << ")\n"
           << "   --mxhnil=<int>         (" << opts.mxhnil << ")\n"
           << "   --mxsteps=<int>        (" << opts.mxsteps << ")\n"
           << "   --safety=<float>       (" << opts.safety << ")\n"
           << "   --bias=<float>         (" << opts.bias << ")\n"
           << "   --growth=<float>       (" << opts.growth << ")\n"
           << "   --pq=<int>             (" << opts.pq << ")\n"
           << "   --k1=<float>           (" << opts.k1 << ")\n"
           << "   --k2=<float>           (" << opts.k2 << ")\n"
           << "   --k3=<float>           (" << opts.k3 << ")\n"
           << "   --etamx1=<float>       (" << opts.etamx1 << ")\n"
           << "   --etamxf=<float>       (" << opts.etamxf << ")\n"
           << "   --h0=<float>           (" << opts.h0 << ")\n"
           << "   --hmin=<float>         (" << opts.hmin << ")\n"
           << "   --hmax=<float>         (" << opts.hmax << ")\n"
           << "   --fixedstep=<int>      (" << opts.fixedstep << ")\n"
           << "   --htrans=<float>       (" << opts.htrans << ")\n"
           << "   --rtol=<float>         (" << opts.rtol << ")\n"
           << "   --atol=<float>         (" << opts.atol << ")\n"
           << "\nAvailable nonlinear solver options (and the default if not provided):\n"
           << "   --predictor=<int>      (" << opts.predictor << ")\n"
           << "   --maxniters=<int>      (" << opts.maxniters << ")\n"
           << "   --nlconvcoef=<float>   (" << opts.nlconvcoef << ")\n"
           << "\nAvailable N_Vector options (and the default if not provided):\n"
           << "   --fusedkernels=<int>   (" << opts.fusedkernels << ")\n"
           << "   --localreduce=<int>    (" << opts.localreduce << ")\n"
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
      if (check_flag((void *) FID, "fopen (load_inputs)", 0)) {
        cerr << "Could not open input file " << options[ifname].argument << std::endl;
        return(-1);
      }
      cout << "Reading options from file: " << options[ifname].argument << std::endl;
      int itest, tablenum;
      while (fgets(line, MAX_LINE_LENGTH, FID) != NULL) {

        /* initialize return flags for line */
        retval = 0; itest = 0;

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
        retval += sscanf(line,"MassUnits = %lf", &udata.MassUnits);
        retval += sscanf(line,"LengthUnits = %lf", &udata.LengthUnits);
        retval += sscanf(line,"TimeUnits = %lf", &udata.TimeUnits);
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
        itest = sscanf(line,"etable = %i",  &tablenum);
        if (itest != 0) {
          opts.etable = (ARKODE_ERKTableID) tablenum;
          retval += itest;
        }
        itest = sscanf(line,"itable = %i",  &tablenum);
        if (itest != 0) {
          opts.itable = (ARKODE_DIRKTableID) tablenum;
          retval += itest;
        }
        itest = sscanf(line,"mtable = %i",  &tablenum);
        if (itest != 0) {
          opts.mtable = (ARKODE_MRITableID) tablenum;
          retval += itest;
        }
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
        retval += sscanf(line,"fixedstep = %i", &opts.fixedstep);
        retval += sscanf(line,"htrans = %lf", &opts.htrans);
        retval += sscanf(line,"rtol = %lf", &opts.rtol);
        retval += sscanf(line,"atol = %lf", &opts.atol);
        retval += sscanf(line,"restart = %i", &restart);
        retval += sscanf(line,"predictor = %i", &opts.predictor);
        retval += sscanf(line,"maxniters = %i", &opts.maxniters);
        retval += sscanf(line,"nlconvcoef = %lf", &opts.nlconvcoef);
        retval += sscanf(line,"fusedkernels = %i", &opts.fusedkernels);
        retval += sscanf(line,"localreduce = %i", &opts.localreduce);

        /* if unable to read the line (and it looks suspicious) issue a warning */
        if (retval == 0 && strstr(line, "=") != NULL && line[0] != '#')
          fprintf(stderr, "load_inputs Warning: parameter line was not interpreted:\n%s", line);
      }
      fclose(FID);

    }

    cout << "Merging command-line and file-based inputs\n";

    // replace any current option with a value specified on the command line
    if (options[ixl].count)     udata.xl          = atof(options[ixl].argument);
    if (options[ixr].count)     udata.xr          = atof(options[ixr].argument);
    if (options[iyl].count)     udata.yl          = atof(options[iyl].argument);
    if (options[iyr].count)     udata.yr          = atof(options[iyr].argument);
    if (options[izl].count)     udata.zl          = atof(options[izl].argument);
    if (options[izr].count)     udata.zr          = atof(options[izr].argument);
    if (options[it0].count)     udata.t0          = atof(options[it0].argument);
    if (options[itf].count)     udata.tf          = atof(options[itf].argument);
    if (options[igam].count)    udata.gamma       = atof(options[igam].argument);
    if (options[imun].count)    udata.MassUnits   = atof(options[imun].argument);
    if (options[ilun].count)    udata.LengthUnits = atof(options[ilun].argument);
    if (options[itun].count)    udata.TimeUnits   = atof(options[itun].argument);
    if (options[inx].count)     udata.nx          = atoi(options[inx].argument);
    if (options[iny].count)     udata.ny          = atoi(options[iny].argument);
    if (options[inz].count)     udata.nz          = atoi(options[inz].argument);
    if (options[ixlb].count)    udata.xlbc        = atoi(options[ixlb].argument);
    if (options[ixrb].count)    udata.xrbc        = atoi(options[ixrb].argument);
    if (options[iylb].count)    udata.ylbc        = atoi(options[iylb].argument);
    if (options[iyrb].count)    udata.yrbc        = atoi(options[iyrb].argument);
    if (options[izlb].count)    udata.zlbc        = atoi(options[izlb].argument);
    if (options[izrb].count)    udata.zrbc        = atoi(options[izrb].argument);
    if (options[icfl].count)    udata.cfl         = atof(options[icfl].argument);
    if (options[inout].count)   udata.nout        = atoi(options[inout].argument);
    if (options[ishow].count)   udata.showstats   = 1;
    if (options[iord].count)    opts.order        = atoi(options[iord].argument);
    if (options[idord].count)   opts.dense_order  = atoi(options[idord].argument);
    if (options[iebt].count)
      opts.etable = (ARKODE_ERKTableID)  atoi(options[iebt].argument);
    if (options[iibt].count)
      opts.itable = (ARKODE_DIRKTableID) atoi(options[iibt].argument);
    if (options[imbt].count)
      opts.mtable = (ARKODE_MRITableID)  atoi(options[imbt].argument);
    if (options[iadmth].count)  opts.adapt_method = atoi(options[iadmth].argument);
    if (options[imnef].count)   opts.maxnef       = atoi(options[imnef].argument);
    if (options[imhnil].count)  opts.mxhnil       = atoi(options[imhnil].argument);
    if (options[imaxst].count)  opts.mxsteps      = atoi(options[imaxst].argument);
    if (options[isfty].count)   opts.safety       = atof(options[isfty].argument);
    if (options[ibias].count)   opts.bias         = atof(options[ibias].argument);
    if (options[igrow].count)   opts.growth       = atof(options[igrow].argument);
    if (options[ipq].count)     opts.pq           = atoi(options[ipq].argument);
    if (options[ik1].count)     opts.k1           = atof(options[ik1].argument);
    if (options[ik2].count)     opts.k2           = atof(options[ik2].argument);
    if (options[ik3].count)     opts.k3           = atof(options[ik3].argument);
    if (options[iemx1].count)   opts.etamx1       = atof(options[iemx1].argument);
    if (options[iemaf].count)   opts.etamxf       = atof(options[iemaf].argument);
    if (options[ih0].count)     opts.h0           = atof(options[ih0].argument);
    if (options[ihmin].count)   opts.hmin         = atof(options[ihmin].argument);
    if (options[ihmax].count)   opts.hmax         = atof(options[ihmax].argument);
    if (options[ifixed].count)  opts.fixedstep    = atoi(options[ifixed].argument);
    if (options[ihtrans].count) opts.htrans       = atof(options[ihtrans].argument);
    if (options[irtol].count)   opts.rtol         = atof(options[irtol].argument);
    if (options[iatol].count)   opts.atol         = atof(options[iatol].argument);
    if (options[irest].count)   restart           = atoi(options[irest].argument);
    if (options[ipred].count)   opts.predictor    = atoi(options[ipred].argument);
    if (options[imxnit].count)  opts.maxniters    = atoi(options[imxnit].argument);
    if (options[inlcoef].count) opts.nlconvcoef   = atof(options[inlcoef].argument);
    if (options[ifk].count)     opts.fusedkernels = atoi(options[ifk].argument);
    if (options[ilr].count)     opts.localreduce  = atoi(options[ilr].argument);

    // pack buffers with final parameter values
    ibuff[0]  = (long int) udata.nx;
    ibuff[1]  = (long int) udata.ny;
    ibuff[2]  = (long int) udata.nz;
    ibuff[3]  = (long int) udata.xlbc;
    ibuff[4]  = (long int) udata.xrbc;
    ibuff[5]  = (long int) udata.ylbc;
    ibuff[6]  = (long int) udata.yrbc;
    ibuff[7]  = (long int) udata.zlbc;
    ibuff[8]  = (long int) udata.zrbc;
    ibuff[9]  = (long int) udata.nout;
    ibuff[10] = (long int) udata.showstats;
    ibuff[11] = (long int) opts.order;
    ibuff[12] = (long int) opts.dense_order;
    ibuff[13] = (long int) opts.etable;
    ibuff[14] = (long int) opts.itable;
    ibuff[15] = (long int) opts.mtable;
    ibuff[16] = (long int) opts.adapt_method;
    ibuff[17] = (long int) opts.maxnef;
    ibuff[18] = (long int) opts.mxhnil;
    ibuff[19] = (long int) opts.mxsteps;
    ibuff[20] = (long int) opts.pq;
    ibuff[21] = (long int) restart;
    ibuff[22] = (long int) opts.predictor;
    ibuff[23] = (long int) opts.maxniters;
    ibuff[24] = (long int) opts.fixedstep;
    ibuff[25] = (long int) opts.fusedkernels;
    ibuff[26] = (long int) opts.localreduce;

    dbuff[0]  = (double) udata.xl;
    dbuff[1]  = (double) udata.xr;
    dbuff[2]  = (double) udata.yl;
    dbuff[3]  = (double) udata.yr;
    dbuff[4]  = (double) udata.zl;
    dbuff[5]  = (double) udata.zr;
    dbuff[6]  = (double) udata.t0;
    dbuff[7]  = (double) udata.tf;
    dbuff[8]  = (double) udata.gamma;
    dbuff[9]  = (double) udata.cfl;
    dbuff[10] = (double) opts.safety;
    dbuff[11] = (double) opts.bias;
    dbuff[12] = (double) opts.growth;
    dbuff[13] = (double) opts.k1;
    dbuff[14] = (double) opts.k2;
    dbuff[15] = (double) opts.k3;
    dbuff[16] = (double) opts.etamx1;
    dbuff[17] = (double) opts.etamxf;
    dbuff[18] = (double) opts.h0;
    dbuff[19] = (double) opts.hmin;
    dbuff[20] = (double) opts.hmax;
    dbuff[21] = (double) opts.rtol;
    dbuff[22] = (double) opts.atol;
    dbuff[23] = (double) opts.nlconvcoef;
    dbuff[24] = (double) udata.MassUnits;
    dbuff[25] = (double) udata.LengthUnits;
    dbuff[26] = (double) udata.TimeUnits;
    dbuff[27] = (double) opts.htrans;

  }

  // perform broadcast and unpack results
  retval = MPI_Bcast(dbuff, 28, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  if (check_flag(&retval, "MPI_Bcast (load_inputs)", 3)) return(-1);
  retval = MPI_Bcast(ibuff, 27, MPI_LONG, 0, MPI_COMM_WORLD);
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
  opts.etable = (ARKODE_ERKTableID)  ibuff[13];
  opts.itable = (ARKODE_DIRKTableID) ibuff[14];
  opts.mtable = (ARKODE_MRITableID)  ibuff[15];
  opts.adapt_method = ibuff[16];
  opts.maxnef = ibuff[17];
  opts.mxhnil = ibuff[18];
  opts.mxsteps = ibuff[19];
  opts.pq = ibuff[20];
  restart = ibuff[21];
  opts.predictor = ibuff[22];
  opts.maxniters = ibuff[23];
  opts.fixedstep = ibuff[24];
  opts.fusedkernels = ibuff[25];
  opts.localreduce = ibuff[26];

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
  opts.nlconvcoef = dbuff[23];
  udata.MassUnits = dbuff[24];
  udata.LengthUnits = dbuff[25];
  udata.TimeUnits = dbuff[26];
  opts.htrans = dbuff[27];

  // setup any derived unit scaling factors
  retval = udata.UpdateUnits();
  if (check_flag(&retval, "UpdateUnits (load_inputs)", 1)) return(retval);

  // return with success
  return(0);
}


// Computes the total of each conserved quantity;
// the root task then outputs these values to screen
int check_conservation(const realtype& t, const N_Vector w, const EulerData& udata)
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
  realtype CellVolume = udata.dx*udata.dy*udata.dz*pow(udata.LengthUnits, 3);
  sumvals[0] *= CellVolume*udata.DensityUnits;
  sumvals[1] *= CellVolume*udata.EnergyUnits;
  retval = MPI_Reduce(sumvals, totvals, 2, MPI_SUNREALTYPE, MPI_SUM, 0, udata.comm);
  if (check_flag(&retval, "MPI_Reduce (check_conservation)", 3)) MPI_Abort(udata.comm, 1);
  if (!outproc)  return(0);
  if (totsave[0] == -ONE) {  // first time through; save/output the values
    printf("   Total mass   = " ESYM "\n", totvals[0]);
    printf("   Total energy = " ESYM "\n", totvals[1]);
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
//   firstlast = { 0 indicates the first output
//               { 1 indicates a normal output
//               { 2 indicates the last output
//   scientific = { 0 use fixed-point notation
//                { 1 use scientific notation
//   units = { 0 output internal 'scaled' solution values
//           { 1 output physical CGS solution values
int print_stats(const realtype& t, const N_Vector w, const int& firstlast,
                const int& scientific, const int& units, void *arkode_mem,
                const EulerData& udata)
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
  realtype *chem = NULL;
  if (udata.nchem > 0) {
    chem = N_VGetSubvectorArrayPointer_MPIManyVector(w,5);
    if (check_flag((void *) chem, "N_VGetSubvectorArrayPointer (print_stats)", 0)) return -1;
  }
  retval = ARKStepGetNumSteps(arkode_mem, &nst);
  if (check_flag(&retval, "ARKStepGetNumSteps (print_stats)", 1)) MPI_Abort(udata.comm, 1);

  // handle output of dimensionless vs CGS values
  realtype DUnits, MUnits, EUnits;
  DUnits = MUnits = EUnits = ONE;
  if (units == 1) {
    DUnits = udata.DensityUnits;
    MUnits = udata.MomentumUnits;
    EUnits = udata.EnergyUnits;
  }

  // accumulate output values
  if (firstlast < 2) {
    for (v=0; v<NVAR; v++)  rmsvals[v] = ZERO;
    for (k=0; k<udata.nzl; k++)
      for (j=0; j<udata.nyl; j++)
        for (i=0; i<udata.nxl; i++) {
          idx = IDX(i,j,k,udata.nxl,udata.nyl,udata.nzl);
          rmsvals[0] += pow(rho[idx]*DUnits, 2);
          rmsvals[1] += pow( mx[idx]*MUnits, 2);
          rmsvals[2] += pow( my[idx]*MUnits, 2);
          rmsvals[3] += pow( mz[idx]*MUnits, 2);
          rmsvals[4] += pow( et[idx]*EUnits, 2);
          if (udata.nchem > 0) {
            for (v=0; v<udata.nchem; v++) {
              idx = BUFIDX(v,i,j,k,udata.nchem,udata.nxl,udata.nyl,udata.nzl);
              rmsvals[5+v] += pow( chem[idx], 2);
            }
          }
        }
    retval = MPI_Reduce(rmsvals, totrms, NVAR, MPI_SUNREALTYPE, MPI_SUM, 0, udata.comm);
    if (check_flag(&retval, "MPI_Reduce (print_stats)", 3)) MPI_Abort(udata.comm, 1);
  }

  // perform output
  if (!outproc)  return(0);
  if (firstlast == 0) {
    cout << "\n      t       ||rho||   ||mx||    ||my||    ||mz||    ||et||   ";
    for (v=0; v<udata.nchem; v++)  cout << " ||c" << v << "||   ";
    cout << "   nst\n";
  }
  if (firstlast != 1) {
    cout <<   "   ------------------------------------------------------------";
    for (v=0; v<udata.nchem; v++)  cout << "----------";
    cout << "-------\n";
  }
  if (firstlast<2) {
    for (v=0; v<NVAR; v++)  totrms[v] = sqrt(totrms[v]/udata.nx/udata.ny/udata.nz);
    if (scientific) {
      printf("  %9.1e %9.1e %9.1e %9.1e %9.1e %9.1e", t,
             totrms[0], totrms[1], totrms[2], totrms[3], totrms[4]);
      for (v=0; v<udata.nchem; v++)  printf(" %9.1e", totrms[5+v]);
    } else {
      printf(" %9.5f %9.5f %9.5f %9.5f %9.5f %9.5f", t,
             totrms[0], totrms[1], totrms[2], totrms[3], totrms[4]);
      for (v=0; v<udata.nchem; v++)  printf(" %9.5f", totrms[5+v]);
    }
    printf("  %6li\n", nst);
  }
  return(0);
}


// Write problem-defining parameters to file
//
// The only parameters that are changed from those used at the simulation start are:
//    restart: current output number, so that simulation resumes normally
//    h0: current step size, so that simulation resumes normally
//    htrans: 0.0 since initial transients are already bypassed
int write_parameters(const realtype& tcur, const realtype& hcur, const int& iout,
                     const EulerData& udata, const ARKODEParameters& opts)
{
  // root process creates restart file
  if (udata.myid == 0) {

    char outname[23];
    FILE *UFID = NULL;
    sprintf(outname, "restart_parameters.txt");
    UFID = fopen(outname,"w");
    if (check_flag((void*) UFID, "fopen (write_parameters)", 0)) return(1);
    fprintf(UFID, "# Euler3D restart file\n");
    fprintf(UFID, "xl = " ESYM "\n", udata.xl);
    fprintf(UFID, "xr = " ESYM "\n", udata.xr);
    fprintf(UFID, "yl = " ESYM "\n", udata.yl);
    fprintf(UFID, "yr = " ESYM "\n", udata.yr);
    fprintf(UFID, "zl = " ESYM "\n", udata.zl);
    fprintf(UFID, "zr = " ESYM "\n", udata.zr);
    fprintf(UFID, "t0 = " ESYM "\n", tcur);
    fprintf(UFID, "tf = " ESYM "\n", udata.tf);
    fprintf(UFID, "gamma = " ESYM "\n", udata.gamma);
    fprintf(UFID, "MassUnits = " ESYM "\n", udata.MassUnits);
    fprintf(UFID, "LengthUnits = " ESYM "\n", udata.LengthUnits);
    fprintf(UFID, "TimeUnits = " ESYM "\n", udata.TimeUnits);
    fprintf(UFID, "nx = %li\n", udata.nx);
    fprintf(UFID, "ny = %li\n", udata.ny);
    fprintf(UFID, "nz = %li\n", udata.nz);
    fprintf(UFID, "xlbc = %i\n", udata.xlbc);
    fprintf(UFID, "xrbc = %i\n", udata.xrbc);
    fprintf(UFID, "ylbc = %i\n", udata.ylbc);
    fprintf(UFID, "yrbc = %i\n", udata.yrbc);
    fprintf(UFID, "zlbc = %i\n", udata.zlbc);
    fprintf(UFID, "zrbc = %i\n", udata.zrbc);
    fprintf(UFID, "cfl = " ESYM "\n", udata.cfl);
    fprintf(UFID, "nout = %i\n", udata.nout-iout);
    fprintf(UFID, "showstats = %i\n", udata.showstats);
    fprintf(UFID, "order = %i\n", opts.order);
    fprintf(UFID, "dense_order = %i\n", opts.dense_order);
    fprintf(UFID, "etable = %i\n",  (int) opts.etable);
    fprintf(UFID, "itable = %i\n",  (int) opts.itable);
    fprintf(UFID, "mtable = %i\n",  (int) opts.mtable);
    fprintf(UFID, "adapt_method = %i\n", opts.adapt_method);
    fprintf(UFID, "maxnef = %i\n", opts.maxnef);
    fprintf(UFID, "mxhnil = %i\n", opts.mxhnil);
    fprintf(UFID, "mxsteps = %i\n", opts.mxsteps);
    fprintf(UFID, "safety = " ESYM "\n", opts.safety);
    fprintf(UFID, "bias = " ESYM "\n", opts.bias);
    fprintf(UFID, "growth = " ESYM "\n", opts.growth);
    fprintf(UFID, "pq = %i\n", opts.pq);
    fprintf(UFID, "k1 = " ESYM "\n", opts.k1);
    fprintf(UFID, "k2 = " ESYM "\n", opts.k2);
    fprintf(UFID, "k3 = " ESYM "\n", opts.k3);
    fprintf(UFID, "etamx1 = " ESYM "\n", opts.etamx1);
    fprintf(UFID, "etamxf = " ESYM "\n", opts.etamxf);
    fprintf(UFID, "h0 = " ESYM "\n", hcur);
    fprintf(UFID, "hmin = " ESYM "\n", opts.hmin);
    fprintf(UFID, "hmax = " ESYM "\n", opts.hmax);
    fprintf(UFID, "fixedstep = %i\n", opts.fixedstep);
    fprintf(UFID, "htrans = " ESYM "\n", opts.htrans);
    fprintf(UFID, "rtol = " ESYM "\n", opts.rtol);
    fprintf(UFID, "atol = " ESYM "\n", opts.atol);
    fprintf(UFID, "fusedkernels = %i\n", opts.fusedkernels);
    fprintf(UFID, "localreduce = %i\n", opts.localreduce);
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
int output_solution(const realtype& tcur, const N_Vector w, const realtype& hcur,
                    const int& iout, const EulerData& udata, const ARKODEParameters& opts)
{
#ifdef USEHDF5
  // reusable variables
  char outname[100];
  char chemname[13];
  realtype *W, **Wtmp;
  realtype domain[6];
  long int i;
  long int N = (udata.nzl)*(udata.nyl)*(udata.nxl);
  int v, retval;
  hid_t acc_template, file_identifier, filespace, memspace, dataset[NVAR], attspace, attr, h5_realtype;
  hsize_t dimens[3], stride[3], count[3], start[3];
  MPI_Info FILE_INFO_TEMPLATE;

  // Output restart parameter file
  retval = write_parameters(tcur, hcur, iout, udata, opts);
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
    cerr << "output_solution error: cannot map 'realtype' to HDF5 type\n";
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
  // how to create the file, and how it will be accessed (ex. via mpi-io).
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
  dataset[0] = H5Dcreate(file_identifier, "Density",     h5_realtype,
                         filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  dataset[1] = H5Dcreate(file_identifier, "x-Momentum",  h5_realtype,
                         filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  dataset[2] = H5Dcreate(file_identifier, "y-Momentum",  h5_realtype,
                         filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  dataset[3] = H5Dcreate(file_identifier, "z-Momentum",  h5_realtype,
                         filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  dataset[4] = H5Dcreate(file_identifier, "TotalEnergy", h5_realtype,
                         filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  for (v=0; v<udata.nchem; v++) {
    sprintf(chemname, "Chemical-%03hu", (unsigned short) v);
    dataset[5+v] = H5Dcreate(file_identifier, chemname, h5_realtype,
                             filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
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

  // scale each fluid field back to CGS units
  N_VScale(udata.DensityUnits,  N_VGetSubvector_MPIManyVector(w,0), N_VGetSubvector_MPIManyVector(w,0));
  N_VScale(udata.MomentumUnits, N_VGetSubvector_MPIManyVector(w,1), N_VGetSubvector_MPIManyVector(w,1));
  N_VScale(udata.MomentumUnits, N_VGetSubvector_MPIManyVector(w,2), N_VGetSubvector_MPIManyVector(w,2));
  N_VScale(udata.MomentumUnits, N_VGetSubvector_MPIManyVector(w,3), N_VGetSubvector_MPIManyVector(w,3));
  N_VScale(udata.EnergyUnits,   N_VGetSubvector_MPIManyVector(w,4), N_VGetSubvector_MPIManyVector(w,4));

  // write each fluid field to disk, and close the associated dataset
  for (v=0; v<5; v++) {
    W = NULL;
    W = N_VGetSubvectorArrayPointer_MPIManyVector(w,v);  // data array
    retval = H5Dwrite(dataset[v], h5_realtype, memspace, filespace, H5P_DEFAULT, W);
    if (check_flag(&retval, "H5Dwrite (output_solution)", 3)) return(-1);
    H5Dclose(dataset[v]);
  }

  // scale each fluid field back to code units
  N_VScale(ONE/udata.DensityUnits,  N_VGetSubvector_MPIManyVector(w,0), N_VGetSubvector_MPIManyVector(w,0));
  N_VScale(ONE/udata.MomentumUnits, N_VGetSubvector_MPIManyVector(w,1), N_VGetSubvector_MPIManyVector(w,1));
  N_VScale(ONE/udata.MomentumUnits, N_VGetSubvector_MPIManyVector(w,2), N_VGetSubvector_MPIManyVector(w,2));
  N_VScale(ONE/udata.MomentumUnits, N_VGetSubvector_MPIManyVector(w,3), N_VGetSubvector_MPIManyVector(w,3));
  N_VScale(ONE/udata.EnergyUnits,   N_VGetSubvector_MPIManyVector(w,4), N_VGetSubvector_MPIManyVector(w,4));

  // write each chemical field to disk, and close the associated dataset
  // (note: we first copy these to be contiguous over this MPI task)
  if (udata.nchem > 0) {
    W = NULL;
    W = N_VGetSubvectorArrayPointer_MPIManyVector(w,5);
    if (check_flag((void *) W, "N_VGetSubvectorArrayPointer (output_solution)", 0)) return -1;
    Wtmp = new realtype*[udata.nchem];
    for (v=0; v<udata.nchem; v++)
      Wtmp[v] = new realtype[N];
    for (i=0; i<N; i++)      // loop over subdomain
      for (v=0; v<udata.nchem; v++)
        Wtmp[v][i] = W[i*udata.nchem+v];
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
#endif

  // return with success
  return(0);
}


// Utility routine to set the time "t" and the state "w" from the restart file
// "output-<restart>.hdf5"
int read_restart(const int& restart, realtype& t, N_Vector w, const EulerData& udata)
{
#ifdef USEHDF5
  // reusable variables
  char inname[100];
  char chemname[13];
  realtype *W, *Wtmp;
  realtype domain[6];
  long int i;
  long int N = (udata.nzl)*(udata.nyl)*(udata.nxl);
  int v, nchem, retval;
  hid_t acc_template, file_identifier, memspace, dataset, dataspace, h5_realtype;
  hsize_t dimens[3], stride[3], count[3], start[3];

  // Set string for input filename
  sprintf(inname, "output-%07i.hdf5", restart);

  // Determine HDF5 equivalent of 'realtype'
  if (sizeof(realtype) == sizeof(float)) {
    h5_realtype = H5T_NATIVE_FLOAT;
  } else if (sizeof(realtype) == sizeof(double)) {
    h5_realtype = H5T_NATIVE_DOUBLE;
  } else if (sizeof(realtype) == sizeof(long double)) {
    h5_realtype = H5T_NATIVE_LDOUBLE;
  } else {
    cerr << "read_restart error: cannot map 'realtype' to HDF5 type\n";
    return(-1);
  }

  // set the file access template for parallel IO access
  // open the file, and
  // release the file access template
  acc_template = H5Pcreate(H5P_FILE_ACCESS);
  file_identifier = H5Fopen(inname, H5F_ACC_RDONLY, H5P_DEFAULT);
  retval = H5Pclose(acc_template);
  if (check_flag(&retval, "H5Pclose (read_restart)", 3)) return(-1);

  //-------------
  // Now read metadata from the output -- first some scalars
  //    current time (use this to check that file precision matches executable)
  dataset = H5Dopen2(file_identifier, "time", H5P_DEFAULT);
  //  if (H5Tget_class(H5Dget_type(dataset)) != h5_realtype) {
  if (!H5Tequal(H5Dget_type(dataset), h5_realtype)) {
    cerr << "read_restart error: file type does not match 'realtype' type\n";
    return(-1);
  }
  dataspace = H5Dget_space(dataset);
  retval = H5Dread(dataset, h5_realtype, H5S_ALL, dataspace, H5P_DEFAULT, &t);
  if (check_flag(&retval, "H5Dread (read_restart)", 3)) return(-1);
  H5Sclose(dataspace);
  H5Dclose(dataset);
  //    number of chemical species -- must match executable
  dataset = H5Dopen2(file_identifier, "nchem", H5P_DEFAULT);
  dataspace = H5Dget_space(dataset);
  retval = H5Dread(dataset, H5T_NATIVE_INT, H5S_ALL, dataspace, H5P_DEFAULT, &nchem);
  if (check_flag(&retval, "H5Dread (read_restart)", 3)) return(-1);
  if (nchem != udata.nchem) {
    cerr << "read_restart error: incompatible number of chemical/tracer fields\n";
    return(-1);
  }
  H5Sclose(dataspace);
  H5Dclose(dataset);

  // second, read the domain bounds and verify against stored values
  dataset = H5Dopen2(file_identifier, "domain", H5P_DEFAULT);
  dataspace = H5Dget_space(dataset);
  if (H5Sget_simple_extent_ndims(dataspace) != 2) {
    cerr << "read_restart error: incompatible domain\n";
    return(-1);
  }
  retval = H5Sget_simple_extent_dims(dataspace, dimens, NULL);
  if ((dimens[0] != 3) || (dimens[1] != 2)) {
    cerr << "read_restart error: incompatible domain\n";
    return(-1);
  }
  retval = H5Dread(dataset, h5_realtype, H5S_ALL, dataspace, H5P_DEFAULT, domain);
  if (check_flag(&retval, "H5Dread (read_restart)", 3)) return(-1);
  if ( abs(domain[0] - udata.zl) > 1e-14 ||
       abs(domain[1] - udata.zr) > 1e-14 ||
       abs(domain[2] - udata.yl) > 1e-14 ||
       abs(domain[3] - udata.yr) > 1e-14 ||
       abs(domain[4] - udata.xl) > 1e-14 ||
       abs(domain[5] - udata.xr) > 1e-14 ) {
    cerr << "read_restart error: incompatible domain\n";
    return(-1);
  }
  H5Sclose(dataspace);
  H5Dclose(dataset);


  //-------------
  // Now read the solution fields

  // first, zero output state
  N_VConst(ZERO, w);

  // define memory space for each field on this process
  dimens[0] = udata.nzl;
  dimens[1] = udata.nyl;
  dimens[2] = udata.nxl;
  memspace = H5Screate_simple(3, dimens, NULL);

  // define hyperslab for each field on this process
  start[0] = udata.ks;
  start[1] = udata.js;
  start[2] = udata.is;
  stride[0] = 1;
  stride[1] = 1;
  stride[2] = 1;
  count[0] = udata.nzl;
  count[1] = udata.nyl;
  count[2] = udata.nxl;

  // read density -- use this one to verify compatible dimensions
  dataset = H5Dopen2(file_identifier, "Density", H5P_DEFAULT);
  dataspace = H5Dget_space(dataset);
  if (H5Sget_simple_extent_ndims(dataspace) != 3) {
    cerr << "read_restart error: incompatible field dimensions\n";
    return(-1);
  }
  retval = H5Sget_simple_extent_dims(dataspace, dimens, NULL);
  if ((dimens[0] != udata.nz) || (dimens[1] != udata.ny) || (dimens[2] != udata.nx)) {
    cerr << "read_restart error: incompatible field size\n";
    return(-1);
  }
  retval = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, start, stride, count, NULL);
  if (check_flag(&retval, "H5Sselect_hyperslab (read_restart)", 3)) return(-1);
  W = N_VGetSubvectorArrayPointer_MPIManyVector(w,0);
  retval = H5Dread(dataset, h5_realtype, memspace, dataspace, H5P_DEFAULT, W);
  if (check_flag(&retval, "H5Dread (output_solution)", 3)) return(-1);
  H5Sclose(dataspace);
  H5Dclose(dataset);

  // read x-Momentum
  dataset = H5Dopen2(file_identifier, "x-Momentum", H5P_DEFAULT);
  dataspace = H5Dget_space(dataset);
  retval = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, start, stride, count, NULL);
  if (check_flag(&retval, "H5Sselect_hyperslab (read_restart)", 3)) return(-1);
  W = N_VGetSubvectorArrayPointer_MPIManyVector(w,1);
  retval = H5Dread(dataset, h5_realtype, memspace, dataspace, H5P_DEFAULT, W);
  if (check_flag(&retval, "H5Dread (output_solution)", 3)) return(-1);
  H5Sclose(dataspace);
  H5Dclose(dataset);

  // read y-Momentum
  dataset = H5Dopen2(file_identifier, "y-Momentum", H5P_DEFAULT);
  dataspace = H5Dget_space(dataset);
  retval = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, start, stride, count, NULL);
  if (check_flag(&retval, "H5Sselect_hyperslab (read_restart)", 3)) return(-1);
  W = N_VGetSubvectorArrayPointer_MPIManyVector(w,2);
  retval = H5Dread(dataset, h5_realtype, memspace, dataspace, H5P_DEFAULT, W);
  if (check_flag(&retval, "H5Dread (output_solution)", 3)) return(-1);
  H5Sclose(dataspace);
  H5Dclose(dataset);

  // read z-Momentum
  dataset = H5Dopen2(file_identifier, "z-Momentum", H5P_DEFAULT);
  dataspace = H5Dget_space(dataset);
  retval = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, start, stride, count, NULL);
  if (check_flag(&retval, "H5Sselect_hyperslab (read_restart)", 3)) return(-1);
  W = N_VGetSubvectorArrayPointer_MPIManyVector(w,3);
  retval = H5Dread(dataset, h5_realtype, memspace, dataspace, H5P_DEFAULT, W);
  if (check_flag(&retval, "H5Dread (output_solution)", 3)) return(-1);
  H5Sclose(dataspace);
  H5Dclose(dataset);

  // read TotalEnergy
  dataset = H5Dopen2(file_identifier, "TotalEnergy", H5P_DEFAULT);
  dataspace = H5Dget_space(dataset);
  retval = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, start, stride, count, NULL);
  if (check_flag(&retval, "H5Sselect_hyperslab (read_restart)", 3)) return(-1);
  W = N_VGetSubvectorArrayPointer_MPIManyVector(w,4);
  retval = H5Dread(dataset, h5_realtype, memspace, dataspace, H5P_DEFAULT, W);
  if (check_flag(&retval, "H5Dread (output_solution)", 3)) return(-1);
  H5Sclose(dataspace);
  H5Dclose(dataset);

  // read remaining chemical fields
  if (udata.nchem > 0) {
    W = NULL;
    W = N_VGetSubvectorArrayPointer_MPIManyVector(w,5);
    if (check_flag((void *) W, "N_VGetSubvectorArrayPointer (output_solution)", 0)) return -1;
    Wtmp = new realtype[N];
    for (v=0; v<udata.nchem; v++) {
      sprintf(chemname, "Chemical-%03hu", (unsigned short) v);
      dataset = H5Dopen2(file_identifier, chemname, H5P_DEFAULT);
      dataspace = H5Dget_space(dataset);
      retval = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, start, stride, count, NULL);
      if (check_flag(&retval, "H5Sselect_hyperslab (read_restart)", 3)) return(-1);
      retval = H5Dread(dataset, h5_realtype, memspace, dataspace, H5P_DEFAULT, Wtmp);
      if (check_flag(&retval, "H5Dread (output_solution)", 3)) return(-1);
      H5Sclose(dataspace);
      H5Dclose(dataset);
      for (i=0; i<N; i++)    // loop over subdomain, copying data into vectors
        W[i*udata.nchem+v] = Wtmp[i];
    }
    delete[] Wtmp;
  }

  // scale each fluid field to code units
  N_VScale(ONE/udata.DensityUnits,  N_VGetSubvector_MPIManyVector(w,0), N_VGetSubvector_MPIManyVector(w,0));
  N_VScale(ONE/udata.MomentumUnits, N_VGetSubvector_MPIManyVector(w,1), N_VGetSubvector_MPIManyVector(w,1));
  N_VScale(ONE/udata.MomentumUnits, N_VGetSubvector_MPIManyVector(w,2), N_VGetSubvector_MPIManyVector(w,2));
  N_VScale(ONE/udata.MomentumUnits, N_VGetSubvector_MPIManyVector(w,3), N_VGetSubvector_MPIManyVector(w,3));
  N_VScale(ONE/udata.EnergyUnits,   N_VGetSubvector_MPIManyVector(w,4), N_VGetSubvector_MPIManyVector(w,4));

  // clean up and close the file
  H5Sclose(memspace);
  H5Fclose(file_identifier);

  // return with success
  return(0);
#else
  cerr << "read_restart error: executable compiled without HDF5 support\n";
  return(-1);
#endif
}


//---- end of file ----
