// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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

#include <array>
#include <initializer_list>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <optional>

namespace roofer {

  template <typename T>
  struct TBox {
    std::array<T, 3> pmin, pmax;
    bool just_cleared;

    TBox() { clear(); };

    TBox(const TBox& otherBox)
        : pmin(otherBox.min()),
          pmax(otherBox.max()),
          just_cleared(otherBox.just_cleared){};

    TBox(std::initializer_list<T> initList) {
      clear();
      auto it = initList.begin();
      pmin[0] = *it++;
      pmin[1] = *it++;
      pmin[2] = *it++;
      pmax[0] = *it++;
      pmax[1] = *it++;
      pmax[2] = *it;
      just_cleared = false;
    }

    std::array<T, 3> min() const { return pmin; };
    std::array<T, 3> max() const { return pmax; };
    T size_x() const { return pmax[0] - pmin[0]; };
    T size_y() const { return pmax[1] - pmin[1]; };
    void set(std::array<T, 3> nmin, std::array<T, 3> nmax) {
      pmin = nmin;
      pmax = nmax;
      just_cleared = false;
    };
    void add(T p[]) {
      if (just_cleared) {
        pmin[0] = p[0];
        pmin[1] = p[1];
        pmin[2] = p[2];
        pmax[0] = p[0];
        pmax[1] = p[1];
        pmax[2] = p[2];
        just_cleared = false;
      }
      pmin[0] = std::min(p[0], pmin[0]);
      pmin[1] = std::min(p[1], pmin[1]);
      pmin[2] = std::min(p[2], pmin[2]);
      pmax[0] = std::max(p[0], pmax[0]);
      pmax[1] = std::max(p[1], pmax[1]);
      pmax[2] = std::max(p[2], pmax[2]);
    };
    void add(std::array<T, 3> a) { add(a.data()); };
    void add(const TBox& otherBox) {
      add(otherBox.min());
      add(otherBox.max());
    };
    void add(TBox& otherBox) {
      add(otherBox.min());
      add(otherBox.max());
    };
    ;
    void add(const std::vector<std::array<T, 3>>& vec) {
      for (auto& p : vec) add(p);
    };
    void add(std::vector<std::array<T, 3>>& vec) {
      for (auto& p : vec) add(p);
    };
    std::optional<TBox> intersect(const TBox& otherBox) const {
      TBox result;
      result.pmin[0] = std::max(pmin[0], otherBox.pmin[0]);
      result.pmin[1] = std::max(pmin[1], otherBox.pmin[1]);
      result.pmin[2] = std::max(pmin[2], otherBox.pmin[2]);
      result.pmax[0] = std::min(pmax[0], otherBox.pmax[0]);
      result.pmax[1] = std::min(pmax[1], otherBox.pmax[1]);
      result.pmax[2] = std::min(pmax[2], otherBox.pmax[2]);
      // FIXME: this can return a box that is flat in one or more dimensions
      if (result.pmin[0] > result.pmax[0] || result.pmin[1] > result.pmax[1] ||
          result.pmin[2] > result.pmax[2]) {
        return std::nullopt;
      }
      return result;
    };
    bool intersects(const TBox& otherBox) const {
      bool intersect_x =
          (pmin[0] < otherBox.pmax[0]) && (pmax[0] > otherBox.pmin[0]);
      bool intersect_y =
          (pmin[1] < otherBox.pmax[1]) && (pmax[1] > otherBox.pmin[1]);
      return intersect_x && intersect_y;
    };
    bool intersects(const std::array<T, 3>& qpoint) const {
      bool intersect_x = (pmin[0] <= qpoint[0]) && (pmax[0] > qpoint[0]);
      bool intersect_y = (pmin[1] <= qpoint[1]) && (pmax[1] > qpoint[1]);
      return intersect_x && intersect_y;
    };
    void clear() {
      pmin.fill(0);
      pmax.fill(0);
      just_cleared = true;
    };
    bool isEmpty() const { return just_cleared; };
    std::array<T, 3> center() const {
      return {(pmax[0] + pmin[0]) / 2, (pmax[1] + pmin[1]) / 2,
              (pmax[2] + pmin[2]) / 2};
    };
    std::string wkt() const {
      if (isEmpty()) return "POLYGON EMPTY";
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(2);
      oss << "POLYGON((";
      oss << pmin[0] << " " << pmin[1] << ", ";
      oss << pmax[0] << " " << pmin[1] << ", ";
      oss << pmax[0] << " " << pmax[1] << ", ";
      oss << pmin[0] << " " << pmax[1] << ", ";
      oss << pmin[0] << " " << pmin[1];
      oss << "))";
      return oss.str();
    }
  };

  typedef TBox<float> Box;
}  // namespace roofer
