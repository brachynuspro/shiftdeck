#pragma once
#include <stdint.h>

// Root CAs every TLS client verifies against (WiFiClientSecure::setCACertBundle).
// certs/x509_crt_bundle.bin is built by tools/gen_crt_bundle.py and embedded by
// board_build.embed_files in platformio.ini.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_certs_x509_crt_bundle_bin_start");
