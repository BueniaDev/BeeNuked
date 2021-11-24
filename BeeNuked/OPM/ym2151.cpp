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

// BeeNuked-YM2151 (WIP)
// Chip Name: YM2151 (OPM)
// Chip Used In: Various arcade machines (Capcom CPS1, Sega System 16x/Super Scaler, etc.), Sharp X68000, Atari 7800, SFG-01 and (early ) SFG-05 (MSX)
// Interesting Trivia:
// 
// The YM2151 was Yamaha's first-ever FM sound chip, and was originally created for some of Yamaha's DX keyboards.
//
//
// BueniaDev's Notes:
//
// This core is a huge WIP at the moment, and a lot of the YM2151's core features are completely unimplemented.
// As such, expect a lot of things to sound completely incorrect (and, because the envelope generator is 
// amongst the completely unimplemented features, kinda loud, as well).
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

    void YM2151::update_frequency(opm_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.freq_num = get_freqnum(channel.keycode, channel.keyfrac, 0);
	    oper.block = channel.block;
	    update_phase(oper);
	}
    }

    void YM2151::key_on(opm_channel &channel, opm_operator &oper)
    {
	if (!oper.is_keyon)
	{
	    oper.is_keyon = true;
	}
    }

    void YM2151::key_off(opm_channel &channel, opm_operator &oper)
    {
	if (oper.is_keyon)
	{
	    oper.is_keyon = false;
	}
    }

    void YM2151::clock_phase(opm_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.phase_counter = ((oper.phase_counter + oper.phase_freq) & 0xFFFFF);
	    oper.phase_output = (oper.phase_counter >> 10);
	}
    }

    void YM2151::channel_output(opm_channel &channel)
    {
	auto &oper_one = channel.opers[0];
	auto &oper_two = channel.opers[1];
	auto &oper_three = channel.opers[2];
	auto &oper_four = channel.opers[3];

	uint32_t oper1_atten = ((oper_one.is_keyon) ? oper_one.total_level : 0x3FF);

	int32_t oper1_output = calc_output(oper_one.phase_output, 0, oper1_atten);
	
	uint32_t algorithm_combo = algorithm_combinations[channel.algorithm];

	array<int16_t, 8> opout;
	opout[0] = 0;
	opout[1] = oper1_output;

	uint32_t oper2_atten = ((oper_two.is_keyon) ? oper_two.total_level : 0x3FF);

	int16_t oper2_output = opout[(algorithm_combo & 1)];

	int32_t oper2_mod = ((oper2_output >> 1) & 0x3FF);
	opout[2] = calc_output(oper_two.phase_output, oper2_mod, oper2_atten);
	opout[5] = (opout[1] + opout[2]);

	uint32_t oper3_atten = ((oper_three.is_keyon) ? oper_three.total_level : 0x3FF);

	int32_t oper3_output = opout[((algorithm_combo >> 1) & 0x7)];

	int32_t oper3_mod = ((oper3_output >> 1) & 0x3FF);
	opout[3] = calc_output(oper_three.phase_output, oper3_mod, oper3_atten);
	opout[6] = (opout[1] + opout[3]);
	opout[7] = (opout[2] + opout[3]);

	uint32_t oper4_atten = ((oper_four.is_keyon) ? oper_four.total_level : 0x3FF);

	int32_t oper4_output = opout[((algorithm_combo >> 4) & 0x7)];

	int32_t phase_mod = ((oper4_output >> 1) & 0x3FF);
	int32_t ch_output = calc_output(oper_four.phase_output, phase_mod, oper4_atten);

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
			cout << "Writing to test/LFO register" << endl;
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
			cout << "Writing to noise register" << endl;
		    }
		    break;
		    case 0x10:
		    {
			cout << "Writing to MSB of timer A" << endl;
		    }
		    break;
		    case 0x11:
		    {
			cout << "Writing to LSB of timer A" << endl;
		    }
		    break;
		    case 0x12:
		    {
			cout << "Writing to timer B" << endl;
		    }
		    break;
		    case 0x14:
		    {
			cout << "Writing to CSM/IRQ flag reset/IRQ enable/timer register" << endl;
		    }
		    break;
		    case 0x18:
		    {
			cout << "Writing to LFO frequency register" << endl;
		    }
		    break;
		    case 0x19:
		    {
			if (testbit(data, 7))
			{
			    cout << "Writing to PMD register" << endl;
			}
			else
			{
			    cout << "Writing to AMD register" << endl;
			}
		    }
		    break;
		    case 0x1B:
		    {
			cout << "Writing to CT2/CT1/LFO waveform register" << endl;
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
			cout << "Writing to YM2151 channel " << dec << ch_num << " feedback/panning register" << endl;
			channel.is_pan_right = testbit(data, 7);
			channel.is_pan_left = testbit(data, 6);
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
			cout << "Writing to YM2151 channel " << dec << ch_num << " PMS/AMS register" << endl;
		    }
		    break;
		}
	    }
	    break;
	    case 0x40:
	    {
		cout << "Writing to DT1 register of YM2151 channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
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
		cout << "Writing to KS/AR register of YM2151 channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0xA0:
	    {
		cout << "Writing to AMS-EN/D1R register of YM2151 channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0xC0:
	    {
		cout << "Writing to DT2/D2R register of YM2151 channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0xE0:
	    {
		cout << "Writing to D1L/RR register of YM2151 channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    default: break;
	}
    }

    uint32_t YM2151::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 64);
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

    void YM2151::clockchip()
    {
	for (auto &channel : channels)
	{
	    clock_phase(channel);
	}

	for (auto &channel : channels)
	{
	    channel_output(channel);
	}
    }

    array<int16_t, 2> YM2151::get_sample()
    {
	array<int32_t, 2> output = {0, 0};

	for (int i = 3; i < 4; i++)
	{
	    output[0] += (channels[i].is_pan_left) ? channels[i].output : 0;
	    output[1] += (channels[i].is_pan_right) ? channels[i].output : 0;
	}

	array<int16_t, 2> final_samples = {0, 0};

	for (int i = 0; i < 2; i++)
	{
	    final_samples[i] = dac_ym3014(output[i]);
	}

	return final_samples;
    }
};