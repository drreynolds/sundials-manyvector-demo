# SUNDIALS Multirate+ManyVector Demo

[Note: this project is in active development; do not expect anything to run (or even compile) at present.]

This is a SUNDIALS-based demonstration application to assess and demonstrate the large-scale parallel performance of new capabilities that have been added to SUNDIALS in recent years.  Namely:

1. ARKode's new multirate integration module, MRIStep, allowing high-order accurate calculations that subcycle "fast" processes within "slow" ones.

2. SUNDIALS' new MPIManyVector module, that allows extreme flexibility in how a solution "vector" is staged on computational resources.

To run this demo you will need modern C and C++ compilers.  All dependencies (SUNDIALS and SuiteSparse) for the demo are installed in-place using Spack, which is included in this repository.

Steps showing the process to download this demo code, install the relevant dependencies, and build the demo in a Linux or OS X environment are as follows:

```bash
  <img src="/tex/068c1c7f0e1c5f0f1a684f2354528284.svg?invert_in_darkmode&sanitize=true" align=middle width=554.6880504pt height=24.65753399999998pt/> cd sundials-manyvector-demo
  <img src="/tex/b2cdfb5d0b534acadb4dfcd1f42a3eec.svg?invert_in_darkmode&sanitize=true" align=middle width=409.3480495499999pt height=24.65753399999998pt/> .spack/bin/spack view symlink libs sundials
  <img src="/tex/a5b76f1ade6adc44568d772ee3fbc99b.svg?invert_in_darkmode&sanitize=true" align=middle width=276.7397110499999pt height=24.65753399999998pt/> make
```

## (Current) Documentation

This test simulates a 3D nonlinear inviscid compressible Euler equation,
<p align="center"><img src="/tex/d9863f826c2348e733804a7429c419ad.svg?invert_in_darkmode&sanitize=true" align=middle width=148.6959507pt height=16.438356pt/></p>
for <img src="/tex/f6051feab6a8ea76ab2f495fa562eb8d.svg?invert_in_darkmode&sanitize=true" align=middle width=72.06049289999999pt height=24.65753399999998pt/>, <img src="/tex/c0ec5af17b136310b0fd99c5825fcb17.svg?invert_in_darkmode&sanitize=true" align=middle width=310.49058974999997pt height=24.65753399999998pt/>, with initial condition <img src="/tex/4de18f840aaa2a5af62b91e3563954a7.svg?invert_in_darkmode&sanitize=true" align=middle width=129.27608594999998pt height=24.65753399999998pt/>, and boundary conditions [`xlbc`,`xrbc`] x [`ylbc`,`yrbc`] x [`zlbc`,`zrbc`], where each may be any one of

* periodic (0),
* homogeneous Neumann (1), or
* homogeneous Dirichlet (2),

under the restriction that if any boundary is set to "periodic" then the opposite face must also indicate a periodic condition.

Here, the 'solution' is given by <img src="/tex/042c5bf126725a346eef6747a67881b2.svg?invert_in_darkmode&sanitize=true" align=middle width=325.15765754999995pt height=27.6567522pt/>, that corresponds to the density, x,y,z-momentum, and the total energy per unit volume.  The fluxes are given by
<p align="center"><img src="/tex/948c50d22af94c45cf1404ddd10af659.svg?invert_in_darkmode&sanitize=true" align=middle width=337.44287115pt height=19.3534671pt/></p>
<p align="center"><img src="/tex/f4cb1d6c35406376ed6ba16a89804198.svg?invert_in_darkmode&sanitize=true" align=middle width=335.63178164999994pt height=20.95157625pt/></p>
<p align="center"><img src="/tex/981524f5fc16c7d3449223c0ef1195ac.svg?invert_in_darkmode&sanitize=true" align=middle width=334.32285314999996pt height=19.3534671pt/></p>

the external force <img src="/tex/c441e18e502be64ac772003edac839dc.svg?invert_in_darkmode&sanitize=true" align=middle width=52.94748029999999pt height=24.65753399999998pt/> is test-problem-dependent, and the ideal gas equation of state gives
<img src="/tex/7267f867419ceb03b618445151c90f0e.svg?invert_in_darkmode&sanitize=true" align=middle width=204.4567305pt height=28.670654099999997pt/> and
<img src="/tex/4d4436280a3ff11311d69455ff6d1e94.svg?invert_in_darkmode&sanitize=true" align=middle width=190.58242334999997pt height=26.76175259999998pt/>,
or equivalently,
<img src="/tex/7f47b73a97ad6216f234a9d6faeeafb2.svg?invert_in_darkmode&sanitize=true" align=middle width=238.10365755pt height=27.94539330000001pt/> and
<img src="/tex/8849d54f53b89b09df8c7cc269fb9da2.svg?invert_in_darkmode&sanitize=true" align=middle width=209.13941069999998pt height=28.92634470000001pt/>

We have the parameters

* R is the specific ideal gas constant (287.14 J/kg/K).
* <img src="/tex/aa8cfea83e4502fbd685d6c095494147.svg?invert_in_darkmode&sanitize=true" align=middle width=14.102064899999991pt height=14.15524440000002pt/> is the specific heat capacity at constant volume (717.5 J/kg/K),
* <img src="/tex/11c596de17c342edeed29f489aa4b274.svg?invert_in_darkmode&sanitize=true" align=middle width=9.423880949999988pt height=14.15524440000002pt/> is the ratio of specific heats, <img src="/tex/e2726c495cdd0298d7c5be83af55bac8.svg?invert_in_darkmode&sanitize=true" align=middle width=158.5934658pt height=28.670654099999997pt/> (1.4),

corresponding to air (predominantly an ideal diatomic gas). The speed of sound in the gas is then given by
<p align="center"><img src="/tex/3f5e478e3d3c690cf7f15c2a9ac1fe4d.svg?invert_in_darkmode&sanitize=true" align=middle width=67.10942039999999pt height=39.452455349999994pt/></p>
The fluid variables above are non-dimensionalized; in standard SI units these would be:

  [rho] = kg / m<img src="/tex/b6c5b75bafc8bbc771fa716cb26245ff.svg?invert_in_darkmode&sanitize=true" align=middle width=6.5525476499999895pt height=26.76175259999998pt/>

  [vx] = [vy] = [vz] = m/s  =>  [mx] = [my] = [mz] = kg / m<img src="/tex/e18b24c87a7c52fd294215d16b42a437.svg?invert_in_darkmode&sanitize=true" align=middle width=6.5525476499999895pt height=26.76175259999998pt/> / s

  [E] = kg / m / s<img src="/tex/e18b24c87a7c52fd294215d16b42a437.svg?invert_in_darkmode&sanitize=true" align=middle width=6.5525476499999895pt height=26.76175259999998pt/>

Note: the above follows the description in section 7.3.1-7.3.3 of https://www.theoretical-physics.net/dev/fluid-dynamics/euler.html

This program solves the problem using a finite volume spatial semi-discretization over a uniform grid of dimensions `nx` x `ny` x `nz`, with fluxes calculated using a 5th-order WENO reconstruction.  The spatial domain uses a 3D domain decomposition approach for parallelism over nprocs MPI processes, with layout `npx` x `npy` x `npz` defined automatically via the `MPI_Dims_create` utility routine.  Each field is stored in its own parallel `N_Vector` object; these are combined together to form the full "solution" vector <img src="/tex/31fae8b8b78ebe01cbfbe2fe53832624.svg?invert_in_darkmode&sanitize=true" align=middle width=12.210846449999991pt height=14.15524440000002pt/> using the `MPIManyVector` `N_Vector` module.  The resulting initial-value problem is solved using a temporally-adaptive explicit Runge Kutta method from ARKode's ARKStep module.  The solution is output to disk and solution statistics are optionally output to the screen at specified frequencies, and run statistics are printed at the end.

Individual test problems may be uniquely specified through an input file and an auxiliarly source code file that should be linked with this main routine at compile time.

The input file, `input_euler3D.txt`, contains:

* the ratio of specific heats, <img src="/tex/11c596de17c342edeed29f489aa4b274.svg?invert_in_darkmode&sanitize=true" align=middle width=9.423880949999988pt height=14.15524440000002pt/> -- `gamma`
* spatial domain, <img src="/tex/9432d83304c1eb0dcb05f092d30a767f.svg?invert_in_darkmode&sanitize=true" align=middle width=11.87217899999999pt height=22.465723500000017pt/> -- `xl`, `xr`, `yl`, `yr`, `zl`, `zr`
* time interval, <img src="/tex/bbde6652efaeb60e967ee67be6440eb7.svg?invert_in_darkmode&sanitize=true" align=middle width=46.033257599999985pt height=24.65753399999998pt/> -- `t0`, `tf`
* spatial discretization dimensions -- `nx`, `ny`, `nz`
* boundary condition types -- `xlbc`, `xrbc`, `ylbc`, `yrbc`, `zlbc`, `zrbc`
* number of desired solution outputs -- `nout`

Additionally, this file contains the parameter `showstats`, a nonzero value enables optional output of RMS averages for each field at the same frequency as the solution is output to disk.

The auxiliary source code file must contain two functions:

* the initial condition function <img src="/tex/d3cb4393199b89ca003e78d3486fa147.svg?invert_in_darkmode&sanitize=true" align=middle width=46.837068299999984pt height=24.65753399999998pt/> must have the signature:

```C++
     int initial_conditions(const realtype& t, N_Vector w, const UserData& udata);
```

* the forcing function <img src="/tex/c441e18e502be64ac772003edac839dc.svg?invert_in_darkmode&sanitize=true" align=middle width=52.94748029999999pt height=24.65753399999998pt/> must have the signature

```C++
     int external_forces(const realtype& t, N_Vector G, const UserData& udata);
```


## Authors
[Daniel R. Reynolds](mailto:reynolds@smu.edu)
