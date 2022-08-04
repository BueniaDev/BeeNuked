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

// BeeNuked-YM2203
// Chip Name: YM2203 (OPN)
// Chip Used In: Various arcade machines (1943, Capcom Bowling, Sega Space Harrier, etc.),
// NEC computers (i.e. PC-88, PC-98, etc.)
// Interesting Trivia:
//
// Yamaha's entire OPN line (which includes the YM2610, YM2608, and the legendary YM2612) was descended directly from the YM2203.
// The YM2203 chip also found heavy usage in NEC's PC-88 and PC-98 computer lines.
//
// BueniaDev's Notes:
// 
// Though this core is slowly approaching completion, the following features are still completely unimplemented:
// 
// Timers
// CSM mode
// Chip reads
//
// However, work is being done on those fronts, so don't lose hope here!

#include "ym2203.h"
using namespace beenuked;

namespace beenuked
{
    YM2203::YM2203()
    {

    }

    YM2203::~YM2203()
    {

    }

    void YM2203::init_tables()
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

    int32_t YM2203::calc_output(uint32_t phase, int32_t mod, uint32_t env)
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

    void YM2203::set_prescaler(int value)
    {
	prescaler_val = value;

	switch (value)
	{
	    case prescaler_six: fm_samples_per_output = 6; configure_ssg_resampler(4, 3); break;
	    case prescaler_three: fm_samples_per_output = 3; configure_ssg_resampler(2, 3); break;
	    case prescaler_two: fm_samples_per_output = 2; configure_ssg_resampler(1, 3); break;
	    default: fm_samples_per_output = 6; configure_ssg_resampler(4, 3); break;
	}
    }

    void YM2203::configure_ssg_resampler(uint8_t out_samples, uint8_t src_samples)
    {
	switch ((out_samples * 10) + src_samples)
	{
	    case 43:
	    {
		resample = [&]() {
		    resample_4_3();
		};
	    }
	    break;
	    case 23:
	    {
		resample = [&]() {
		    resample_2_3();
		};
	    }
	    break;
	    case 13:
	    {
		resample = [&]() {
		    resample_1_3();
		};
	    }
	    break;
	    default:
	    {
		resample = [&]() {
		    resample_nop();
		};
	    }
	    break;
	}
    }

    void YM2203::add_last(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale)
    {
	sum0 += (last_ssg_samples[0] * scale);
	sum1 += (last_ssg_samples[1] * scale);
	sum2 += (last_ssg_samples[2] * scale);
    }

    void YM2203::clock_and_add(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale)
    {
	if (ssg_inter != NULL)
	{
	    ssg_inter->clockSSG();
	    last_ssg_samples = ssg_inter->getSamples();
	}

	add_last(sum0, sum1, sum2, scale);
    }

    void YM2203::write_to_output(int32_t sum0, int32_t sum1, int32_t sum2, int divisor)
    {
	ssg_samples[0] = (sum0 / divisor);
	ssg_samples[1] = (sum1 / divisor);
	ssg_samples[2] = (sum2 / divisor);
	ssg_sample_index += 1;
    }

    void YM2203::resample_4_3()
    {
	int32_t sum0 = 0;
	int32_t sum1 = 0;
	int32_t sum2 = 0;
	int step = (ssg_sample_index & 0x3);

	add_last(sum0, sum1, sum2, step);

	if (step != 3)
	{
	    clock_and_add(sum0, sum1, sum2, (3 - step));
	}

	write_to_output(sum0, sum1, sum2, 3);
    }

    void YM2203::resample_2_3()
    {
	int32_t sum0 = 0;
	int32_t sum1 = 0;
	int32_t sum2 = 0;

	if (testbit(ssg_sample_index, 0))
	{
	    clock_and_add(sum0, sum1, sum2, 2);
	    clock_and_add(sum0, sum1, sum2, 1);
	}
	else
	{
	    add_last(sum0, sum1, sum2, 1);
	    clock_and_add(sum0, sum1, sum2, 2);
	}

	write_to_output(sum0, sum1, sum2, 3);
    }

    void YM2203::resample_1_3()
    {
	int32_t sum0 = 0;
	int32_t sum1 = 0;
	int32_t sum2 = 0;

	for (int rep = 0; rep < 3; rep++)
	{
	    clock_and_add(sum0, sum1, sum2);
	}

	write_to_output(sum0, sum1, sum2, 3);
    }

    void YM2203::resample_nop()
    {
	ssg_sample_index += 1;
    }

    void YM2203::update_prescaler()
    {
	switch (chip_address)
	{
	    case 0x2D: set_prescaler(prescaler_six); break;
	    case 0x2E:
	    {
		if (prescaler_val == prescaler_six)
		{
		    set_prescaler(prescaler_three);
		}
	    }
	    break;
	    case 0x2F: set_prescaler(prescaler_two); break;
	    default: break;
	}
    }

    void YM2203::write_reg(uint8_t reg, uint8_t data)
    {
	// TODO: Complete implementation of register writes

	int reg_group = (reg & 0xF0);

	switch (reg_group)
	{
	    // SSG writes
	    case 0x00:
	    {
		if (ssg_inter != NULL)
		{
		    ssg_inter->writeIO(1, data);
		}
	    }
	    break;
	    // 0x10-0x1F are invalid YM2203 registers
	    case 0x10: break;
	    // Mode register writes
	    case 0x20: write_mode(reg, data); break;
	    // FM register writes
	    default: write_fmreg(reg, data); break;
	}
    }

    void YM2203::write_mode(uint8_t reg, uint8_t data)
    {
	switch (reg)
	{
	    case 0x21: break;
	    case 0x22: break;
	    case 0x24:
	    {
		cout << "Writing to upper 8 bits of timer A" << endl;
	    }
	    break;
	    case 0x25:
	    {
		cout << "Writing to lower 2 bits of timer A" << endl;
	    }
	    break;
	    case 0x26:
	    {
		cout << "Writing to timer B" << endl;
	    }
	    break;
	    case 0x27:
	    {
		int ch3_mode = ((data >> 6) & 0x3);
		cout << "Channel 3 mode: " << dec << ch3_mode << endl;
		update_ch3_mode(ch3_mode);

		if (testbit(data, 5))
		{
		    cout << "Resetting timer B..." << endl;
		}

		if (testbit(data, 4))
		{
		    cout << "Resetting timer A..." << endl;
		}

		if (testbit(data, 3))
		{
		    cout << "Timer B flag enabled..." << endl;
		}
		else
		{
		    cout << "Timer B flag disabled..." << endl;
		}

		if (testbit(data, 2))
		{
		    cout << "Timer A flag enabled..." << endl;
		}
		else
		{
		    cout << "Timer A flag disabled..." << endl;
		}

		if (testbit(data, 1))
		{
		    cout << "Starting timer B..." << endl;
		}
		else
		{
		    cout << "Stopping timer B..." << endl;
		}

		if (testbit(data, 0))
		{
		    cout << "Starting timer A..." << endl;
		}
		else
		{
		    cout << "Stopping timer A..." << endl;
		}
	    }
	    break;
	    case 0x28:
	    {
		int ch_num = (data & 0x3);

		if (ch_num == 3)
		{
		    return;
		}

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
	    default: break;
	}
    }

    void YM2203::write_fmreg(uint8_t reg, uint8_t data)
    {
	int reg_group = (reg & 0xF0);
	int reg_addr = (reg & 0xF);
	int ch_num = (reg_addr & 0x3);

	if (ch_num == 3)
	{
	    return;
	}

	int oper_index = ((reg_addr >> 2) & 0x3);
	array<int, 4> oper_table = {0, 2, 1, 3};
	int oper_num = oper_table[oper_index];

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
		switch (oper_index)
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
		    break;
		    case 3:
		    {
			auto &channel3 = channels[2];
			channel3.oper_fnums[ch_num] = ((channel3.oper_fnums[ch_num] & 0xFF) | ((data & 0x7) << 8));
			channel3.oper_block[ch_num] = ((data >> 3) & 0x7);
		    }
		    break;
		}
	    }
	    break;
	    case 0xB0:
	    {
		if (oper_index == 0)
		{
		    channel.algorithm = (data & 0x7);
		    channel.feedback = ((data >> 3) & 0x7);
		}
	    }
	    break;
	}
    }

    void YM2203::update_frequency(opn_channel &channel)
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

    void YM2203::update_ch3_mode(int val)
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

    void YM2203::update_ksr(opn_operator &oper)
    {
	oper.ksr_val = (oper.keycode >> oper.key_scaling);
	calc_oper_rate(oper);
    }

    void YM2203::calc_oper_rate(opn_operator &oper)
    {
	int p_rate = 0;

	switch (oper.env_state)
	{
	    case opn_oper_state::Attack: p_rate = oper.attack_rate; break;
	    case opn_oper_state::Decay: p_rate = oper.decay_rate; break;
	    case opn_oper_state::Sustain: p_rate = oper.sustain_rate; break;
	    // Extend 4-bit release rate to 5 bits
	    case opn_oper_state::Release: p_rate = ((oper.release_rate << 1) | 1); break;
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

    int32_t YM2203::get_env_output(opn_operator &oper)
    {
	int32_t env_output = oper.env_output;

	if (oper.ssg_enable && (oper.env_state != opn_oper_state::Release) && (oper.ssg_inv ^ oper.ssg_att))
	{
	    env_output = ((0x200 - env_output) & 0x3FF);
	}

	return env_output;
    }

    void YM2203::start_envelope(opn_operator &oper)
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
	    oper.env_state = (oper.sustain_level == 0) ? opn_oper_state::Sustain : opn_oper_state::Decay;
	}
	else
	{
	    if (oper.env_output > 0)
	    {
		oper.env_state = opn_oper_state::Attack;
	    }
	    else
	    {
		oper.env_state = (oper.sustain_level == 0) ? opn_oper_state::Sustain : opn_oper_state::Decay;
	    }
	}

	calc_oper_rate(oper);
    }

    void YM2203::key_on(opn_channel &chan, opn_operator &oper)
    {
	if (!oper.is_keyon && (!chan.is_csm_keyon || (chan.number != 2)))
	{
	    oper.is_keyon = true;

	    start_envelope(oper);
	    calc_oper_rate(oper);

	    oper.phase_counter = 0;
	    oper.ssg_inv = false;
	}
    }

    void YM2203::key_off(opn_channel &chan, opn_operator &oper)
    {
	// TODO: Implement CSM mode
	if (oper.is_keyon && (!chan.is_csm_keyon || (chan.number != 2)))
	{
	    if (oper.env_state < opn_oper_state::Release)
	    {
		if (oper.ssg_enable && (oper.ssg_inv ^ oper.ssg_att))
		{
		    oper.env_output = ((0x200 - oper.env_output) & 0x3FF);
		}

		oper.env_state = opn_oper_state::Release;
		calc_oper_rate(oper);
	    }

	    oper.is_keyon = false;
	}
    }

    void YM2203::update_frequency(opn_operator &oper)
    {
	update_phase(oper);
	update_ksr(oper);
    }

    void YM2203::update_phase(opn_operator &oper)
    {
	oper.keycode = ((oper.block << 2) | fnum_to_keycode[(oper.freq_num >> 7)]);
	uint32_t phase_result = oper.freq_num;
	uint32_t phase_shifted = ((phase_result << oper.block) >> 1);

	// Calculate and add in the detune
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

	// Add in the multiply value
	if (oper.multiply == 0)
	{
	    // Special case for MUL = 0
	    phase_shifted >>= 1;
	}
	else
	{
	    phase_shifted *= oper.multiply;
	    phase_shifted &= 0xFFFFF;
	}

	oper.phase_freq = phase_shifted;
    }

    void YM2203::clock_phase(opn_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.phase_counter = ((oper.phase_counter + oper.phase_freq) & 0xFFFFF);
	    oper.phase_output = (oper.phase_counter >> 10);
	}
    }

    void YM2203::clock_envelope(opn_channel &channel)
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
		    case opn_oper_state::Attack:
		    {
			oper.env_output += ((~oper.env_output * atten_inc) >> 4);

			if (oper.env_output <= 0)
			{
			    oper.env_output = 0;
			    oper.env_state = (oper.sustain_level == 0) ? opn_oper_state::Sustain : opn_oper_state::Decay;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opn_oper_state::Decay:
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
			    oper.env_state = opn_oper_state::Sustain;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opn_oper_state::Sustain:
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
		    case opn_oper_state::Release:
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
				oper.env_state = opn_oper_state::Off;
			    }
			}
		    }
		    break;
		    default: break;
		}
	    }
	}
    }

    void YM2203::clock_ssg_eg(opn_channel &channel)
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

		if (oper.env_state != opn_oper_state::Attack)
		{
		    if ((oper.env_state != opn_oper_state::Release) && !oper.ssg_hold)
		    {
			start_envelope(oper);
		    }
		    else if ((oper.env_state == opn_oper_state::Release) || !(oper.ssg_inv ^ oper.ssg_att))
		    {
			oper.env_output = 0x3FF;
		    }
		}
	    }
	}
    }

    void YM2203::channel_output(opn_channel &channel)
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

	uint32_t oper1_atten = (get_env_output(oper_one) + oper_one.total_level);

	oper_one.outputs[1] = oper_one.outputs[0];
	oper_one.outputs[0] = calc_output(oper_one.phase_output, feedback, oper1_atten);

	uint32_t algorithm_combo = algorithm_combinations[channel.algorithm];

	array<int16_t, 8> opout;
	opout[0] = 0;
	opout[1] = oper_one.outputs[0];

	uint32_t oper2_atten = (get_env_output(oper_two) + oper_two.total_level);

	int32_t oper2_mod = ((opout[(algorithm_combo & 1)] >> 1) & 0x3FF);
	opout[2] = calc_output(oper_two.phase_output, oper2_mod, oper2_atten);
	opout[5] = (opout[1] + opout[2]);

	uint32_t oper3_atten = (get_env_output(oper_three) + oper_three.total_level);

	int32_t oper3_mod = ((opout[((algorithm_combo >> 1) & 0x7)] >> 1) & 0x3FF);
	opout[3] = calc_output(oper_three.phase_output, oper3_mod, oper3_atten);
	opout[6] = (opout[1] + opout[3]);
	opout[7] = (opout[2] + opout[3]);

	uint32_t oper4_atten = (get_env_output(oper_four) + oper_four.total_level);

	int32_t phase_mod = ((opout[((algorithm_combo >> 4) & 0x7)] >> 1) & 0x3FF);
	int32_t ch_output = calc_output(oper_four.phase_output, phase_mod, oper4_atten);

	// YM2203 is full 14-bit with no intermediate clipping
	if (testbit(algorithm_combo, 7))
	{
	    ch_output = clamp((ch_output + opout[1]), -32768, 32767);
	}

	if (testbit(algorithm_combo, 8))
	{
	    ch_output = clamp((ch_output + opout[2]), -32768, 32767);
	}

	if (testbit(algorithm_combo, 9))
	{
	    ch_output = clamp((ch_output + opout[3]), -32768, 32767);
	}

	channel.output = ch_output;
    }

    void YM2203::clock_envelope_gen()
    {
	for (auto &channel : channels)
	{
	    clock_envelope(channel);
	}
    }

    void YM2203::clock_fm()
    {
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

	// Generate channel output
	for (auto &channel : channels)
	{
	    channel_output(channel);
	}
    }

    void YM2203::output_fm()
    {
	int32_t output = 0;

	for (int i = 0; i < 3; i++)
	{
	    output += channels[i].output;
	}

	int16_t fm_sample = dac_ym3014(output);
	last_samples[3] = fm_sample;
    }

    uint32_t YM2203::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 12);
    }

    void YM2203::set_ssg_interface(OPNSSGInterface *inter)
    {
	ssg_inter = inter;
    }

    void YM2203::init()
    {
	reset();
    }

    void YM2203::reset()
    {
	set_prescaler(prescaler_six);
	last_samples.fill(0);

	init_tables();
	for (int ch = 0; ch < 3; ch++)
	{
	    channels[ch].number = ch;
	}

	for (auto &channel : channels)
	{
	    for (auto &oper : channel.opers)
	    {
		oper.env_output = 0x3FF;
		oper.env_state = opn_oper_state::Off;
	    }
	}
    }

    void YM2203::writeIO(int port, uint8_t data)
    {
	if ((port & 1) == 0)
	{
	    chip_address = data;

	    if (chip_address < 0x10)
	    {
		if (ssg_inter != NULL)
		{
		    ssg_inter->writeIO(0, data);
		}
	    }

	    update_prescaler();
	}
	else
	{
	    write_reg(chip_address, data);
	}
    }

    void YM2203::clockchip()
    {
	if ((ssg_sample_index % fm_samples_per_output) == 0)
	{
	    clock_fm();
	    output_fm();
	}

	if (resample)
	{
	    resample();
	}

	copy(ssg_samples.begin(), ssg_samples.end(), last_samples.begin());
    }

    vector<int32_t> YM2203::get_samples()
    {
	vector<int32_t> final_samples;

	for (auto &sample : last_samples)
	{
	    final_samples.push_back(sample);
	}

	return final_samples;
    }
}