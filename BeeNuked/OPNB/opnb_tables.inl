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

// ADPCM delta table
array<uint16_t, 49> adpcm_steps = 
{
     16,  17,   19,   21,   23,   25,   28,
     31,  34,   37,   41,   45,   50,   55,
     60,  66,   73,   80,   88,   97,  107,
    118, 130,  143,  157,  173,  190,  209,
    230, 253,  279,  307,  337,  371,  408,
    449, 494,  544,  598,  658,  724,  796,
    876, 963, 1060, 1166, 1282, 1411, 1552
};

// ADPCM step increment values
array<int8_t, 8> adpcm_steps_inc =
{
    -1, -1, -1, -1,
     2,  5,  7,  9
};

// Delta-T ADPCM step scale values
array<uint8_t, 8> delta_t_step_scale = 
{
    57,  57,  57,  57,
    77, 102, 128, 153
};