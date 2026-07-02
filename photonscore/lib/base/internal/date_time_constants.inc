/*
    Copyright (c) 2026 Photonscore GmbH

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef PHOTONSCORE_BASE_INTERNAL_DATE_TIME_CONSTANTS_INC
#define PHOTONSCORE_BASE_INTERNAL_DATE_TIME_CONSTANTS_INC

#include <cstdint>
#include <limits>

namespace {

const int64_t InvalidTicks = std::numeric_limits<int64_t>::min();

// Number of 100ns ticks per time unit
const int64_t TicksPerMillisecond = 10000;
const int64_t TicksPerSecond = TicksPerMillisecond * 1000;
const int64_t TicksPerMinute = TicksPerSecond * 60;
const int64_t TicksPerHour = TicksPerMinute * 60;
const int64_t TicksPerDay = TicksPerHour * 24;

// Number of milliseconds per time unit
const int MillisPerSecond = 1000;
const int MillisPerMinute = MillisPerSecond * 60;
const int MillisPerHour = MillisPerMinute * 60;
const int MillisPerDay = MillisPerHour * 24;

// Number of days in a non-leap year
const int DaysPerYear = 365;
// Number of days in 4 years
const int DaysPer4Years = DaysPerYear * 4 + 1;  // 1461
// Number of days in 100 years
const int DaysPer100Years = DaysPer4Years * 25 - 1;  // 36524
// Number of days in 400 years
const int DaysPer400Years = DaysPer100Years * 4 + 1;  // 146097
// Number of days from 1/1/0001 to 12/31/1600
const int DaysTo1601 = DaysPer400Years * 4;  // 584388
// Number of days from 1/1/0001 to 12/30/1899
const int DaysTo1899 = DaysPer400Years * 4 + DaysPer100Years * 3 - 367;
// Number of days from 1/1/0001 to 12/31/1969
const int DaysTo1970 = DaysPer400Years * 4 + DaysPer100Years * 3 +
                       DaysPer4Years * 17 + DaysPerYear;  // 719,162
// Number of days from 1/1/0001 to 12/31/9999
const int DaysTo10000 = DaysPer400Years * 25 - 366;  // 3652059

}  // namespace

#endif  // PHOTONSCORE_BASE_INTERNAL_DATE_TIME_CONSTANTS_INC

// vim:ft=cpp
