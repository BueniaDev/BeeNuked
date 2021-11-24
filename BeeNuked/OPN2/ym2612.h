/*
    This file is part of the BeeNuked engine.
    Copyright (C) 2021 BueniaDev.

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

#ifndef BEENUKED_YM2612
#define BEENUKED_YM2612

#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <array>
using namespace std;

#ifndef M_PI
#define M_PI 3.1415926535
#endif

namespace beenuked
{
    class YM2612
    {
	public:
	    YM2612();
	    ~YM2612();

	    uint32_t get_sample_rate(uint32_t clock_rate);
	    void writeIO(int port, uint8_t data);
	    void clockchip();
	    array<int16_t, 2> get_sample();

	private:
	    template<typename T>
	    bool testbit(T reg, int bit)
	    {
		return ((reg >> bit) & 1) ? true : false;
	    }

	    uint8_t chip_address = 0;
	    bool is_addr_a1 = false;

	    void write_port0(uint8_t reg, uint8_t data);

	    bool is_dac_enabled = false;
	    uint16_t dac_data = 0;

	    int32_t dac_discontinuity(int32_t val);
    };
};

#endif // BEENUKED_YM2612