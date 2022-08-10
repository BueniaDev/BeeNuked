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

#ifndef BEENUKED_YM2612
#define BEENUKED_YM2612

#include "utils.h"

namespace beenuked
{
    enum OPN2Type : int
    {
	YM2612_Chip = 0,
	YM3438_Chip = 1,
    };

    class YM2612
    {
	public:
	    YM2612();
	    ~YM2612();

	    uint32_t get_sample_rate(uint32_t clock_rate);
	    void init(OPN2Type chiptype = YM2612_Chip);
	    void writeIO(int port, uint8_t data);
	    void clockchip();
	    vector<int32_t> get_samples();

	private:
	    template<typename T>
	    bool testbit(T reg, int bit)
	    {
		return ((reg >> bit) & 1) ? true : false;
	    }

	    void set_chip_type(OPN2Type type);
	    void reset();

	    OPN2Type chip_type;

	    bool is_ym2612()
	    {
		return (chip_type == YM2612_Chip);
	    }

	    uint8_t chip_address = 0;
	    bool is_addr_a1 = false;

	    void write_port0(uint8_t reg, uint8_t data);
	    void write_port1(uint8_t reg, uint8_t data);

	    bool is_dac_enabled = false;
	    uint16_t dac_data = 0;

	    int32_t dac_discontinuity(int32_t val);

	    void write_mode(uint8_t reg, uint8_t data);
	    void write_fmreg(bool is_port1, uint8_t reg, uint8_t data);

	    void init_tables();

	    array<uint32_t, 256> sine_table;
	    array<uint32_t, 256> exp_table;

	    int32_t calc_output(uint32_t phase, int32_t mod, uint32_t env);

	    uint32_t env_timer = 0;
	    uint32_t env_clock = 0;
	    uint32_t lfo_counter = 0;
	    int lfo_am = 0;
	    int32_t lfo_raw_pm = 0;

	    uint16_t timera_freq = 0;
	    uint8_t timerb_freq = 0;
	    int timerb_subcounter = 0;

	    uint16_t timera_counter = 0;
	    uint8_t timerb_counter = 0;

	    bool is_timera_running = false;
	    bool is_timerb_running = false;

	    bool is_timera_loaded = false;
	    bool is_timerb_loaded = false;

	    bool is_timera_enabled = false;
	    bool is_timerb_enabled = false;

	    void set_status_bit(int bit);
	    void reset_status_bit(int bit);

	    bool is_lfo_enabled = false;
	    int lfo_rate = 0;

	    uint8_t opn2_status = 0;

	    enum opn2_oper_state : int
	    {
		Attack = 0,
		Decay = 1,
		Sustain = 2,
		Release = 3,
		Off = 4,
	    };

	    struct opn2_operator
	    {
		int keycode = 0;
		int freq_num = 0;
		int block = 0;
		int multiply = 0;
		int detune = 0;

		int key_scaling = 0;
		int ksr_val = 0;
		int attack_rate = 0;
		int decay_rate = 0;
		int sustain_rate = 0;
		int sustain_level = 0;
		int release_rate = 0;

		int total_level = 0;

		bool ssg_enable = false;
		bool ssg_att = false;
		bool ssg_alt = false;
		bool ssg_hold = false;
		bool ssg_inv = false;

		uint32_t phase_counter = 0;
		uint32_t phase_freq = 0;
		uint32_t phase_output = 0;
		bool is_keyon = false;

		bool lfo_enable = false;

		int lfo_pm_sens = 0;
		int lfo_am_sens = 0;

		int32_t env_output = 0;
		int env_rate = 0;
		opn2_oper_state env_state;

		array<int32_t, 2> outputs = {0, 0};
	    };

	    struct opn2_channel
	    {
		int number = 0;
		int freq_num = 0;
		int block = 0;
		int ch_mode = 0;

		array<int, 3> oper_fnums = {0, 0, 0};
		array<int, 3> oper_block = {0, 0, 0};
		bool is_csm_keyon = false;
		bool is_left_output = false;
		bool is_right_output = false;
		int lfo_pm_sens = 0;
		int lfo_am_sens = 0;

		int feedback = 0;
		int algorithm = 0;
		int32_t output = 0;
		array<opn2_operator, 4> opers;
	    };

	    array<opn2_channel, 6> channels;

	    void update_frequency(opn2_channel &channel);
	    void update_lfo(opn2_channel &channel);
	    void update_frequency(opn2_operator &oper);
	    void update_phase(opn2_operator &oper);
	    void update_ksr(opn2_operator &oper);
	    void calc_oper_rate(opn2_operator &oper);
	    void start_envelope(opn2_operator &oper);

	    void key_on(opn2_channel &chan, opn2_operator &oper);
	    void key_off(opn2_channel &chan, opn2_operator &oper);
	    int32_t get_env_output(opn2_operator &oper);
	    int32_t get_lfo_pm(opn2_operator &oper);
	    uint32_t get_lfo_am(opn2_channel &channel);

	    void update_ch3_mode(int val);

	    void clock_envelope_gen();
	    void clock_timers();
	    void clock_lfo();
	    void clock_phase(opn2_channel &channel);
	    void clock_ssg_eg(opn2_channel &channel);
	    void clock_envelope(opn2_channel &channel);

	    void channel_output(opn2_channel &channel);

	    #include "opn2_tables.inl"
    };
};

#endif // BEENUKED_YM2612