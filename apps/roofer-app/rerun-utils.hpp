// Copyright (c) 2018-2025 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ravi Peters

#ifdef RF_USE_RERUN
// Adapters so we can log eigen vectors as rerun positions:
template <>
struct rerun::CollectionAdapter<rerun::Position3D, roofer::PointCollection> {
  /// Borrow for non-temporary.
  Collection<rerun::Position3D> operator()(
      const roofer::PointCollection& container) {
    return Collection<rerun::Position3D>::borrow(container.data(),
                                                 container.size());
  }

  // Do a full copy for temporaries (otherwise the data might be deleted when
  // the temporary is destroyed).
  Collection<rerun::Position3D> operator()(
      roofer::PointCollection&& container) {
    std::vector<rerun::Position3D> positions(container.size());
    memcpy(positions.data(), container.data(),
           container.size() * sizeof(roofer::arr3f));
    return Collection<rerun::Position3D>::take_ownership(std::move(positions));
  }
};
template <>
struct rerun::CollectionAdapter<rerun::Position3D, roofer::TriangleCollection> {
  /// Borrow for non-temporary.
  Collection<rerun::Position3D> operator()(
      const roofer::TriangleCollection& container) {
    return Collection<rerun::Position3D>::borrow(container[0].data(),
                                                 container.vertex_count());
  }

  // Do a full copy for temporaries (otherwise the data might be deleted when
  // the temporary is destroyed).
  Collection<rerun::Position3D> operator()(
      roofer::TriangleCollection&& container) {
    std::vector<rerun::Position3D> positions(container.size());
    memcpy(positions.data(), container[0].data(),
           container.vertex_count() * sizeof(roofer::arr3f));
    return Collection<rerun::Position3D>::take_ownership(std::move(positions));
  }
};
#endif
