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

// BeeNuked-YM2610 (WIP)
// Chip Name: YM2610 (OPNB) / YM2610B (OPNBB)
// Chip Used In: Various Taito arcade machines and most famously, the Neo Geo
//
// Interesting Trivia:
//
// In the Neo Geo's early days, FM and PCM were used about equally for the music, resulting in the Neo Geo's
// early titles sounding like a really cool-sounding duet between a Mega Drive and a Super Famicom.
// However, FM ended up getting marginalized in later titles that tried to compete with CD-ROM games,
// which some felt robbed the Neo Geo of its audial magic.
//
//
// BueniaDev's Notes:
//
// Although quite a few features of this chip are implemented here, the FM channels are completely unimplemented, and so are quite a few other features.
//
// However, work is being done to improve this core, so don't lose hope here!

#include "ym2610.h"
using namespace beenuked;

namespace beenuked
{
    YM2610::YM2610()
    {
	fm_samples_per_output = 1;
	reset();
    }

    YM2610::~YM2610()
    {

    }

    void YM2610::add_last(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale)
    {
	sum0 += (last_ssg_samples[0] * scale);
	sum1 += (last_ssg_samples[1] * scale);
	sum2 += (last_ssg_samples[2] * scale);
    }

    void YM2610::clock_and_add(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale)
    {
	if (inter != NULL)
	{
	    inter->clockSSG();
	    last_ssg_samples = inter->getSSGSamples();
	}

	add_last(sum0, sum1, sum2, scale);
    }

    void YM2610::write_to_output(int32_t sum0, int32_t sum1, int32_t sum2, int divisor)
    {
	ssg_sample = ((sum0 + sum1 + sum2) * 2 / (3 * divisor));
	ssg_sample_index += 1;
    }

    void YM2610::resample()
    {
	int32_t sum0 = 0;
	int32_t sum1 = 0;
	int32_t sum2 = 0;

	if (testbit(ssg_sample_index, 0))
	{
	    add_last(sum0, sum1, sum2, 1);
	}

	clock_and_add(sum0, sum1, sum2, 2);
	clock_and_add(sum0, sum1, sum2, 2);
	clock_and_add(sum0, sum1, sum2, 2);
	clock_and_add(sum0, sum1, sum2, 2);

	if (!testbit(ssg_sample_index, 0))
	{
	    clock_and_add(sum0, sum1, sum2, 1);
	}

	write_to_output(sum0, sum1, sum2, 9);
    }

    void YM2610::write_port0(uint8_t reg, uint8_t data)
    {
	switch ((reg & 0xF0))
	{
	    case 0x00:
	    {
		if (reg < 0x0E)
		{
		    if (inter != NULL)
		    {
			inter->writeSSG(1, data);
		    }
		}
	    }
	    break;
	    case 0x10: write_delta_t(reg, data); break;
	    case 0x20:
	    {
		write_mode(reg, data);
	    }
	    break;
	    default:
	    {
		write_fmreg(false, reg, data);
	    }
	    break;
	}
    }

    void YM2610::write_port1(uint8_t reg, uint8_t data)
    {
	if (reg < 0x30)
	{
	    write_adpcm(reg, data);
	}
	else
	{
	    write_fmreg(true, reg, data);
	}
    }

    void YM2610::write_mode(uint8_t reg, uint8_t data)
    {
	switch (reg)
	{
	    case 0x21: break;
	    case 0x22:
	    {
		if (testbit(data, 3))
		{
		    cout << "LFO enabled" << endl;
		}
		else
		{
		    cout << "LFO disabled" << endl;
		}
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
		cout << "Channel 3 mode: " << dec << ch3_mode << endl;

		if (testbit(data, 4))
		{
		    reset_status_bit(0);
		}

		if (testbit(data, 5))
		{
		    reset_status_bit(1);
		}

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
	    }
	    break;
	    case 0x28:
	    {
		int ch_num = (data & 0x3);

		if (ch_num == 3)
		{
		    return;
		}

		if (testbit(data, 2))
		{
		    ch_num += 4;
		}

		cout << "Key-on/off for channel number " << dec << ch_num << endl;
	    }
	    break;
	}
    }

    void YM2610::write_adpcm(uint8_t reg, uint8_t data)
    {
	switch (reg)
	{
	    case 0x00:
	    {
		if (!testbit(data, 7))
		{
		    for (int i = 0; i < 6; i++)
		    {
			if (testbit(data, i))
			{
			    adpcm_key_on(adpcm_channels[i]);
			}
		    }
		}
		else
		{
		    for (int i = 0; i < 6; i++)
		    {
			if (testbit(data, i))
			{
			    adpcm_key_off(adpcm_channels[i]);
			}
		    }
		}
	    }
	    break;
	    case 0x01:
	    {
		adpcm_tl_val = (data & 0x3F);
	    }
	    break;
	    case 0x02: break;
	    default:
	    {
		int ch_num = (reg & 0x7);

		if (ch_num >= 6)
		{
		    return;
		}

		auto &channel = adpcm_channels[ch_num];

		switch ((reg & 0x38))
		{
		    case 0x08:
		    {
			channel.is_pan_left = testbit(data, 7);
			channel.is_pan_right = testbit(data, 6);
			channel.ch_level = (data & 0x1F);
		    }
		    break;
		    case 0x10:
		    {
			channel.start_addr = ((channel.start_addr & 0xFF00) | data);
		    }
		    break;
		    case 0x18:
		    {
			channel.start_addr = ((channel.start_addr & 0x00FF) | (data << 8));
		    }
		    break;
		    case 0x20:
		    {
			channel.end_addr = ((channel.end_addr & 0xFF00) | data);
		    }
		    break;
		    case 0x28:
		    {
			channel.end_addr = ((channel.end_addr & 0x00FF) | (data << 8));
		    }
		    break;
		    default: break;
		}
	    }
	    break;
	}
    }

    uint8_t YM2610::fetch_adpcm_rom(uint32_t address)
    {
	if (inter == NULL)
	{
	    return 0;
	}

	return inter->readMemory(BeeNukedAccessType::ADPCM, address);
    }

    uint8_t YM2610::fetch_delta_t_rom(uint32_t address)
    {
	if (inter == NULL)
	{
	    return 0;
	}

	return inter->readMemory(BeeNukedAccessType::DeltaT, address);
    }

    void YM2610::clock_adpcm_channel(opnb_adpcm &channel)
    {
	if (!channel.is_keyon)
	{
	    channel.adpcm_accum = 0;
	    return;
	}

	uint8_t data = 0;

	if (!channel.is_high_nibble)
	{
	    uint32_t end = ((channel.end_addr + 1) << 8);

	    if (((channel.current_addr ^ end) & 0xFFFFF) == 0)
	    {
		channel.is_keyon = false;
		channel.adpcm_accum = 0;
		return;
	    }

	    channel.current_byte = fetch_adpcm_rom(channel.current_addr++);
	    data = (channel.current_byte >> 4);
	}
	else
	{
	    data = (channel.current_byte & 0xF);
	}

	channel.is_high_nibble = !channel.is_high_nibble;

	int32_t delta = (2 * (data & 0x7) + 1) * adpcm_steps[channel.adpcm_step] / 8;

	if (testbit(data, 3))
	{
	    delta = -delta;
	}

	channel.adpcm_accum = ((channel.adpcm_accum + delta) & 0xFFF);

	int8_t step_inc = adpcm_steps_inc[(data & 0x7)];
	channel.adpcm_step = clamp((channel.adpcm_step + step_inc), 0, 48);
    }

    void YM2610::adpcm_channel_output(opnb_adpcm &channel)
    {
	int ch_volume = ((channel.ch_level ^ 0x1F) + (adpcm_tl_val ^ 0x3F));

	if (ch_volume >= 63)
	{
	    return;
	}

	int8_t mul = (15 - (ch_volume & 7));
	uint8_t shift = (1 + (ch_volume >> 3));

	channel.adpcm_output = (((int16_t(channel.adpcm_accum << 4) * mul) >> (4 + shift)) & ~3);
    }

    void YM2610::write_delta_t(uint8_t reg, uint8_t data)
    {
	switch (reg)
	{
	    case 0x10:
	    {
		delta_t_channel.is_repeat = testbit(data, 4);

		if (testbit(data, 7))
		{
		    delta_t_channel.current_addr = (delta_t_channel.start_addr << 8);
		    delta_t_channel.is_high_nibble = false;
		    delta_t_channel.current_byte = 0;
		    delta_t_channel.delta_t_pos = 0;
		    delta_t_channel.delta_t_accum = 0;
		    delta_t_channel.prev_accum = 0;
		    delta_t_channel.current_step = 127;
		    delta_t_channel.is_keyon = true;
		}
		else
		{
		    delta_t_channel.is_keyon = false;
		}

		if (testbit(data, 0))
		{
		    delta_t_channel.is_high_nibble = false;
		    delta_t_channel.current_byte = 0;
		    delta_t_channel.delta_t_pos = 0;
		    delta_t_channel.current_addr = 0;
		    delta_t_channel.delta_t_accum = 0;
		    delta_t_channel.prev_accum = 0;
		    delta_t_channel.current_step = 127;
		}
	    }
	    break;
	    case 0x11:
	    {
		delta_t_channel.is_pan_left = testbit(data, 7);
		delta_t_channel.is_pan_right = testbit(data, 6);
	    }
	    break;
	    case 0x12:
	    {
		delta_t_channel.start_addr = ((delta_t_channel.start_addr & 0xFF00) | data);
	    }
	    break;
	    case 0x13:
	    {
		delta_t_channel.start_addr = ((delta_t_channel.start_addr & 0xFF) | (data << 8));
	    }
	    break;
	    case 0x14:
	    {
		delta_t_channel.end_addr = ((delta_t_channel.end_addr & 0xFF00) | data);
	    }
	    break;
	    case 0x15:
	    {
		delta_t_channel.end_addr = ((delta_t_channel.end_addr & 0xFF) | (data << 8));
	    }
	    break;
	    case 0x19:
	    {
		delta_t_channel.delta_n = ((delta_t_channel.delta_n & 0xFF00) | data);
	    }
	    break;
	    case 0x1A:
	    {
		delta_t_channel.delta_n = ((delta_t_channel.delta_n & 0xFF) | (data << 8));
	    }
	    break;
	    case 0x1B:
	    {
		delta_t_channel.ch_volume = data;
	    }
	    break;
	    case 0x1C:
	    {
		cout << "Writing to Delta-T ADPCM flag control register" << endl;
	    }
	    break;
	}
    }

    array<int32_t, 2> YM2610::generate_adpcm_sample()
    {
	array<int32_t, 2> mixed_samples = {0, 0};

	for (auto &channel : adpcm_channels)
	{
	    mixed_samples[0] += (channel.is_pan_left) ? channel.adpcm_output : 0;
	    mixed_samples[1] += (channel.is_pan_right) ? channel.adpcm_output : 0;
	}

	mixed_samples[0] += (delta_t_channel.is_pan_left) ? delta_t_channel.delta_t_output : 0;
	mixed_samples[1] += (delta_t_channel.is_pan_right) ? delta_t_channel.delta_t_output : 0;

	return mixed_samples;
    }

    void YM2610::clock_adpcm()
    {
	for (auto &channel : adpcm_channels)
	{
	    clock_adpcm_channel(channel);
	}
    }

    void YM2610::clock_delta_t()
    {
	if (!delta_t_channel.is_keyon)
	{
	    return;
	}

	uint32_t position = (delta_t_channel.delta_t_pos + delta_t_channel.delta_n);
	delta_t_channel.delta_t_pos = (position & 0xFFFF);

	if (position < 0x10000)
	{
	    return;
	}

	if ((delta_t_channel.current_addr >> 8) > delta_t_channel.end_addr)
	{
	    if (delta_t_channel.is_repeat)
	    {
		delta_t_channel.current_addr = (delta_t_channel.start_addr << 8);
		delta_t_channel.is_high_nibble = false;
		delta_t_channel.current_byte = 0;
		delta_t_channel.delta_t_pos = 0;
		delta_t_channel.delta_t_accum = 0;
		delta_t_channel.prev_accum = 0;
		delta_t_channel.current_step = 127;
	    }
	    else
	    {
		delta_t_channel.delta_t_accum = 0;
		delta_t_channel.prev_accum = 0;
		return;
	    }
	}

	if (!delta_t_channel.is_high_nibble)
	{
	    delta_t_channel.current_byte = fetch_delta_t_rom(delta_t_channel.current_addr++);
	    delta_t_channel.current_addr &= 0xFFFFFF;
	}

	uint8_t data = (uint8_t(delta_t_channel.current_byte << (4 * delta_t_channel.is_high_nibble)) >> 4);
	delta_t_channel.is_high_nibble = !delta_t_channel.is_high_nibble;

	delta_t_channel.prev_accum = delta_t_channel.delta_t_accum;

	int32_t delta = (2 * (data & 0x7) + 1) * delta_t_channel.current_step / 8;

	if (testbit(data, 3))
	{
	    delta = -delta;
	}

	delta_t_channel.delta_t_accum = clamp((delta_t_channel.delta_t_accum + delta), -32768, 32767);

	uint8_t step_scale = delta_t_step_scale[(data & 0x7)];

	delta_t_channel.current_step = clamp(((delta_t_channel.current_step * step_scale) / 64), 127, 24576);
    }

    void YM2610::delta_t_output()
    {
	auto m_prev_accum = delta_t_channel.prev_accum;
	auto m_position = delta_t_channel.delta_t_pos;
	auto m_accum = delta_t_channel.delta_t_accum;

	int32_t result = ((m_prev_accum * int32_t((m_position ^ 0xFFFF) + 1) + m_accum * int32_t(m_position)) >> 16);

	result = ((result * int32_t(delta_t_channel.ch_volume)) >> 9);
	delta_t_channel.delta_t_output = result;
    }

    void YM2610::adpcm_output()
    {
	for (auto &channel : adpcm_channels)
	{
	    adpcm_channel_output(channel);
	}

	delta_t_output();
    }

    void YM2610::write_fmreg(bool is_port1, uint8_t reg, uint8_t data)
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

	switch (reg_group)
	{
	    case 0x30:
	    {
		cout << "Writing to detune/multiply register of channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0x40:
	    {
		cout << "Writing to total-level register of channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0x50:
	    {
		cout << "Writing to key-scaling/attack-rate register of channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0x60:
	    {
		cout << "Writing to lfo-enable/decay-rate register of channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0x70:
	    {
		cout << "Writing to sustain-rate register of channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0x80:
	    {
		cout << "Writing to sustain-level/release-rate register of channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0x90:
	    {
		cout << "Writing to SSG register of channel " << dec << ch_num << ", operator " << dec << oper_num << endl;
	    }
	    break;
	    case 0xA0:
	    {
		switch (oper_reg)
		{
		    case 0:
		    {
			cout << "Writing to frequency LSB register of channel " << dec << ch_num << endl;
		    }
		    break;
		    case 1:
		    {
			cout << "Writing to frequency MSB/block register of channel " << dec << ch_num << endl;
		    }
		    break;
		    case 2:
		    {
			if (!is_port1)
			{
			    cout << "Writing to frequency LSB register of channel 3, operator " << dec << ch_num << endl;
			}
		    }
		    break;
		    case 3:
		    {
			if (!is_port1)
			{
			    cout << "Writing to frequency MSB/block register of channel 3, operator " << dec << ch_num << endl;
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
			cout << "Writing to feedback/algorithm register of channel " << dec << ch_num << endl;
		    }
		    break;
		    case 1:
		    {
			cout << "Writing to panning/PMS/AMS register of channel " << dec << ch_num << endl;
		    }
		    break;
		}
	    }
	    break;
	}
    }

    void YM2610::set_status_bit(int bit)
    {
	opnb_status = setbit(opnb_status, bit);

	if (irq_handler)
	{
	    irq_handler(true);
	}
    }

    void YM2610::reset_status_bit(int bit)
    {
	opnb_status = resetbit(opnb_status, bit);

	if (irq_handler)
	{
	    irq_handler(false);
	}
    }

    void YM2610::clock_timers()
    {
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

    void YM2610::clock_fm_and_adpcm()
    {
	clock_timers();
	env_timer += 1;

	if (env_timer == 3)
	{
	    env_timer = 0;
	    env_clock += 1;
	    clock_adpcm();
	}

	clock_delta_t();
	adpcm_output();
    }

    void YM2610::output_fm_and_adpcm()
    {
	array<int32_t, 2> mixed_samples = {0, 0};
	array<int32_t, 2> adpcm_samples = generate_adpcm_sample();

	for (int i = 0; i < 2; i++)
	{
	    mixed_samples[i] += adpcm_samples[i];
	}

	copy(mixed_samples.begin(), mixed_samples.end(), (last_samples.begin() + 1));
    }

    void YM2610::adpcm_key_on(opnb_adpcm &channel)
    {
	channel.is_keyon = true;
	channel.current_addr = (channel.start_addr << 8);
	channel.is_high_nibble = false;
	channel.current_byte = 0;
	channel.adpcm_accum = 0;
	channel.adpcm_step = 0;
    }

    void YM2610::adpcm_key_off(opnb_adpcm &channel)
    {
	channel.is_keyon = false;
    }

    uint32_t YM2610::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 144);
    }

    void YM2610::reset()
    {
	for (auto &channel : adpcm_channels)
	{
	    channel.is_pan_left = true;
	    channel.is_pan_right = true;
	    channel.ch_level = 0x1F;
	    channel.is_keyon = false;
	    channel.is_high_nibble = false;
	    channel.current_byte = 0;
	    channel.current_addr = 0;
	    channel.adpcm_accum = 0;
	    channel.adpcm_step = 0;
	    channel.adpcm_output = 0;
	}

	last_samples.fill(0);
	timera_counter = 1023;
	timerb_counter = 255;
    }

    void YM2610::setInterface(BeeNukedInterface *cb)
    {
	inter = cb;
    }

    uint8_t YM2610::readIO(int port)
    {
	uint8_t temp = 0x00;
	port &= 3;
	switch (port)
	{
	    // Status 0 register (YM2203 compatible)
	    case 0: temp = opnb_status; break;
	    case 1:
	    {
		if (chip_address < 0x0E)
		{
		    cout << "Reading from YM2610 on-board PSG chip" << endl;
		    temp = 0x00;
		}
		else if (chip_address < 0x10)
		{
		    temp = 0xFF;
		}
		else if (chip_address == 0xFF)
		{
		    // FF: ID code
		    temp = 0x01;
		}
	    }
	    break;
	    case 2:
	    {
		// cout << "Reading from YM2610 ADPCM status register" << endl;
		temp = 0x00;
	    }
	    break;
	    case 3: temp = 0; break;
	    default: temp = 0x00; break;
	}

	return temp;
    }

    void YM2610::writeIO(int port, uint8_t data)
    {
	port &= 3;

	switch (port)
	{
	    case 0:
	    {
		chip_address = data;
		is_addr_a1 = false;

		if (chip_address < 0xE)
		{
		    if (inter != NULL)
		    {
			inter->writeSSG(0, data);
		    }
		}
	    }
	    break;
	    case 1:
	    {
		if (is_addr_a1)
		{
		    return; // Verified on real YM2608
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
		    return; // Verified on real YM2608
		}

		write_port1(chip_address, data);
	    }
	    break;
	}
    }

    void YM2610::writeADPCM_ROM(uint32_t rom_size, uint32_t data_start, uint32_t data_len, vector<uint8_t> rom_data)
    {
	adpcm_rom.resize(rom_size, 0xFF);

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

	copy(rom_data.begin(), (rom_data.begin() + data_length), (adpcm_rom.begin() + data_start));
    }

    void YM2610::writeDelta_ROM(uint32_t rom_size, uint32_t data_start, uint32_t data_len, vector<uint8_t> rom_data)
    {
	delta_t_rom.resize(rom_size, 0xFF);

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

	copy(rom_data.begin(), (rom_data.begin() + data_length), (delta_t_rom.begin() + data_start));
    }

    void YM2610::clockchip()
    {
	if ((ssg_sample_index % fm_samples_per_output) == 0)
	{
	    clock_fm_and_adpcm();
	    output_fm_and_adpcm();
	}

	resample();
	last_samples[0] = ssg_sample;
    }

    vector<int32_t> YM2610::get_samples()
    {
	vector<int32_t> mixed_samples;

	for (auto &sample : last_samples)
	{
	    mixed_samples.push_back(sample);
	}

	return mixed_samples;
    }
}