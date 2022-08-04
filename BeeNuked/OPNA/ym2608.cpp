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

// BeeNuked-YM2608 (WIP)
// Chip Name: YM2608 (OPNA)
// Chip Used In: NEC computers (i.e. PC-88, PC-98, etc.) and a few arcade machines
//
// Interesting Trivia:
//
// The YM2608, utilized in the PC-98 system, would go on to be utilized in the first 5 games in the iconic Touhou franchise, which is still around today!
//
// BueniaDev's Notes:
//
// This implementation is a huge WIP, and lots of features are currently unimplemented.
// As such, expect a lot of things to not work correctly here.
// However, work is being done to improve this core, so don't lose hope here!

#include "ym2608.h"
using namespace beenuked;

namespace beenuked
{
    YM2608::YM2608()
    {

    }

    YM2608::~YM2608()
    {

    }

    void YM2608::update_prescaler()
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

    void YM2608::set_prescaler(int value)
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

    void YM2608::configure_ssg_resampler(uint8_t out_samples, uint8_t src_samples)
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

    void YM2608::add_last(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale)
    {
	sum0 += (last_ssg_samples[0] * scale);
	sum1 += (last_ssg_samples[1] * scale);
	sum2 += (last_ssg_samples[2] * scale);
    }

    void YM2608::clock_and_add(int32_t &sum0, int32_t &sum1, int32_t &sum2, int scale)
    {
	if (ssg_inter != NULL)
	{
	    ssg_inter->clockSSG();
	    last_ssg_samples = ssg_inter->getSamples();
	}

	add_last(sum0, sum1, sum2, scale);
    }

    void YM2608::write_to_output(int32_t sum0, int32_t sum1, int32_t sum2, int divisor)
    {
	last_samples[0] = ((sum0 + sum1 + sum2) * 2 / (3 * divisor));
	ssg_sample_index += 1;
    }

    void YM2608::resample_4_3()
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

    void YM2608::resample_2_3()
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

    void YM2608::resample_1_3()
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

    void YM2608::resample_nop()
    {
	ssg_sample_index += 1;
    }

    void YM2608::write_port0(uint8_t reg, uint8_t data)
    {
	switch ((reg & 0xF0))
	{
	    case 0x00:
	    {
		if (ssg_inter != NULL)
		{
		    ssg_inter->writeIO(1, data);
		}
	    }
	    break;
	    case 0x10:
	    {
		cout << "Writing value of " << hex << int(data) << " to YM2608 ADPCM-A register of " << hex << int(reg) << endl;
	    }
	    break;
	    default:
	    {
		cout << "Writing value of " << hex << int(data) << " to YM2608 FM register of " << hex << int(reg) << endl;
	    }
	    break;
	}
    }

    uint32_t YM2608::get_sample_rate(uint32_t clk_rate)
    {
	return (clk_rate / 24);
    }

    void YM2608::init()
    {
	reset();
    }

    void YM2608::reset()
    {
	set_prescaler(prescaler_six);
	last_samples.fill(0);
    }

    void YM2608::set_ssg_interface(OPNASSGInterface *inter)
    {
	ssg_inter = inter;
    }

    uint8_t YM2608::readIO(int port)
    {
	return 0;
    }

    void YM2608::writeIO(int port, uint8_t data)
    {
	port &= 3;

	switch (port)
	{
	    case 0:
	    {
		chip_address = data;
		is_addr_a1 = false;

		if (chip_address < 0x10)
		{
		    if (ssg_inter != NULL)
		    {
			ssg_inter->writeIO(0, data);
		    }
		}
		else if ((chip_address >= 0x2D) && (chip_address <= 0x2F))
		{
		    update_prescaler();
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

		cout << "Writing value of " << hex << int(data) << " to YM2608 port 1 register of " << hex << int(chip_address) << endl;
	    }
	    break;
	}
    }

    void YM2608::clockchip()
    {
	if (resample)
	{
	    resample();
	}
    }

    vector<int32_t> YM2608::get_samples()
    {
	vector<int32_t> final_samples;
	for (auto &sample : last_samples)
	{
	    final_samples.push_back(sample);
	}
	return final_samples;
    }
}