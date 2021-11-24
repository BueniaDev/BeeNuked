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

// BeeNuked-YM2612 (WIP)
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
// TODO: Write up initial notes

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
		    default: cout << "Writing value of " << hex << int(data) << " to YM2612 mode register of " << hex << int(reg) << endl; break;
		}
	    }
	    break;
	    default: cout << "Writing value of " << hex << int(data) << " to YM2612 FM register of " << hex << int(reg) << endl; break;
	}
    }

    uint32_t YM2612::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 144);
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

		cout << "Writing value of " << hex << int(data) << " to YM2612 port 1 register of " << hex << int(chip_address) << endl;
	    }
	    break;
	}
    }

    void YM2612::clockchip()
    {
	return;
    }

    array<int16_t, 2> YM2612::get_sample()
    {
	int32_t sample_zero = dac_discontinuity(0);

	array<int32_t, 2> output = {sample_zero, sample_zero};

	if (is_dac_enabled)
	{
	    int32_t dac_sample = dac_discontinuity(int16_t(dac_data << 7) >> 7);
	    output[0] += dac_sample;
	    output[1] += dac_sample;
	}

	array<int32_t, 2> mixed_samples = {0, 0};
	array<int16_t, 2> final_samples = {0, 0};

	for (int i = 0; i < 2; i++)
	{
	    int32_t sample = (output[i] << 7);

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

	final_samples[0] = mixed_samples[0];
	final_samples[1] = mixed_samples[1];
	return final_samples;
    }

};