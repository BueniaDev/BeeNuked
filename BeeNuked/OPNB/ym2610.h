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

#ifndef BEENUKED_YM2610
#define BEENUKED_YM2610

#include "utils.h"

namespace beenuked
{
    class YM2610
    {
	public:
	    YM2610();
	    ~YM2610();

	    uint32_t get_sample_rate(uint32_t clock_rate);
	    void setInterface(BeeNukedInterface *inter);
	    void reset();
	    uint8_t readIO(int port);
	    void writeIO(int port, uint8_t data);
	    void writeADPCM_ROM(uint32_t rom_size, uint32_t data_start, uint32_t data_len, vector<uint8_t> rom_data);
	    void writeDelta_ROM(uint32_t rom_size, uint32_t data_start, uint32_t data_len, vector<uint8_t> rom_data);
	    void clockchip();
	    vector<int32_t> get_samples();

	    void writeADPCM_ROM(vector<uint8_t> rom_data)
	    {
		writeADPCM_ROM(rom_data.size(), 0, rom_data.size(), rom_data);
	    }

	    void writeDelta_ROM(vector<uint8_t> rom_data)
	    {
		writeDelta_ROM(rom_data.size(), 0, rom_data.size(), rom_data);
	    }

	private:
	    template<typename T>
	    bool testbit(T reg, int bit)
	    {
		return ((reg >> bit) & 1) ? true : false;
	    }

	    template<typename T>
	    T setbit(T reg, int bit)
	    {
		return (reg | (1 << bit));
	    }

	    template<typename T>
	    T resetbit(T reg, int bit)
	    {
		return (reg & ~(1 << bit));
	    }

	    BeeNukedInterface *inter = NULL;

	    uint32_t ssg_sample_index = 0;

	    int fm_samples_per_output = 1;

	    array<int32_t, 3> last_samples = {0, 0, 0};

	    uint8_t chip_address = 0;
	    bool is_addr_a1 = false;
	    vector<uint8_t> adpcm_rom;
	    vector<uint8_t> delta_t_rom;

	    uint8_t fetch_adpcm_rom(uint32_t address);
	    uint8_t fetch_delta_t_rom(uint32_t address);

	    struct opnb_adpcm
	    {
		bool is_keyon = false;
		bool is_pan_left = false;
		bool is_pan_right = false;
		int ch_level = 0;
		bool is_high_nibble = false;
		uint8_t current_byte = 0;
		uint32_t start_addr = 0;
		uint32_t end_addr = 0;
		uint32_t current_addr = 0;
		int32_t adpcm_accum = 0;
		int32_t adpcm_step = 0;
		int32_t adpcm_output = 0;
	    };

	    struct opnb_delta_t
	    {
		bool is_repeat = false;
		bool is_keyon = false;
		bool is_pan_left = false;
		bool is_pan_right = false;
		uint32_t start_addr = 0;
		uint32_t end_addr = 0;
		uint32_t current_addr = 0;
		bool is_high_nibble = false;
		uint8_t current_byte = 0;
		uint32_t delta_t_pos = 0;
		int32_t delta_t_accum = 0;
		int32_t prev_accum = 0;
		int32_t current_step = 0;
		uint32_t delta_n = 0;
		uint32_t ch_volume = 0;
		int32_t delta_t_output = 0;
	    };

	    int adpcm_tl_val = 0;

	    array<opnb_adpcm, 6> adpcm_channels;
	    opnb_delta_t delta_t_channel;

	    void adpcm_key_on(opnb_adpcm &channel);
	    void adpcm_key_off(opnb_adpcm &channel);

	    uint32_t env_timer = 0;
	    uint32_t env_clock = 0;

	    void clock_fm_and_adpcm();
	    void clock_timers();
	    void clock_adpcm();
	    void clock_adpcm_channel(opnb_adpcm &channel);
	    void clock_delta_t();

	    void output_fm_and_adpcm();
	    void adpcm_output();
	    void adpcm_channel_output(opnb_adpcm &channel);
	    void delta_t_output();

	    array<int32_t, 2> generate_adpcm_sample();

	    enum opnb_oper_state : int
	    {
		Attack = 0,
		Decay = 1,
		Sustain = 2,
		Release = 3,
		Off = 4,
	    };

	    struct opnb_operator
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
		opnb_oper_state env_state;

		array<int32_t, 2> outputs = {0, 0};
	    };

	    struct opnb_channel
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
		array<opnb_operator, 4> opers;
	    };

	    array<opnb_channel, 6> channels;

	    void write_port0(uint8_t reg, uint8_t data);
	    void write_port1(uint8_t reg, uint8_t data);

	    void write_adpcm(uint8_t reg, uint8_t data);
	    void write_delta_t(uint8_t reg, uint8_t data);

	    void write_mode(uint8_t reg, uint8_t data);
	    void write_fmreg(bool is_port1, uint8_t reg, uint8_t data);

	    uint16_t timera_freq = 0;
	    uint16_t timerb_freq = 0;

	    uint16_t timera_counter = 0;
	    uint16_t timerb_counter = 0;

	    bool is_timera_running = false;
	    bool is_timerb_running = false;

	    bool is_timera_enabled = false;
	    bool is_timerb_enabled = false;

	    uint8_t opnb_status = 0;
	    bool opnb_irq = false;

	    void set_status_bit(int bit);
	    void reset_status_bit(int bit);

	    int32_t ssg_sample = 0;
	    array<int32_t, 3> last_ssg_samples = {0, 0, 0};

	    void add_last(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale = 1);
	    void clock_and_add(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale = 1);
	    void write_to_output(int32_t sum0, int32_t sum1, int32_t sum2, int divisor = 1);

	    void resample();

	    #include "opnb_tables.inl"
    };
};

#endif // BEENUKED_YM2610