"""Builds certs/x509_crt_bundle.bin, the root CAs the firmware verifies TLS against.

    security find-certificate -a -p /System/Library/Keychains/SystemRootCertificates.keychain > roots.pem
    python3 tools/gen_crt_bundle.py roots.pem certs/x509_crt_bundle.bin

ESP-IDF bundle format, read by WiFiClientSecure's esp_crt_bundle.c: u16 count, then per
root u16 subject_len, u16 key_len, subject DER, SubjectPublicKeyInfo DER. Sorted by
subject DER because the firmware binary-searches it with memcmp on the issuer.
"""
import struct
import sys

from cryptography import x509
from cryptography.hazmat.primitives import serialization

src, out = sys.argv[1], sys.argv[2]
pem = open(src, "rb").read()
entries = []
for block in pem.split(b"-----END CERTIFICATE-----")[:-1]:
    try:
        crt = x509.load_pem_x509_certificate(block + b"-----END CERTIFICATE-----")
        key = crt.public_key().public_bytes(serialization.Encoding.DER,
                                            serialization.PublicFormat.SubjectPublicKeyInfo)
    except Exception as e:  # a root cryptography cannot parse cannot be used by mbedTLS either
        print("skipped:", e)
        continue
    entries.append((crt.subject.public_bytes(), key))
entries.sort()

blob = struct.pack(">H", len(entries))
for name, key in entries:
    blob += struct.pack(">HH", len(name), len(key)) + name + key
open(out, "wb").write(blob)

# self-check: walk the file the way esp_crt_bundle_init() does
n, off, names = struct.unpack_from(">H", blob)[0], 2, []
for _ in range(n):
    nl, kl = struct.unpack_from(">HH", blob, off)
    names.append(blob[off + 4:off + 4 + nl])
    off += 4 + nl + kl
assert off == len(blob) and names == sorted(names) and n == len(entries)
print(f"{n} roots, {len(blob)} bytes -> {out}")
