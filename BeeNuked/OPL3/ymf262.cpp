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
// This chip formed the basis for the iconic SoundBlaster 16 sound card, which would go on to become
// the basis for PC sound throughout much of the 1990s.
//
// BueniaDev's Notes:
//
// This core is still a huge WIP, and lots of features are currently unimplemented.
// As such, expect a lot of things to work incorrectly (and due to the envelope generator being
// unimplemented, possibly quite loud as well).
// However, work is being done to improve this core, so don't lose hope here!

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

    uint32_t YMF262::get_sample_rate(uint32_t clock_rate)
    {
	return (clock_rate / 288);
    }

    void YMF262::init()
    {
	reset();
    }

    void YMF262::reset()
    {
	return;
    }

    void YMF262::write_port0(uint8_t reg, uint8_t data)
    {
	cout << "Writing value of " << hex << int(data) << " to YMF262 port 0 register of " << hex << int(reg) << endl;
    }

    void YMF262::write_port1(uint8_t reg, uint8_t data)
    {
	cout << "Writing value of " << hex << int(data) << " to YMF262 port 1 register of " << hex << int(reg) << endl;
    }

    void YMF262::writeIO(int port, uint8_t data)
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
		    return;
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
		    return;
		}

		write_port1(chip_address, data);
	    }
	    break;
	}
    }

    void YMF262::clockchip()
    {
	return;
    }

    vector<int32_t> YMF262::get_samples()
    {
	array<int32_t, 4> output = {0, 0, 0, 0};

	// YMF262 has 4 total outputs
	vector<int32_t> final_samples;
	final_samples.push_back(0);
	final_samples.push_back(0);
	final_samples.push_back(0);
	final_samples.push_back(0);
	return final_samples;
    }
};