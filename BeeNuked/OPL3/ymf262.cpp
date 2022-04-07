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

    void YMF262::write_port0(uint8_t reg, uint8_t data)
    {
	string opl_mode = (is_opl3_mode) ? "OPL3" : "OPL2";
	cout << "Writing value of " << hex << int(data) << " to YMF262 port 0 register of " << hex << int(reg) << " in " << opl_mode << " mode" << endl;
    }

    void YMF262::write_port1(uint8_t reg, uint8_t data)
    {
	if (is_opl3_mode)
	{
	    switch (reg)
	    {
		case 0x04:
		{
		    for (int i = 0; i < 6; i++)
		    {
			if (testbit(data, i))
			{
			    int ch = (i < 3) ? i : ((i - 3) + 9);
			    cout << "Four-op mode enabled for channel pair " << dec << int(ch) << " - " << dec << int(ch + 3) << endl;
			}
		    }
		}
		break;
		case 0x05: write_opl3_select(data); break;
		default:
		{
		    cout << "Writing value of " << hex << int(data) << " to YMF262 port 1 register of " << hex << int(reg) << " in OPL3 mode" << endl;
		}
		break;
	    }
	}
	else
	{
	    switch (reg)
	    {
		case 0x05: write_opl3_select(data); break;
		default:
		{
		    cout << "Writing value of " << hex << int(data) << " to YMF262 register of " << hex << int(reg) << " in OPL2 mode" << endl;
		}
		break;
	    }
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
	reset();
    }

    void YMF262::reset()
    {
	is_opl3_mode = false;
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
	return;
    }

    vector<int32_t> YMF262::get_samples()
    {
	// YMF262 has 4 total outputs
	vector<int32_t> final_samples;
	final_samples.push_back(0);
	final_samples.push_back(0);
	final_samples.push_back(0);
	final_samples.push_back(0);
	return final_samples;
    }
};