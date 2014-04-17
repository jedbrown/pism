// Copyright (C) 2011, 2012, 2013, 2014 PISM Authors
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
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

#ifndef _PISMMOHRCOULOMBYIELDSTRESS_H_
#define _PISMMOHRCOULOMBYIELDSTRESS_H_

#include "PISMDiagnostic.hh"
#include "PISMYieldStress.hh"
#include "PISMHydrology.hh"
#include "iceModelVec.hh"


//! \brief PISM's default basal yield stress model which applies the Mohr-Coulomb model of deformable, pressurized till.
class PISMMohrCoulombYieldStress : public PISMYieldStress
{
public:
  PISMMohrCoulombYieldStress(IceGrid &g, const PISMConfig &conf, PISMHydrology *hydro);

  virtual ~PISMMohrCoulombYieldStress();

  virtual PetscErrorCode init(PISMVars &vars);

  virtual void add_vars_to_output(std::string keyword, std::set<std::string> &result);

  virtual PetscErrorCode define_variables(std::set<std::string> vars, const PIO &nc,
                                          PISM_IO_Type nctype);

  virtual PetscErrorCode write_variables(std::set<std::string> vars, const PIO &nc);

  virtual PetscErrorCode update(double my_t, double my_dt);

  virtual PetscErrorCode basal_material_yield_stress(IceModelVec2S &result);

protected:
  IceModelVec2S m_till_phi, m_tauc, m_tillwat, m_Po;
  IceModelVec2S m_bwat;  // only allocated and used if tauc_add_transportable_water = true
  IceModelVec2S *m_bed_topography;
  IceModelVec2Int *m_mask;
  PISMVars *m_variables;
  PISMHydrology *m_hydrology;

  PetscErrorCode allocate();
  PetscErrorCode topg_to_phi();
  PetscErrorCode tauc_to_phi();
};

#endif /* _PISMMOHRCOULOMBYIELDSTRESS_H_ */
