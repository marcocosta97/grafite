/*
 * This file is part of Grafite <https://github.com/marcocosta97/grafite>.
 * Copyright (C) 2023 Marco Costa.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include "grafite/grafite.hpp"

int main()
{
    std::vector<int> keys (1000000);
    std::generate(keys.begin(), keys.end(), std::rand);
    keys.push_back(42);

    // Construct the Grafite range filter
    auto bpk = 12.0;
    auto g = grafite::filter(keys.begin(), keys.end(), bpk);

    // Query the Grafite range filter
    auto left = 40, right = 44;
    auto result = g.query(left, right);
    std::cout << "Query result: " << result << std::endl;

    return 0;
}