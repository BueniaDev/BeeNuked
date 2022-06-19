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

// BeeNuked-YM2151
// Chip Name: YM2151 (OPM)
// Chip Used In: Various arcade machines (Capcom CPS1, Sega System 16x/Super Scaler, etc.), Sharp X68000, Atari 7800, SFG-01 and (early ) SFG-05 (MSX)
// Interesting Trivia:
// 
// The YM2151 was Yamaha's first-ever FM sound chip, and was originally created for some of Yamaha's DX keyboards.
//
//
// BueniaDev's Notes:
//
// Even though several of the YM2151's core features, including the envelope generator,
// are implemented here, the following features are still completely unimplemented:
//
// CSM mode
// IRQ-related functionality
//
// However, work is being done on all of those fronts, so don't lose hope here!

#include "ym2151.h"
using namespace beenuked;

namespace beenuked
{
    YM2151::YM2151()
    {

    }

    YM2151::~YM2151()
    {

    }

    void YM2151::init_tables()
    {
	for (int i = 0; i < 256; i++)
	{
	    double phase_normalized = (double((i << 1) + 1) / 512.f);
	    double sine_result_normalized = sin(phase_normalized * (M_PI / 2));

	    double sine_result_as_att = -log(sine_result_normalized) / log(2.0);

	    uint32_t sine_result = uint32_t((sine_result_as_att * 256.f) + 0.5);
	    sine_table[i] = sine_result;
	}

	for (int i = 0; i < 256; i++)
	{
	    double entry_normalized = (double(i + 1) / 256.f);
	    double res_normalized = pow(2, -entry_normalized);

	    uint32_t result = uint32_t((res_normalized * 2048.f) + 0.5);
	    exp_table[i] = result;
	}

	for (int index = 0; index < 256; index++)
	{
	    // Waveform 0 is a sawtooth wave
	    uint8_t am = (255 - index);
	    int8_t pm = int8_t(index);
	    lfo_table[0][index] = (am | (pm << 8));

	    // Waveform 1 is a square wave
	    am = testbit(index, 7) ? 0 : 0xFF;
	    pm = int8_t(am ^ 0x80);
	    lfo_table[1][index] = (am | (pm << 8));

	    // Waveform 2 is a triangle wave
	    am = testbit(index, 7) ? (index << 1) : ((255 - index) << 1);
	    pm = int8_t(testbit(index, 6) ? am : ~am);
	    lfo_table[2][index] = (am | (pm << 8));

	    // Waveform 3 is noise, which is filled in dynamically
	    lfo_table[3][index] = 0;
	}
    }

    int32_t YM2151::calc_output(uint32_t phase, int32_t mod, uint32_t env)
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

    int YM2151::calc_rate(int p_rate, int rks)
    {
	if (p_rate == 0)
	{
	    return 0;
	}
	else
	{
	    return min(63, ((p_rate * 2) + rks));
	}
    }

    uint32_t YM2151::get_freqnum(int keycode, int keyfrac, int32_t delta)
    {
	int adjusted_keycode = (keycode - (keycode >> 2));
	int32_t eff_freq = ((adjusted_keycode << 6) | keyfrac);
	return opm_freqnums[(eff_freq + delta)];
    }

    void YM2151::update_phase(opm_operator &oper)
    {
	uint32_t phase_result = oper.freq_num;
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

    void YM2151::update_ksr(opm_operator &oper)
    {
	oper.ksr_val = (oper.keycode >> oper.key_scaling);
	calc_oper_rate(oper);
    }

    void YM2151::calc_oper_rate(opm_operator &oper)
    {
	int p_rate = 0;

	switch (oper.env_state)
	{
	    case opm_oper_state::Attack: p_rate = oper.attack_rate; break;
	    case opm_oper_state::Decay: p_rate = oper.decay_rate; break;
	    case opm_oper_state::Sustain: p_rate = oper.sustain_rate; break;
	    // Extend 4-bit release rate to 5 bits
	    case opm_oper_state::Release: p_rate = ((oper.release_rate << 1) | 1); break;
	    default: p_rate = 0; break;
	}

	oper.env_rate = calc_rate(p_rate, oper.ksr_val);
    }

    void YM2151::update_lfo(opm_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.lfo_am_sens = channel.lfo_am_sens;
	    oper.lfo_pm_sens = channel.lfo_pm_sens;
	    update_frequency(channel, oper);
	}
    }

    void YM2151::update_frequency(opm_channel &channel, opm_operator &oper)
    {
	int32_t detune_delta = detune2_table[oper.detune2];

	// Convert the coarse detune cents value to 1/64ths
	int16_t delta = ((detune_delta * 64 + 50) / 100);

	uint32_t pm_sens = channel.lfo_pm_sens;

	if (pm_sens != 0)
	{
	    if (pm_sens < 6)
	    {
		delta += (lfo_raw_pm >> (6 - pm_sens));
	    }
	    else
	    {
		delta += (lfo_raw_pm << (pm_sens - 5));
	    }
	}

	oper.freq_num = get_freqnum(channel.keycode, channel.keyfrac, delta);
	oper.block = channel.block;
	oper.keycode = ((channel.block << 2) | (channel.keycode >> 2));
	update_phase(oper);
	update_ksr(oper);
    }

    void YM2151::update_frequency(opm_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    update_frequency(channel, oper);
	}
    }

    void YM2151::start_envelope(opm_operator &oper)
    {
	if (calc_rate(oper.attack_rate, oper.ksr_val) >= 62)
	{
	    oper.env_output = 0;
	    oper.env_state = (oper.sustain_level == 0) ? opm_oper_state::Sustain : opm_oper_state::Decay;
	}
	else
	{
	    if (oper.env_output > 0)
	    {
		oper.env_state = opm_oper_state::Attack;
	    }
	    else
	    {
		oper.env_state = (oper.sustain_level == 0) ? opm_oper_state::Sustain : opm_oper_state::Decay;
	    }
	}

	calc_oper_rate(oper);
    }

    void YM2151::key_on(opm_channel &channel, opm_operator &oper)
    {
	if (!oper.is_keyon)
	{
	    oper.is_keyon = true;
	    start_envelope(oper);
	    oper.phase_counter = 0;
	}
    }

    void YM2151::key_off(opm_channel &channel, opm_operator &oper)
    {
	if (oper.is_keyon)
	{
	    oper.is_keyon = false;

	    if (oper.env_state < opm_oper_state::Release)
	    {
		oper.env_state = opm_oper_state::Release;
		calc_oper_rate(oper);
	    }
	}
    }

    void YM2151::clock_phase(opm_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    update_frequency(channel, oper);
	    oper.phase_counter = ((oper.phase_counter + oper.phase_freq) & 0xFFFFF);
	    oper.phase_output = (oper.phase_counter >> 10);
	}
    }

    void YM2151::clock_envelope(opm_channel &channel)
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
		    case opm_oper_state::Attack:
		    {
			oper.env_output = ((~oper.env_output * atten_inc) >> 4);

			if (oper.env_output <= 0)
			{
			    oper.env_output = 0;
			    oper.env_state = (oper.sustain_level == 0) ? opm_oper_state::Sustain : opm_oper_state::Decay;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opm_oper_state::Decay:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= oper.sustain_level)
			{
			    oper.env_state = opm_oper_state::Sustain;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opm_oper_state::Sustain:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= 0x400)
			{
			    oper.env_output = 0x3FF;
			}
		    }
		    break;
		    case opm_oper_state::Release:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= 0x400)
			{
			    oper.env_output = 0x3FF;
			    oper.env_state = opm_oper_state::Off;
			}
		    }
		    break;
		    default: break;
		}
	    }
	}
    }

    void YM2151::clock_lfo()
    {
	uint32_t freq = noise_freq;

	for (int rep = 0; rep < 2; rep++)
	{
	    noise_lfsr <<= 1;
	    noise_lfsr |= (testbit(noise_lfsr, 17) ^ testbit(noise_lfsr, 14) ^ 1);

	    if (noise_counter++ >= freq)
	    {
		noise_counter = 0;
		noise_state = testbit(noise_lfsr, 17);
	    }
	}

	uint8_t rate = lfo_rate;
	lfo_counter += ((0x10 | (rate & 0xF)) << (rate >> 4));

	if (lfo_reset)
	{
	    lfo_counter = 0;
	}

	uint32_t lfo = ((lfo_counter >> 22) & 0xFF);

	uint32_t lfo_noise = ((noise_lfsr >> 17) & 0xFF);
	lfo_table[3][((lfo + 1) & 0xFF)] = (lfo_noise | (lfo_noise << 8));

	int32_t ampm = lfo_table[lfo_waveform][lfo];

	lfo_am = ((ampm & 0xFF) * lfo_am_sens) >> 7;
	lfo_raw_pm = (((ampm >> 8) * int32_t(lfo_pm_sens)) >> 7);
    }

    uint32_t YM2151::get_lfo_am(opm_channel &channel)
    {
	uint32_t am_sens = channel.lfo_am_sens;

	if (am_sens == 0)
	{
	    return 0;
	}

	return (lfo_am << (am_sens - 1));
    }

    void YM2151::channel_output(opm_channel &channel)
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

	uint32_t oper1_atten = (oper_one.total_level + oper_one.env_output + oper1_am);

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

	uint32_t oper2_atten = (oper_two.total_level + oper_two.env_output + oper2_am);

	int16_t oper2_output = opout[(algorithm_combo & 1)];

	int32_t oper2_mod = ((oper2_output >> 1) & 0x3FF);
	opout[2] = calc_output(oper_two.phase_output, oper2_mod, oper2_atten);
	opout[5] = (opout[1] + opout[2]);

	uint32_t oper3_am = 0;

	if (oper_three.lfo_enable)
	{
	    oper3_am += get_lfo_am(channel);
	}

	uint32_t oper3_atten = (oper_three.total_level + oper_three.env_output + oper3_am);

	int32_t oper3_output = opout[((algorithm_combo >> 1) & 0x7)];

	int32_t oper3_mod = ((oper3_output >> 1) & 0x3FF);
	opout[3] = calc_output(oper_three.phase_output, oper3_mod, oper3_atten);
	opout[6] = (opout[1] + opout[3]);
	opout[7] = (opout[2] + opout[3]);

	uint32_t oper4_am = 0;

	if (oper_four.lfo_enable)
	{
	    oper4_am += get_lfo_am(channel);
	}

	uint32_t oper4_atten = (oper_four.total_level + oper_four.env_output);

	int32_t oper4_output = opout[((algorithm_combo >> 4) & 0x7)];

	int32_t phase_mod = ((oper4_output >> 1) & 0x3FF);

	int32_t ch_output = 0;

	if (noise_enable && (channel.number == 7))
	{
	    int32_t noise_atten = ((oper4_atten ^ 0x3FF) << 1);
	    ch_output = testbit(noise_state, 0) ? -noise_atten : noise_atten;
	}
	else
	{
	    oper4_atten += oper4_am;
	    ch_output = calc_output(oper_four.phase_output, phase_mod, oper4_atten);
	}

	// YM2151 is full 14-bit with no intermediate clipping
	if (testbit(algorithm_combo, 7))
	{
	    ch_output = clamp<int32_t>((ch_output + opout[1]), -32768, 32767);
	}

	if (testbit(algorithm_combo, 8))
	{
	    ch_output = clamp<int32_t>((ch_output + opout[2]), -32768, 32767);
	}

	if (testbit(algorithm_combo, 9))
	{
	    ch_output = clamp<int32_t>((ch_output + opout[3]), -32768, 32767);
	}

	channel.output = ch_output;
    }

    void YM2151::set_status_bit(int bit)
    {
	opm_status |= (1 << bit);

	/*
	if (irq_inter != NULL)
	{
	    irq_inter->handle_irq(true);
	}
	*/
    }

    void YM2151::reset_status_bit(int bit)
    {
	opm_status &= ~(1 << bit);

	/*
	if (irq_inter != NULL)
	{
	    irq_inter->handle_irq(false);
	}
	*/
    }

    void YM2151::clock_timers()
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

    void YM2151::write_reg(uint8_t reg, uint8_t data)
    {
	int reg_group = (reg & 0xE0);
	int reg_addr = (reg & 0x1F);

	int ch_num = (reg_addr & 0x7);
	int oper_index = ((reg_addr >> 3) & 0x3);
	array<int, 4> oper_table = {0, 2, 1, 3};
	int oper_num = oper_table[oper_index];

	auto &channel = channels[ch_num];
	auto &ch_oper = channel.opers[oper_num];

	switch (reg_group)
	{
	    case 0x00:
	    {
		switch (reg)
		{
		    case 0x01:
		    {
			lfo_reset = testbit(data, 1);
		    }
		    break;
		    case 0x08:
		    {
			int channel_num = (data & 0x7);
			auto &ch_key = channels[channel_num];

			for (int i = 0; i < 4; i++)
			{
			    if (testbit(data, (3 + i)))
			    {
				key_on(ch_key, ch_key.opers[i]);
			    }
			    else
			    {
				key_off(ch_key, ch_key.opers[i]);
			    }
			}
		    }
		    break;
		    case 0x0F:
		    {
			noise_enable = testbit(data, 7);
			noise_freq = (data & 0x1F);
		    }
		    break;
		    case 0x10:
		    {
			timera_freq = ((timera_freq & 0x3) | (data << 2));
		    }
		    break;
		    case 0x11:
		    {
			timera_freq = ((timera_freq & 0x3FC) | (data & 0x3));
		    }
		    break;
		    case 0x12:
		    {
			timerb_freq = data;
		    }
		    break;
		    case 0x14:
		    {
			// cout << "Writing to CSM/IRQ flag reset/IRQ enable register" << endl;

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
		    case 0x18:
		    {
			lfo_rate = data;
		    }
		    break;
		    case 0x19:
		    {
			if (testbit(data, 7))
			{
			    lfo_pm_sens = (data & 0x7F);
			}
			else
			{
			    lfo_am_sens = (data & 0x7F);
			}
		    }
		    break;
		    case 0x1B:
		    {
			cout << "Writing to CT2/CT1 register" << endl;
			lfo_waveform = (data & 0x3);
		    }
		    break;
		    default: break;
		}
	    }
	    break;
	    case 0x20:
	    {
		switch (reg & 0x18)
		{
		    case 0x00:
		    {
			channel.is_pan_right = testbit(data, 7);
			channel.is_pan_left = testbit(data, 6);
			channel.feedback = ((data >> 4) & 0x7);
			channel.algorithm = (data & 0x7);
		    }
		    break;
		    case 0x08:
		    {
			channel.block = ((data >> 4) & 0x7);
			channel.keycode = (data & 0xF);
			update_frequency(channel);
		    }
		    break;
		    case 0x10:
		    {
			channel.keyfrac = ((data >> 2) & 0x3F);
			update_frequency(channel);
		    }
		    break;
		    case 0x18:
		    {
			channel.lfo_pm_sens = ((data >> 4) & 0x7);
			channel.lfo_am_sens = (data & 0x3);
			update_lfo(channel);
		    }
		    break;
		}
	    }
	    break;
	    case 0x40:
	    {
		ch_oper.detune = ((data >> 4) & 0x7);
		ch_oper.multiply = (data & 0xF);
		update_phase(ch_oper);
	    }
	    break;
	    case 0x60:
	    {
		ch_oper.total_level = ((data & 0x7F) << 3);
	    }
	    break;
	    case 0x80:
	    {
		ch_oper.key_scaling = (3 - (data >> 6));
		ch_oper.attack_rate = (data & 0x1F);
		update_ksr(ch_oper);
	    }
	    break;
	    case 0xA0:
	    {
		ch_oper.lfo_enable = testbit(data, 7);
		ch_oper.decay_rate = (data & 0x1F);
	    }
	    break;
	    case 0xC0:
	    {
		ch_oper.detune2 = ((data >> 6) & 0x3);
		ch_oper.sustain_rate = (data & 0x1F);
		update_frequency(channel, ch_oper);
	    }
	    break;
	    case 0xE0:
	    {
		int sl_rate = (data >> 4);
		int sus_level = (sl_rate == 15) ? 31 : sl_rate;
		ch_oper.sustain_level = (sus_level << 5);
		ch_oper.release_rate = (data & 0xF);
	    }
	    break;
	    default: break;
	}
    }

    uint32_t YM2151::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 64);
    }

    void YM2151::init()
    {
	reset();
    }

    void YM2151::reset()
    {
	init_tables();

	env_timer = 0;
	env_clock = 0;
	lfo_counter = 0;

	lfo_rate = 0;
	lfo_reset = 0;

	lfo_pm_sens = 0;
	lfo_am_sens = 0;
	lfo_waveform = 0;
	noise_freq = 0;
	noise_enable = false;

	noise_lfsr = 1;
	noise_state = 0;
	noise_lfo = 0;

	for (int i = 0; i < 8; i++)
	{
	    channels[i].number = i;
	}

	for (auto &channel : channels)
	{
	    for (auto &oper : channel.opers)
	    {
		oper.env_output = 0x3FF;
		oper.env_state = opm_oper_state::Off;
	    }
	}

	timera_counter = 1023;
	timerb_counter = 255;
    }

    uint8_t YM2151::readIO(int port)
    {
	uint8_t data = 0xFF;
	if ((port & 1) == 1)
	{
	    data = opm_status;
	}

	return data;
    }

    void YM2151::writeIO(int port, uint8_t data)
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

    void YM2151::clock_channel_eg()
    {
	env_clock += 1;

	for (auto &channel : channels)
	{
	    clock_envelope(channel);
	}
    }

    void YM2151::clockchip()
    {
	clock_timers();
	clock_lfo();

	env_timer += 1;

	if (env_timer == 3)
	{
	    env_timer = 0;
	    clock_channel_eg();
	}

	for (auto &channel : channels)
	{
	    clock_phase(channel);
	}

	for (auto &channel : channels)
	{
	    channel_output(channel);
	}
    }

    vector<int32_t> YM2151::get_samples()
    {
	array<int32_t, 2> output = {0, 0};

	for (int i = 0; i < 8; i++)
	{
	    output[0] += (channels[i].is_pan_left) ? channels[i].output : 0;
	    output[1] += (channels[i].is_pan_right) ? channels[i].output : 0;
	}

	vector<int32_t> final_samples;

	for (int i = 0; i < 2; i++)
	{
	    final_samples.push_back(dac_ym3014(output[i]));
	}

	return final_samples;
    }
};