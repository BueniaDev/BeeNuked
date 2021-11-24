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

// BeeNuked-YM2413 (WIP)
// Chip Name: YM2413 (OPLL)
// Chip Used In: Early Capcom arcade games, Sega Master System (Japanese version), MSX-MUSIC expansion card, etc.
// Interesting Trivia:
//
// Unlike its direct ancestors, the YM2413 uses a time-multiplexed 9-bit DAC for audio output.
//
//
// BueniaDev's Notes:
//
// This core is a huge WIP at the moment, and a lot of the YM2413's core features are completely unimplemented.
// As such, expect a lot of things to sound completely incorrect (and, because the envelope generator is 
// amongst the completely unimplemented features, kinda loud, as well).
//
// However, work is being done on all of those fronts, so don't lose hope here!

#include "ym2413.h"
using namespace beenuked;

namespace beenuked
{
    YM2413::YM2413()
    {
	init_tables();

	for (int i = 0; i < 9; i++)
	{
	    channels[i].number = i;
	    channels[i].opers[1].is_carrier = true;
	}

	inst_patch = ym2413_instruments;
    }

    YM2413::~YM2413()
    {

    }

    void YM2413::init_tables()
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

    int32_t YM2413::calc_output(int32_t phase, int32_t mod, uint32_t env)
    {
	uint32_t atten = min<uint32_t>(127, env);

	uint32_t combined_phase = ((phase + mod) & 0x3FF);

	bool sign_bit = testbit(combined_phase, 9);
	bool mirror_bit = testbit(combined_phase, 8);
	uint8_t quarter_phase = (combined_phase & 0xFF);

	if (mirror_bit)
	{
	    quarter_phase = ~quarter_phase;
	}

	uint32_t sine_result = sine_table[quarter_phase];

	uint32_t attenuation = (atten << 4);
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

    void YM2413::update_frequency(opll_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.freq_num = channel.freq_num;
	    oper.block = channel.block;
	    update_frequency(oper);
	}
    }

    void YM2413::update_frequency(opll_operator &oper)
    {
	update_phase(oper);
	update_total_level(oper);
    }

    void YM2413::update_phase(opll_operator &oper)
    {
	uint32_t phase_level = oper.freq_num;
	int multiply = mul_table[oper.multiply];
	oper.phase_freq = (((phase_level * multiply) << oper.block) >> 1);
    }

    void YM2413::update_total_level(opll_operator &oper)
    {
	int temp_ksl = 16 * oper.block - ksl_table[(oper.freq_num >> 5)];
	int ksl_val = (oper.ksl == 0) ? 0 : (max(0, temp_ksl) >> (3 - oper.ksl));

	if (oper.is_carrier)
	{
	    oper.tll_val = ((oper.volume << 3) + ksl_val);
	}
	else
	{
	    oper.tll_val = ((oper.total_level << 1) + ksl_val);
	}
    }

    void YM2413::clock_phase(opll_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.phase_counter = ((oper.phase_counter + oper.phase_freq) & 0x7FFFF);
	    oper.phase_output = (oper.phase_counter >> 9);
	}
    }

    void YM2413::channel_output(opll_channel &channel)
    {
	auto &mod_slot = channel.opers[0];
	auto &car_slot = channel.opers[1];

	int32_t feedback = 0;

	if (channel.feedback != 0)
	{
	    feedback = ((mod_slot.outputs[0] + mod_slot.outputs[1]) >> (10 - channel.feedback));
	}

	uint32_t mod_tll = (mod_slot.tll_val);
	uint32_t mod_env = (mod_tll);

	mod_slot.outputs[1] = mod_slot.outputs[0];
	mod_slot.outputs[0] = calc_output(mod_slot.phase_output, feedback, mod_env);

	int32_t phase_mod = ((mod_slot.outputs[0] >> 1) & 0x3FF);

	uint32_t car_tll = (car_slot.tll_val);
	uint32_t car_env = (car_tll);

	int32_t ch_output = calc_output(car_slot.phase_output, phase_mod, car_env);
	channel.output = (ch_output >> 5);
    }

    void YM2413::set_patch(opll_channel &channel, int patch_index)
    {
	channel.inst_number = patch_index;
	update_instrument(channel);
    }

    void YM2413::update_instrument(opll_channel &channel)
    {
	auto patch_values = inst_patch[channel.inst_number];
	channel.opers[0].multiply = (patch_values[0] & 0xF);
	channel.opers[1].multiply = (patch_values[1] & 0xF);
	channel.opers[0].ksl = (patch_values[2] >> 6);
	channel.opers[0].total_level = (patch_values[2] & 0x3F);
	channel.opers[1].ksl = (patch_values[3] >> 6);
	channel.feedback = (patch_values[3] & 0x7);

	for (auto &oper : channel.opers)
	{
	    update_frequency(oper);
	}
    }

    void YM2413::write_reg(uint8_t reg, uint8_t data)
    {
	int reg_group = (reg & 0xF0);
	int reg_addr = (reg & 0xF);

	switch (reg_group)
	{
	    case 0x00:
	    {
		if (reg_addr <= 0x07)
		{
		    inst_patch[0][reg_addr] = data;

		    for (auto &channel : channels)
		    {
			if (channel.inst_number == 0)
			{
			    update_instrument(channel);
			}
		    }
		}
		else
		{
		    switch (reg_addr)
		    {
			case 0x0E:
			{
			    cout << "Writing to rhythm mode register..." << endl;
			}
			break;
		    }
		}
	    }
	    break;
	    case 0x10:
	    {
		if (reg_addr >= 9)
		{
		    // Verified on a real YM2413
		    reg_addr -= 9;
		}

		auto &channel = channels[reg_addr];
		channel.freq_num = ((channel.freq_num & 0x100) | data);
		update_frequency(channel);
	    }
	    break;
	    case 0x20:
	    {
		if (reg_addr >= 9)
		{
		    // Verified on a real YM2413
		    reg_addr -= 9;
		}

		cout << "Writing to channel " << dec << int(reg_addr) << " sustain/key-on register" << endl;
		auto &channel = channels[reg_addr];
		channel.freq_num = ((channel.freq_num & 0xFF) | ((data & 0x1) << 8));
		channel.block = ((data >> 1) & 0x7);
		update_frequency(channel);
	    }
	    break;
	    case 0x30:
	    {
		if (reg_addr >= 9)
		{
		    // Verified on a real YM2413
		    reg_addr -= 9;
		}

		cout << "Writing to channel " << dec << int(reg_addr) << " instrument/volume register" << endl;

		auto &channel = channels[reg_addr];

		if (reg_addr >= 6)
		{
		    cout << "Updating volume in rhythm mode..." << endl;
		}
		else
		{
		    set_patch(channel, (data >> 4));
		}

		channel.opers[1].volume = (data & 0xF);
		update_total_level(channel.opers[1]);
	    }
	    break;
	}
    }

    uint32_t YM2413::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 72);
    }

    void YM2413::writeIO(int port, uint8_t data)
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

    void YM2413::clockchip()
    {
	for (auto &channel : channels)
	{
	    clock_phase(channel);
	}

	for (int i = 0; i < 6; i++)
	{
	    channel_output(channels[i]);
	}
    }

    array<int16_t, 2> YM2413::get_sample()
    {
	int32_t output = 0;
	output += channels[0].output;
	int32_t sample = ((output << 7) / 9);

	int16_t final_sample = clamp(sample, -32768, 32767);
	return {final_sample, final_sample};
    }
}