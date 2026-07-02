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

#ifndef PHOTONSCORE_BASE_DATE_TIME_H
#define PHOTONSCORE_BASE_DATE_TIME_H

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>

namespace photonscore {
namespace base {

// Implementation of DateTime class similar to .NET struct.
class DateTime {
 public:
  enum DayOfWeek {
    kSunday = 0,
    kMonday = 1,
    kTuesday = 2,
    kWednesday = 3,
    kThursday = 4,
    kFriday = 5,
    kSaturday = 6,
  };

  enum Month {
    kJanuary = 1,
    kFebruary = 2,
    kMarch = 3,
    kApril = 4,
    kMay = 5,
    kJune = 6,
    kJuly = 7,
    kAugust = 8,
    kSeptember = 9,
    kOctober = 10,
    kNovember = 11,
    kDecember = 12,
  };

  DateTime();
  DateTime(int64_t ticks);
  DateTime(int year, int month, int day);
  DateTime(int year, int month, int day, int hour, int minute, int second);
  DateTime(int year, int month, int day, int hour, int minute, int second,
           int millisecond);

  bool valid() const;

  int64_t ticks() const { return ticks_; }
  int millisecond() const;
  int second() const;
  int minute() const;
  int hour() const;
  int day() const;
  Month month() const;
  int year() const;

  DayOfWeek day_of_week() const;
  int day_of_year() const;

  DateTime AddTicks(int64_t ticks) const;
  DateTime AddMilliseconds(double milliseconds) const;
  DateTime AddSeconds(double seconds) const;
  DateTime AddMinutes(double minutes) const;
  DateTime AddHours(double hours) const;
  DateTime AddDays(double days) const;
  DateTime AddMonths(double months) const;
  DateTime AddYears(double years) const;

  std::string ToString() const;
  std::time_t ToCTime() const;

  static bool IsLeapYear(int year);
  static int64_t DateToTicks(int year, int month, int day);
  static int64_t TimeToTicks(int hour, int minute, int second);
  static int DaysInMonth(int year, int month);
  static DateTime Now();
  static DateTime FromString(const std::string& value);
  static DateTime FromCTime(std::time_t c_time);

  bool operator==(const DateTime& x) const { return ticks_ == x.ticks_; }
  bool operator!=(const DateTime& x) const { return ticks_ != x.ticks_; }
  bool operator<=(const DateTime& x) const { return ticks_ <= x.ticks_; }
  bool operator>=(const DateTime& x) const { return ticks_ >= x.ticks_; }
  bool operator<(const DateTime& x) const { return ticks_ < x.ticks_; }
  bool operator>(const DateTime& x) const { return ticks_ > x.ticks_; }

 private:
  void Init(int year, int month, int day, int hour = 0, int minute = 0,
            int second = 0, int millisecond = 0);

  int64_t ticks_;
};

}  // namespace base
}  // namespace photonscore

#endif  // PHOTONSCORE_BASE_DATE_TIME_H
