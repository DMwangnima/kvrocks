/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include <gtest/gtest.h>

#include <memory>

#include "test_base.h"
#include "types/redis_string.h"

class RedisStringTest : public TestBase {
 protected:
  explicit RedisStringTest() { string_ = std::make_unique<redis::String>(storage_, "string_ns"); }
  ~RedisStringTest() override = default;

  void SetUp() override {
    key_ = "test-string-key";
    pairs_ = {
        {"test-string-key1", "test-strings-value1"}, {"test-string-key2", "test-strings-value2"},
        {"test-string-key3", "test-strings-value3"}, {"test-string-key4", "test-strings-value4"},
        {"test-string-key5", "test-strings-value5"}, {"test-string-key6", "test-strings-value6"},
    };
  }

  std::unique_ptr<redis::String> string_;
  std::vector<StringPair> pairs_;
};

TEST_F(RedisStringTest, Append) {
  uint64_t ret = 0;
  for (size_t i = 0; i < 32; i++) {
    rocksdb::Status s = string_->Append(key_, "a", &ret);
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(static_cast<uint64_t>(i + 1), ret);
  }
  auto s = string_->Del(key_);
}

TEST_F(RedisStringTest, GetAndSet) {
  for (auto &pair : pairs_) {
    string_->Set(pair.key.ToString(), pair.value.ToString());
  }
  for (auto &pair : pairs_) {
    std::string got_value;
    string_->Get(pair.key.ToString(), &got_value);
    EXPECT_EQ(pair.value, got_value);
  }
  for (auto &pair : pairs_) {
    auto s = string_->Del(pair.key);
  }
}

TEST_F(RedisStringTest, MGetAndMSet) {
  string_->MSet(pairs_);
  std::vector<Slice> keys;
  std::vector<std::string> values;
  keys.reserve(pairs_.size());
  for (const auto &pair : pairs_) {
    keys.emplace_back(pair.key);
  }
  string_->MGet(keys, &values);
  for (size_t i = 0; i < pairs_.size(); i++) {
    EXPECT_EQ(pairs_[i].value, values[i]);
  }
  for (auto &pair : pairs_) {
    auto s = string_->Del(pair.key);
  }
}

TEST_F(RedisStringTest, IncrByFloat) {
  double f = 0.0;
  double max_float = std::numeric_limits<double>::max();
  string_->IncrByFloat(key_, 1.0, &f);
  EXPECT_EQ(1.0, f);
  string_->IncrByFloat(key_, max_float - 1, &f);
  EXPECT_EQ(max_float, f);
  string_->IncrByFloat(key_, 1.2, &f);
  EXPECT_EQ(max_float, f);
  string_->IncrByFloat(key_, -1 * max_float, &f);
  EXPECT_EQ(0, f);
  string_->IncrByFloat(key_, -1 * max_float, &f);
  EXPECT_EQ(-1 * max_float, f);
  string_->IncrByFloat(key_, -1.2, &f);
  EXPECT_EQ(-1 * max_float, f);
  // key hold value is not the number
  string_->Set(key_, "abc");
  rocksdb::Status s = string_->IncrByFloat(key_, 1.2, &f);
  EXPECT_TRUE(s.IsInvalidArgument());
  s = string_->Del(key_);
}

TEST_F(RedisStringTest, IncrBy) {
  int64_t ret = 0;
  string_->IncrBy(key_, 1, &ret);
  EXPECT_EQ(1, ret);
  string_->IncrBy(key_, INT64_MAX - 1, &ret);
  EXPECT_EQ(INT64_MAX, ret);
  rocksdb::Status s = string_->IncrBy(key_, 1, &ret);
  EXPECT_TRUE(s.IsInvalidArgument());
  string_->IncrBy(key_, INT64_MIN + 1, &ret);
  EXPECT_EQ(0, ret);
  string_->IncrBy(key_, INT64_MIN, &ret);
  EXPECT_EQ(INT64_MIN, ret);
  s = string_->IncrBy(key_, -1, &ret);
  EXPECT_TRUE(s.IsInvalidArgument());
  // key hold value is not the number
  string_->Set(key_, "abc");
  s = string_->IncrBy(key_, 1, &ret);
  EXPECT_TRUE(s.IsInvalidArgument());
  s = string_->Del(key_);
}

TEST_F(RedisStringTest, GetEmptyValue) {
  const std::string key = "empty_value_key";
  auto s = string_->Set(key, "");
  EXPECT_TRUE(s.ok());
  std::string value;
  s = string_->Get(key, &value);
  EXPECT_TRUE(s.ok() && value.empty());
}

TEST_F(RedisStringTest, GetSet) {
  int64_t ttl = 0;
  int64_t now = 0;
  rocksdb::Env::Default()->GetCurrentTime(&now);
  std::vector<std::string> values = {"a", "b", "c", "d"};
  for (size_t i = 0; i < values.size(); i++) {
    std::string old_value;
    auto s = string_->Expire(key_, now * 1000 + 100000);
    string_->GetSet(key_, values[i], &old_value);
    if (i != 0) {
      EXPECT_EQ(values[i - 1], old_value);
      auto s = string_->TTL(key_, &ttl);
      EXPECT_TRUE(ttl == -1);
    } else {
      EXPECT_TRUE(old_value.empty());
    }
  }
  auto s = string_->Del(key_);
}
TEST_F(RedisStringTest, GetDel) {
  for (auto &pair : pairs_) {
    string_->Set(pair.key.ToString(), pair.value.ToString());
  }
  for (auto &pair : pairs_) {
    std::string got_value;
    string_->GetDel(pair.key.ToString(), &got_value);
    EXPECT_EQ(pair.value, got_value);

    std::string second_got_value;
    auto s = string_->GetDel(pair.key.ToString(), &second_got_value);
    EXPECT_TRUE(!s.ok() && s.IsNotFound());
  }
}

TEST_F(RedisStringTest, MSetXX) {
  bool flag = false;
  string_->SetXX(key_, "test-value", 3000, &flag);
  EXPECT_FALSE(flag);
  string_->Set(key_, "test-value");
  string_->SetXX(key_, "test-value", 3000, &flag);
  EXPECT_TRUE(flag);
  int64_t ttl = 0;
  auto s = string_->TTL(key_, &ttl);
  EXPECT_TRUE(ttl >= 2000 && ttl <= 4000);
  s = string_->Del(key_);
}

TEST_F(RedisStringTest, MSetNX) {
  bool flag = false;
  string_->MSetNX(pairs_, 0, &flag);
  EXPECT_TRUE(flag);
  std::vector<Slice> keys;
  std::vector<std::string> values;
  keys.reserve(pairs_.size());
  for (const auto &pair : pairs_) {
    keys.emplace_back(pair.key);
  }
  string_->MGet(keys, &values);
  for (size_t i = 0; i < pairs_.size(); i++) {
    EXPECT_EQ(pairs_[i].value, values[i]);
  }

  std::vector<StringPair> new_pairs{
      {"a", "1"}, {"b", "2"}, {"c", "3"}, {pairs_[0].key, pairs_[0].value}, {"d", "4"},
  };
  string_->MSetNX(pairs_, 0, &flag);
  EXPECT_FALSE(flag);

  for (auto &pair : pairs_) {
    auto s = string_->Del(pair.key);
  }
}

TEST_F(RedisStringTest, MSetNXWithTTL) {
  bool flag = false;
  string_->SetNX(key_, "test-value", 3000, &flag);
  int64_t ttl = 0;
  auto s = string_->TTL(key_, &ttl);
  EXPECT_TRUE(ttl >= 2000 && ttl <= 4000);
  s = string_->Del(key_);
}

TEST_F(RedisStringTest, SetEX) {
  string_->SetEX(key_, "test-value", 3000);
  int64_t ttl = 0;
  auto s = string_->TTL(key_, &ttl);
  EXPECT_TRUE(ttl >= 2000 && ttl <= 4000);
  s = string_->Del(key_);
}

TEST_F(RedisStringTest, SetRange) {
  uint64_t ret = 0;
  string_->Set(key_, "hello,world");
  string_->SetRange(key_, 6, "redis", &ret);
  EXPECT_EQ(11, ret);
  std::string value;
  string_->Get(key_, &value);
  EXPECT_EQ("hello,redis", value);

  string_->SetRange(key_, 6, "test", &ret);
  EXPECT_EQ(11, ret);
  string_->Get(key_, &value);
  EXPECT_EQ("hello,tests", value);

  string_->SetRange(key_, 6, "redis-1234", &ret);
  string_->Get(key_, &value);
  EXPECT_EQ("hello,redis-1234", value);

  string_->SetRange(key_, 15, "1", &ret);
  EXPECT_EQ(16, ret);
  string_->Get(key_, &value);
  EXPECT_EQ(16, value.size());
  auto s = string_->Del(key_);
}

TEST_F(RedisStringTest, CAS) {
  int flag = 0;
  std::string key = "cas_key", value = "cas_value", new_value = "new_value";

  auto status = string_->Set(key, value);
  ASSERT_TRUE(status.ok());

  status = string_->CAS("non_exist_key", value, new_value, 10000, &flag);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(-1, flag);

  status = string_->CAS(key, "cas_value_err", new_value, 10000, &flag);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(0, flag);

  status = string_->CAS(key, value, new_value, 10000, &flag);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(1, flag);

  std::string current_value;
  status = string_->Get(key, &current_value);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(new_value, current_value);

  int64_t ttl = 0;
  status = string_->TTL(key, &ttl);
  EXPECT_TRUE(ttl >= 9000 && ttl <= 11000);

  status = string_->Del(key);
}

TEST_F(RedisStringTest, CAD) {
  int ret = 0;
  std::string key = "cas_key", value = "cas_value";

  auto status = string_->Set(key, value);
  ASSERT_TRUE(status.ok());

  status = string_->CAD("non_exist_key", value, &ret);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(-1, ret);

  status = string_->CAD(key, "cas_value_err", &ret);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(0, ret);

  status = string_->CAD(key, value, &ret);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(1, ret);

  std::string current_value;
  status = string_->Get(key, &current_value);
  ASSERT_TRUE(status.IsNotFound());

  status = string_->Del(key);
}
