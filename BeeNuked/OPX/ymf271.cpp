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

// BeeNuked-YMF271 (WIP)
// Chip Name: YMF271 (OPX)
// Chip Used In:
// Seibu SPI System and Jaleco Mega System 32
//
// Interesting Trivia:
//
// This chip is unique in that even MAME's own implementation isn't accurate enough compared to real hardware.
// However, the ymfm project has their eyes on trying to implement this chip.
//
// BueniaDev's Notes:
//
// This core is derived from MAME's implementation of this chip, which, as described above, is still far from perfect.
// As such, expect this implementation (and MAME's) to be updated as new information becomes available about this chip.

#include <ymf271.h>
using namespace std;

namespace beenuked
{
    YMF271::YMF271()
    {

    }

    YMF271::~YMF271()
    {

    }

    void YMF271::init_tables()
    {
	for (int i = 0; i < 128; i++)
	{
	    double db = 0.75f * double(i);
	    total_level_table[i] = int(65536.f / pow(10.f, (db / 20.f)));
	}

	for (int i = 0; i < 16; i++)
	{
	    attenutation_table[i] = int(65536.f / pow(10.f, (channel_att_table[i] / 20.0)));
	}

	for (int i = 0; i < 8; i++)
	{
	    waveform_table[i].fill(0);
	}

	for (int i = 0; i < 1024; i++)
	{
	    double mod = sin(((i * 2) + 1) * M_PI / 1024);

	    waveform_table[0][i] = int16_t(mod * 32767);
	}
    }

    int64_t YMF271::calc_slot_volume(opx_slot &slot)
    {
	int64_t volume = 0;

	int64_t env_volume = 65536;
	volume = ((env_volume * total_level_table[slot.total_level]) >> 16);
	return volume;
    }

    void YMF271::write_fm(int bank, uint8_t reg, uint8_t data)
    {
	int group_num = fm_table[(reg & 0xF)];

	if (group_num == -1)
	{
	    return;
	}

	int reg_addr = (reg >> 4);

	bool is_sync_reg = false;

	switch (reg_addr)
	{
	    case 0:
	    case 9:
	    case 10:
	    case 12:
	    case 13:
	    case 14:
	    {
		is_sync_reg = true;
	    }
	    break;
	}

	bool is_sync_mode = false;

	switch (groups[group_num].sync)
	{
	    case 0:
	    {
		if (bank == 0)
		{
		    is_sync_mode = true;
		}
	    }
	    break;
	    case 1:
	    {
		if ((bank == 0) || (bank == 1))
		{
		    is_sync_mode = true;
		}
	    }
	    break;
	    case 2:
	    {
		cout << "3-slot + 1-slot mode" << endl;

		if (bank == 0)
		{
		    is_sync_mode = true;
		}
	    }
	    break;
	    default: break;
	}

	if (is_sync_mode && is_sync_reg)
	{
	    switch (groups[group_num].sync)
	    {
		// 4-slot mode
		case 0:
		{
		    for (int i = 0; i < 4; i++)
		    {
			int slot_num = ((12 * i) + group_num);
			write_fm_reg(slot_num, reg_addr, data);
		    }
		}
		break;
		// 2x 2-slot mode
		case 1:
		{
		    if (bank == 0)
		    {
			for (int i = 0; i < 2; i++)
			{
			    int slot_num = ((12 * (i * 2)) + group_num);
			    write_fm_reg(slot_num, reg_addr, data);
			}
		    }
		    else
		    {
			for (int i = 0; i < 2; i++)
			{
			    int slot_num = ((12 * ((i * 2) + 1)) + group_num);
			    write_fm_reg(slot_num, reg_addr, data);
			}
		    }
		}
		break;
		case 2:
		{
		    for (int i = 0; i < 3; i++)
		    {
			int slot_num = ((12 * i) + group_num);
			cout << "Writing value of " << hex << int(data) << " to YMF271 slot " << dec << slot_num << " three-slot register of " << hex << int(reg_addr) << endl;
			write_fm_reg(slot_num, reg_addr, data);
		    }
		}
		break;
	    }
	}
	else
	{
	    int slot_num = ((12 * bank) + group_num);
	    write_fm_reg(slot_num, reg_addr, data);
	}
    }

    void YMF271::key_on(opx_slot &slot)
    {
	slot.step = 0;
	slot.step_ptr = 0;
	slot.is_key_on = true;
	calculate_step(slot);
    }

    void YMF271::key_off(opx_slot &slot)
    {
	if (slot.is_key_on)
	{
	    slot.is_key_on = false;
	}
    }

    void YMF271::calculate_step(opx_slot &slot)
    {
	double st = 0.f;

	if (slot.waveform == 7)
	{
	    st = double(2 * (slot.freq_num | 2048)) * pow_table[slot.block] * fs_frequency[slot.fs];
	    st = st * multiple_table[slot.multiply];

	    st /= double(524288 / 65536);

	    slot.step = uint32_t(st);
	}
	else
	{
	    st = double(2 * slot.freq_num) * pow_table[slot.block];
	    st = st * multiple_table[slot.multiply] * 1024;

	    st /= double(536870912 / 65536);

	    slot.step = uint32_t(st);
	}
    }

    void YMF271::write_fm_reg(int slot_num, int reg, uint8_t data)
    {
	auto &slot = slots[slot_num];

	switch (reg)
	{
	    case 0x0:
	    {
		slot.ext_enable = testbit(data, 7);
		slot.ext_out = ((data >> 3) & 0xF);

		if (testbit(data, 0))
		{
		    key_on(slot);
		}
		else
		{
		    key_off(slot);
		}
	    }
	    break;
	    case 0x1:
	    {
		cout << "Setting lfo-frequency register of slot " << dec << int(slot_num) << endl;
	    }
	    break;
	    case 0x2:
	    {
		cout << "Setting lfo wave/PMS/AMS register of slot " << dec << int(slot_num) << endl;
	    }
	    break;
	    case 0x3:
	    {
		cout << "Setting detune register of slot " << dec << int(slot_num) << endl;
		slot.multiply = (data & 0xF);
	    }
	    break;
	    case 0x4:
	    {
		slot.total_level = (data & 0x7F);
	    }
	    break;
	    case 0x5:
	    {
		cout << "Setting attack rate/keyscale register of slot " << dec << int(slot_num) << endl;
	    }
	    break;
	    case 0x6:
	    {
		cout << "Setting decay 1 rate register of slot " << dec << int(slot_num) << endl;
	    }
	    break;
	    case 0x7:
	    {
		cout << "Setting decay 2 rate register of slot " << dec << int(slot_num) << endl;
	    }
	    break;
	    case 0x8:
	    {
		cout << "Setting release rate/decay 1 level register of slot " << dec << int(slot_num) << endl;
	    }
	    break;
	    case 0x9:
	    {
		slot.freq_num = (((slot.freq_hi & 0xF) << 8) | data);
		slot.block = (slot.freq_hi >> 4);
	    }
	    break;
	    case 0xA:
	    {
		slot.freq_hi = data;
	    }
	    break;
	    case 0xB:
	    {
		cout << "Setting feedback/accumulator-on register of slot " << dec << int(slot_num) << endl;
		slot.waveform = (data & 0x7);
	    }
	    break;
	    case 0xC:
	    {
		cout << "Setting algorithm register of slot " << dec << int(slot_num) << endl;
		slot.algorithm = (data & 0xF);
	    }
	    break;
	    case 0xD:
	    {
		slot.ch_level[0] = (data >> 4);
		slot.ch_level[1] = (data & 0xF);
	    }
	    break;
	    case 0xE:
	    {
		slot.ch_level[2] = (data >> 4);
		slot.ch_level[3] = (data & 0xF);
	    }
	    break;
	    default: break;
	}
    }

    void YMF271::write_pcm(uint8_t reg, uint8_t data)
    {
	int slot_num = pcm_table[(reg & 0xF)];

	if (slot_num == -1)
	{
	    return;
	}

	auto &slot = slots[slot_num];

	int reg_addr = (reg >> 4);

	switch (reg_addr)
	{
	    case 0x0:
	    {
		slot.start_address = ((slot.start_address & 0x7FFF00) | data);
	    }
	    break;
	    case 0x1:
	    {
		slot.start_address = ((slot.start_address & 0x7F00FF) | (data << 8));
	    }
	    break;
	    case 0x2:
	    {
		slot.start_address = ((slot.start_address & 0x00FFFF) | ((data & 0x7F) << 16));
		slot.is_alt_loop = testbit(data, 7);

		// A/L bit is unimplemented (even in MAME's own implementation)
		if (slot.is_alt_loop)
		{
		    cout << "Undocumented A/L bit detected. Please contact BueniaDev." << endl;
		}
	    }
	    break;
	    case 0x3:
	    {
		slot.end_address = ((slot.end_address & 0x7FFF00) | data);
	    }
	    break;
	    case 0x4:
	    {
		slot.end_address = ((slot.end_address & 0x7F00FF) | (data << 8));
	    }
	    break;
	    case 0x5:
	    {
		slot.end_address = ((slot.end_address & 0x00FFFF) | ((data & 0x7F) << 16));
	    }
	    break;
	    case 0x6:
	    {
		slot.loop_address = ((slot.loop_address & 0x7FFF00) | data);
	    }
	    break;
	    case 0x7:
	    {
		slot.loop_address = ((slot.loop_address & 0x7F00FF) | (data << 8));
	    }
	    break;
	    case 0x8:
	    {
		slot.loop_address = ((slot.loop_address & 0x00FFFF) | ((data & 0x7F) << 16));
	    }
	    break;
	    case 0x9:
	    {
		slot.fs = (data & 0x3);
		slot.is_12_bit = testbit(data, 2);
		slot.srcnote = ((data >> 3) & 0x3);
		slot.srcb = ((data >> 5) & 0x7);
	    }
	    break;
	}
    }

    void YMF271::write_timer(uint8_t reg, uint8_t data)
    {
	if ((reg & 0xF0) == 0)
	{
	    int group_num = fm_table[(reg & 0xF)];

	    if (group_num == -1)
	    {
		return;
	    }

	    auto &group = groups[group_num];
	    group.sync = (data & 0x3);
	    group.is_pfm = testbit(data, 7);
	}
	else
	{
	    switch (reg)
	    {
		case 0x10:
		{
		    cout << "Writing to upper 8-bits of YMF271 timer A" << endl;
		}
		break;
		case 0x11:
		{
		    cout << "Writing to lower 2-bits of YMF271 timer A" << endl;
		}
		break;
		case 0x12:
		{
		    cout << "Writing to YMF271 timer B" << endl;
		}
		break;
		case 0x13:
		{
		    cout << "Writing to YMF271 timer control register" << endl;
		}
		break;
		case 0x14:
		{
		    cout << "Writing to YMF271 lower 8-bits of external address" << endl;
		}
		break;
		case 0x15:
		{
		    cout << "Writing to YMF271 middle 8-bits of external address" << endl;
		}
		break;
		case 0x16:
		{
		    cout << "Writing to YMF271 upper 7-bits/RW-bit of external address" << endl;
		}
		break;
		case 0x17:
		{
		    cout << "Writing to YMF271 external address control register" << endl;
		}
		break;
		case 0x20:
		case 0x21:
		case 0x22: break; // Test registers
		default: break;
	    }
	}
    }

    uint8_t YMF271::readROM(uint32_t addr)
    {
	addr &= 0x7FFFFF;
	return opx_rom.at(addr);
    }

    void YMF271::update_pcm(opx_group &group, opx_slot &slot)
    {
	if (!slot.is_key_on)
	{
	    return;
	}

	if (slot.waveform != 7)
	{
	    cout << "Error: Waveform " << dec << int(slot.waveform) << " in PCM update" << endl;
	    exit(1);
	}

	if ((slot.step_ptr >> 16) > slot.end_address)
	{
	    slot.step_ptr = slot.step_ptr - ((uint64_t)slot.end_address << 16) + ((uint64_t)slot.loop_address << 16);

	    if ((slot.step_ptr >> 16) > slot.end_address)
	    {
		slot.step_ptr &= 0xFFFF;
		slot.step_ptr |= ((uint64_t)slot.loop_address << 16);

		if ((slot.step_ptr >> 16) > slot.end_address)
		{
		    slot.step_ptr &= 0xFFFF;
		    slot.step_ptr |= ((uint64_t)slot.end_address << 16);
		}
	    }
	}
	
	int16_t sample = 0;

	if (!slot.is_12_bit)
	{
	    sample = (readROM(slot.start_address + (slot.step_ptr >> 16)) << 8);
	}
	else
	{
	    sample = 0;
	}

	int64_t final_volume = calc_slot_volume(slot);

	for (int i = 0; i < 4; i++)
	{
	    int ch_vol = ((final_volume * attenutation_table[slot.ch_level[i]]) >> 16);
	    ch_vol = min(ch_vol, 65536);
	    group.outputs[i] += ((sample * ch_vol) >> 16);
	}

	slot.step_ptr += slot.step;
    }

    void YMF271::update_fm_2op(opx_group &group, opx_slot &slot1, opx_slot &slot3)
    {
	int algorithm = (slot1.algorithm & 0x3);

	int combo = algorithm_2op_combinations[algorithm];

	array<int64_t, 2> opout;
	opout[0] = 0;
	opout[1] = calculate_op(slot1, 0);

	int64_t input = opout[((combo >> 1) & 0x1)];

	int64_t phase_mod = ((input << 8) * 16);

	int64_t phase_out = calculate_op(slot3, phase_mod);

	for (int i = 0; i < 4; i++)
	{
	    int64_t output3 = (phase_out * attenutation_table[slot3.ch_level[i]]);
	    group.outputs[i] += ((output3) >> 16);
	}
    }

    int64_t YMF271::calculate_op(opx_slot &slot, int64_t input)
    {
	int64_t slot_output = 0;

	int64_t env = calc_slot_volume(slot);

	auto step_ptr = (((slot.step_ptr + input) >> 16) & 0x3FF);

	slot_output = waveform_table[slot.waveform][step_ptr];
	slot_output = ((slot_output * env) >> 16);
	slot.step_ptr += slot.step;
	return slot_output;
    }

    uint32_t YMF271::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 384);
    }

    void YMF271::init()
    {
	reset();
    }

    void YMF271::reset()
    {
	init_tables();
	for (int i = 0; i < 48; i++)
	{
	    auto &slot = slots[i];
	    slot.number = i;
	    slot.start_address = 0;
	    slot.end_address = 0;
	    slot.loop_address = 0;
	    slot.step = 0;
	    slot.step_ptr = 0;
	    slot.is_key_on = false;
	}

	for (int i = 0; i < 12; i++)
	{
	    auto &group = groups[i];
	    group.outputs.fill(0);
	}
    }

    void YMF271::writeIO(int port, uint8_t data)
    {
	port &= 0xF;

	switch (port)
	{
	    case 0x0: chip_address[0] = data; break;
	    case 0x2: chip_address[1] = data; break;
	    case 0x4: chip_address[2] = data; break;
	    case 0x6: chip_address[3] = data; break;
	    case 0x8: chip_address[4] = data; break;
	    case 0xC: chip_address[5] = data; break;
	    case 0x1:
	    {
		write_fm(0, chip_address[0], data);
	    }
	    break;
	    case 0x3:
	    {
		write_fm(1, chip_address[1], data);
	    }
	    break;
	    case 0x5:
	    {
		write_fm(2, chip_address[2], data);
	    }
	    break;
	    case 0x7:
	    {
		write_fm(3, chip_address[3], data);
	    }
	    break;
	    case 0x9:
	    {
		write_pcm(chip_address[4], data);
	    }
	    break;
	    case 0xD:
	    {
		write_timer(chip_address[5], data);
	    }
	    break;
	}
    }

    void YMF271::writeROM(uint32_t rom_size, uint32_t data_start, uint32_t data_len, vector<uint8_t> rom_data)
    {
	opx_rom.resize(rom_size, 0xFF);

	uint32_t data_length = data_len;
	uint32_t data_end = (data_start + data_len);

	if (data_start > rom_size)
	{
	    return;
	}

	if (data_end > rom_size)
	{
	    data_length = (rom_size - data_start);
	}

	copy(rom_data.begin(), (rom_data.begin() + data_length), (opx_rom.begin() + data_start));
    }

    void YMF271::clockchip()
    {
	for (int i = 0; i < 12; i++)
	{
	    auto &slot_group = groups[i];
	    slot_group.outputs.fill(0);

	    if (slot_group.is_pfm && (slot_group.sync != 3))
	    {
		cout << "Unimplemented YMF271 PFM detected, contact BueniaDev" << endl;
	    }

	    switch (slot_group.sync)
	    {
		case 1:
		{
		    for (int j = 0; j < 2; j++)
		    {
			int slot1 = (i + (j * 12));
			int slot3 = (i + ((j + 2) * 12));
			update_fm_2op(slot_group, slots[slot1], slots[slot3]);
		    }
		}
		break;
		case 2:
		{
		    // TODO: Implement FM
		    auto &slot = slots[(i + 36)];
		    update_pcm(slot_group, slot);
		}
		break;
		case 3:
		{
		    for (int j = 0; j < 4; j++)
		    {
			int pcm_slot_num = (i + (j * 12));
			auto &slot = slots[pcm_slot_num];
			update_pcm(slot_group, slot);
		    }
		}
		break;
		default: break;
	    }
	}
    }

    vector<int32_t> YMF271::get_samples()
    {
	array<int32_t, 4> mixed_samples = {0, 0};

	for (int i = 0; i < 4; i++)
	{
	    for (int j = 0; j < 12; j++)
	    {
		int32_t old_sample = mixed_samples[i];
		int32_t new_sample = clamp((groups[j].outputs[i] >> 2), -32768, 32767);
		mixed_samples[i] = (old_sample + new_sample);
	    }
	}

	vector<int32_t> final_samples;
	final_samples.push_back(mixed_samples[0]);
	final_samples.push_back(mixed_samples[1]);
	final_samples.push_back(mixed_samples[2]);
	final_samples.push_back(mixed_samples[3]);
	return final_samples;
    }
}