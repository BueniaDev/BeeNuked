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

// BeeNuked-YMF262 (WIP)
// Chip Name: YMF262 (OPL3)
// Chip Used In:
// Sound Blaster 16 and 16-bit Pro AudioSpectrum cards
//
// Interesting Trivia:
//
// TODO: Insert trivia here
//
// BueniaDev's Notes:
//
// TODO: Write up initial notes here

#include <ymf262.h>
using namespace std;

namespace beenuked
{
    YMF262::YMF262()
    {

    }

    YMF262::~YMF262()
    {

    }

    void YMF262::init_tables()
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

    uint32_t YMF262::fetch_sine_result(uint32_t phase, int wave_sel, bool &is_negate)
    {
	bool sign_bit = testbit(phase, 9);
	bool mirror_bit = testbit(phase, 8);
	uint8_t quarter_phase = (phase & 0xFF);

	uint32_t sine_result = 0;

	switch ((wave_sel & 0x7))
	{
	    case 0:
	    {
		if (mirror_bit)
		{
		    quarter_phase = ~quarter_phase;
		}

		sine_result = sine_table[quarter_phase];

		is_negate = sign_bit;
	    }
	    break;
	    case 1:
	    {
		if (mirror_bit)
		{
		    quarter_phase = ~quarter_phase;
		}

		sine_result = sine_table[quarter_phase];

		if (sign_bit)
		{
		    sine_result = 0xFFF;
		}

		is_negate = false;
	    }
	    break;
	    case 2:
	    {
		if (mirror_bit)
		{
		    quarter_phase = ~quarter_phase;
		}

		sine_result = sine_table[quarter_phase];

		is_negate = false;
	    }
	    break;
	    case 3:
	    {
		sine_result = sine_table[quarter_phase];

		if (mirror_bit)
		{
		    sine_result = 0xFFF;
		}

		is_negate = false;
	    }
	    break;
	    case 4:
	    {
		is_negate = false;

		if (!sign_bit && mirror_bit)
		{
		    is_negate = true;
		}

		if (testbit(phase, 7))
		{
		    quarter_phase = ~quarter_phase;
		}

		uint8_t phase_output = ((quarter_phase & 0x7F) << 1);
		sine_result = sine_table[phase_output];

		if (sign_bit)
		{
		    sine_result = 0xFFF;
		}
	    }
	    break;
	    case 5:
	    {
		is_negate = false;

		if (testbit(phase, 7))
		{
		    quarter_phase = ~quarter_phase;
		}

		uint8_t phase_output = ((quarter_phase & 0x7F) << 1);
		sine_result = sine_table[phase_output];

		if (sign_bit)
		{
		    sine_result = 0xFFF;
		}
	    }
	    break;
	    case 6:
	    {
		sine_result = 0;
		is_negate = sign_bit;
	    }
	    break;
	    case 7:
	    {
		is_negate = sign_bit;

		uint32_t phase_output = (phase & 0x1FF);

		if (sign_bit)
		{
		    phase_output ^= 0x1FF;
		}

		sine_result = (phase_output << 3);
	    }
	    break;
	    default:
	    {
		sine_result = 0;
	    }
	    break;
	}

	return sine_result;
    }

    int32_t YMF262::calc_output(int32_t phase, int32_t mod, uint32_t env, int wave_sel)
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

    void YMF262::write_port0(uint8_t reg, uint8_t data)
    {
	write_fmreg(false, reg, data);
    }

    void YMF262::write_port1(uint8_t reg, uint8_t data)
    {
	if (is_opl3_mode)
	{
	    switch (reg)
	    {
		case 0x01: break; // Test register
		case 0x04:
		{
		    for (int i = 0; i < 6; i++)
		    {
			int ch_num = (i < 3) ? i : ((i - 3) + 9);
			auto &master_channel = channels[ch_num];
			auto &minion_channel = channels[(ch_num + 3)];

			bool is_extended = testbit(data, i);

			master_channel.is_extended = is_extended;
			minion_channel.is_minion_channel = is_extended;

			if (testbit(data, i))
			{
			    cout << "Four-op mode enabled for channel pair " << dec << int(ch_num) << " - " << dec << int(ch_num + 3) << endl;
			}
			else
			{
			    cout << "Four-op mode disabled for channel pair " << dec << int(ch_num) << " - " << dec << int(ch_num + 3) << endl;
			}
		    }
		}
		break;
		case 0x05: write_opl3_select(data); break;
		default: write_fmreg(true, reg, data); break;
	    }
	}
	else
	{
	    switch (reg)
	    {
		case 0x05: write_opl3_select(data); break;
		default: write_fmreg(false, reg, data); break;
	    }
	}
    }

    int YMF262::get_channel_state(int ch_num)
    {
	if (is_opl3_mode)
	{
	    auto &channel = channels[ch_num];

	    switch (ch_num)
	    {
		case 0:
		case 1:
		case 2:
		case 9:
		case 10:
		case 11: return (channel.is_extended) ? 2 : 1; break;
		case 3:
		case 4:
		case 5:
		case 12:
		case 13:
		case 14: return (channel.is_minion_channel) ? 0 : 1; break;
		default: return 1; break;
	    }
	}
	else
	{
	    return 1;
	}
    }

    int YMF262::get_oper_state(int oper_num)
    {
	return get_channel_state(oper_num / 2);
    }

    void YMF262::update_frequency(opl3_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.freq_num = channel.freq_num;
	    oper.block = channel.block;
	    update_frequency(oper);
	}
    }

    void YMF262::update_frequency(opl3_operator &oper)
    {
	update_phase(oper);
	update_total_level(oper);
    }

    void YMF262::update_phase(opl3_operator &oper)
    {
	uint32_t phase_result = oper.freq_num;
	int multiply = multiply_table[oper.multiply];
	oper.phase_freq = (((phase_result * multiply) << oper.block) >> 1);
    }

    // Total level calculation (KSL calculation derived from Nuked-OPL3)
    void YMF262::update_total_level(opl3_operator &oper)
    {
	int temp_ksl = ((16 * oper.block - ksl_table[(oper.freq_num >> 6)]) << 1);
	array<int, 4> kslx_table = {3, 1, 2, 0};
	int ksl_val = (oper.ksl == 0) ? 0 : (max(0, temp_ksl) >> kslx_table[oper.ksl]);
	oper.tll_val = ((oper.total_level << 2) + ksl_val);
    }

    void YMF262::clock_phase(opl3_channel &channel)
    {
	for (auto &oper : channel.opers)
	{
	    oper.phase_counter = ((oper.phase_counter + oper.phase_freq) & 0xFFFFF);
	    oper.phase_output = (oper.phase_counter >> 10);
	}
    }

    void YMF262::channel_output(opl3_channel &channel)
    {
	if (channel.is_minion_channel)
	{
	    return;
	}

	if (channel.is_extended)
	{
	    // TODO: Four-operator output
	    return;
	}
	else
	{
	    output_2op(channel);
	}
    }

    void YMF262::output_2op(opl3_channel &channel)
    {
	auto &mod_slot = channel.opers[0];
	auto &car_slot = channel.opers[1];

	uint32_t mod_env = mod_slot.tll_val;
	uint32_t car_env = car_slot.tll_val;

	mod_slot.outputs[1] = mod_slot.outputs[0];
	mod_slot.outputs[0] = calc_output(mod_slot.phase_output, 0, mod_env, mod_slot.wave_sel);

	if (channel.is_algorithm)
	{
	    int32_t car_output = calc_output(car_slot.phase_output, 0, car_env, car_slot.wave_sel);
	    int32_t ch_output = (mod_slot.outputs[0] + car_output);
	    channel.output = clamp(ch_output, -32768, 32767);
	}
	else
	{
	    int32_t phase_mod = ((mod_slot.outputs[0] >> 1) & 0x3FF);
	    channel.output = calc_output(car_slot.phase_output, phase_mod, car_env, car_slot.wave_sel);
	}
    }

    void YMF262::write_fmreg(bool is_port1, uint8_t reg, uint8_t data)
    {
	int reg_group = (reg & 0xF0);
	int reg_addr = (reg & 0x1F);

	int ch_offs = (is_port1) ? 9 : 0;

	switch (reg_group)
	{
	    case 0x00:
	    {
		switch (reg_addr)
		{
		    case 0x01: break;
		    case 0x02:
		    {
			cout << "Setting timer 1 to value of " << dec << int(data) << endl;
		    }
		    break;
		    case 0x03:
		    {
			cout << "Setting timer 2 to value of " << dec << int(data) << endl;
		    }
		    break;
		    case 0x04:
		    {
			if (testbit(data, 7))
			{
			    cout << "Clearing IRQ flag" << endl;
			}
			else
			{
			    cout << "Setting IRQ mask/enabling timers" << endl;
			}
		    }
		    break;
		    case 0x08:
		    {
			cout << "Setting note select register to value of " << hex << int(data) << endl;
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


		int op_num = (slot_num + (ch_offs * 2));
		int state = get_oper_state(op_num);

		switch (state)
		{
		    case 0:
		    {
			cout << "Setting AM/VIB/KSR/EG/MUL register of four-op channel lower operator of " << dec << int(op_num) << endl;
		    }
		    break;
		    case 1:
		    {
			cout << "Setting AM/VIB/KSR/EG register of two-op channel operator of " << dec << int(op_num) << endl;
			int ch_num = (op_num / 2);
			int oper_num = (op_num % 2);
			auto &ch_oper = channels[ch_num].opers[oper_num];

			ch_oper.multiply = (data & 0xF);
			update_phase(ch_oper);
		    }
		    break;
		    case 2:
		    {
			cout << "Setting AM/VIB/KSR/EG/MUL register of four-op channel upper operator of " << dec << int(op_num) << endl;
		    }
		    break;
		}
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

		int op_num = (slot_num + (ch_offs * 2));
		int state = get_oper_state(op_num);

		switch (state)
		{
		    case 0:
		    {
			cout << "Setting KSL/TL register of four-op channel lower operator of " << dec << int(op_num) << endl;
		    }
		    break;
		    case 1:
		    {
			int ch_num = (op_num / 2);
			int oper_num = (op_num % 2);
			auto &ch_oper = channels[ch_num].opers[oper_num];

			ch_oper.ksl = ((data >> 6) & 0x3);
			ch_oper.total_level = (data & 0x3F);
			update_total_level(ch_oper);
		    }
		    break;
		    case 2:
		    {
			cout << "Setting KSL/TL register of four-op channel upper operator of " << dec << int(op_num) << endl;
		    }
		    break;
		}
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

		int op_num = (slot_num + (ch_offs * 2));
		int state = get_oper_state(op_num);

		switch (state)
		{
		    case 0:
		    {
			cout << "Setting AR/DR register of four-op channel lower operator of " << dec << int(op_num) << endl;
		    }
		    break;
		    case 1:
		    {
			cout << "Setting AR/DR register of two-op channel operator of " << dec << int(op_num) << endl;
		    }
		    break;
		    case 2:
		    {
			cout << "Setting AR/DR register of four-op channel upper operator of " << dec << int(op_num) << endl;
		    }
		    break;
		}
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

		int op_num = (slot_num + (ch_offs * 2));
		int state = get_oper_state(op_num);

		switch (state)
		{
		    case 0:
		    {
			cout << "Setting SL/RR register of four-op channel lower operator of " << dec << int(op_num) << endl;
		    }
		    break;
		    case 1:
		    {
			cout << "Setting SL/RR register of two-op channel operator of " << dec << int(op_num) << endl;
		    }
		    break;
		    case 2:
		    {
			cout << "Setting SL/RR register of four-op channel upper operator of " << dec << int(op_num) << endl;
		    }
		    break;
		}
	    }
	    break;
	    case 0xA0:
	    {
		int ch_num = (reg & 0xF);

		if (ch_num > 8)
		{
		    return;
		}

		int channel_num = (ch_num + ch_offs);
		int ch_state = get_channel_state(channel_num);

		switch (ch_state)
		{
		    case 1:
		    {
			auto &channel = channels[channel_num];
			channel.freq_num = ((channel.freq_num & 0x300) | data);
			update_frequency(channel);
		    }
		    break;
		    case 2:
		    {
			cout << "Setting FNUM LSB of four-op channel " << dec << int(channel_num) << endl;
		    }
		    break;
		    default: break;
		}
	    }
	    break;
	    case 0xB0:
	    {
		if (reg == 0xBD)
		{
		    if (is_port1)
		    {
			return;
		    }

		    cout << "Writing to rhythm register" << endl;
		    return;
		}

		int ch_num = (reg & 0xF);

		if (ch_num > 8)
		{
		    return;
		}

		int channel_num = (ch_num + ch_offs);
		int ch_state = get_channel_state(channel_num);

		switch (ch_state)
		{
		    case 1:
		    {
			cout << "Setting key-on register of two-op channel " << dec << int(channel_num) << endl;
			auto &channel = channels[channel_num];
			channel.freq_num = ((channel.freq_num & 0xFF) | ((data & 0x3) << 8));
			channel.block = ((data >> 2) & 0x7);
			update_frequency(channel);
		    }
		    break;
		    case 2:
		    {
			cout << "Setting key-on/FNUM MSB/block register of four-op channel " << dec << int(channel_num) << endl;
		    }
		    break;
		    default: break;
		}
	    }
	    break;
	    case 0xC0:
	    {
		int ch_num = (reg & 0xF);

		if (ch_num > 8)
		{
		    return;
		}

		int channel_num = (ch_num + ch_offs);
		int ch_state = get_channel_state(channel_num);

		switch (ch_state)
		{
		    case 1:
		    {
			cout << "Setting feedback register of two-op channel " << dec << int(channel_num) << endl;
			auto &channel = channels[channel_num];

			if (is_opl3_mode)
			{
			    // In OPL3 mode, output can be enabled/disabled
			    for (int i = 0; i < 4; i++)
			    {
				channel.is_output[i] = testbit(data, (4 + i));
			    }
			}
			else
			{
			    // In OPL2 mode, output is always enabled
			    channel.is_output[0] = true;
			    channel.is_output[1] = true;
			}

			channel.is_algorithm = testbit(data, 0);
		    }
		    break;
		    case 2:
		    {
			cout << "Setting output/feedback/algorithm register of four-op channel " << dec << int(channel_num) << endl;
		    }
		    break;
		    default: break;
		}
	    }
	    break;
	    case 0xE0:
	    case 0xF0:
	    {
		int slot_num = slot_array[reg_addr];

		if (slot_num < 0)
		{
		    return;
		}

		int op_num = (slot_num + (ch_offs * 2));
		int state = get_oper_state(op_num);

		switch (state)
		{
		    case 0:
		    {
			cout << "Setting waveform select register of four-op channel lower operator of " << dec << int(op_num) << endl;
		    }
		    break;
		    case 1:
		    {
			int ch_num = (op_num / 2);
			int oper_num = (op_num % 2);
			auto &ch_oper = channels[ch_num].opers[oper_num];

			ch_oper.wave_sel = (data & 0x7);

			if (!is_opl3_mode)
			{
			    ch_oper.wave_sel &= 3;
			}
		    }
		    break;
		    case 2:
		    {
			cout << "Setting waveform select register of four-op channel upper operator of " << dec << int(op_num) << endl;
		    }
		    break;
		}
	    }
	    break;
	}
    }

    void YMF262::write_opl3_select(uint8_t data)
    {
	is_opl3_mode = testbit(data, 0);
    }

    uint32_t YMF262::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 288);
    }

    void YMF262::init()
    {
	init_tables();
	reset();
    }

    void YMF262::reset()
    {
	is_opl3_mode = false;

	for (int i = 0; i < 18; i++)
	{
	    channels[i].number = i;
	}
    }

    void YMF262::writeIO(int port, uint8_t data)
    {
	switch ((port & 3))
	{
	    case 0:
	    {
		chip_address = data;
	    }
	    break;
	    case 1:
	    {
		write_port0(chip_address, data);
	    }
	    break;
	    case 2:
	    {
		chip_address = data;
	    }
	    break;
	    case 3:
	    {
		write_port1(chip_address, data);
	    }
	    break;
	}
    }

    void YMF262::clockchip()
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

    vector<int32_t> YMF262::get_samples()
    {
	array<int32_t, 4> output = {0, 0, 0, 0};

	for (int i = 0; i < 4; i++)
	{
	    for (int j = 0; j < 4; j++)
	    {
		output[j] += (channels[i].is_output[j]) ? channels[i].output : 0;
	    }
	}

	// YMF262 has 4 total outputs
	vector<int32_t> final_samples;
	final_samples.push_back(output[0]);
	final_samples.push_back(output[1]);
	final_samples.push_back(output[2]);
	final_samples.push_back(output[3]);
	return final_samples;
    }
};