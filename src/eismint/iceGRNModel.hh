// Copyright (C) 2004-2009 Nathan Shemonski and Ed Bueler
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2 of the License, or (at your option) any later
// version.
//
// PISM is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License
// along with PISM; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#ifndef __iceGRNModel_hh
#define __iceGRNModel_hh

#include <petscvec.h>
#include "../base/grid.hh"
#include "../base/materials.hh"
#include "../base/iceModel.hh"
#include "../coupler/pccoupler.hh"


class PISMEISGREENPDDCoupler : public PISMPDDCoupler {

public:
  PISMEISGREENPDDCoupler();

protected:
  virtual PetscScalar getSummerWarming(
       const PetscScalar elevation, const PetscScalar latitude, const PetscScalar Tma);
};


//! Implements EISMINT-Greenland experiments.
/*!
This derived class adds, essentially, only the minimum functionality needed
to implement the choices state in \lo \cite{RitzEISMINT}\elo, the EISMINT-Greenland 
specification.

Some specific choices implemented here:
- A PDD is always used, and it has an elevation- and latitude-dependent amount of summer warming.
- An enhancement factor of 3.0 is used.
- There is special code to ``clean out'' Ellsmere Island (and Iceland) so ice won't spread to
  edge of computational grid; this should probably be moved to the scripts which set up the
  bootstrap file.

A separate driver is used, namely src/pgrn.cc.
 */
class IceGRNModel : public IceModel {

public:
  IceGRNModel(IceGrid &g);
  virtual PetscErrorCode setFromOptions();
  using IceModel::initFromOptions;
  PetscErrorCode attachEISGREENPDDPCC(PISMEISGREENPDDCoupler &p);
  virtual PetscErrorCode initFromOptions(PetscTruth doHook = PETSC_TRUE);

protected:
  PISMEISGREENPDDCoupler *pddPCC; // points to same PISMAtmosCoupler as IceModel::atmosPCC,
                                  //   but we access PDD parameters through this pointer

  int expernum;  // SSL2 is 1, CCL3 is 3, GWL3 is 4
  virtual PetscErrorCode additionalAtStartTimestep();

private:
  PetscTruth     inFileSet, 
                 noEllesmereIcelandDelete,
                 haveSurfaceTemp,
                 haveGeothermalFlux;
  PetscScalar    calculateMeanAnnual(PetscScalar h, PetscScalar lat);
  PetscErrorCode updateTs();
  PetscErrorCode ellePiecewiseFunc(PetscScalar lon, PetscScalar *lat);
  PetscErrorCode cleanExtraLand();
};
#endif

