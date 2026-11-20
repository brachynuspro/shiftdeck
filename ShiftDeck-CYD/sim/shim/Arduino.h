// Host-side stand-in for the Arduino/ESP32 core: just enough for TFT_eSPI
// and the ShiftDeck views to compile and render on a desktop.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>
#include <type_traits>

#define PROGMEM
#define PI 3.1415926535897932384626433832795
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define pgm_read_byte(a)  (*(const uint8_t*)(a))
#define pgm_read_word(a)  (*(const uint16_t*)(a))
#define pgm_read_dword(a) (*(const uint32_t*)(a))
#define pgm_read_pointer(a) (*(void* const*)(a))
#define DEC 10
#define HEX 16
#define bitRead(v, b) (((v) >> (b)) & 1)
#define digitalPinToBitMask(p) (1u << ((p) & 31))
inline long random(long howbig) { return howbig > 0 ? rand() % howbig : 0; }
inline char* ltoa(long v, char* buf, int base) { if (base == 16) sprintf(buf, "%lx", v); else sprintf(buf, "%ld", v); return buf; }
inline char* ultoa(unsigned long v, char* buf, int base) { if (base == 16) sprintf(buf, "%lx", v); else sprintf(buf, "%lu", v); return buf; }
inline char* dtostrf(double v, signed char w, unsigned char p, char* buf) { sprintf(buf, "%*.*f", w, p, v); return buf; }

template <class A, class B> inline typename std::common_type<A, B>::type min(A a, B b) { return a < b ? a : b; }
template <class A, class B> inline typename std::common_type<A, B>::type max(A a, B b) { return a > b ? a : b; }
using std::abs;

typedef bool boolean;
typedef uint8_t byte;

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
inline void delayMicroseconds(unsigned int) {}
inline void yield() {}
void pinMode(int pin, int mode);
void digitalWrite(int pin, int val);
int digitalRead(int pin);
inline void ledcWrite(int, int) {}

class String {
public:
    String() {}
    String(const char* s) : _s(s ? s : "") {}
    String(const std::string& s) : _s(s) {}
    String(char c) : _s(1, c) {}
    template <class T, class = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    String(T v) : _s(std::to_string(v)) {}
    const char* c_str() const { return _s.c_str(); }
    unsigned int length() const { return _s.size(); }
    bool isEmpty() const { return _s.empty(); }
    bool reserve(unsigned int n) { _s.reserve(n); return true; }
    char operator[](unsigned int i) const { return i < _s.size() ? _s[i] : 0; }
    char& operator[](unsigned int i) { return _s[i]; }
    bool startsWith(const String& p) const { return _s.compare(0, p._s.size(), p._s) == 0; }
    bool endsWith(const String& p) const { return _s.size() >= p._s.size() && _s.compare(_s.size() - p._s.size(), p._s.size(), p._s) == 0; }
    void remove(unsigned int i) { if (i < _s.size()) _s.erase(i); }
    void remove(unsigned int i, unsigned int n) { if (i < _s.size()) _s.erase(i, n); }
    String substring(unsigned int a) const { return a < _s.size() ? String(_s.substr(a)) : String(); }
    String substring(unsigned int a, unsigned int b) const { return a < _s.size() && b > a ? String(_s.substr(a, b - a)) : String(); }
    int indexOf(char c) const { auto p = _s.find(c); return p == std::string::npos ? -1 : (int)p; }
    int lastIndexOf(char c) const { auto p = _s.rfind(c); return p == std::string::npos ? -1 : (int)p; }
    void trim() { while (!_s.empty() && isspace((unsigned char)_s.back())) _s.pop_back(); size_t i = 0; while (i < _s.size() && isspace((unsigned char)_s[i])) i++; _s.erase(0, i); }
    long toInt() const { return atol(_s.c_str()); }
    void toCharArray(char* buf, unsigned int n) const { if (n) { strncpy(buf, _s.c_str(), n - 1); buf[n - 1] = 0; } }
    String& operator+=(const String& o) { _s += o._s; return *this; }
    String& operator+=(const char* o) { _s += o; return *this; }
    String& operator+=(char c) { _s += c; return *this; }
    bool operator==(const String& o) const { return _s == o._s; }
    bool operator!=(const String& o) const { return _s != o._s; }
    bool operator==(const char* o) const { return _s == o; }
    bool operator!=(const char* o) const { return _s != o; }
    bool operator<(const String& o) const { return _s < o._s; }
    friend String operator+(const String& a, const String& b) { return String(a._s + b._s); }
    friend String operator+(const String& a, const char* b) { return String(a._s + b); }
    friend String operator+(const char* a, const String& b) { return String(std::string(a) + b._s); }
    friend String operator+(const String& a, char c) { return String(a._s + c); }
    template <class T, class = typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, char>::value>::type>
    friend String operator+(const String& a, T v) { return String(a._s + std::to_string(v)); }
private:
    std::string _s;
};

class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
    size_t print(const char* s) { size_t n = 0; while (*s) n += write((uint8_t)*s++); return n; }
    size_t print(const String& s) { return print(s.c_str()); }
    size_t print(char c) { return write((uint8_t)c); }
    template <class T> size_t print(T v) { return print(String(v)); }
    size_t println(const char* s = "") { return print(s) + print("\n"); }
    template <class T> size_t println(T v) { return print(v) + print("\n"); }
};

struct SerialStub : public Print {
    size_t write(uint8_t c) override { return 1; }
    void begin(int) {}
    template <class... A> void printf(const char*, A...) {}
};
extern SerialStub Serial;

struct EspStub {
    uint32_t getFreeHeap() { return 112 * 1024; }
    uint32_t getMinFreeHeap() { return 74 * 1024; }
    void restart() { exit(0); }
};
extern EspStub ESP;

// ---- FreeRTOS ----
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
typedef uint32_t TickType_t;
#define portMAX_DELAY 0xFFFFFFFF
#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE 1
#define pdPASS 1
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (void*)1; }
inline SemaphoreHandle_t xSemaphoreCreateBinary() { return (void*)1; }
inline int xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return 1; }
inline int xSemaphoreGive(SemaphoreHandle_t) { return 1; }
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return (void*)1; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t, TickType_t) { return 1; }
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t) { return 1; }
inline void vTaskDelay(TickType_t) {}
