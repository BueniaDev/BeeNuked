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

#ifndef BEENUKED_UTILS_H
#define BEENUKED_UTILS_H

#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <array>
#include <vector>
#include <functional>
#include <cassert>
using namespace std;

#ifndef M_PI
#define M_PI 3.1415926535
#endif

namespace beenuked
{
    enum BeeNukedAccessType : int
    {
	ADPCM = 0,
	DeltaT = 1
    };

    class BeeNukedInterface
    {
	public:
	    BeeNukedInterface()
	    {

	    }

	    ~BeeNukedInterface()
	    {

	    }

	    virtual void writeSSG(int port, uint8_t data)
	    {
		cout << "Writing value of " << hex << int(data) << " to OPN SSG port of " << dec << int(port) << endl;
	    }

	    virtual void clockSSG()
	    {
		cout << "Clocking SSG..." << endl;
	    }

	    virtual array<int32_t, 3> getSSGSamples()
	    {
		cout << "Fetching samples..." << endl;
		return {0, 0, 0};
	    }

	    virtual uint8_t readMemory(BeeNukedAccessType type, uint32_t addr)
	    {
		(void)type;
		(void)addr;
		return 0;
	    }

	    virtual void writeMemory(BeeNukedAccessType type, uint32_t addr, uint8_t data)
	    {
		(void)type;
		(void)addr;
		(void)data;
		return;
	    }
    };
};

#endif // BEENUKED_UTILS_H