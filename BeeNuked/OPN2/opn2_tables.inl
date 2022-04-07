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

#define create_algorithm(op2in, op3in, op4in, op1out, op2out, op3out) \
    (op2in | (op3in << 1) | (op4in << 4) | (op1out << 7) | (op2out << 8) | (op3out << 9))

array<uint16_t, 8> algorithm_combinations = 
{
    create_algorithm(1,2,3, 0,0,0), // Algorithm 0: O1 -> O2 -> O3 -> O4 -> out (O4)
    create_algorithm(0,5,3, 0,0,0), // Algorithm 1: (O1 + O2) -> O3 -> O4 -> out (O4)
    create_algorithm(0,2,6, 0,0,0), // Algorithm 2: (O1 + (O2 -> O3)) -> O4 -> out (O4)
    create_algorithm(1,0,7, 0,0,0), // Algorithm 3: ((O1 -> O2) + O3) -> O4 -> out (O4)
    create_algorithm(1,0,3, 0,1,0), // Algorithm 4: ((O1 -> O2) + (O3 -> O4)) -> out (O2 + O4)
    create_algorithm(1,1,1, 0,1,1), // Algorithm 5: ((O1 -> O2) + (O1 -> O3) + (O1 -> O4)) -> out (O2 + O3 + O4)
    create_algorithm(1,0,0, 0,1,1), // Algorithm 6: ((O1 -> O2) + O3 + O4) -> out (O2 + O3 + O4)
    create_algorithm(0,0,0, 1,1,1), // Algorithm 7: (O1 + O2 + O3 + O4) -> out (O1 + O2 + O3 + O4)
};


array<uint8_t, 16> fnum_to_keycode =
{
    // F11 = 0
    0, 0, 0, 0, 0, 0, 0, 1,
    // F11 = 1
    2, 3, 3, 3, 3, 3, 3, 3
};

// Detune table (courtesy of Nemesis)
array<array<uint32_t, 4>, 32> detune_table = 
{
    0, 0,  1,  2,   // 0  (0x00)
    0, 0,  1,  2,   // 1  (0x01)
    0, 0,  1,  2,   // 2  (0x02)
    0, 0,  1,  2,   // 3  (0x03)
    0, 1,  2,  2,   // 4  (0x04)
    0, 1,  2,  3,   // 5  (0x05)
    0, 1,  2,  3,   // 6  (0x06)
    0, 1,  2,  3,   // 7  (0x07)
    0, 1,  2,  4,   // 8  (0x08)
    0, 1,  3,  4,   // 9  (0x09)
    0, 1,  3,  4,   // 10 (0x0A)
    0, 1,  3,  5,   // 11 (0x0B)
    0, 2,  4,  5,   // 12 (0x0C)
    0, 2,  4,  6,   // 13 (0x0D)
    0, 2,  4,  6,   // 14 (0x0E)
    0, 2,  5,  7,   // 15 (0x0F)
    0, 2,  5,  8,   // 16 (0x10)
    0, 3,  6,  8,   // 17 (0x11)
    0, 3,  6,  9,   // 18 (0x12)
    0, 3,  7, 10,   // 19 (0x13)
    0, 4,  8, 11,   // 20 (0x14)
    0, 4,  8, 12,   // 21 (0x15)
    0, 4,  9, 13,   // 22 (0x16)
    0, 5, 10, 14,   // 23 (0x17)
    0, 5, 11, 16,   // 24 (0x18)
    0, 6, 12, 17,   // 25 (0x19)
    0, 6, 13, 19,   // 26 (0x1A)
    0, 7, 14, 20,   // 27 (0x1B)
    0, 8, 16, 22,   // 28 (0x1C)
    0, 8, 16, 22,   // 29 (0x1D)
    0, 8, 16, 22,   // 30 (0x1E)
    0, 8, 16, 22,   // 31 (0x1F)
};

// Table for counter shift values (courtesy of Nemesis)
array<uint8_t, 64> counter_shift_table = 
{
    11, 11, 11, 11, 
    10, 10, 10, 10,
     9,  9,  9,  9,
     8,  8,  8,  8,
     7,  7,  7,  7,
     6,  6,  6,  6,
     5,  5,  5,  5,
     4,  4,  4,  4,
     3,  3,  3,  3,
     2,  2,  2,  2,
     1,  1,  1,  1,
     0,  0,  0,  0,
     0,  0,  0,  0,
     0,  0,  0,  0,
     0,  0,  0,  0,
     0,  0,  0,  0
};

// Table for attenuation increment values (courtesy of Nemesis)
array<array<uint8_t, 8>, 64> att_inc_table =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 2, 2, 1, 2, 2, 2, 
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 4, 4, 2, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 8, 4, 8, 4, 8, 4, 8, 4, 8, 4, 8, 8, 8, 4, 8, 8, 8, 
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 
};

array<uint8_t, 8> lfo_max_count = 
{
    109, 78, 72, 68,
     63, 45,  9,  6
};

array<array<uint8_t, 8>, 8> lfo_pm_shifts =
{
    0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,
    0x77, 0x77, 0x77, 0x77, 0x72, 0x72, 0x72, 0x72,
    0x77, 0x77, 0x77, 0x72, 0x72, 0x72, 0x17, 0x17,
    0x77, 0x77, 0x72, 0x72, 0x17, 0x17, 0x12, 0x12,
    0x77, 0x77, 0x72, 0x17, 0x17, 0x17, 0x12, 0x07,
    0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01,
    0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01,
    0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01
};