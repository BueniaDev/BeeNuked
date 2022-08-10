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

#ifndef BEENUKED_YM2413
#define BEENUKED_YM2413

#include "utils.h"

namespace beenuked
{
    enum OPLLType : int
    {
	YM2413_Chip = 0,
	VRC7_Chip = 1,
	YM2423_Chip = 2,
	YMF281_Chip = 3,
    };

    class YM2413
    {
	public:
	    YM2413();
	    ~YM2413();

	    uint32_t get_sample_rate(uint32_t clock_rate);
	    void init(OPLLType type = YM2413_Chip);
	    void writeIO(int port, uint8_t data);
	    void clockchip();
	    uint32_t set_mask(uint32_t mask);
	    uint32_t toggle_mask(uint32_t mask);
	    vector<int32_t> get_samples();

	    uint32_t get_mask_ch(int ch)
	    {
		if ((ch < 0) || (ch >= 9))
		{
		    throw out_of_range("Invalid channel number");
		}

		return getbitmask(ch);
	    }

	    uint32_t get_mask_hh()
	    {
		return getbitmask(9);
	    }

	    uint32_t get_mask_cym()
	    {
		return getbitmask(10);
	    }

	    uint32_t get_mask_tom()
	    {
		return getbitmask(11);
	    }

	    uint32_t get_mask_sd()
	    {
		return getbitmask(12);
	    }

	    uint32_t get_mask_bd()
	    {
		return getbitmask(13);
	    }

	    uint32_t get_mask_rhythm()
	    {
		uint32_t mask = 0;

		for (int bit = 9; bit <= 13; bit++)
		{
		    mask |= getbitmask(bit);
		}

		return mask;
	    }

	private:
	    int getbitmask(int bit)
	    {
		return (1 << bit);
	    }

	    template<typename T>
	    bool testbit(T reg, int bit)
	    {
		return ((reg >> bit) & 1) ? true : false;
	    }

	    uint8_t chip_address = 0;

	    int32_t calc_output(int32_t phase, int32_t mod, uint32_t env, bool is_ws);

	    uint32_t fetch_sine_result(uint32_t phase, bool wave_sel, bool &is_negate);

	    bool is_vrc7()
	    {
		return (chip_type == VRC7_Chip);
	    }

	    uint32_t channel_mask = 0;

	    void write_reg(uint8_t reg, uint8_t data);

	    OPLLType chip_type;

	    void init_tables();

	    void set_chip_type(OPLLType type);
	    void reset();

	    array<uint32_t, 256> sine_table;
	    array<uint32_t, 256> exp_table;

	    typedef array<uint8_t, 8> opll_inst;
	    typedef array<opll_inst, 19> opll_patch;

	    #include "opll_tables.inl"

	    opll_patch inst_patch;

	    enum opll_oper_state : int
	    {
		Damp = 0,
		Attack = 1,
		Decay = 2,
		Sustain = 3,
		Release = 4,
		Off = 5
	    };

	    struct opll_operator
	    {
		bool is_carrier = false;
		uint32_t freq_num = 0;
		int block = 0;

		bool is_am = false;
		bool is_vibrato = false;
		bool is_ws = false;

		int multiply = 0;
		int ksl = 0;
		int total_level = 0;
		int volume = 0;
		int tll_val = 0;
		int rks_val = 0;

		bool is_ksr = false;
		bool is_keyon = false;
		bool is_rhythm = false;
		bool phase_keep = false;

		bool is_sustained = false;
		bool sustain_flag = false;

		int attack_rate = 0;
		int decay_rate = 0;
		int sustain_level = 0;
		int release_rate = 0;

		int32_t env_output = 0;
		int env_rate = 0;
		opll_oper_state env_state = opll_oper_state::Off;

		uint32_t phase_counter = 0;
		uint32_t phase_freq = 0;
		uint32_t phase_output = 0;
		int32_t rhythm_output = 0;
		array<int32_t, 2> outputs = {0, 0};
	    };

	    struct opll_channel
	    {
		int number = 0;
		uint32_t freq_num = 0;
		int block = 0;
		int feedback = 0;
		uint8_t inst_vol_reg = 0;
		int inst_number = 0;
		int32_t output = 0;
		uint8_t lfo_am = 0;
		array<opll_operator, 2> opers;
	    };

	    uint32_t env_clock = 0;
	    uint32_t am_clock = 0;
	    uint32_t pm_clock = 0;
	    uint32_t noise_lfsr = 0;

	    bool short_noise = false;

	    array<opll_channel, 9> channels;

	    void update_frequency(opll_channel &channel);
	    void update_frequency(opll_operator &oper);
	    void update_phase(opll_operator &oper);
	    void update_total_level(opll_operator &oper);
	    void update_rks(opll_operator &oper);
	    void calc_oper_rate(opll_operator &oper);
	    void clock_phase(opll_channel &channel);
	    void clock_envelope(opll_channel &channel);
	    void channel_output(opll_channel &channel);

	    void clock_ampm();
	    void clock_short_noise();
	    void clock_noise(int cycle);

	    void hihat_output();
	    void snare_output();
	    void tom_output();
	    void cym_output();

	    void key_on(opll_operator &oper);
	    void key_off(opll_operator &oper);
	    void update_key_status(opll_channel &channel, bool val);
	    void update_key_status(opll_operator &oper, bool val);
	    void update_sus_flag(opll_channel &channel, bool flag);

	    int calc_rate(int p_rate, int rks);

	    void set_patch(opll_channel &channel, int patch_index);

	    void update_instrument(opll_channel &channel);

	    bool is_rhythm_enabled = false;
    };
};

#endif // BEENUKED_YM2413