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

#ifndef BEENUKED_YMF262
#define BEENUKED_YMF262

#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <array>
#include <vector>
using namespace std;

#ifndef M_PI
#define M_PI 3.1415926535
#endif

namespace beenuked
{
    class YMF262
    {
	public:
	    YMF262();
	    ~YMF262();

	    uint32_t get_sample_rate(uint32_t clock_rate);
	    void init();
	    void writeIO(int port, uint8_t data);
	    void clockchip();
	    vector<int32_t> get_samples();

	private:
	    template<typename T>
	    bool testbit(T reg, int bit)
	    {
		return ((reg >> bit) & 1) ? true : false;
	    }

	    void reset();

	    uint8_t chip_address = 0;

	    void write_port0(uint8_t reg, uint8_t data);
	    void write_port1(uint8_t reg, uint8_t data);

	    void write_opl3_select(uint8_t data);

	    bool is_opl3_mode = false;
    };
};

#endif // BEENUKED_YMF262