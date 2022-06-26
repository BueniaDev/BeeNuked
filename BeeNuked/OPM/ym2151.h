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

#ifndef BEENUKED_YM2151
#define BEENUKED_YM2151

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
    class YM2151
    {
	public:
	    YM2151();
	    ~YM2151();

	    uint32_t get_sample_rate(uint32_t clock_rate);
	    void init();
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

	    uint8_t chip_address = 0;

	    void init_tables();

	    array<uint32_t, 256> sine_table;
	    array<uint32_t, 256> exp_table;
	    array<array<int16_t, 256>, 4> lfo_table;

	    void write_reg(uint8_t reg, uint8_t data);

	    enum opm_oper_state : int
	    {
		Attack = 0,
		Decay = 1,
		Sustain = 2,
		Release = 3,
		Off = 4,
	    };

	    struct opm_operator
	    {
		int keycode = 0;
		int freq_num = 0;
		int block = 0;
		int multiply = 0;
		int total_level = 0;

		int key_scaling = 0;
		int ksr_val = 0;

		int detune = 0;
		int detune2 = 0;

		bool lfo_enable = false;

		int lfo_pm_sens = 0;
		int lfo_am_sens = 0;

		int attack_rate = 0;
		int decay_rate = 0;
		int sustain_rate = 0;
		int sustain_level = 0;
		int release_rate = 0;

		bool is_keyon = false;
		uint32_t phase_counter = 0;
		uint32_t phase_freq = 0;
		uint32_t phase_output = 0;

		int env_rate = 0;
		int32_t env_output = 0;
		opm_oper_state env_state = opm_oper_state::Off;

		array<int32_t, 2> outputs;
	    };

	    struct opm_channel
	    {
		int number = 0;
		int keyfrac = 0;
		int keycode = 0;
		int block = 0;
		int feedback = 0;
		int algorithm = 0;
		int lfo_pm_sens = 0;
		int lfo_am_sens = 0;
		bool is_pan_left = false;
		bool is_pan_right = false;
		int32_t output = 0;
		array<opm_operator, 4> opers;
	    };

	    uint32_t env_timer = 0;
	    uint32_t env_clock = 0;
	    uint32_t lfo_counter = 0;

	    uint32_t lfo_rate = 0;
	    bool lfo_reset = false;

	    int lfo_pm_sens = 0;
	    int lfo_am_sens = 0;
	    int lfo_waveform = 0;

	    int noise_freq = 0;
	    bool noise_enable = false;

	    uint32_t noise_lfsr = 1;
	    uint8_t noise_counter = 0;
	    uint8_t noise_state = 0;
	    uint8_t noise_lfo = 0;

	    uint8_t lfo_am = 0;
	    int32_t lfo_raw_pm = 0;

	    array<opm_channel, 8> channels;

	    void update_frequency(opm_channel &channel);
	    void update_frequency(opm_channel &channel, opm_operator &oper);
	    void update_phase(opm_operator &oper);
	    void update_ksr(opm_operator &oper);
	    void update_lfo(opm_channel &channel);
	    void calc_oper_rate(opm_operator &oper);
	    void start_envelope(opm_operator &oper);

	    void clock_phase(opm_channel &channel);
	    void clock_envelope(opm_channel &channel);
	    void clock_lfo();
	    void clock_timers();
	    void clock_channel_eg();
	    void channel_output(opm_channel &channel);

	    uint32_t get_freqnum(int &block, int keycode, int keyfrac, int32_t delta);
	    uint32_t get_lfo_am(opm_channel &channel);

	    int32_t calc_output(uint32_t phase, int32_t mod, uint32_t env);

	    int calc_rate(int p_rate, int rks);

	    void key_on(opm_channel &channel, opm_operator &oper);
	    void key_off(opm_channel &channel, opm_operator &oper);

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

	    uint8_t opm_status = 0;

	    #include "opm_tables.inl"
	    #include "ym3014.inl"
    };
};

#endif // BEENUKED_YM2151