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

// BeeNuked-YM2612
// Chip Name: YM2612 (OPN2)/YM3438 (OPN2C)
// Chip Used In: 
// YM2612: Fujitsu FM Towns computers and (most famously) Sega's legendary Mega Drive console
// YM3438: Model 2 Mega Drive consoles, and a few of Sega's later Super Scaler consoles
// (i.e. System 18 and System 32), as well as Sega Model 1 (as a de-facto sound timer, with no audio output)
//
// Interesting Trivia:
//
// The YM2612's 9-bit multiplexed DAC causes the crossover distortion forming the basis of the chip's famous
// "ladder effect".
//
//
// BueniaDev's Notes:
//
// This core is slowly approaching completion at this point, with a lot of the YM2612's core features, 
// including SSG-EG, fully implemented.
// However, the following features are completely unimplemented in this core:
//
// CSM mode (related to timers)
// Status flag reads

#include "ym2612.h"
using namespace beenuked;

namespace beenuked
{
    YM2612::YM2612()
    {

    }

    YM2612::~YM2612()
    {

    }

    void YM2612::init_tables()
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

    int32_t YM2612::calc_output(uint32_t phase, int32_t mod, uint32_t env)
    {
	uint32_t atten = min<uint32_t>(0x3FF, env);

	uint32_t combined_phase = ((phase + mod) & 0x3FF); 

	bool sign_bit = testbit(combined_phase, 9);
	bool mirror_bit = testbit(combined_phase, 8);
	uint8_t quarter_phase = (combined_phase & 0xFF);

	if (mirror_bit)
	{
	    quarter_phase = ~quarter_phase;
	}

	uint32_t sine_result = sine_table[quarter_phase];

	uint32_t attenuation = (atten << 2);
	uint32_t combined_atten = ((sine_result + attenuation) & 0x1FFF);

	int shift_count = ((combined_atten >> 8) & 0x1F);
	uint8_t exp_index = (combined_atten & 0xFF);
	uint32_t exp_result = exp_table[exp_index];

	uint32_t output_shifted = ((exp_result << 2) >> shift_count);

	int32_t exp_output = int32_t(output_shifted);

	if (sign_bit)
	{
	    exp_output = -exp_output;
	}

	return exp_output;
    }

    int32_t YM2612::dac_discontinuity(int32_t val)
    {
	// if (chip_type == OPN2Type::YM2612_Chip)
	if (true)
	{
	    return (val < 0) ? (val - 2) : (val + 3);
	}
	else
	{
	    return val;
	}
    }

    void YM2612::write_port0(uint8_t reg, uint8_t data)
    {
	int reg_group = (reg & 0xF0);

	switch (reg_group)
	{
	    case 0x20:
	    {
		switch (reg)
		{
		    case 0x2A:
		    {
			dac_data = (dac_data & ~0x1FE) | ((data ^ 0x80) << 1);
		    }
		    break;
		    case 0x2B:
		    {
			is_dac_enabled = testbit(data, 7);
		    }
		    break;
		    case 0x2C:
		    {
			dac_data = (dac_data & ~1) | testbit(data, 3);
		    }
		    break;
		    default: write_mode(reg, data); break;
		}
	    }
	    break;
	    default: write_fmreg(false, reg, data); break;
	}
    }

    void YM2612::write_port1(uint8_t reg, uint8_t data)
    {
	write_fmreg(true, reg, data);
    }

    void YM2612::write_mode(uint8_t reg, uint8_t data)
    {
	switch (reg)
	{
	    case 0x21: break; // Test register
	    case 0x22:
	    {
		is_lfo_enabled = testbit(data, 3);
		lfo_rate = (data & 0x7);
	    }
	    break;
	    case 0x24:
	    {
		timera_freq = ((timera_freq & 0x3) | (data << 2)); 
	    }
	    break;
	    case 0x25:
	    {
		timera_freq = ((timera_freq & 0x3FC) | (data & 0x3));
	    }
	    break;
	    case 0x26:
	    {
		timerb_freq = data;
	    }
	    break;
	    case 0x27:
	    {
		int ch3_mode = ((data >> 6) & 0x3);

		if (ch3_mode == 2)
		{
		    cout << "CSM mode enabled" << endl;
		}

		update_ch3_mode(ch3_mode);

		if (testbit(data, 0) && !is_timera_running)
		{
		    timera_counter = 1023;
		    is_timera_loaded = true;
		}

		if (testbit(data, 1) && !is_timerb_running)
		{
		    timerb_counter = 255;
		    is_timerb_loaded = true;
		}

		is_timera_running = testbit(data, 0);
		is_timerb_running = testbit(data, 1);
		is_timera_enabled = testbit(data, 2);
		is_timerb_enabled = testbit(data, 3);

		if (testbit(data, 4))
		{
		    reset_status_bit(0);
		}

		if (testbit(data, 5))
		{
		    reset_status_bit(1);
		}
	    }
	    break;
	    case 0x28:
	    {
		int ch_addr = (data & 0x7);

		if ((ch_addr & 0x3) == 3)
		{
		    return;
		}

		int ch_num = (ch_addr < 3) ? ch_addr : (ch_addr - 1);

		auto &channel = channels[ch_num];

		for (int i = 0; i < 4; i++)
		{
		    if (testbit(data, (4 + i)))
		    {
			key_on(channel, channel.opers[i]);
		    }
		    else
		    {
			key_off(channel, channel.opers[i]);
		    }
		}
	    }
	    break;
	}
    }

    void YM2612::write_fmreg(bool is_port1, uint8_t reg, uint8_t data)
    {
	int reg_group = (reg & 0xF0);
	int reg_addr = (reg & 0xF);

	int ch_num = (reg_addr & 0x3);

	if (ch_num == 3)
	{
	    return;
	}

	if (is_port1)
	{
	    ch_num += 3;
	}

	int oper_reg = ((reg_addr >> 2) & 0x3);
	array<int, 4> oper_table = {0, 2, 1, 3};
	int oper_num = oper_table[oper_reg];

	auto &channel = channels[ch_num];
	auto &ch_oper = channel.opers[oper_num];

	switch (reg_group)
	{
	    case 0x30:
	    {
		ch_oper.detune = ((data >> 4) & 0x7);
		ch_oper.multiply = (data & 0xF);
		update_frequency(ch_oper);
	    }
	    break;
	    case 0x40:
	    {
		ch_oper.total_level = ((data & 0x7F) << 3);
	    }
	    break;
	    case 0x50:
	    {
		ch_oper.key_scaling = (3 - (data >> 6));
		ch_oper.attack_rate = (data & 0x1F);
		update_ksr(ch_oper);
	    }
	    break;
	    case 0x60:
	    {
		ch_oper.lfo_enable = testbit(data, 7);
		ch_oper.decay_rate = (data & 0x1F);
	    }
	    break;
	    case 0x70:
	    {
		ch_oper.sustain_rate = (data & 0x1F);
	    }
	    break;
	    case 0x80:
	    {
		int sl_val = (data >> 4);

		// When all the bits of SL are set, SL is set to 93db
		int sus_level = (sl_val == 15) ? 31 : sl_val;
		ch_oper.sustain_level = (sus_level << 5);

		ch_oper.release_rate = (data & 0xF);
	    }
	    break;
	    case 0x90:
	    {
		ch_oper.ssg_hold = testbit(data, 0);
		ch_oper.ssg_alt = testbit(data, 1);
		ch_oper.ssg_att = testbit(data, 2);
		ch_oper.ssg_enable = testbit(data, 3);
	    }
	    break;
	    case 0xA0:
	    {
		switch (oper_reg)
		{
		    case 0:
		    {
			channel.freq_num = ((channel.freq_num & 0x700) | data);
			update_frequency(channel);
		    }
		    break;
		    case 1:
		    {
			channel.freq_num = ((channel.freq_num & 0xFF) | ((data & 0x7) << 8));
			channel.block = ((data >> 3) & 0x7);
		    }
		    break;
		    case 2:
		    {
			if (!is_port1)
			{
			    auto &channel3 = channels[2];
			    channel3.oper_fnums[ch_num] = ((channel3.oper_fnums[ch_num] & 0x700) | data);
			    if (channel3.ch_mode != 0)
			    {
				auto &oper = channel3.opers[ch_num];
				oper.freq_num = channel3.oper_fnums[ch_num];
				oper.block = channel3.oper_block[ch_num];
				update_frequency(oper);
			    }
			}
		    }
		    break;
		    case 3:
		    {
			if (!is_port1)
			{
			    auto &channel3 = channels[2];
			    channel3.oper_fnums[ch_num] = ((channel3.oper_fnums[ch_num] & 0xFF) | ((data & 0x7) << 8));
			    channel3.oper_block[ch_num] = ((data >> 3) & 0x7);
			}
		    }
		    break;
		}
	    }
	    break;
	    case 0xB0:
	    {
		switch (oper_reg)
		{
		    case 0:
		    {
			channel.feedback = ((data >> 3) & 0x7);
			channel.algorithm = (data & 0x7);
		    }
		    break;
		    case 1:
		    {
			channel.is_left_output = testbit(data, 7);
			channel.is_right_output = testbit(data, 6);
			channel.lfo_am_sens = ((data >> 4) & 0x3);
			channel.lfo_pm_sens = (data & 0x7);
			update_lfo(channel);
		    }
		    break;
		}
	    }
	    break;
	}
    }

    void YM2612::update_frequency(opn2_channel &channel)
    {
	if ((channel.number != 2) || (channel.ch_mode == 0))
	{
	    // Standard frequency update
	    for (auto &oper : channel.opers)
	    {
		oper.freq_num = channel.freq_num;
		oper.block = channel.block;
		update_frequency(oper);
	    }
	}
	else
	{
	    // NOTE: In this implementation, operators 1-3 have their frequencies updated seperately
	    // in channel 3 special mode
	    auto &oper = channel.opers[3];
	    oper.freq_num = channel.freq_num;
	    oper.block = channel.block;
	    update_frequency(oper);
	}
    }

    void YM2612::update_frequency(opn2_operator &oper)
    {
	update_phase(oper);
	update_ksr(oper);
    }

    void YM2612::update_lfo(opn2_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.lfo_am_sens = channel.lfo_am_sens;
	    oper.lfo_pm_sens = channel.lfo_pm_sens;
	    update_frequency(oper);
	}
    }

    int32_t YM2612::get_lfo_pm(opn2_operator &oper)
    {
	int fnum_bits = (oper.freq_num >> 4);

	int32_t abs_pm = lfo_raw_pm;

	if (lfo_raw_pm < 0)
	{
	    abs_pm = -lfo_raw_pm;
	}

	uint32_t shifts = lfo_pm_shifts[oper.lfo_pm_sens][(abs_pm & 0x7)];

	int32_t adjust = (fnum_bits >> (shifts & 0xF)) + (fnum_bits >> (shifts >> 4));

	if (oper.lfo_pm_sens > 5)
	{
	    adjust <<= (oper.lfo_pm_sens - 5);
	}

	adjust >>= 2;

	int32_t lfo_value = adjust;

	if (lfo_raw_pm < 0)
	{
	    lfo_value = -lfo_value;
	}

	return lfo_value;
    }

    uint32_t YM2612::get_lfo_am(opn2_channel &channel)
    {
	uint32_t am_shift = ((1 << (channel.lfo_am_sens ^ 3)) - 1);

	return ((lfo_am << 1) >> am_shift);
    }

    void YM2612::update_phase(opn2_operator &oper)
    {
	oper.keycode = ((oper.block << 2) | fnum_to_keycode[(oper.freq_num >> 7)]);
	uint32_t phase_result = (oper.freq_num << 1);

	if (oper.lfo_pm_sens != 0)
	{
	    phase_result += get_lfo_pm(oper);
	    phase_result &= 0xFFF;
	}

	uint32_t phase_shifted = ((phase_result << oper.block) >> 2);

	int detune_index = (oper.detune & 0x3);
	bool detune_sign = testbit(oper.detune, 2);
	uint32_t detune_increment = detune_table[oper.keycode][detune_index];

	if (detune_sign)
	{
	    phase_shifted -= detune_increment;
	    // The detune increment can cause the phase value
	    // to underflow, so we mask the phase value to 17 bits
	    phase_shifted &= 0x1FFFF;
	}
	else
	{
	    phase_shifted += detune_increment;
	}

	// Add in the multiply
	if (oper.multiply == 0)
	{
	    phase_shifted >>= 1;
	}
	else
	{
	    phase_shifted *= oper.multiply;
	    phase_shifted &= 0xFFFFF;
	}

	oper.phase_freq = phase_shifted;
    }

    void YM2612::update_ksr(opn2_operator &oper)
    {
	oper.ksr_val = (oper.keycode >> oper.key_scaling);
	calc_oper_rate(oper);
    }

    void YM2612::start_envelope(opn2_operator &oper)
    {
	int att_rate = oper.attack_rate;
	int env_rate = 0;

	if (att_rate != 0)
	{
	    int calc_rate = ((att_rate * 2) + oper.ksr_val);
	    env_rate = min(63, calc_rate);
	}

	if (env_rate >= 62)
	{
	    oper.env_output = 0;
	    oper.env_state = (oper.sustain_level == 0) ? opn2_oper_state::Sustain : opn2_oper_state::Decay;
	}
	else
	{
	    if (oper.env_output > 0)
	    {
		oper.env_state = opn2_oper_state::Attack;
	    }
	    else
	    {
		oper.env_state = (oper.sustain_level == 0) ? opn2_oper_state::Sustain : opn2_oper_state::Decay;
	    }
	}

	calc_oper_rate(oper);
    }

    void YM2612::key_on(opn2_channel &chan, opn2_operator &oper)
    {
	if (!oper.is_keyon && (!chan.is_csm_keyon || (chan.number != 2)))
	{
	    oper.is_keyon = true;

	    start_envelope(oper);
	    oper.phase_counter = 0;
	    oper.ssg_inv = false;
	}
    }

    void YM2612::key_off(opn2_channel &chan, opn2_operator &oper)
    {
	// TODO: Implement CSM mode
	if (oper.is_keyon && (!chan.is_csm_keyon || (chan.number != 2)))
	{
	    if (oper.env_state < opn2_oper_state::Release)
	    {
		if (oper.ssg_enable && (oper.ssg_inv ^ oper.ssg_att))
		{
		    oper.env_output = ((0x200 - oper.env_output) & 0x3FF);
		}

		oper.env_state = opn2_oper_state::Release;
		calc_oper_rate(oper);
	    }

	    oper.is_keyon = false;
	}
    }

    void YM2612::calc_oper_rate(opn2_operator &oper)
    {
	int p_rate = 0;

	switch (oper.env_state)
	{
	    case opn2_oper_state::Attack: p_rate = oper.attack_rate; break;
	    case opn2_oper_state::Decay: p_rate = oper.decay_rate; break;
	    case opn2_oper_state::Sustain: p_rate = oper.sustain_rate; break;
	    // Extend 4-bit release rate to 5 bits
	    case opn2_oper_state::Release: p_rate = ((oper.release_rate << 1) | 1); break;
	    default: p_rate = 0; break;
	}

	if (p_rate == 0)
	{
	    oper.env_rate = 0;
	}
	else
	{
	    int env_rate = ((p_rate * 2) + oper.ksr_val);
	    oper.env_rate = min(63, env_rate);
	}
    }

    void YM2612::set_status_bit(int bit)
    {
	opn2_status |= (1 << bit);

	/*
	if (irq_inter != NULL)
	{
	    irq_inter->handle_irq(true);
	}
	*/
    }

    void YM2612::reset_status_bit(int bit)
    {
	opn2_status &= ~(1 << bit);

	/*
	if (irq_inter != NULL)
	{
	    irq_inter->handle_irq(false);
	}
	*/
    }

    void YM2612::clock_timers()
    {
	// TODO: Implement CSM mode
	if (is_timera_running)
	{
	    if (timera_counter != 1023)
	    {
		timera_counter += 1;
	    }
	    else
	    {
		if (is_timera_loaded)
		{
		    is_timera_loaded = false;
		}
		else if (is_timera_enabled)
		{
		    set_status_bit(0);
		}

		timera_counter = timera_freq;
	    }
	}

	timerb_subcounter += 1;

	if (timerb_subcounter == 16)
	{
	    timerb_subcounter = 0;

	    if (is_timerb_running)
	    {
		if (timerb_counter != 255)
		{
		    timerb_counter += 1;
		}
		else
		{
		    if (is_timerb_loaded)
		    {
			is_timerb_loaded = false;
		    }
		    else if (is_timerb_enabled)
		    {
			set_status_bit(1);
		    }

		    timerb_counter = timerb_freq;
		}
	    }
	}
	else if (is_timerb_loaded)
	{
	    is_timerb_loaded = false;
	    timerb_counter = timerb_freq;
	}
    }

    void YM2612::clock_lfo()
    {
	if (!is_lfo_enabled)
	{
	    lfo_counter = 0;
	    lfo_am = 0;
	    lfo_raw_pm = 0;
	    return;
	}

	uint32_t sub_count = uint8_t(lfo_counter++);

	if (sub_count >= lfo_max_count[lfo_rate])
	{
	    lfo_counter += (0x101 - sub_count);
	}

	lfo_am = ((lfo_counter >> 8) & 0x3F);

	if (!testbit(lfo_counter, 14))
	{
	    lfo_am ^= 0x3F;
	}

	int32_t pm_val = ((lfo_counter >> 10) & 0x7);

	if (testbit(lfo_counter, 13))
	{
	    pm_val ^= 7;
	}

	lfo_raw_pm = pm_val;

	if (testbit(lfo_counter, 14))
	{
	    lfo_raw_pm = -lfo_raw_pm;
	}
    }

    void YM2612::clock_phase(opn2_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    update_frequency(oper);
	    oper.phase_counter = ((oper.phase_counter + oper.phase_freq) & 0xFFFFF);
	    oper.phase_output = (oper.phase_counter >> 10);
	}
    }

    void YM2612::clock_ssg_eg(opn2_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    if (oper.ssg_enable && (oper.env_output >= 0x200))
	    {
		if (oper.ssg_alt && (!oper.ssg_hold || !oper.ssg_inv))
		{
		    oper.ssg_inv = !oper.ssg_inv;
		}

		if (!oper.ssg_alt && !oper.ssg_hold)
		{
		    oper.phase_counter = 0;
		}

		if (oper.env_state != opn2_oper_state::Attack)
		{
		    if ((oper.env_state != opn2_oper_state::Release) && !oper.ssg_hold)
		    {
			start_envelope(oper);
		    }
		    else if ((oper.env_state == opn2_oper_state::Release) || !(oper.ssg_inv ^ oper.ssg_att))
		    {
			oper.env_output = 0x3FF;
		    }
		}
	    }
	}
    }

    void YM2612::clock_envelope_gen()
    {
	for (auto &channel : channels)
	{
	    clock_envelope(channel);
	}
    }

    void YM2612::clock_envelope(opn2_channel &channel)
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
		    case opn2_oper_state::Attack:
		    {
			oper.env_output += ((~oper.env_output * atten_inc) >> 4);

			if (oper.env_output <= 0)
			{
			    oper.env_output = 0;
			    oper.env_state = (oper.sustain_level == 0) ? opn2_oper_state::Sustain : opn2_oper_state::Decay;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opn2_oper_state::Decay:
		    {
			if (oper.ssg_enable)
			{
			    if (oper.env_output < 0x200)
			    {
				oper.env_output += (4 * atten_inc);
			    }
			}
			else
			{
			    oper.env_output += atten_inc;
			}

			if (oper.env_output >= oper.sustain_level)
			{
			    oper.env_state = opn2_oper_state::Sustain;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opn2_oper_state::Sustain:
		    {
			if (oper.ssg_enable)
			{
			    if (oper.env_output < 0x200)
			    {
				oper.env_output += (4 * atten_inc);
			    }
			}
			else
			{
			    oper.env_output += atten_inc;
			    if (oper.env_output >= 0x3FF)
			    {
				oper.env_output = 0x3FF;
			    }
			}
		    }
		    break;
		    case opn2_oper_state::Release:
		    {
			if (oper.ssg_enable)
			{
			    if (oper.env_output < 0x200)
			    {
				oper.env_output += (4 * atten_inc);
			    }
			}
			else
			{
			    oper.env_output += atten_inc;

			    if (oper.env_output >= 0x3FF)
			    {
				oper.env_output = 0x3FF;
				oper.env_state = opn2_oper_state::Off;
			    }
			}
		    }
		    break;
		    default: break;
		}
	    }
	}
    }

    void YM2612::update_ch3_mode(int val)
    {
	auto &channel = channels[2];
	channel.ch_mode = val;

	for (auto &oper : channel.opers)
	{
	    oper.freq_num = channel.freq_num;
	    oper.block = channel.block;
	    update_frequency(oper);
	}
    }

    int32_t YM2612::get_env_output(opn2_operator &oper)
    {
	int32_t env_output = oper.env_output;

	if (oper.ssg_enable && (oper.env_state != opn2_oper_state::Release) && (oper.ssg_inv ^ oper.ssg_att))
	{
	    env_output = ((0x200 - env_output) & 0x3FF);
	}

	return env_output;
    }

    void YM2612::channel_output(opn2_channel &channel)
    {
	auto &oper_one = channel.opers[0];
	auto &oper_two = channel.opers[1];
	auto &oper_three = channel.opers[2];
	auto &oper_four = channel.opers[3];

	int32_t feedback = 0;

	if (channel.feedback != 0)
	{
	    feedback = ((oper_one.outputs[0] + oper_one.outputs[1]) >> (10 - channel.feedback));
	}

	uint32_t oper1_am = 0;

	if (oper_one.lfo_enable)
	{
	    oper1_am += get_lfo_am(channel);
	}

	uint32_t oper1_atten = (get_env_output(oper_one) + oper1_am + oper_one.total_level);

	oper_one.outputs[1] = oper_one.outputs[0];
	oper_one.outputs[0] = calc_output(oper_one.phase_output, feedback, oper1_atten);

	uint32_t algorithm_combo = algorithm_combinations[channel.algorithm];

	array<int16_t, 8> opout;
	opout[0] = 0;
	opout[1] = oper_one.outputs[0];

	uint32_t oper2_am = 0;

	if (oper_two.lfo_enable)
	{
	    oper2_am += get_lfo_am(channel);
	}

	uint32_t oper2_atten = (get_env_output(oper_two) + oper2_am + oper_two.total_level);

	int32_t oper2_mod = ((opout[(algorithm_combo & 1)] >> 1) & 0x3FF);
	opout[2] = calc_output(oper_two.phase_output, oper2_mod, oper2_atten);
	opout[5] = (opout[1] + opout[2]);

	uint32_t oper3_am = 0;

	if (oper_three.lfo_enable)
	{
	    oper3_am += get_lfo_am(channel);
	}

	uint32_t oper3_atten = (get_env_output(oper_three) + oper3_am + oper_three.total_level);

	int32_t oper3_mod = ((opout[((algorithm_combo >> 1) & 0x7)] >> 1) & 0x3FF);
	opout[3] = calc_output(oper_three.phase_output, oper3_mod, oper3_atten);
	opout[6] = (opout[1] + opout[3]);
	opout[7] = (opout[2] + opout[3]);

	uint32_t oper4_am = 0;

	if (oper_four.lfo_enable)
	{
	    oper4_am += get_lfo_am(channel);
	}

	uint32_t oper4_atten = (get_env_output(oper_four) + oper4_am + oper_four.total_level);

	int32_t phase_mod = ((opout[((algorithm_combo >> 4) & 0x7)] >> 1) & 0x3FF);
	int32_t ch_output = calc_output(oper_four.phase_output, phase_mod, oper4_atten);

	ch_output >>= 5;

	if (testbit(algorithm_combo, 7))
	{
	    ch_output = clamp((ch_output + (opout[1] >> 5)), -257, 256);
	}

	if (testbit(algorithm_combo, 8))
	{
	    ch_output = clamp((ch_output + (opout[2] >> 5)), -257, 256);
	}

	if (testbit(algorithm_combo, 9))
	{
	    ch_output = clamp((ch_output + (opout[3] >> 5)), -257, 256);
	}

	channel.output = ch_output;
    }

    void YM2612::init()
    {
	reset();
    }

    uint32_t YM2612::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 144);
    }

    void YM2612::reset()
    {
	init_tables();

	for (int ch = 0; ch < 6; ch++)
	{
	    channels[ch].number = ch;
	}

	for (auto &channel : channels)
	{
	    for (auto &oper : channel.opers)
	    {
		oper.env_output = 0x3FF;
		oper.env_state = opn2_oper_state::Off;
	    }
	}

	timera_counter = 1023;
	timerb_counter = 255;
    }

    void YM2612::writeIO(int port, uint8_t data)
    {
	switch ((port & 3))
	{
	    case 0:
	    {
		chip_address = data;
		is_addr_a1 = false;
	    }
	    break;
	    case 1:
	    {
		if (is_addr_a1)
		{
		    // Verified on a real YM2608
		    break;
		}

		write_port0(chip_address, data);
	    }
	    break;
	    case 2:
	    {
		chip_address = data;
		is_addr_a1 = true;
	    }
	    break;
	    case 3:
	    {
		if (!is_addr_a1)
		{
		    // Verified on a real YM2608
		    break;
		}

		write_port1(chip_address, data);
	    }
	    break;
	}
    }

    void YM2612::clockchip()
    {
	// TODO: Clock other components (i.e. LFO, AMS/PMS, etc.)
	clock_timers(); // Clock timers
	clock_lfo(); // Clock LFO

	for (auto &channel : channels)
	{
	    clock_ssg_eg(channel);
	}

	env_timer += 1;

	// Update the envelope generator every 3 cycles
	if (env_timer == 3)
	{
	    env_timer = 0;
	    env_clock += 1;
	    clock_envelope_gen();
	}

	// Clock the phase generator
	for (auto &channel : channels)
	{
	    clock_phase(channel);
	}

	// Output audio
	for (auto &channel : channels)
	{
	    channel_output(channel);
	}
    }

    vector<int32_t> YM2612::get_samples()
    {
	int32_t sample_zero = dac_discontinuity(0);

	array<int32_t, 2> output = {sample_zero, sample_zero};

	int last_fm_channel = is_dac_enabled ? 5 : 6;

	// Mix in FM channels
	for (int ch_num = 0; ch_num < last_fm_channel; ch_num++)
	{
	    auto &channel = channels[ch_num];
	    int32_t fm_sample = dac_discontinuity(channel.output);
	    output[0] += fm_sample;
	    output[1] += fm_sample;
	}

	// Mix in DAC channel (if enabled)
	if (is_dac_enabled)
	{
	    int32_t dac_sample = dac_discontinuity(int16_t(dac_data << 7) >> 7);
	    output[0] += dac_sample;
	    output[1] += dac_sample;
	}

	array<int32_t, 2> mixed_samples = {0, 0};

	for (int i = 0; i < 2; i++)
	{
	    int32_t sample = (output[i] * 128);

	    // if (chip_type == OPN2Type::YM2612_Chip)
	    if (true)
	    {
		mixed_samples[i] = (sample * 64 / (6 * 65));
	    }
	    else
	    {
		mixed_samples[i] = (sample / 6);
	    }
	}

	vector<int32_t> final_samples;

	for (auto &sample : mixed_samples)
	{
	    final_samples.push_back(sample);
	}

	return final_samples;
    }

};