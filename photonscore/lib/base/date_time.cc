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

#include "photonscore/lib/base/date_time.h"
#include "photonscore/lib/base/internal/date_time_constants.inc"

#include <fmt/format.h>

#include <chrono>
#include <regex>

namespace photonscore {
namespace base {

namespace {

enum DatePart {
  kYear = 0,
  kDayOfYear = 1,
  kMonth = 2,
  kDay = 3,
};

const int DaysToMonth365[] = {0,   31,  59,  90,  120, 151, 181,
                              212, 243, 273, 304, 334, 365};
const int DaysToMonth366[] = {0,   31,  60,  91,  121, 152, 182,
                              213, 244, 274, 305, 335, 366};

const int64_t MaxSeconds = std::numeric_limits<int64_t>::max() / TicksPerSecond;
const int64_t MinSeconds = std::numeric_limits<int64_t>::min() / TicksPerSecond;

int GetDatePart(int64_t ticks, DatePart part) {
  int n = (int)(ticks / TicksPerDay);
  int y400 = n / DaysPer400Years;
  n -= y400 * DaysPer400Years;
  int y100 = n / DaysPer100Years;
  if (y100 == 4) y100 = 3;
  n -= y100 * DaysPer100Years;
  int y4 = n / DaysPer4Years;
  n -= y4 * DaysPer4Years;
  int y1 = n / DaysPerYear;
  if (y1 == 4) y1 = 3;
  if (part == kYear) {
    return y400 * 400 + y100 * 100 + y4 * 4 + y1 + 1;
  }
  n -= y1 * DaysPerYear;
  if (part == kDayOfYear) return n + 1;
  bool leapYear = y1 == 3 && (y4 != 24 || y100 == 3);
  auto days = leapYear ? DaysToMonth366 : DaysToMonth365;
  int m = (n >> 5) + 1;
  while (n >= days[m]) m++;
  if (part == kMonth) return m;
  return n - days[m - 1] + 1;
}

int ParseInt(const std::string& str) {
  return str.size() > 0 ? std::stoi(str) : 0;
}

}  // namespace

DateTime::DateTime() : ticks_(InvalidTicks) {}

DateTime::DateTime(int64_t ticks) : ticks_(ticks) {}

DateTime::DateTime(int year, int month, int day) { Init(year, month, day); }

DateTime::DateTime(int year, int month, int day, int hour, int minute,
                   int second) {
  Init(year, month, day, hour, minute, second);
}

DateTime::DateTime(int year, int month, int day, int hour, int minute,
                   int second, int millisecond) {
  Init(year, month, day, hour, minute, second, millisecond);
}

void DateTime::Init(int year, int month, int day, int hour, int minute,
                    int second, int millisecond) {
  ticks_ = InvalidTicks;

  auto date = DateToTicks(year, month, day);
  if (date == InvalidTicks) return;

  auto time = TimeToTicks(hour, minute, second);
  if (time == InvalidTicks) return;

  if (millisecond < 0 || millisecond >= MillisPerSecond) return;

  ticks_ = date + time + millisecond * TicksPerMillisecond;
}

bool DateTime::valid() const { return ticks_ != InvalidTicks; }

int DateTime::millisecond() const {
  return int((ticks_ / TicksPerMillisecond) % 1000);
}

int DateTime::second() const { return int((ticks_ / TicksPerSecond) % 60); }

int DateTime::minute() const { return int((ticks_ / TicksPerMinute) % 60); }

int DateTime::hour() const { return int((ticks_ / TicksPerHour) % 24); }

int DateTime::day() const { return GetDatePart(ticks_, kDay); }
DateTime::Month DateTime::month() const {
  return (DateTime::Month)GetDatePart(ticks_, kMonth);
}
int DateTime::year() const { return GetDatePart(ticks_, kYear); }

DateTime::DayOfWeek DateTime::day_of_week() const {
  return DateTime::DayOfWeek((ticks_ / TicksPerDay + 1) % 7);
}

int DateTime::day_of_year() const { return GetDatePart(ticks_, kDayOfYear); }

DateTime DateTime::AddTicks(int64_t ticks) const {
  return DateTime(ticks_ + ticks);
}

DateTime DateTime::AddMilliseconds(double milliseconds) const {
  return DateTime(ticks_ + int64_t(milliseconds * TicksPerMillisecond));
}

DateTime DateTime::AddSeconds(double seconds) const {
  return DateTime(ticks_ + int64_t(seconds * TicksPerSecond));
}

DateTime DateTime::AddMinutes(double minutes) const {
  return DateTime(ticks_ + int64_t(minutes * TicksPerMinute));
}

DateTime DateTime::AddHours(double hours) const {
  return DateTime(ticks_ + int64_t(hours * TicksPerHour));
}

DateTime DateTime::AddDays(double days) const {
  return DateTime(ticks_ + int64_t(days * TicksPerDay));
}

DateTime DateTime::AddMonths(double months) const {
  if (months < -120000 || months > 120000) return DateTime();
  int y = GetDatePart(ticks_, kYear);
  int m = GetDatePart(ticks_, kMonth);
  int d = GetDatePart(ticks_, kDay);
  int i = m - 1 + int(months);
  if (i >= 0) {
    m = i % 12 + 1;
    y = y + i / 12;
  } else {
    m = 12 + (i + 1) % 12;
    y = y + (i - 11) / 12;
  }
  if (y < 1 || y > 9999) return DateTime();
  int days = DaysInMonth(y, m);
  if (days < 0) return DateTime();
  if (d > days) d = days;
  return DateTime(DateToTicks(y, m, d) + ticks_ % TicksPerDay);
}

DateTime DateTime::AddYears(double years) const {
  return AddMonths(years * 12);
}

bool DateTime::IsLeapYear(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int64_t DateTime::DateToTicks(int year, int month, int day) {
  if (year >= 1 && year <= 9999 && month >= 1 && month <= 12) {
    auto days = IsLeapYear(year) ? DaysToMonth366 : DaysToMonth365;
    if (day >= 1 && day <= days[month] - days[month - 1]) {
      int y = year - 1;
      int n = y * 365 + y / 4 - y / 100 + y / 400 + days[month - 1] + day - 1;
      return n * TicksPerDay;
    }
  }
  return InvalidTicks;
}

int64_t DateTime::TimeToTicks(int hour, int minute, int second) {
  int64_t totalSeconds =
      int64_t(hour) * 3600 + int64_t(minute) * 60 + int64_t(second);
  if (totalSeconds > MaxSeconds || totalSeconds <= MinSeconds)
    return InvalidTicks;
  return totalSeconds * TicksPerSecond;
}

int DateTime::DaysInMonth(int year, int month) {
  if (month < 1 || month > 12 || year < 1 || year > 9999) return -1;
  auto days = IsLeapYear(year) ? DaysToMonth366 : DaysToMonth365;
  return days[month] - days[month - 1];
}

DateTime DateTime::Now() {
  auto since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch());
  return DateTime(DaysTo1970 * TicksPerDay + since_epoch.count() * 10 +
                  2 * TicksPerHour);
}

DateTime DateTime::FromString(const std::string& value) {
  auto re = std::regex(
      ".*(\\d{4})-(\\d{2})-(\\d{2})"                  // __YYYY.MM.DD_
      "(.(\\d{2}):(\\d{2}):(\\d{2})(\\.(\\d*))?)?");  // HH:MM:SS.MS
  std::smatch match;
  if (!std::regex_match(value, match, re)) {
    return DateTime();
  }

  int year = ParseInt(match.str(1));
  int month = ParseInt(match.str(2));
  int day = ParseInt(match.str(3));

  int hour = ParseInt(match.str(5));
  int minute = ParseInt(match.str(6));
  int second = ParseInt(match.str(7));
  int millisecond = ParseInt(match.str(9));

  return {year, month, day, hour, minute, second, millisecond};
}

std::string DateTime::ToString() const {
  return fmt::format("{0:03}-{1:02}-{2:02} {3:02}:{4:02}:{5:02}.{6:03}", year(),
                     (int)month(), day(), hour(), minute(), second(),
                     millisecond());
}

std::time_t DateTime::ToCTime() const {
  struct tm t;
  t.tm_year = year();
  t.tm_mon = month();
  t.tm_mday = day();
  t.tm_hour = hour();
  t.tm_min = minute();
  t.tm_sec = second();

  return std::mktime(&t);
}

DateTime DateTime::FromCTime(std::time_t c_time) {
  struct tm* t = ::localtime(&c_time);
  return DateTime(1900 + t->tm_year, 1 + t->tm_mon, t->tm_mday,  // YMD
                  t->tm_hour, t->tm_min, t->tm_sec);             // HMS
}

}  // namespace base
}  // namespace photonscore
