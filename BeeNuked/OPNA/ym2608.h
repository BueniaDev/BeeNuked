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

#ifndef BEENUKED_YM2608
#define BEENUKED_YM2608

#include "utils.h"

namespace beenuked
{
    class YM2608
    {
	public:
	    YM2608();
	    ~YM2608();

	    uint32_t get_sample_rate(uint32_t clk_rate);
	    void init();
	    void setInterface(BeeNukedInterface *cb);

	    uint8_t readIO(int port);
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

	    enum : int
	    {
		prescaler_six = 0,
		prescaler_three = 1,
		prescaler_two = 2
	    };

	    BeeNukedInterface *inter = NULL;

	    int prescaler_val = 0;

	    int fm_samples_per_output = 0;

	    uint64_t ssg_sample_index = 0;

	    uint8_t chip_address = 0;
	    bool is_addr_a1 = false;

	    array<int32_t, 3> last_samples = {0, 0, 0};

	    void write_port0(uint8_t reg, uint8_t data);

	    void set_prescaler(int value);

	    void configure_ssg_resampler(uint8_t out_samples, uint8_t src_samples);

	    using resample_func = function<void()>;

	    resample_func resample;

	    void resample_4_3();
	    void resample_2_3();
	    void resample_1_3();
	    void resample_nop();

	    array<int32_t, 3> ssg_samples = {0, 0, 0};
	    array<int32_t, 3> last_ssg_samples = {0, 0, 0};

	    void add_last(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale = 1);
	    void clock_and_add(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale = 1);
	    void write_to_output(int32_t sum0, int32_t sum1, int32_t sum2, int divisor = 1);

	    void update_prescaler();

	    void clock_fm_and_adpcm();
	    void output_fm_and_adpcm();

	    void clock_adpcm();

	    void write_adpcm(uint8_t reg, uint8_t data);

	    struct opna_adpcm
	    {
		bool is_keyon = false;
		bool is_pan_left = false;
		bool is_pan_right = false;
		int total_level = 0;
		uint32_t start_address = 0;
		uint32_t end_address = 0;
		uint32_t current_address = 0;
		bool is_high_nibble = false;
		uint8_t current_byte = 0;
		int32_t reg_accum = 0;
		int32_t current_step = 0;
		array<int32_t, 2> output = {0, 0};
	    };

	    int adpcm_total_level = 0;
	    array<opna_adpcm, 6> adpcm_channels;

	    uint32_t env_timer = 0;
	    uint32_t env_clock = 0;
	    uint32_t adpcm_ch_clock = 0;

	    #include "ym2608_adpcm_rom.inl"
	    #include "opna_tables.inl"

	    void adpcm_key_on(opna_adpcm &channel);

	    void clock_adpcm(opna_adpcm &channel);
	    void output_adpcm(opna_adpcm &channel);

	    uint8_t fetchADPCMROM(uint32_t address);
    };
};

#endif // BEENUKED_YM2608