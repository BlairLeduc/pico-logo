#!/usr/bin/env python3
#
#  Pico Logo
#  Copyright 2026 Blair Leduc. See LICENSE for details.
#
#  Generate devices/picocalc/ca_certs.c: a trimmed bundle of CA root
#  certificates embedded as a C string, used by the HTTPS client to verify
#  servers.
#
#  Usage:
#    python3 gen_ca_certs.py /etc/ssl/cert.pem > ../ca_certs.c
#
#  The input is any PEM bundle of trusted roots (e.g. the system trust store or
#  the Mozilla/curl cacert.pem). Only the roots whose subject CN is listed in
#  WANTED below are kept, to bound the flash footprint and the RAM cost of
#  parsing the trust store during the TLS handshake. Edit WANTED to add roots
#  for hosts you need to reach.
#
import subprocess
import sys

# Subject CNs of the roots to bundle. A small, high-coverage set covering the
# bulk of the public web (Let's Encrypt, DigiCert, Google, Amazon, Sectigo,
# GlobalSign, Microsoft/Baltimore).
WANTED = [
    "ISRG Root X1",
    "DigiCert Global Root CA",
    "DigiCert Global Root G2",
    "DigiCert High Assurance EV Root CA",
    "Baltimore CyberTrust Root",
    "Amazon Root CA 1",
    "GlobalSign Root CA",
    "GlobalSign",                 # GlobalSign Root CA - R3 (CN is just "GlobalSign")
    "GTS Root R1",
    "USERTrust RSA Certification Authority",
    "AAA Certificate Services",
]


def split_pem_blocks(text):
    blocks = []
    cur = []
    inside = False
    for line in text.splitlines():
        if line.startswith("-----BEGIN CERTIFICATE-----"):
            inside = True
            cur = [line]
        elif line.startswith("-----END CERTIFICATE-----"):
            cur.append(line)
            blocks.append("\n".join(cur) + "\n")
            inside = False
        elif inside:
            cur.append(line)
    return blocks


def subject_cn(pem):
    out = subprocess.run(
        ["openssl", "x509", "-noout", "-subject", "-nameopt", "RFC2253"],
        input=pem, capture_output=True, text=True,
    ).stdout
    for part in out.replace("subject=", "").strip().split(","):
        part = part.strip()
        if part.startswith("CN="):
            return part[3:]
    return ""


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: gen_ca_certs.py <pem-bundle> > ../ca_certs.c")
    with open(sys.argv[1]) as f:
        text = f.read()

    kept = []
    seen = set()
    for pem in split_pem_blocks(text):
        cn = subject_cn(pem)
        if cn in WANTED and cn not in seen:
            seen.add(cn)
            kept.append((cn, pem))

    print("//")
    print("//  Pico Logo")
    print("//  Copyright 2026 Blair Leduc. See LICENSE for details.")
    print("//")
    print("//  Trimmed CA root bundle for the HTTPS client. GENERATED FILE -- do not")
    print("//  edit by hand. Regenerate with devices/picocalc/tools/gen_ca_certs.py.")
    print("//")
    print("")
    print('#include "ca_certs.h"')
    print("")
    print("const char ca_certs_pem[] =")
    for cn, pem in kept:
        print(f'    // {cn}')
        for line in pem.splitlines():
            print(f'    "{line}\\n"')
    print("    ;")
    print("")
    print("const size_t ca_certs_pem_len = sizeof(ca_certs_pem);")


if __name__ == "__main__":
    main()
