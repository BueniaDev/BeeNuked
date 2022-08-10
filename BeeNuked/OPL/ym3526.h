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

#ifndef BEENUKED_YM3526
#define BEENUKED_YM3526

#include "utils.h"

namespace beenuked
{
    enum OPLType : int
    {
	YM3526_Chip = 0,
	Y8950_Chip = 1,
	YM3812_Chip = 2,
    };

    class YM3526
    {
	public:
	    YM3526();
	    ~YM3526();

	    uint32_t get_sample_rate(uint32_t clock_rate);
	    void init(OPLType type = YM3526_Chip);
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

	    template<typename T>
	    bool inRangeEx(T reg, int low, int high)
	    {
		int val = int(reg);
		return ((val >= low) && (val <= high));
	    }

	    BeeNukedInterface *inter = NULL;

	    void set_chip_type(OPLType type);
	    void reset();

	    void init_tables();
	    int32_t calc_output(int32_t phase, int32_t mod, uint32_t atten, int wave_sel);

	    OPLType chip_type;

	    bool is_opl()
	    {
		return (chip_type == YM3526_Chip);
	    }

	    bool is_opl2()
	    {
		return (chip_type == YM3812_Chip);
	    }

	    bool is_y8950()
	    {
		return (chip_type == Y8950_Chip);
	    }

	    struct opl_delta_t
	    {
		bool is_exec = false;
		bool is_record = false;
		bool is_external = false;
		bool is_repeat = false;
		bool is_dram_8bit = false;
		bool is_rom_ram = false;
		uint32_t current_addr = 0;
		uint32_t start_address = 0;
		uint32_t stop_address = 0;
		uint32_t limit_address = 0;
		uint32_t current_pos = 0;
		uint16_t delta_n = 0;
		int32_t reg_accum = 0;
		int32_t prev_accum = 0;
		int adpcm_step = 0;
		uint32_t adpcm_buffer = 0;
		int num_nibbles = 0;
		int ch_volume = 0;
		bool is_keyon = false;
		bool is_int_keyon = false;
		int32_t adpcm_output = 0;
	    };

	    opl_delta_t delta_t_channel;

	    void clock_delta_t();
	    void delta_t_output();

	    bool request_adpcm_data();

	    void latch_addresses()
	    {
		delta_t_channel.current_addr = 0;

		if (delta_t_channel.is_external)
		{
		    delta_t_channel.current_addr = (delta_t_channel.start_address << get_delta_t_shift());
		}
	    }

	    uint8_t readROM(uint32_t address);

	    void append_buffer_byte(uint8_t data)
	    {
		delta_t_channel.adpcm_buffer |= (data << (24 - 4 * delta_t_channel.num_nibbles));
		delta_t_channel.num_nibbles += 2;
	    }

	    uint32_t consume_nibbles(uint8_t count)
	    {
		uint32_t result = (delta_t_channel.adpcm_buffer >> (32 - 4 * count));
		delta_t_channel.adpcm_buffer <<= (4 * count);

		if (delta_t_channel.num_nibbles > count)
		{
		    delta_t_channel.num_nibbles = (delta_t_channel.num_nibbles - count);
		}
		else
		{
		    delta_t_channel.num_nibbles = 0;
		}

		return result;
	    }

	    bool advance_delta_t_address();

	    int get_delta_t_shift()
	    {
		if (delta_t_channel.is_rom_ram)
		{
		    return 5;
		}

		if (delta_t_channel.is_dram_8bit)
		{
		    return 5;
		}

		return 2;
	    }

	    enum opl_oper_state : int
	    {
		Attack = 0,
		Decay = 1,
		Sustain = 2,
		Release = 3,
		Off = 4,
	    };

	    struct opl_operator
	    {
		bool is_carrier = false;
		uint32_t freq_num = 0;
		int block = 0;
		int multiply = 0;
		int ksl = 0;
		int total_level = 0;
		int tll_val = 0;

		bool is_sustained = false;
		bool is_keyon = false;
		bool is_ksr = 0;

		bool is_am = false;
		bool is_vibrato = false;

		int rks_val = 0;

		int attack_rate = 0;
		int decay_rate = 0;
		int sustain_level = 0;
		int release_rate = 0;

		int wave_sel = 0;

		int env_rate = 0;

		bool is_rhythm = false;
		bool phase_keep = false;

		int32_t env_output = 0;
		opl_oper_state env_state;

		uint32_t phase_counter = 0;
		uint32_t phase_freq = 0;
		uint32_t phase_output = 0;
		int32_t rhythm_output = 0;
		array<int32_t, 2> outputs = {0, 0};
	    };

	    struct opl_channel
	    {
		int number = 0;
		uint32_t freq_num = 0;
		int block = 0;
		int32_t output = 0;
		int feedback = 0;
		uint8_t lfo_am = 0;
		bool is_decay_algorithm = false;
		array<opl_operator, 2> opers;
	    };

	    uint32_t env_clock = 0;
	    uint32_t am_clock = 0;
	    uint32_t pm_clock = 0;
	    uint32_t noise_lfsr = 0;

	    uint8_t lfo_am = 0;

	    bool short_noise = false;
	    bool is_csm_mode = false;

	    bool note_select = false;
	    bool is_am_mode = false;
	    bool is_pm_mode = false;

	    array<opl_channel, 9> channels;

	    array<uint32_t, 256> sine_table;
	    array<uint32_t, 256> exp_table;

	    uint32_t fetch_sine_result(uint32_t phase, int wave_sel, bool &is_negate);

	    void write_reg(uint8_t reg, uint8_t data);
	    void write_adpcm_reg(uint8_t reg, uint8_t data);

	    uint8_t chip_address = 0;

	    bool is_ws_enable = false;

	    void update_frequency(opl_channel &channel);
	    void update_frequency(opl_operator &oper);
	    void update_phase(opl_operator &oper);
	    void update_key_status(opl_channel &channel, bool val);
	    void update_key_status(opl_operator &oper, bool val);
	    void update_total_level(opl_operator &oper);
	    void update_rks(opl_operator &oper);
	    void update_eg(opl_operator &oper);
	    void calc_oper_rate(opl_operator &oper);

	    void key_on(opl_operator &oper);
	    void key_off(opl_operator &oper);

	    void clock_ampm();
	    void clock_short_noise();
	    void clock_noise(int cycle);
	    void clock_phase(opl_channel &channel);
	    void clock_envelope(opl_channel &channel);

	    void channel_output(opl_channel &channel);
	    void bd_output();
	    void hihat_output();
	    void snare_output();
	    void tom_output();
	    void cym_output();

	    bool is_rhythm_enabled = false;

	    #include "opl_tables.inl"
	    #include "ym3014.inl"
    };
};

#endif // BEENUKED_YM3526