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

#ifndef BEENUKED_YMF271
#define BEENUKED_YMF271

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
    class YMF271
    {
	public:
	    YMF271();
	    ~YMF271();

	    uint32_t get_sample_rate(uint32_t clock_rate);
	    void init();
	    void writeROM(uint32_t rom_size, uint32_t data_start, uint32_t data_len, vector<uint8_t> rom_data);
	    void writeIO(int port, uint8_t data);
	    void clockchip();
	    vector<int32_t> get_samples();

	    void writeROM(vector<uint8_t> rom_data)
	    {
		writeROM(rom_data.size(), 0, rom_data.size(), rom_data);
	    }

	private:
	    template<typename T>
	    bool testbit(T reg, int bit)
	    {
		return ((reg >> bit) & 1) ? true : false;
	    }

	    void init_tables();
	    void reset();

	    void write_fm(int bank, uint8_t reg, uint8_t data);
	    void write_pcm(uint8_t reg, uint8_t data);
	    void write_timer(uint8_t reg, uint8_t data);

	    void write_fm_reg(int slot_num, int reg, uint8_t data);

	    struct opx_group
	    {
		int sync = 0;
		bool is_pfm = false;
		array<int32_t, 4> outputs = {0, 0, 0, 0};
	    };

	    struct opx_slot
	    {
		int number = 0;
		uint32_t start_address = 0;
		bool is_alt_loop = false;
		uint32_t end_address = 0;
		uint32_t loop_address = 0;
		uint64_t step_ptr = 0;
		uint32_t step = 0;
		int multiply = 0;
		int total_level = 0 ;
		uint32_t freq_num = 0;
		uint8_t freq_hi = 0;
		int block = 0;
		int fs = 0;
		bool is_12_bit = false;
		int srcnote = 0;
		int srcb = 0;

		int ext_out = 0;
		bool ext_enable = false;

		int waveform = 0;
		bool is_key_on = false;
		array<int, 4> ch_level = {0, 0, 0, 0};
	    };

	    array<uint8_t, 6> chip_address;

	    array<opx_group, 12> groups;
	    array<opx_slot, 48> slots;

	    void key_on(opx_slot &slot);
	    void key_off(opx_slot &slot);

	    void calculate_step(opx_slot &slot);

	    void update_pcm(opx_slot &slot);

	    array<int, 16> attenutation_table;
	    array<int, 128> total_level_table;

	    array<int32_t, 4> outputs = {0, 0, 0, 0};

	    int64_t calc_slot_volume(opx_slot &slot);

	    uint8_t readROM(uint32_t addr);

	    vector<uint8_t> opx_rom;

	    #include "opx_tables.inl"
	};
};


#endif // BEENUKED_YMF271