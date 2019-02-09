/* Copyright (C) 2013, 2014, 2015, 2016, 2017, 2018, 2019 PISM Authors
 *
 * This file is part of PISM.
 *
 * PISM is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * PISM is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PISM; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "EigenCalving.hh"

#include "pism/util/IceGrid.hh"
#include "pism/util/error_handling.hh"
#include "pism/util/IceModelVec2CellType.hh"
#include "pism/stressbalance/StressBalance.hh"
#include "pism/geometry/Geometry.hh"
#include "pism/frontretreat/util/remove_narrow_tongues.hh"

namespace pism {
namespace calving {

EigenCalving::EigenCalving(IceGrid::ConstPtr grid)
  : StressCalving(grid, 2) {

  m_K = m_config->get_double("calving.eigen_calving.K");
}

EigenCalving::~EigenCalving() {
  // empty
}

void EigenCalving::init() {

  m_log->message(2, "* Initializing the 'eigen-calving' mechanism...\n");

  if (fabs(m_grid->dx() - m_grid->dy()) / std::min(m_grid->dx(), m_grid->dy()) > 1e-2) {
    throw RuntimeError::formatted(PISM_ERROR_LOCATION, "-calving eigen_calving using a non-square grid cell is not implemented (yet);\n"
                                  "dx = %f, dy = %f, relative difference = %f",
                                  m_grid->dx(), m_grid->dy(),
                                  fabs(m_grid->dx() - m_grid->dy()) / std::max(m_grid->dx(), m_grid->dy()));
  }

  m_strain_rates.set(0.0);

}

void EigenCalving::update(double dt,
                          const Geometry &geometry,
                          const IceModelVec2Int &bc_mask,
                          const IceModelVec2V &ice_velocity,
                          IceModelVec2CellType &cell_type,
                          IceModelVec2S &Href,
                          IceModelVec2S &ice_thickness) {

  compute_retreat_rate(cell_type, ice_velocity, m_horizontal_retreat_rate);

  update_geometry(dt,
                  geometry.sea_level_elevation,
                  geometry.bed_elevation,
                  bc_mask,
                  m_horizontal_retreat_rate,
                  cell_type, Href, ice_thickness);

  // remove narrow ice tongues
  remove_narrow_tongues(cell_type, ice_thickness);

  // update cell_type again
  GeometryCalculator gc(*m_config);
  gc.compute_mask(geometry.sea_level_elevation,
                  geometry.bed_elevation, ice_thickness, cell_type);
}

//! \brief Uses principal strain rates to apply "eigencalving" with constant K.
/*!
  See equation (26) in [\ref Winkelmannetal2011].
*/
void EigenCalving::compute_retreat_rate(const IceModelVec2CellType &cell_type,
                                        const IceModelVec2V &ice_velocity,
                                        IceModelVec2S &result) const {

  prepare_mask(cell_type, m_mask);

  // Distance (grid cells) from calving front where strain rate is evaluated
  int offset = m_stencil_width;

  // eigenCalvOffset allows adjusting the transition from
  // compressive to extensive flow regime
  const double eigenCalvOffset = 0.0;

  stressbalance::compute_2D_principal_strain_rates(ice_velocity,
                                                   m_mask,
                                                   m_strain_rates);
  m_strain_rates.update_ghosts();

  IceModelVec::AccessList list{&m_mask, &result, &m_strain_rates};

  // Compute the horizontal calving rate
  for (Points pt(*m_grid); pt; pt.next()) {
    const int i = pt.i(), j = pt.j();

    // Find partially filled or empty grid boxes on the icefree ocean, which
    // have floating ice neighbors after the mass continuity step
    if (m_mask.ice_free_ocean(i, j) and m_mask.next_to_floating_ice(i, j)) {

      // Average of strain-rate eigenvalues in adjacent floating grid cells to be used for
      // eigen-calving:
      double
        eigen1 = 0.0,
        eigen2 = 0.0;
      {
        int N = 0;
        for (int p = -1; p < 2; p += 2) {
          const int I = i + p * offset;
          if (m_mask.floating_ice(I, j) and not m_mask.ice_margin(I, j)) {
            eigen1 += m_strain_rates(I, j, 0);
            eigen2 += m_strain_rates(I, j, 1);
            N += 1;
          }
        }

        for (int q = -1; q < 2; q += 2) {
          const int J = j + q * offset;
          if (m_mask.floating_ice(i, J) and not m_mask.ice_margin(i, J)) {
            eigen1 += m_strain_rates(i, J, 0);
            eigen2 += m_strain_rates(i, J, 1);
            N += 1;
          }
        }

        if (N > 0) {
          eigen1 /= N;
          eigen2 /= N;
        }
      }

      // Calving law
      //
      // eigen1 * eigen2 has units [s^-2] and calving_rate_horizontal
      // [m*s^1] hence, eigen_calving_K has units [m*s]
      if (eigen2 > eigenCalvOffset and eigen1 > 0.0) {
        // spreading in all directions
        result(i, j) = m_K * eigen1 * (eigen2 - eigenCalvOffset);
      } else {
        result(i, j) = 0.0;
      }

    } else { // end of "if (ice_free_ocean and next_to_floating)"
      result(i, j) = 0.0;
    }
  } // end of the loop over grid points

}

MaxTimestep EigenCalving::max_timestep(const IceModelVec2CellType &cell_type,
                                       const IceModelVec2V &ice_velocity) const {

  if (not m_restrict_timestep) {
    return MaxTimestep("eigencalving");
  }

  compute_retreat_rate(cell_type, ice_velocity, m_tmp);

  auto info = FrontRetreat::max_timestep(m_tmp);

  m_log->message(3,
                 "  eigencalving: maximum rate = %.2f m/year gives dt=%.5f years\n"
                 "                mean rate    = %.2f m/year over %d cells\n",
                 convert(m_sys, info.rate_max, "m second-1", "m year-1"),
                 convert(m_sys, info.dt.value(), "seconds", "years"),
                 convert(m_sys, info.rate_mean, "m second-1", "m year-1"),
                 info.N_cells);

  return MaxTimestep(info.dt.value(), "eigencalving");
}

DiagnosticList EigenCalving::diagnostics_impl() const {
  return {{"eigen_calving_rate",
        Diagnostic::Ptr(new FrontRetreatRate(this, "eigen_calving_rate",
                                             "horizontal calving rate due to eigen-calving"))}};
}

} // end of namespace calving
} // end of namespace pism
