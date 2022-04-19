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

	    void init_tables();
	    int32_t calc_output(int32_t phase, int32_t mod, uint32_t atten, int wave_sel);
	    uint32_t fetch_sine_result(uint32_t phase, int wave_sel, bool &is_negate);

	    uint8_t chip_address = 0;

	    void write_port0(uint8_t reg, uint8_t data);
	    void write_port1(uint8_t reg, uint8_t data);

	    void write_opl3_select(uint8_t data);

	    void write_fmreg(bool is_port1, uint8_t reg, uint8_t data);

	    int get_channel_state(int ch_num);
	    int get_oper_state(int oper_num);

	    struct opl3_operator
	    {
		uint32_t freq_num = 0;
		int block = 0;

		int multiply = 0;
		int ksl = 0;
		int total_level = 0;
		int tll_val = 0;

		int wave_sel = 0;

		uint32_t phase_counter = 0;
		uint32_t phase_freq = 0;
		uint32_t phase_output = 0;
		array<int32_t, 2> outputs = {0, 0};
	    };

	    struct opl3_channel
	    {
		bool is_extended = false;
		bool is_minion_channel = false;
		bool is_algorithm = false;
		array<bool, 4> is_output = {false, false, false, false};
		int number = 0;
		uint32_t freq_num = 0;
		int block = 0;
		int32_t output = 0;
		array<opl3_operator, 2> opers;
	    };

	    array<opl3_channel, 18> channels;

	    array<uint32_t, 256> sine_table;
	    array<uint32_t, 256> exp_table;

	    void update_frequency(opl3_channel &channel);
	    void update_frequency(opl3_operator &oper);

	    void update_phase(opl3_operator &oper);
	    void update_total_level(opl3_operator &oper);

	    void clock_phase(opl3_channel &channel);

	    void channel_output(opl3_channel &channel);

	    void output_2op(opl3_channel &channel);

	    #include "opl3_tables.inl"

	    bool is_opl3_mode = false;
    };
};

#endif // BEENUKED_YMF262