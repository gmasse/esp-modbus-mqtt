#include <Url.h>
#include <unity.h>


void test_url_1(void) {
  Url url("http://userid:password@example.com:8080/blah_(wikipedia)_blah#cite-1?q=Test%20URL-encoded%20stuff");
  TEST_ASSERT_EQUAL_STRING("http", url.Protocol.c_str());
  TEST_ASSERT_EQUAL_STRING("example.com", url.Host.c_str());
  TEST_ASSERT_EQUAL_STRING("8080", url.Port.c_str());
  TEST_ASSERT_EQUAL_STRING("/blah_(wikipedia)_blah#cite-1", url.Path.c_str());
  TEST_ASSERT_EQUAL_STRING("q=Test%20URL-encoded%20stuff", url.Query.c_str());
}

void process() {
  UNITY_BEGIN();
  RUN_TEST(test_url_1);
  UNITY_END();
}

#ifdef ARDUINO

#include <Arduino.h>
void setup() {
  // NOTE!!! Wait for >2 secs
  // if board doesn't support software reset via Serial.DTR/RTS
  delay(2000);

  process();
}

void loop() {
}

#else

int main(int argc, char **argv) {
  process();
  return 0;
}

#endif
