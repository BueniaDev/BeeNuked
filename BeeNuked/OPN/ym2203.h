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

#ifndef BEENUKED_YM2203
#define BEENUKED_YM2203

#include "utils.h"

namespace beenuked
{
    class YM2203
    {
	public:
	    YM2203();
	    ~YM2203();

	    uint32_t get_sample_rate(uint32_t clock_rate);
	    void init();
	    void setInterface(BeeNukedInterface *cb);
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

	    array<int32_t, 4> last_samples = {0, 0, 0, 0};

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

	    void write_reg(uint8_t reg, uint8_t data);
	    void write_mode(uint8_t reg, uint8_t data);
	    void write_fmreg(uint8_t reg, uint8_t data);

	    uint32_t env_timer = 0;
	    uint32_t env_clock = 0;

	    enum opn_oper_state : int
	    {
		Attack = 0,
		Decay = 1,
		Sustain = 2,
		Release = 3,
		Off = 4,
	    };

	    struct opn_operator
	    {
		int freq_num = 0;
		int block = 0;
		int multiply = 0;
		int detune = 0;
		int total_level = 0;

		int keycode = 0;
		int key_scaling = 0;
		int ksr_val = 0;
		int attack_rate = 0;
		int decay_rate = 0;
		int sustain_rate = 0;
		int sustain_level = 0;
		int release_rate = 0;

		uint32_t phase_counter = 0;
		uint32_t phase_freq = 0;
		uint32_t phase_output = 0;
		bool is_keyon = false;

		bool ssg_enable = false;
		bool ssg_att = false;
		bool ssg_alt = false;
		bool ssg_hold = false;
		bool ssg_inv = false;

		int32_t env_output = 0;
		int env_rate = 0;
		opn_oper_state env_state;

		array<int32_t, 2> outputs = {0, 0};
	    };

	    struct opn_channel
	    {
		int number = 0;
		int freq_num = 0;
		int block = 0;
		int ch_mode = 0;

		array<int, 3> oper_fnums = {0, 0, 0};
		array<int, 3> oper_block = {0, 0, 0};
		bool is_csm_keyon = false;

		int feedback = 0;
		int algorithm = 0;
		int32_t output = 0;
		array<opn_operator, 4> opers;
	    };

	    array<opn_channel, 3> channels;

	    void init_tables();

	    array<uint32_t, 256> sine_table;
	    array<uint32_t, 256> exp_table;

	    int32_t calc_output(uint32_t phase, int32_t mod, uint32_t env);

	    void update_frequency(opn_channel &channel);
	    void update_frequency(opn_operator &oper);
	    void update_phase(opn_operator &oper);
	    void update_ksr(opn_operator &oper);
	    void calc_oper_rate(opn_operator &oper);

	    void clock_phase(opn_channel &channel);
	    void clock_envelope_gen();
	    void clock_ssg_eg(opn_channel &channel);
	    void clock_envelope(opn_channel &channel);

	    void update_ch3_mode(int val);
	    void start_envelope(opn_operator &oper);

	    void key_on(opn_channel &chan, opn_operator &oper);
	    void key_off(opn_channel &chan, opn_operator &oper);
	    int32_t get_env_output(opn_operator &oper);

	    void channel_output(opn_channel &channel);
	    void clock_fm();
	    void output_fm();

	    #include "opn_tables.inl"
	    #include "ym3014.inl"
    };
};

#endif // BEENUKED_YM2203