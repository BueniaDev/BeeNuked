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

// Opout values (for 2-operator FM):
// 0 = 0
// 1 = Slot 1 output

// Algorithm op (for 2-operator FM):
// --x: 1=Use slot 3 for feedback, 0=Use slot 1 for feedback
// -x-: Use opout[x] as slot 3 input
// x--: Include opout[1] in final sum

#define create_algorithm_2op(feedback, op3in, op1out) \
	(feedback | (op3in << 1) | (op1out << 2))

array<uint16_t, 4> algorithm_2op_combinations = 
{
    // <--------|
    // +--[S1]--|--+--[S3]-->
    create_algorithm_2op(0, 1, 0),
    // <-----------------|
    // +--[S1]--+--[S3]--|-->
    create_algorithm_2op(1, 1, 0), 
    //  --[S3]-----|
    // <--------|  |
    // +--[S1]--|--+-->
    create_algorithm_2op(0, 0, 1),
    // <--------|  +--[S3]--|
    // +--[S1]--|--|--------+-->
    create_algorithm_2op(0, 1, 1)
};


array<int, 16> fm_table = 
{
    0, 1, 2, -1,
    3, 4, 5, -1,
    6, 7, 8, -1,
    9, 10, 11, -1
};

array<int, 16> pcm_table = 
{
    0, 4, 8, -1,
    12, 16, 20, -1,
    24, 28, 32, -1,
    36, 40, 44, -1
};

array<double, 16> pow_table =
{
    128, 256, 512, 1024, 2048, 4096, 8192, 16384,
    0.5, 1, 2, 4, 8, 16, 32, 64
};

array<double, 4> fs_frequency = 
{
    1.0/1.0, 1.0/2.0, 1.0/4.0, 1.0/8.0
};

array<double, 16> multiple_table =
{
    0.5, 1,  2,  3,  4,  5,  6,  7,
      8, 9, 10, 11, 12, 13, 14, 15
};

array<double, 16> channel_att_table = 
{
    0.0,  2.5,  6.0,  8.5, 12.0, 14.5, 18.1, 20.6,
   24.1, 26.6, 30.1, 32.6, 36.1, 96.1, 96.1, 96.1
};