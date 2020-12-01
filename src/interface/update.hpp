//========================================================================================
// (C) (or copyright) 2020. Triad National Security, LLC. All rights reserved.
//
// This program was produced under U.S. Government contract 89233218CNA000001 for Los
// Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC
// for the U.S. Department of Energy/National Nuclear Security Administration. All rights
// in the program are reserved by Triad National Security, LLC, and the U.S. Department
// of Energy/National Nuclear Security Administration. The Government is granted for
// itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
// license in this material to reproduce, prepare derivative works, distribute copies to
// the public, perform publicly and display publicly, and to permit others to do so.
//========================================================================================
#ifndef INTERFACE_UPDATE_HPP_
#define INTERFACE_UPDATE_HPP_

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "defs.hpp"
#include "interface/metadata.hpp"
#include "interface/params.hpp"
#include "interface/state_descriptor.hpp"

#include "kokkos_abstraction.hpp"

namespace parthenon {

namespace Update {

template <typename T>
TaskStatus FluxDivergence(std::shared_ptr<T> &in, std::shared_ptr<T> &dudt_obj);

template <typename F, typename T>
void UpdateData(const std::vector<F> &flags, T &in, T &dudt, const Real dt, T &out) {
  const auto &in_pack = in->PackVariables(flags);
  auto out_pack = out->PackVariables(flags);
  const auto &dudt_pack = dudt->PackVariables(flags);
  parthenon::par_for(
      DEFAULT_LOOP_PATTERN, "UpdateMeshData", DevExecSpace(), 0, in_pack.GetDim(5) - 1, 0,
      in_pack.GetDim(4) - 1, 0, in_pack.GetDim(3) - 1, 0, in_pack.GetDim(2) - 1, 0,
      in_pack.GetDim(1) - 1,
      KOKKOS_LAMBDA(const int b, const int l, const int k, const int j, const int i) {
        out_pack(b, l, k, j, i) = in_pack(b, l, k, j, i) + dt * dudt_pack(b, l, k, j, i);
      });
}

template <typename T>
void UpdateIndependentData(T &in, T &dudt, const Real dt, T &out) {
  UpdateData(std::vector<MetadataFlag>({Metadata::Independent}), in, dudt, dt, out);
}

template <typename F, typename T>
void AverageData(const std::vector<F> &flags, T &c1, T &c2, const Real wgt1) {
  auto c1_pack = c1->PackVariables(flags);
  const auto &c2_pack = c2->PackVariables(flags);
  parthenon::par_for(
      DEFAULT_LOOP_PATTERN, "AverageMeshData", DevExecSpace(), 0, c1_pack.GetDim(5) - 1,
      0, c1_pack.GetDim(4) - 1, 0, c1_pack.GetDim(3) - 1, 0, c1_pack.GetDim(2) - 1, 0,
      c1_pack.GetDim(1) - 1,
      KOKKOS_LAMBDA(const int b, const int l, const int k, const int j, const int i) {
        c1_pack(b, l, k, j, i) =
            wgt1 * c1_pack(b, l, k, j, i) + (1 - wgt1) * c2_pack(b, l, k, j, i);
      });
}

template <typename T>
void AverageIndependentData(T &c1, T &c2, const Real wgt1) {
  AverageData(std::vector<MetadataFlag>({Metadata::Independent}), c1, c2, wgt1);
}

template <typename T>
TaskStatus EstimateTimestep(std::shared_ptr<T> &rc) {
  Kokkos::Profiling::pushRegion("Task_EstimateTimestep");
  Real dt_min = std::numeric_limits<Real>::max();
  for (const auto &pkg : rc->GetParentPointer()->packages) {
    Real dt = pkg.second->EstimateTimestep(rc);
    dt_min = std::min(dt_min, dt);
  }
  rc->SetAllowedDt(dt_min);
  Kokkos::Profiling::popRegion(); // Task_EstimateTimestep
  return TaskStatus::complete;
}

template <typename T>
TaskStatus FillDerived(std::shared_ptr<T> &rc) {
  Kokkos::Profiling::pushRegion("Task_FillDerived");
  auto pm = rc->GetParentPointer();
  Kokkos::Profiling::pushRegion("PreFillDerived");
  for (const auto &pkg : pm->packages) {
    pkg.second->PreFillDerived(rc);
  }
  Kokkos::Profiling::popRegion(); // PreFillDerived
  Kokkos::Profiling::pushRegion("FillDerived");
  for (const auto &pkg : pm->packages) {
    pkg.second->FillDerived(rc);
  }
  Kokkos::Profiling::popRegion(); // FillDerived
  Kokkos::Profiling::pushRegion("PostFillDerived");
  for (const auto &pkg : pm->packages) {
    pkg.second->PostFillDerived(rc);
  }
  Kokkos::Profiling::popRegion(); // PostFillDerived
  Kokkos::Profiling::popRegion(); // Task_FillDerived
  return TaskStatus::complete;
}

} // namespace Update

} // namespace parthenon

#endif // INTERFACE_UPDATE_HPP_
