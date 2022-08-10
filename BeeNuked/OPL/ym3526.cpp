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

// BeeNuked-YM3526
// Chip Name: YM3526 (OPL)/ Y8950 (MSX-Audio)/ YM3812 (OPL2)
// Chip Used In: 
// MSX-Audio expansion (Y8950)
// C64 Sound Expander, Terra Cresta, and Bubble Bobble (YM3526)
// AdLib, Sound Blaster, and 8-bit Pro AudioSpectrum PC sound cards (YM3812)
//
// Interesting Trivia:
//
// Numerous Yamaha synthesizers used either the OPL2 or the cut-down YM2413 (aka. OPLL).
// In addition, the OPL2 was most famously used in arguably two of the most iconic
// PC sound cards of all time, the AdLib and the original Sound Blaster, and the OPL even
// found use in several arcade games, including both Terra Cresta and Bubble Bobble.
//
// BueniaDev's Notes:
//
// Although this core is getting closer and closer to completion, the following features are still
// completely unimplemented:
// CSM/IRQ functionality (related to timers)
// ADPCM status reads (Y8950 only)
//
// However, work is continuing to be done on this core, so don't lose hope here!

#include <ym3526.h>
using namespace std;

namespace beenuked
{
    YM3526::YM3526()
    {

    }

    YM3526::~YM3526()
    {

    }

    void YM3526::init_tables()
    {
	for (uint32_t index = 0; index < 256; index++)
	{
	    double phase_normalized = (static_cast<double>((index << 1) + 1) / 512.f);
	    double sine_result_normalized = sin(phase_normalized * (M_PI / 2));

	    double sine_result_as_attenuation = -log(sine_result_normalized) / log(2.0);

	    uint32_t sine_result = static_cast<uint32_t>((sine_result_as_attenuation * 256.f) + 0.5);
	    sine_table[index] = sine_result;
	}

	for (uint32_t index = 0; index < 256; index++)
	{
	    double entry_normalized = (static_cast<double>((index + 1)) / 256.f);
	    double res_normalized = pow(2, -entry_normalized);

	    uint32_t exp_result = static_cast<uint32_t>((res_normalized * 2048.f) + 0.5);
	    exp_table[index] = exp_result;
	}
    }

    uint32_t YM3526::fetch_sine_result(uint32_t phase, int wave_sel, bool &is_negate)
    {
	bool sign_bit = testbit(phase, 9);
	bool mirror_bit = testbit(phase, 8);
	uint8_t quarter_phase = (phase & 0xFF);

	if (mirror_bit)
	{
	    quarter_phase = ~quarter_phase;
	}

	uint32_t sine_result = sine_table[quarter_phase];

	switch ((wave_sel & 0x3))
	{
	    case 0:
	    {
		is_negate = sign_bit;
	    }
	    break;
	    case 1:
	    {
		if (sign_bit)
		{
		    sine_result = 0xFFF;
		}

		is_negate = false;
	    }
	    break;
	    case 2:
	    {
		is_negate = false;
	    }
	    break;
	    case 3:
	    {
		if (mirror_bit)
		{
		    sine_result = 0xFFF;
		}

		is_negate = false;
	    }
	    break;
	}

	return sine_result;
    }

    int32_t YM3526::calc_output(int32_t phase, int32_t mod, uint32_t env, int wave_sel)
    {
	uint32_t atten = min<uint32_t>(511, env);

	uint32_t combined_phase = ((phase + mod) & 0x3FF);

	bool is_negate = false;

	uint32_t sine_result = fetch_sine_result(combined_phase, wave_sel, is_negate);

	uint32_t attenuation = (atten << 3);
	uint32_t combined_atten = ((sine_result + attenuation) & 0x1FFF);

	int shift_count = ((combined_atten >> 8) & 0x1F);
	uint8_t exp_index = (combined_atten & 0xFF);
	uint32_t exp_result = exp_table[exp_index];

	uint32_t output_shifted = ((exp_result << 2) >> shift_count);

	int32_t exp_output = int32_t(output_shifted);

	if (is_negate)
	{
	    exp_output = -exp_output;
	}

	return exp_output;
    }

    void YM3526::write_adpcm_reg(uint8_t reg, uint8_t data)
    {
	switch (reg)
	{
	    case 0x07:
	    {
		// cout << "Writing value of " << hex << int(data) << " to Y8950 sp-off register" << endl;

		delta_t_channel.is_exec = testbit(data, 7);
		delta_t_channel.is_record = testbit(data, 6);
		delta_t_channel.is_external = testbit(data, 5);
		delta_t_channel.is_repeat = testbit(data, 4);

		if (testbit(data, 0))
		{
		    delta_t_channel.is_int_keyon = false;
		}
		else
		{
		    delta_t_channel.is_int_keyon = false;
		    delta_t_channel.current_addr = 0xFFFFFFFF;

		    if (delta_t_channel.is_exec)
		    {
			delta_t_channel.is_int_keyon = true;
			delta_t_channel.current_pos = 0;
			delta_t_channel.adpcm_buffer = 0;
			delta_t_channel.num_nibbles = 0;
			delta_t_channel.reg_accum = 0;
			delta_t_channel.is_keyon = true;
			delta_t_channel.adpcm_step = 127;
		    }
		}
	    }
	    break;
	    case 0x08:
	    {
		// cout << "Writing value of " << hex << int(data) << " to Y8950 keyboard split/sample/da-ad register" << endl;
		delta_t_channel.is_dram_8bit = testbit(data, 1);
		delta_t_channel.is_rom_ram = testbit(data, 0);
	    }
	    break;
	    case 0x09:
	    {
		delta_t_channel.start_address = ((delta_t_channel.start_address & 0xFF00) | data);
	    }
	    break;
	    case 0x0A:
	    {
		delta_t_channel.start_address = ((delta_t_channel.start_address & 0xFF) | (data << 8));
	    }
	    break;
	    case 0x0B:
	    {
		delta_t_channel.stop_address = ((delta_t_channel.stop_address & 0xFF00) | data);
	    }
	    break;
	    case 0x0C:
	    {
		delta_t_channel.stop_address = ((delta_t_channel.stop_address & 0xFF) | (data << 8));
	    }
	    break;
	    case 0x0D:
	    {
		cout << "Writing value of " << hex << int(data) << " to Y8950 prescale LSB" << endl;
	    }
	    break;
	    case 0x0E:
	    {
		cout << "Writing value of " << hex << int(data) << " to Y8950 prescale MSB" << endl;
	    }
	    break;
	    case 0x0F:
	    {
		cout << "Writing value of " << hex << int(data) << " to Y8950 APDCM data" << endl;
	    }
	    break;
	    case 0x10:
	    {
		delta_t_channel.delta_n = ((delta_t_channel.delta_n & 0xFF00) | data);
	    }
	    break;
	    case 0x11:
	    {
		delta_t_channel.delta_n = ((delta_t_channel.delta_n & 0xFF) | (data << 8));
	    }
	    break;
	    case 0x12:
	    {
		delta_t_channel.ch_volume = data;
	    }
	    break;
	}
    }

    bool YM3526::request_adpcm_data()
    {
	if (delta_t_channel.current_addr == 0xFFFFFFFF)
	{
	    latch_addresses();
	}

	if (!delta_t_channel.is_external)
	{
	    return false;
	}

	uint8_t data = readROM(delta_t_channel.current_addr);
	append_buffer_byte(data);
	// write_adpcm_reg(0xF, data);
	return advance_delta_t_address();
    }

    bool YM3526::advance_delta_t_address()
    {
	int shift = get_delta_t_shift();
	int mask = ((1 << shift) - 1);

	assert(delta_t_channel.current_addr != 0xFFFFFFFF);

	if ((delta_t_channel.current_addr & mask) == mask)
	{
	    uint32_t unit_addr = (delta_t_channel.current_addr >> shift);

	    if (unit_addr == delta_t_channel.stop_address)
	    {
		return true;
	    }
	    else if (unit_addr == delta_t_channel.limit_address)
	    {
		delta_t_channel.current_addr = 0;
		return false;
	    }
	}

	delta_t_channel.current_addr = ((delta_t_channel.current_addr + 1) & 0xFFFFFF);
	return false;
    }

    uint8_t YM3526::readROM(uint32_t address)
    {
	if (inter == NULL)
	{
	    return 0;
	}

	return inter->readMemory(BeeNukedAccessType::DeltaT, address);
    }

    void YM3526::clock_delta_t()
    {
	if (!delta_t_channel.is_exec || delta_t_channel.is_record || !delta_t_channel.is_int_keyon)
	{
	    delta_t_channel.prev_accum = delta_t_channel.reg_accum;
	    delta_t_channel.current_pos = 0;
	    delta_t_channel.is_int_keyon = false;
	    return;
	}

	uint32_t current_pos = (delta_t_channel.current_pos + delta_t_channel.delta_n);
	delta_t_channel.current_pos = uint16_t(current_pos);

	if (current_pos < 0x10000)
	{
	    return;
	}

	if (delta_t_channel.num_nibbles != 0)
	{
	    uint8_t data = consume_nibbles(1);

	    int32_t delta = (2 * (data & 0x7) + 1) * delta_t_channel.adpcm_step / 8;

	    if (testbit(data, 3))
	    {
		delta = -delta;
	    }

	    delta_t_channel.prev_accum = delta_t_channel.reg_accum;
	    delta_t_channel.reg_accum = clamp<int32_t>((delta_t_channel.reg_accum + delta), -32768, 32767);

	    uint8_t step_scale = adpcm_step_scale.at(data & 0x7);

	    delta_t_channel.adpcm_step = clamp(((delta_t_channel.adpcm_step * step_scale) / 64), 127, 24576);

	    if (delta_t_channel.num_nibbles == 0)
	    {
		delta_t_channel.adpcm_step = 127;

		// delta_t_channel.is_eos = true;

		if (!delta_t_channel.is_repeat)
		{
		    delta_t_channel.is_int_keyon = false;
		}
	    }
	}

	if (delta_t_channel.is_int_keyon && (delta_t_channel.num_nibbles < 3))
	{
	    if (request_adpcm_data())
	    {
		consume_nibbles(3);

		assert(delta_t_channel.num_nibbles == 1);

		if (delta_t_channel.is_repeat)
		{
		    latch_addresses();
		}
	    }
	}
    }

    void YM3526::delta_t_output()
    {
	int32_t result = ((delta_t_channel.prev_accum * int32_t((delta_t_channel.current_pos ^ 0xFFFF) + 1) + delta_t_channel.reg_accum * int32_t(delta_t_channel.current_pos)) >> 16);
	result = ((result * delta_t_channel.ch_volume) >> 11);

	delta_t_channel.adpcm_output = result;
    }

    void YM3526::write_reg(uint8_t reg, uint8_t data)
    {
	int reg_group = (reg & 0xF0);
	int reg_addr = (reg & 0x1F);

	switch (reg_group)
	{
	    case 0x00:
	    case 0x10:
	    {
		switch (reg_addr)
		{
		    case 0x01:
		    {
			if (is_opl2())
			{
			    is_ws_enable = testbit(data, 5);
			}
		    }
		    break;
		    case 0x02:
		    {
			timer1_freq = data;
		    }
		    break;
		    case 0x03:
		    {
			timer2_freq = data;
		    }
		    break;
		    case 0x04:
		    {
			if (testbit(data, 7))
			{
			    opl_status = 0;
			    write_reg(0x04, 0x00);
			}
			else
			{
			    if (testbit(data, 0) && !is_timer1_running)
			    {
				timer1_counter = (timer1_freq << 2);
			    }

			    if (testbit(data, 1) && !is_timer2_running)
			    {
				timer2_counter = (timer2_freq << 4);
			    }

			    is_timer1_running = testbit(data, 0);
			    is_timer2_running = testbit(data, 1);
			    is_timer2_disabled = testbit(data, 5);
			    is_timer1_disabled = testbit(data, 6);
			}
		    }
		    break;
		    default: 
		    {
			if (inRangeEx(reg, 0x07, 0x12) || inRangeEx(reg, 0x15, 0x17))
			{
			    if (reg == 0x08)
			    {
				is_csm_mode = testbit(data, 7);
				note_select = testbit(data, 6);

				if (is_y8950())
				{
				    write_adpcm_reg(reg, ((data & 0xF) | 0x80));
				}

				return;
			    }

			    if (is_y8950())
			    {
				write_adpcm_reg(reg, data);
			    }
			}
			else
			{
			    cout << "Unrecognized register write to register of " << hex << int(reg) << endl;
			}
		    }
		    break;
		}
	    }
	    break;
	    case 0x20:
	    case 0x30:
	    {
		int slot_num = slot_array[reg_addr];

		if (slot_num < 0)
		{
		    return;
		}

		int ch_num = (slot_num / 2);
		int oper_num = (slot_num % 2);

		auto &ch_oper = channels[ch_num].opers[oper_num];

		ch_oper.is_am = testbit(data, 7);
		ch_oper.is_vibrato = testbit(data, 6);
		ch_oper.is_sustained = testbit(data, 5);
		ch_oper.is_ksr = testbit(data, 4);
		ch_oper.multiply = (data & 0xF);
		update_phase(ch_oper);
		update_rks(ch_oper);
	    }
	    break;
	    case 0x40:
	    case 0x50:
	    {
		int slot_num = slot_array[reg_addr];

		if (slot_num < 0)
		{
		    return;
		}

		int ch_num = (slot_num / 2);
		int oper_num = (slot_num % 2);

		auto &ch_oper = channels[ch_num].opers[oper_num];

		ch_oper.ksl = ((data >> 6) & 0x3);
		ch_oper.total_level = (data & 0x3F);
		update_total_level(ch_oper);
	    }
	    break;
	    case 0x60:
	    case 0x70:
	    {
		int slot_num = slot_array[reg_addr];

		if (slot_num < 0)
		{
		    return;
		}

		int ch_num = (slot_num / 2);
		int oper_num = (slot_num % 2);

		auto &ch_oper = channels[ch_num].opers[oper_num];

		ch_oper.attack_rate = (data >> 4);
		ch_oper.decay_rate = (data & 0xF);
		update_eg(ch_oper);
	    }
	    break;
	    case 0x80:
	    case 0x90:
	    {
		int slot_num = slot_array[reg_addr];

		if (slot_num < 0)
		{
		    return;
		}

		int ch_num = (slot_num / 2);
		int oper_num = (slot_num % 2);

		auto &ch_oper = channels[ch_num].opers[oper_num];

		ch_oper.sustain_level = ((data >> 4) << 4);
		ch_oper.release_rate = (data & 0xF);
		update_eg(ch_oper);
	    }
	    break;
	    case 0xA0:
	    {
		int ch_num = (reg & 0xF);

		if (ch_num > 8)
		{
		    return;
		}

		auto &channel = channels[ch_num];
		channel.freq_num = ((channel.freq_num & 0x300) | data);
		update_frequency(channel);
	    }
	    break;
	    case 0xB0:
	    {
		if (reg == 0xBD)
		{
		    is_am_mode = testbit(data, 7);
		    is_pm_mode = testbit(data, 6);

		    bool rhythm_enable = testbit(data, 5);

		    if (rhythm_enable && !is_rhythm_enabled)
		    {
			is_rhythm_enabled = true;
			channels[7].opers[0].is_rhythm = true;
			channels[7].opers[0].phase_keep = true;
			channels[7].opers[1].is_rhythm = true;
			channels[8].opers[0].is_rhythm = true;
			channels[8].opers[1].is_rhythm = true;
			channels[8].opers[1].phase_keep = true;
			is_rhythm_enabled = true;
		    }
		    else if (!rhythm_enable && is_rhythm_enabled)
		    {
			channels[7].opers[0].is_rhythm = false;
			channels[7].opers[0].phase_keep = false;
			channels[7].opers[1].is_rhythm = false;
			channels[8].opers[0].is_rhythm = false;
			channels[8].opers[1].is_rhythm = false;
			channels[8].opers[1].phase_keep = false;
			is_rhythm_enabled = false;
		    }

		    if (rhythm_enable)
		    {
			update_key_status(channels[6], testbit(data, 4));
			update_key_status(channels[7].opers[0], testbit(data, 0));
			update_key_status(channels[7].opers[1], testbit(data, 3));
			update_key_status(channels[8].opers[0], testbit(data, 2));
			update_key_status(channels[8].opers[1], testbit(data, 1));
		    }
		    else
		    {
			update_key_status(channels[6], false);
			update_key_status(channels[7], false);
			update_key_status(channels[8], false);
		    }

		    return;
		}

		int ch_num = (reg & 0xF);

		if (ch_num > 8)
		{
		    return;
		}

		auto &channel = channels[ch_num];

		update_key_status(channel, testbit(data, 5));
		channel.freq_num = ((channel.freq_num & 0xFF) | ((data & 0x3) << 8));
		channel.block = ((data >> 2) & 0x7);
		update_frequency(channel);
	    }
	    break;
	    case 0xC0:
	    {
		int ch_num = (reg & 0xF);

		if (ch_num > 8)
		{
		    return;
		}

		auto &channel = channels[ch_num];

		channel.feedback = ((data >> 1) & 0x7);
		channel.is_decay_algorithm = testbit(data, 0);
	    }
	    break;
	    case 0xE0:
	    case 0xF0:
	    {
		if (!is_ws_enable)
		{
		    return;
		}

		int slot_num = slot_array[reg_addr];

		if (slot_num < 0)
		{
		    return;
		}

		int ch_num = (slot_num / 2);
		int oper_num = (slot_num % 2);

		auto &ch_oper = channels[ch_num].opers[oper_num];
		ch_oper.wave_sel = (data & 0x3);
	    }
	    break;
	    default: cout << "Unrecognized register write to group register of " << hex << int(reg) << endl; break;
	}
    }

    void YM3526::update_frequency(opl_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.freq_num = channel.freq_num;
	    oper.block = channel.block;
	    update_frequency(oper);
	}
    }

    void YM3526::update_frequency(opl_operator &oper)
    {
	update_phase(oper);
	update_total_level(oper);
	update_rks(oper);
    }

    void YM3526::update_key_status(opl_channel &channel, bool val)
    {
	for (auto &oper : channel.opers)
	{
	    update_key_status(oper, val);
	}
    }

    void YM3526::update_key_status(opl_operator &oper, bool val)
    {
	if (val == true)
	{
	    key_on(oper);
	}
	else
	{
	    key_off(oper);
	}
    }

    void YM3526::key_on(opl_operator &oper)
    {
	if (!oper.is_keyon)
	{
	    oper.is_keyon = true;

	    int att_rate = oper.attack_rate;
	    int env_rate = 0;
	
	    if (att_rate != 0)
	    {
		int calc_rate = ((att_rate * 4) + oper.rks_val);
		env_rate = min(63, calc_rate);
	    }

	    if (env_rate >= 60)
	    {
		oper.env_state = opl_oper_state::Decay;
		oper.env_output = 0;
	    }
	    else
	    {
		oper.env_state = opl_oper_state::Attack;
	    }

	    oper.phase_counter = 0;

	    calc_oper_rate(oper);
	}
    }

    void YM3526::key_off(opl_operator &oper)
    {
	if (oper.is_keyon)
	{
	    oper.is_keyon = false;

	    if (oper.env_state < opl_oper_state::Release)
	    {
		oper.env_state = opl_oper_state::Release;
		calc_oper_rate(oper);
	    }
	}
    }

    void YM3526::update_phase(opl_operator &oper)
    {
	int8_t lfo_pm = 0;

	if (oper.is_vibrato)
	{
	    lfo_pm = pm_table[((oper.freq_num >> 7) & 7)][(pm_clock >> 19)];

	    if (!is_pm_mode)
	    {
		lfo_pm >>= 1;
	    }
	}

	uint32_t phase_result = (oper.freq_num + lfo_pm);
	int multiply = multiply_table[oper.multiply];
	oper.phase_freq = (((phase_result * multiply) << oper.block) >> 1);
    }

    // Total level calculation (KSL calculation derived from Nuked-OPL3)
    void YM3526::update_total_level(opl_operator &oper)
    {
	int temp_ksl = ((16 * oper.block - ksl_table[(oper.freq_num >> 6)]) << 1);
	array<int, 4> kslx_table = {3, 1, 2, 0};
	int ksl_val = (oper.ksl == 0) ? 0 : (max(0, temp_ksl) >> kslx_table[oper.ksl]);
	oper.tll_val = ((oper.total_level << 2) + ksl_val);
    }

    // Key scaling rate calculation (verified on a real YM3812)
    void YM3526::update_rks(opl_operator &oper)
    {
	bool freq_num = false;

	// If NTS = 0, then bit 9 of FNUM is used.
	// Otherwise, if NTS = 1, then bit 8 of FNUM is used.
	// (The above info was verified on a real YM3812)
	if (note_select)
	{
	    freq_num = testbit(oper.freq_num, 8);
	}
	else
	{
	    freq_num = testbit(oper.freq_num, 9);
	}

	int block_fnum = ((oper.block << 1) | freq_num);

	oper.rks_val = (oper.is_ksr) ? block_fnum : (block_fnum >> 2);
	calc_oper_rate(oper);
    }

    void YM3526::update_eg(opl_operator &oper)
    {
	calc_oper_rate(oper);
    }

    void YM3526::calc_oper_rate(opl_operator &oper)
    {
	int p_rate = 0;

	switch (oper.env_state)
	{
	    case opl_oper_state::Attack: p_rate = oper.attack_rate; break;
	    case opl_oper_state::Decay: p_rate = oper.decay_rate; break;
	    case opl_oper_state::Sustain:
	    {
		if (oper.is_sustained)
		{
		    p_rate = 0;
		}
		else
		{
		    p_rate = oper.release_rate;
		}
	    }
	    break;
	    case opl_oper_state::Release: p_rate = oper.release_rate; break;
	    default: p_rate = 0; break;
	}

	if (p_rate == 0)
	{
	    oper.env_rate = 0;
	}
	else
	{
	    int temp_rate = ((p_rate * 4) + oper.rks_val);
	    oper.env_rate = min(63, temp_rate);
	}
    }

    void YM3526::clock_timers()
    {
	if (is_timer1_running)
	{
	    if (timer1_counter != 1023)
	    {
		timer1_counter += 1;
	    }
	    else if (!is_timer1_disabled)
	    {
		opl_status |= 0xC0;
		timer1_counter = (timer1_freq << 2);
	    }
	}

	if (is_timer2_running)
	{
	    if (timer2_counter != 4095)
	    {
		timer2_counter += 1;
	    }
	    else if (!is_timer2_disabled)
	    {
		opl_status |= 0xA0;
		timer2_counter = (timer2_freq << 4);
	    }
	}
    }

    void YM3526::clock_ampm()
    {
	pm_clock = ((pm_clock + 512) & 0x3FFFFF);
	am_clock += 1;

	for (auto &channel : channels)
	{
	    channel.lfo_am = am_table[(((am_clock >> 6) % 210) >> (is_am_mode ? 0 : 2))];
	}
    }

    void YM3526::clock_phase(opl_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    update_phase(oper);
	    oper.phase_counter = ((oper.phase_counter + oper.phase_freq) & 0xFFFFF);
	    oper.phase_output = (oper.phase_counter >> 10);
	}
    }

    void YM3526::clock_envelope(opl_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    uint32_t counter_shift_val = counter_shift_table[oper.env_rate];

	    if ((env_clock % (1 << counter_shift_val)) == 0)
	    {
		int update_cycle = ((env_clock >> counter_shift_val) & 0x7);
		auto atten_inc = att_inc_table[oper.env_rate][update_cycle];

		switch (oper.env_state)
		{
		    case opl_oper_state::Attack:
		    {
			oper.env_output += ((~oper.env_output * atten_inc) >> 3);

			if (oper.env_output <= 0)
			{
			    oper.env_output = 0;
			    oper.env_state = opl_oper_state::Decay;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opl_oper_state::Decay:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= oper.sustain_level)
			{
			    oper.env_state = opl_oper_state::Sustain;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opl_oper_state::Sustain:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= 511)
			{
			    oper.env_output = 511;
			}
		    }
		    break;
		    case opl_oper_state::Release:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= 511)
			{
			    oper.env_output = 511;
			    oper.env_state = opl_oper_state::Off;
			}
		    }
		    break;
		}
	    }
	}
    }

    void YM3526::clock_short_noise()
    {
	uint32_t phase_hihat = channels[7].opers[0].phase_output;
	uint32_t phase_cymbal = channels[8].opers[1].phase_output;

	bool h_bit2 = testbit(phase_hihat, 2);
	bool h_bit7 = testbit(phase_hihat, 7);
	bool h_bit3 = testbit(phase_hihat, 3);

	bool c_bit3 = testbit(phase_cymbal, 3);
	bool c_bit5 = testbit(phase_cymbal, 5);

	short_noise = ((h_bit2 != h_bit7) || (h_bit3 != c_bit5) || (c_bit3 != c_bit5));
    }

    void YM3526::clock_noise(int cycle)
    {
	for (int i = 0; i < cycle; i++)
	{
	    if (testbit(noise_lfsr, 0))
	    {
		noise_lfsr ^= 0x800200;
	    }

	    noise_lfsr >>= 1;
	}
    }

    void YM3526::channel_output(opl_channel &channel)
    {
	auto &mod_slot = channel.opers[0];
	auto &car_slot = channel.opers[1];

	int32_t feedback = 0;

	if (channel.feedback != 0)
	{
	    feedback = ((mod_slot.outputs[0] + mod_slot.outputs[1]) >> (10 - channel.feedback));
	}

	uint8_t mod_am = mod_slot.is_am ? channel.lfo_am : 0;
	uint32_t mod_env = (mod_slot.tll_val + mod_slot.env_output + mod_am);

	uint8_t car_am = car_slot.is_am ? channel.lfo_am : 0;
	uint32_t car_env = (car_slot.tll_val + car_slot.env_output + car_am);

	mod_slot.outputs[1] = mod_slot.outputs[0];
	mod_slot.outputs[0] = calc_output(mod_slot.phase_output, feedback, mod_env, mod_slot.wave_sel);

	if (channel.is_decay_algorithm)
	{
	    int32_t car_output = calc_output(car_slot.phase_output, 0, car_env, car_slot.wave_sel);
	    int32_t ch_output = (mod_slot.outputs[0] >> 1);
	    ch_output += (car_output >> 1);
	    channel.output = clamp(ch_output, -32768, 32767);
	}
	else
	{
	    int32_t phase_mod = ((mod_slot.outputs[0] >> 1) & 0x3FF);
	    int32_t ch_output = calc_output(car_slot.phase_output, phase_mod, car_env, car_slot.wave_sel);
	    channel.output = (ch_output >> 1);
	}
    }

    void YM3526::bd_output()
    {
	auto &channel = channels[6];
	auto &mod_slot = channel.opers[0];
	auto &car_slot = channel.opers[1];

	int32_t feedback = 0;

	if (channel.feedback != 0)
	{
	    feedback = ((mod_slot.outputs[0] + mod_slot.outputs[1]) >> (10 - channel.feedback));
	}

	uint8_t mod_am = mod_slot.is_am ? channel.lfo_am : 0;
	uint32_t mod_env = (mod_slot.tll_val + mod_slot.env_output + mod_am);

	uint8_t car_am = car_slot.is_am ? channel.lfo_am : 0;
	uint32_t car_env = (car_slot.tll_val + car_slot.env_output + car_am);

	mod_slot.outputs[1] = mod_slot.outputs[0];
	mod_slot.outputs[0] = calc_output(mod_slot.phase_output, feedback, mod_env, mod_slot.wave_sel);

	if (channel.is_decay_algorithm)
	{
	    int32_t car_output = calc_output(car_slot.phase_output, 0, car_env, car_slot.wave_sel);
	    int32_t ch_output = (car_output >> 1);
	    channel.output = clamp(ch_output, -32768, 32767);
	}
	else
	{
	    int32_t phase_mod = ((mod_slot.outputs[0] >> 1) & 0x3FF);
	    int32_t ch_output = calc_output(car_slot.phase_output, phase_mod, car_env, car_slot.wave_sel);
	    channel.output = (ch_output >> 1);
	}
    }

    void YM3526::hihat_output()
    {
	auto &slot = channels[7].opers[0];

	uint32_t phase = 0;

	if (short_noise)
	{
	    phase = testbit(noise_lfsr, 0) ? 0x2D0 : 0x234;
	}
	else
	{
	    phase = testbit(noise_lfsr, 0) ? 0x34 : 0xD0;
	}

	uint32_t rhythm_env = (slot.env_output + slot.tll_val);
	int32_t rhythm_output = calc_output(phase, 0, rhythm_env, slot.wave_sel);
	slot.rhythm_output = (rhythm_output >> 1);
    }

    void YM3526::snare_output()
    {
	auto &phase_slot = channels[7].opers[0];
	auto &slot = channels[7].opers[1];

	uint32_t phase = 0;

	if (testbit(phase_slot.phase_output, 8))
	{
	    phase = testbit(noise_lfsr, 0) ? 0x300 : 0x200;
	}
	else
	{
	    phase = testbit(noise_lfsr, 0) ? 0 : 0x100;
	}

	uint32_t rhythm_env = (slot.env_output + slot.tll_val);
	int32_t rhythm_output = calc_output(phase, 0, rhythm_env, slot.wave_sel);
	slot.rhythm_output = (rhythm_output >> 1);
    }

    void YM3526::tom_output()
    {
	auto &slot = channels[8].opers[0];

	uint32_t rhythm_env = (slot.env_output + slot.tll_val);
	int32_t rhythm_output = calc_output(slot.phase_output, 0, rhythm_env, slot.wave_sel);
	slot.rhythm_output = (rhythm_output >> 1);
    }

    void YM3526::cym_output()
    {
	auto &slot = channels[8].opers[1];

	uint32_t phase = short_noise ? 0x300 : 0x100;

	uint32_t rhythm_env = (slot.env_output + slot.tll_val);
	int32_t rhythm_output = calc_output(phase, 0, rhythm_env, slot.wave_sel);
	slot.rhythm_output = (rhythm_output >> 1);
    }

    uint32_t YM3526::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 72);
    }

    void YM3526::init(OPLType type)
    {
	set_chip_type(type);
	reset();
    }

    void YM3526::set_chip_type(OPLType type)
    {
	if (chip_type != type)
	{
	    chip_type = type;
	}
    }

    void YM3526::reset()
    {
	init_tables();
	env_clock = 0;
	am_clock = 0;
	pm_clock = 0;
	short_noise = false;
	noise_lfsr = 1;

	for (int i = 0; i < 9; i++)
	{
	    channels[i].number = i;
	    channels[i].opers[1].is_carrier = true;
	}

	for (auto &channel : channels)
	{
	    for (auto &oper : channel.opers)
	    {
		oper.env_output = 511;
		oper.env_state = opl_oper_state::Off;
		oper.wave_sel = 0;
	    }
	}

	timer1_counter = 1023;
	timer2_counter = 255;
    }

    void YM3526::setInterface(BeeNukedInterface *cb)
    {
	inter = cb;
    }

    uint8_t YM3526::readIO(int port)
    {
	uint8_t data = 0xFF;

	if ((port & 1) == 0)
	{
	    data = opl_status;
	}


	return data;
    }

    void YM3526::writeIO(int port, uint8_t data)
    {
	if ((port & 1) == 0)
	{
	    chip_address = data;
	}
	else
	{
	    write_reg(chip_address, data);
	}
    }

    void YM3526::clockchip()
    {
	env_clock += 1;
	clock_timers();
	clock_ampm();
	clock_short_noise();

	for (auto &channel : channels)
	{
	    clock_phase(channel);
	    clock_envelope(channel);
	}

	for (int i = 0; i < 6; i++)
	{
	    channel_output(channels[i]);
	}

	if (!is_rhythm_enabled)
	{
	    channel_output(channels[6]);
	}
	else
	{
	    bd_output();
	    channels[6].output = (channels[6].output * 2);
	}

	clock_noise(14);

	if (!is_rhythm_enabled)
	{
	    channel_output(channels[7]);
	}
	else
	{
	    hihat_output();
	    snare_output();

	    int32_t ch7_output = 0;

	    for (auto &oper : channels[7].opers)
	    {
		ch7_output += oper.rhythm_output;
	    }

	    ch7_output = clamp(ch7_output, -32768, 32767);

	    channels[7].output = (ch7_output * 2);
	}

	clock_noise(2);

	if (!is_rhythm_enabled)
	{
	    channel_output(channels[8]);
	}
	else
	{
	    tom_output();
	    cym_output();

	    int32_t ch8_output = 0;

	    for (auto &oper : channels[8].opers)
	    {
		ch8_output += oper.rhythm_output;
	    }

	    ch8_output = clamp(ch8_output, -32768, 32767);

	    channels[8].output = (ch8_output * 2);
	}

	clock_noise(2);

	if (is_y8950())
	{
	    clock_delta_t();
	    delta_t_output();
	}
	else
	{
	    delta_t_channel.adpcm_output = 0;
	}
    }

    vector<int32_t> YM3526::get_samples()
    {
	int32_t output = 0;

	for (int i = 0; i < 9; i++)
	{
	    output += channels[i].output;
	}

	output += delta_t_channel.adpcm_output;

	int32_t sample = dac_ym3014(output);

	vector<int32_t> final_samples;
	final_samples.push_back(sample);
	return final_samples;
    }
};