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

// BeeNuked-YM2413
// Chip Name: YM2413 (OPLL)
// Chip Used In: Early Capcom arcade games, Sega Master System (Japanese version), MSX-MUSIC expansion card, etc.
// Interesting Trivia:
//
// Unlike its direct ancestors, the YM2413 uses a time-multiplexed 9-bit DAC for audio output.
//
//
// BueniaDev's Notes:
//
// This core is pretty much complete at this point, with the vast majority of the YM2413's features fully implemented.
// However, the YM2413's test register is completely unimplemented, which a few MSX-MUSIC-related features
// actually rely on.
//
// However, that shouldn't be a problem for the vast majority of use cases.

#include "ym2413.h"
using namespace beenuked;

namespace beenuked
{
    YM2413::YM2413()
    {

    }

    YM2413::~YM2413()
    {

    }

    int YM2413::calc_rate(int p_rate, int rks)
    {
	if (p_rate == 0)
	{
	    return 0;
	}
	else
	{
	    return min(63, ((p_rate * 4) + rks));
	}
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

    uint32_t YM2413::fetch_sine_result(uint32_t phase, bool wave_sel, bool &is_negate)
    {
	bool sign_bit = testbit(phase, 9);
	bool mirror_bit = testbit(phase, 8);
	uint8_t quarter_phase = (phase & 0xFF);

	if (mirror_bit)
	{
	    quarter_phase = ~quarter_phase;
	}

	uint32_t sine_result = sine_table[quarter_phase];

	if (sign_bit && wave_sel)
	{
	    sine_result = 0xFFF;
	    is_negate = false;
	}
	else
	{
	    is_negate = sign_bit;
	}

	return sine_result;
    }

    int32_t YM2413::calc_output(int32_t phase, int32_t mod, uint32_t env, bool is_ws)
    {
	uint32_t atten = min<uint32_t>(127, env);

	uint32_t combined_phase = ((phase + mod) & 0x3FF);

	bool is_negate = false;

	uint32_t sine_result = fetch_sine_result(combined_phase, is_ws, is_negate);

	uint32_t attenuation = (atten << 4);
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
	update_rks(oper);
    }

    void YM2413::update_phase(opll_operator &oper)
    {
	int8_t pitch_mod = (oper.is_vibrato) ? pm_table[((oper.freq_num >> 6) & 7)][((pm_clock >> 10) & 7)] : 0;
	uint32_t phase_level = (oper.freq_num * 2 + pitch_mod);
	int multiply = mul_table[oper.multiply];
	oper.phase_freq = (((phase_level * multiply) << oper.block) >> 2);
    }

    void YM2413::update_total_level(opll_operator &oper)
    {
	int temp_ksl = 16 * oper.block - ksl_table[(oper.freq_num >> 5)];
	int ksl_val = (oper.ksl == 0) ? 0 : (max(0, temp_ksl) >> (3 - oper.ksl));

	if (oper.is_carrier || oper.is_rhythm)
	{
	    oper.tll_val = ((oper.volume << 3) + ksl_val);
	}
	else
	{
	    oper.tll_val = ((oper.total_level << 1) + ksl_val);
	}
    }

    void YM2413::update_rks(opll_operator &oper)
    {
	int block_fnum = ((oper.block << 1) | (oper.freq_num >> 8));
	oper.rks_val = (oper.is_ksr) ? block_fnum : (block_fnum >> 2);
	calc_oper_rate(oper);
    }

    void YM2413::calc_oper_rate(opll_operator &oper)
    {
	int p_rate = 0;

	if ((!oper.is_carrier && !oper.is_rhythm) && !oper.is_keyon)
	{
	    p_rate = 0;
	}
	else
	{
	    switch (oper.env_state)
	    {
		case opll_oper_state::Damp: p_rate = 12; break; // Damper speed before key-on is 12
		case opll_oper_state::Attack: p_rate = oper.attack_rate; break;
		case opll_oper_state::Decay: p_rate = oper.decay_rate; break;
		case opll_oper_state::Sustain: p_rate = (oper.is_sustained) ? 0 : oper.release_rate; break;
		case opll_oper_state::Release:
		{
		    if (oper.sustain_flag)
		    {
			p_rate = 5;
		    }
		    else if (oper.is_sustained)
		    {
			p_rate = oper.release_rate;
		    }
		    else
		    {
			p_rate = 7;
		    }
		}
		break;
		default: break;
	    }
	}

	oper.env_rate = calc_rate(p_rate, oper.rks_val);
    }

    void YM2413::update_sus_flag(opll_channel &channel, bool flag)
    {
	for (auto &oper : channel.opers)
	{
	    oper.sustain_flag = flag;
	}
    }

    void YM2413::update_key_status(opll_channel &channel, bool val)
    {
	for (auto &oper : channel.opers)
	{
	    update_key_status(oper, val);
	}
    }

    void YM2413::update_key_status(opll_operator &oper, bool val)
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

    void YM2413::key_on(opll_operator &oper)
    {
	if (!oper.is_keyon)
	{
	    oper.is_keyon = true;
	    oper.env_state = opll_oper_state::Damp;
	    calc_oper_rate(oper);
	}
    }

    void YM2413::key_off(opll_operator &oper)
    {
	if (oper.is_keyon)
	{
	    oper.is_keyon = false;

	    if (oper.is_carrier || oper.is_rhythm)
	    {
		if (oper.env_state < opll_oper_state::Release)
		{
		    oper.env_state = opll_oper_state::Release;
		    calc_oper_rate(oper);
		}
	    }
	}
    }

    void YM2413::clock_phase(opll_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    update_phase(oper);
	    oper.phase_counter = ((oper.phase_counter + oper.phase_freq) & 0x7FFFF);
	    oper.phase_output = (oper.phase_counter >> 9);
	}
    }

    void YM2413::clock_envelope(opll_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    uint32_t counter_shift_val = counter_shift_table[oper.env_rate];
	    int shift_mask = ((1 << counter_shift_val) - 1);

	    int eg_mask = (oper.env_state == opll_oper_state::Attack) ? (shift_mask & ~3) : shift_mask;
	    auto att_inc_table = (oper.env_state == opll_oper_state::Attack) ? att_inc_attack : att_inc_decay;

	    if ((env_clock & eg_mask) == 0)
	    {
		int update_cycle = ((env_clock >> counter_shift_val) & 0xF);
		auto atten_inc = att_inc_table[oper.env_rate][update_cycle];

		switch (oper.env_state)
		{
		    case opll_oper_state::Damp:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= 124)
			{
			    if (calc_rate(oper.attack_rate, oper.rks_val) >= 60)
			    {
				oper.env_output = 0;
				oper.env_state = (oper.sustain_level == 0) ? opll_oper_state::Sustain : opll_oper_state::Decay;
			    }
			    else
			    {
				oper.env_output = 127;
				oper.env_state = opll_oper_state::Attack;
			    }

			    if ((oper.is_carrier || oper.is_rhythm))
			    {
				if (!oper.is_rhythm)
				{
				    // If the operator's not in rhythm mode,
				    // reset both of the channel's operators'
				    // phase counters
				    for (auto &ch_oper : channel.opers)
				    {
					if (!ch_oper.phase_keep)
					{
					    ch_oper.phase_counter = 0;
					}
				    }
				}
				else
				{
				    // Otherwise, just reset the current
				    // operator's phase counter
				    if (!oper.phase_keep)
				    {
					oper.phase_counter = 0;
				    }
				}
			    }

			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opll_oper_state::Attack:
		    {
			oper.env_output += ((~oper.env_output * atten_inc) >> 4);

			if (oper.env_output <= 0)
			{
			    oper.env_output = 0;
			    oper.env_state = opll_oper_state::Decay;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opll_oper_state::Decay:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= oper.sustain_level)
			{
			    oper.env_state = opll_oper_state::Sustain;
			    calc_oper_rate(oper);
			}
		    }
		    break;
		    case opll_oper_state::Sustain:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= 124)
			{
			    oper.env_output = 127;
			}
		    }
		    break;
		    case opll_oper_state::Release:
		    {
			oper.env_output += atten_inc;

			if (oper.env_output >= 124)
			{
			    oper.env_output = 127;
			    oper.env_state = opll_oper_state::Off;
			}
		    }
		    break;
		    default: break;
		}
	    }
	}
    }

    void YM2413::clock_ampm()
    {
	am_clock += 1;
	pm_clock += 1;

	for (auto &channel : channels)
	{
	    channel.lfo_am = am_table[((am_clock >> 6) % 210)];
	}
    }

    void YM2413::clock_short_noise()
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

    void YM2413::clock_noise(int cycle)
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

    void YM2413::channel_output(opll_channel &channel)
    {
	auto &mod_slot = channel.opers[0];
	auto &car_slot = channel.opers[1];

	int32_t feedback = 0;

	if (channel.feedback != 0)
	{
	    feedback = ((mod_slot.outputs[0] + mod_slot.outputs[1]) >> (10 - channel.feedback));
	}

	uint8_t mod_am = mod_slot.is_am ? channel.lfo_am : 0;
	uint32_t mod_tll = (mod_slot.tll_val + mod_am);
	uint32_t mod_env = (mod_slot.env_output + mod_tll);

	mod_slot.outputs[1] = mod_slot.outputs[0];
	mod_slot.outputs[0] = calc_output(mod_slot.phase_output, feedback, mod_env, mod_slot.is_ws);

	int32_t phase_mod = ((mod_slot.outputs[0] >> 1) & 0x3FF);

	uint8_t car_am = car_slot.is_am ? channel.lfo_am : 0;
	uint32_t car_tll = (car_slot.tll_val + car_am);
	uint32_t car_env = (car_slot.env_output + car_tll);

	int32_t ch_output = calc_output(car_slot.phase_output, phase_mod, car_env, car_slot.is_ws);
	channel.output = (ch_output >> 5);
    }

    void YM2413::hihat_output()
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
	int32_t rhythm_output = calc_output(phase, 0, rhythm_env, slot.is_ws);
	slot.rhythm_output = (rhythm_output >> 5);
    }

    void YM2413::snare_output()
    {
	auto &slot = channels[7].opers[1];

	uint32_t phase = 0;

	if (testbit(slot.phase_output, 8))
	{
	    phase = testbit(noise_lfsr, 0) ? 0x300 : 0x200;
	}
	else
	{
	    phase = testbit(noise_lfsr, 0) ? 0 : 0x100;
	}

	uint32_t rhythm_env = (slot.env_output + slot.tll_val);
	int32_t rhythm_output = calc_output(phase, 0, rhythm_env, slot.is_ws);
	slot.rhythm_output = (rhythm_output >> 5);
    }

    void YM2413::tom_output()
    {
	auto &slot = channels[8].opers[0];

	uint32_t rhythm_env = (slot.env_output + slot.tll_val);
	int32_t rhythm_output = calc_output(slot.phase_output, 0, rhythm_env, slot.is_ws);
	slot.rhythm_output = (rhythm_output >> 5);
    }

    void YM2413::cym_output()
    {
	auto &slot = channels[8].opers[1];

	uint32_t phase = short_noise ? 0x300 : 0x100;

	uint32_t rhythm_env = (slot.env_output + slot.tll_val);
	int32_t rhythm_output = calc_output(phase, 0, rhythm_env, slot.is_ws);
	slot.rhythm_output = (rhythm_output >> 5);
    }

    void YM2413::set_patch(opll_channel &channel, int patch_index)
    {
	channel.inst_number = patch_index;
	update_instrument(channel);
    }

    void YM2413::update_instrument(opll_channel &channel)
    {
	auto patch_values = inst_patch[channel.inst_number];
	channel.opers[0].is_am = testbit(patch_values[0], 7);
	channel.opers[0].is_vibrato = testbit(patch_values[0], 6);
	channel.opers[0].is_sustained = testbit(patch_values[0], 5);
	channel.opers[0].is_ksr = testbit(patch_values[0], 4);
	channel.opers[0].multiply = (patch_values[0] & 0xF);
	channel.opers[1].is_am = testbit(patch_values[1], 7);
	channel.opers[1].is_vibrato = testbit(patch_values[1], 6);
	channel.opers[1].is_sustained = testbit(patch_values[1], 5);
	channel.opers[1].is_ksr = testbit(patch_values[1], 4);
	channel.opers[1].multiply = (patch_values[1] & 0xF);
	channel.opers[0].ksl = (patch_values[2] >> 6);
	channel.opers[0].total_level = (patch_values[2] & 0x3F);
	channel.opers[1].ksl = (patch_values[3] >> 6);
	channel.opers[1].is_ws = testbit(patch_values[3], 4);
	channel.opers[0].is_ws = testbit(patch_values[3], 3);
	channel.feedback = (patch_values[3] & 0x7);
	channel.opers[0].attack_rate = (patch_values[4] >> 4);
	channel.opers[0].decay_rate = (patch_values[4] & 0xF);
	channel.opers[1].attack_rate = (patch_values[5] >> 4);
	channel.opers[1].decay_rate = (patch_values[5] & 0xF);
	channel.opers[0].sustain_level = ((patch_values[6] >> 4) << 3);
	channel.opers[0].release_rate = (patch_values[6] & 0xF);
	channel.opers[1].sustain_level = ((patch_values[7] >> 4) << 3);
	channel.opers[1].release_rate = (patch_values[7] & 0xF);

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
			    bool rhythm_enable = testbit(data, 5);

			    if (rhythm_enable && !is_rhythm_enabled)
			    {
				set_patch(channels[6], 16);
				channels[7].opers[0].is_rhythm = true;
				channels[7].opers[0].phase_keep = true;
				channels[7].opers[1].is_rhythm = true;
				channels[8].opers[0].is_rhythm = true;
				channels[8].opers[1].is_rhythm = true;
				channels[8].opers[1].phase_keep = true;

				set_patch(channels[7], 17);
				set_patch(channels[8], 18);

				channels[7].opers[0].volume = (channels[7].inst_vol_reg >> 4);
				update_total_level(channels[7].opers[0]);

				channels[8].opers[0].volume = (channels[8].inst_vol_reg >> 4);
				update_total_level(channels[8].opers[0]);

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

				for (int i = 6; i <= 8; i++)
				{
				    int inst_index = (channels[i].inst_vol_reg >> 4);
				    set_patch(channels[i], inst_index);
				}

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
				update_key_status(channels[7].opers[0], false);
				update_key_status(channels[7].opers[1], false);
				update_key_status(channels[8].opers[0], false);
				update_key_status(channels[8].opers[1], false);
			    }
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

		auto &channel = channels[reg_addr];
		channel.freq_num = ((channel.freq_num & 0xFF) | ((data & 0x1) << 8));
		channel.block = ((data >> 1) & 0x7);
		update_frequency(channel);
		update_key_status(channel, testbit(data, 4));
		update_sus_flag(channel, testbit(data, 5));
	    }
	    break;
	    case 0x30:
	    {
		if (reg_addr >= 9)
		{
		    // Verified on a real YM2413
		    reg_addr -= 9;
		}

		auto &channel = channels[reg_addr];

		channel.inst_vol_reg = data;

		if (is_rhythm_enabled && (reg_addr >= 6))
		{
		    switch (reg_addr)
		    {
			case 0x07:
			case 0x08:
			{
			    channel.opers[0].volume = (data >> 4);
			    update_total_level(channel.opers[0]);
			}
			break;
		    }
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

    void YM2413::init(OPLLType type)
    {
	set_chip_type(type);
	reset();
    }

    void YM2413::set_chip_type(OPLLType type)
    {
	if (chip_type != type)
	{
	    chip_type = type;
	}
    }

    void YM2413::reset()
    {
	init_tables();
	channel_mask = 0;
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
		oper.env_output = 127;
		oper.env_state = opll_oper_state::Off;
	    }
	}

	switch (chip_type)
	{
	    case YM2413_Chip: inst_patch = ym2413_instruments; break;
	    case VRC7_Chip: inst_patch = vrc7_instruments; break;
	    case YM2423_Chip: inst_patch = ym2423_instruments; break;
	    case YMF281_Chip: inst_patch = ymf281_instruments; break;
	    default: inst_patch = ym2413_instruments; break;
	}
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
	env_clock += 1;
	clock_ampm();

	if (!is_vrc7())
	{
	    clock_short_noise();
	}

	for (auto &channel : channels)
	{
	    clock_phase(channel);
	    clock_envelope(channel);
	}

	for (int i = 0; i < 6; i++)
	{
	    if (!testbit(channel_mask, i))
	    {
		channel_output(channels[i]);
	    }
	}

	// VRC7 has no rhythm channels, compared to OPLL
	if (is_vrc7())
	{
	    return;
	}

	if (!is_rhythm_enabled)
	{
	    if (!testbit(channel_mask, 6))
	    {
		channel_output(channels[6]);
	    }
	}
	else
	{
	    if (!testbit(channel_mask, 13))
	    {
		channel_output(channels[6]);
		channels[6].output = (channels[6].output * 2);
	    }
	}

	clock_noise(14);

	if (!is_rhythm_enabled)
	{
	    if (!testbit(channel_mask, 7))
	    {
		channel_output(channels[7]);
	    }
	}
	else
	{
	    if (!testbit(channel_mask, 9))
	    {
		hihat_output();
	    }

	    if (!testbit(channel_mask, 12))
	    {
		snare_output();
	    }

	    int32_t ch7_output = (channels[7].opers[0].rhythm_output + channels[7].opers[1].rhythm_output);
	    ch7_output = clamp(ch7_output, -257, 256);
	    channels[7].output = (ch7_output * 2);
	}

	clock_noise(2);

	if (!is_rhythm_enabled)
	{
	    if (!testbit(channel_mask, 8))
	    {
		channel_output(channels[8]);
	    }
	}
	else
	{
	    if (!testbit(channel_mask, 11))
	    {
		tom_output();
	    }

	    if (!testbit(channel_mask, 10))
	    {
		cym_output();
	    }

	    int32_t ch8_output = (channels[8].opers[0].rhythm_output + channels[8].opers[1].rhythm_output);
	    ch8_output = clamp(ch8_output, -257, 256);
	    channels[8].output = (ch8_output * 2);
	}

	clock_noise(2);
    }

    vector<int32_t> YM2413::get_samples()
    {
	int32_t output = 0;

	for (auto &channel : channels)
	{
	    output += channel.output;
	}

	int32_t sample = ((output * 128) / 9);

	vector<int32_t> final_samples;
	final_samples.push_back(sample);
	return final_samples;
    }

    uint32_t YM2413::set_mask(uint32_t mask)
    {
	uint32_t ret = channel_mask;
	channel_mask = mask;
	return ret;
    }

    uint32_t YM2413::toggle_mask(uint32_t mask)
    {
	uint32_t ret = channel_mask;
	channel_mask ^= mask;
	return ret;
    }
}