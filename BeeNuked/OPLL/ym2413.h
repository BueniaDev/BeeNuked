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

#ifndef BEENUKED_YM2413
#define BEENUKED_YM2413

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
    class YM2413
    {
	public:
	    YM2413();
	    ~YM2413();

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

	    int32_t calc_output(int32_t phase, int32_t mod, uint32_t env);

	    void write_reg(uint8_t reg, uint8_t data);

	    void init_tables();

	    array<uint32_t, 256> sine_table;
	    array<uint32_t, 256> exp_table;

	    typedef array<uint8_t, 8> opll_inst;
	    typedef array<opll_inst, 19> opll_patch;

	    #include "opll_tables.inl"

	    opll_patch inst_patch;

	    struct opll_operator
	    {
		bool is_carrier = false;
		uint32_t freq_num = 0;
		int block = 0;
		int multiply = 0;
		int ksl = 0;
		int total_level = 0;
		int volume = 0;
		int tll_val = 0;
		uint32_t phase_counter = 0;
		uint32_t phase_freq = 0;
		uint32_t phase_output = 0;
		array<int32_t, 2> outputs = {0, 0};
	    };

	    struct opll_channel
	    {
		int number = 0;
		uint32_t freq_num = 0;
		int block = 0;
		int feedback = 0;
		uint32_t inst_vol_reg = 0;
		int inst_number = 0;
		int32_t output = 0;
		array<opll_operator, 2> opers;
	    };

	    array<opll_channel, 9> channels;

	    void update_frequency(opll_channel &channel);
	    void update_frequency(opll_operator &oper);
	    void update_phase(opll_operator &oper);
	    void update_total_level(opll_operator &oper);
	    void clock_phase(opll_channel &channel);
	    void channel_output(opll_channel &channel);

	    void set_patch(opll_channel &channel, int patch_index);

	    void update_instrument(opll_channel &channel);
    };
};

#endif // BEENUKED_YM2413