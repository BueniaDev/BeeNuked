/*
    This file is part of the BeeNuked engine.
    Copyright (C) 2022 BueniaDev.

    BeeNuked is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BeeNuked is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with BeeNuked.  If not, see <https://www.gnu.org/licenses/>.
*/

// Table for determining slot order
array<int, 32> slot_array =
{
     0,  2,  4,  1,  3,  5, -1, -1,
     6,  8, 10,  7,  9, 11, -1, -1,
    12, 14, 16, 13, 15, 17, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1
};

// Table for multiplying factors
array<int, 16> multiply_table =
{
     1,  2,  4,  6,  8, 10, 12, 14,
    16, 18, 20, 20, 24, 24, 30, 30
};

// Table for KSL factors
array<uint8_t, 16> ksl_table =
{
    112, 64, 48, 38, 32, 26, 22, 18,
     16, 12, 10,  8,  6,  4,  2,  0
};