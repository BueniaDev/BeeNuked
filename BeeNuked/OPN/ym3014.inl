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

// YM3014 DAC emulation (derived from the ymfm engine)
// (https://github.com/aaronsgiles/ymfm)

#if defined(__GNUC__)

uint8_t beenuked_clz(uint32_t value)
{
    if (value == 0)
    {
	return 32;
    }

    return __builtin_clz(value);
}

#elif defined(_MSC_VER)

uint8_t beenuked_clz(uint32_t value)
{
     unsigned long index;
     return _BitScanReverse(&index, value) ? uint8_t(31U - index) : 32U;
}

#else

inline uint8_t beenuked_clz(uint32_t value)
{
    if (value == 0)
    {
	return 32;
    }

    uint32_t temp_val = value;

    uint8_t count;

    for (count = 0; int32_t(temp_val) >= 0; count++)
    {
	temp_val <<= 1;
    }

    return count;
}

#endif

int16_t encode_fp(int32_t val)
{
    if (val < -32768)
    {
	return 0x1C00;
    }

    if (val > 32767)
    {
	return 0x1FFF;
    }

    int32_t scanvalue = (val ^ (int32_t(val) >> 31));

    int exponent = 7 - beenuked_clz(scanvalue << 17);

    exponent = max(exponent, 1);

    int32_t mantissa = (val >> (exponent - 1));

    return (((exponent << 10) | (mantissa & 0x3FF)) ^ 0x200);
}

int16_t decode_fp(int16_t val)
{
    val ^= 0x1E00;
    return (int16_t(val << 6) >> ((val >> 10) & 0x7));
}

int16_t dac_ym3014(int32_t val)
{
    if (val < -32768)
    {
	return -32768;
    }

    if (val > 32767)
    {
	return 32767;
    }

    int32_t scanvalue = (val ^ (int32_t(val) >> 31));

    int exponent = 7 - beenuked_clz(scanvalue << 17);

    exponent = max(exponent, 1);
    exponent -= 1;

    return ((val >> exponent) << exponent);
}