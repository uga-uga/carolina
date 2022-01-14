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

#include "branch.hpp"
#include "scaler_module.hpp"

struct V830 final : public ScalerModule {
    V830(const double trigger_frequency)
        : ScalerModule(trigger_frequency), counter_values("counter_values") {
        for (size_t i = 0; i < 32; ++i) {
            counter_values.leaves[i] = 0.;
        }
    }
    ~V830() {}

    Branch<double, 32> counter_values;

    long long get_counts(const size_t leaf) override final {
        return counter_values.leaves[leaf];
    }

    void add_counts(const size_t leaf, const long long counts) override final {
        counter_values.leaves[leaf] += (double)counts;
    }

    void reset_raw_leaves() override final;
    void set_up_raw_branches_for_reading(TTree *tree) override final;
    void set_up_raw_branches_for_writing(TTree *tree) override final;
};