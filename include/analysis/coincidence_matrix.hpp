/*
     This file is part of carolina.

    carolina is free software: you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free Software
   Foundation, either version 3 of the License, or (at your option) any later
   version.

    carolina is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    carolina. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>

using std::string;

#include <utility>

using std::pair;

#include <vector>

using std::vector;

#include "histogram_properties.hpp"

struct CoincidenceMatrix {
    const string name;

    const vector<size_t> detectors_x;
    const vector<size_t> detectors_y;

    const Histogram x_axis;
    const Histogram y_axis;

    vector<pair<size_t, size_t>> get_coincidence_pairs() const;
};