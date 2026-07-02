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

#ifndef PHOTONSCORE_LIB_BASE_STATUS_H
#define PHOTONSCORE_LIB_BASE_STATUS_H

#include <fmt/format.h>

#include <cassert>
#include <vector>

#define DEFINE_STATUS_FOR_CODE(NAME)                                  \
  template <typename... Args>                                         \
  static Status NAME(fmt::string_view format, const Args&... args) {  \
    std::string message = fmt::format(fmt::runtime(format), args...); \
    return Status(Code::k##NAME, message.data(), message.size());     \
  }

namespace photonscore {

class Status {
 public:
  enum Code {
    kOK = 0,
    kUnknown = 1,
    kInvalidArgument = 2,
    kFailedPrecondition = 3,
    kOutOfRange = 4,
    kNotFound = 5,
    kAlreadyExists = 6,
    kResourceExhausted = 7,
    kUnimplemented = 8,
    kInternal = 9,
    kDataLoss = 10,
    kIoError = 11,
    kInvalidFormat = 12,
    kEndOfFile = 13,
  };

  Status() : state_(nullptr) {}
  Status(const Status& other) : state_(nullptr) { CopyState(other); }
  Status(Status&& other) : state_(nullptr) { std::swap(state_, other.state_); }

  ~Status() { delete[] state_; }

  Code code() const {
    return state_ == nullptr ? Code::kOK  //
                             : static_cast<Code>(state_[0]);
  }

  Code error_code() const {
    return state_ == nullptr ? Code::kOK  //
                             : static_cast<Code>(state_[0]);
  }

  const std::string& code_string() const {
    static const std::vector<std::string> strings = {
        "OK",
        "Unknown",
        "InvalidArgument",
        "FailedPrecondition",
        "OutOfRange",
        "NotFound",
        "AlreadyExists",
        "ResourceExhausted",
        "Unimplemented",
        "Internal",
        "DataLoss",
        "IoError",
        "InvalidFormat",
        "EndOfFile",
    };
    auto c = code();
    if (c < 0) c = kUnknown;
    if (c > 13) c = kUnknown;
    return strings[c];
  }

  static Status OK() { return Status(); }

  bool ok() const { return state_ == nullptr; }

  std::string message() const {
    if (state_ == nullptr) {
      return std::string();
    } else {
      return std::string(state_ + 1 + 4, message_length());
    }
  }

  Status& operator=(const Status& other) {
    delete[] state_;
    state_ = nullptr;
    CopyState(other);
    return *this;
  }

  Status& operator=(Status&& other) {
    delete[] state_;
    state_ = nullptr;
    std::swap(state_, other.state_);
    return *this;
  }

  DEFINE_STATUS_FOR_CODE(Unknown);
  DEFINE_STATUS_FOR_CODE(InvalidArgument);
  DEFINE_STATUS_FOR_CODE(FailedPrecondition);
  DEFINE_STATUS_FOR_CODE(OutOfRange);
  DEFINE_STATUS_FOR_CODE(NotFound);
  DEFINE_STATUS_FOR_CODE(AlreadyExists);
  DEFINE_STATUS_FOR_CODE(ResourceExhausted);
  DEFINE_STATUS_FOR_CODE(Unimplemented);
  DEFINE_STATUS_FOR_CODE(Internal);
  DEFINE_STATUS_FOR_CODE(DataLoss);
  DEFINE_STATUS_FOR_CODE(IoError);
  DEFINE_STATUS_FOR_CODE(InvalidFormat);
  DEFINE_STATUS_FOR_CODE(EndOfFile);

 private:
  // state_[0] == Status::Code
  // state_[1..4] == message length
  // state_[5...] == message text
  const char* state_;

  Status(Code code, const char* message, size_t message_length) {
    auto buf = new char[message_length + 1 + 4];
    buf[0] = static_cast<char>(code);
    buf[1] = (message_length >> 24) & 0xFF;
    buf[2] = (message_length >> 16) & 0xFF;
    buf[3] = (message_length >> 8) & 0xFF;
    buf[4] = (message_length >> 0) & 0xFF;
    std::memcpy(buf + 1 + 4, message, message_length);
    state_ = buf;
  }

  size_t message_length() const {
    assert(state_ != nullptr);
    size_t res = 0;
    res += *reinterpret_cast<const unsigned char*>(state_ + 1);
    res <<= 8;
    res += *reinterpret_cast<const unsigned char*>(state_ + 2);
    res <<= 8;
    res += *reinterpret_cast<const unsigned char*>(state_ + 3);
    res <<= 8;
    res += *reinterpret_cast<const unsigned char*>(state_ + 4);
    return res;
  }

  size_t buffer_size() const {
    assert(state_ != nullptr);
    return message_length() + /* error_code */ 1 + /* message_length */ 4;
  }

  void CopyState(const Status& other) {
    assert(state_ == nullptr);
    if (other.state_ != nullptr) {
      size_t n = other.buffer_size();
      char* buf = new char[n];
      std::memcpy(buf, other.state_, n);
      state_ = buf;
    }
  }
};

}  // namespace photonscore

#undef DEFINE_STATUS_FOR_CODE

#endif  // PHOTONSCORE_LIB_BASE_STATUS_H
