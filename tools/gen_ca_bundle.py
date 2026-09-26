#!/usr/bin/env python3
"""Build romfs/cacert.pem -- the trust store pkHouse ships inside the NRO.

The Switch has no trust store homebrew can borrow, so anything speaking HTTPS
has to carry its own. Shipping the full Mozilla bundle would work and costs
about 184 KB, but the cost that matters is not the size: mbedTLS parses the
whole file on every handshake, and the console is slow at it.

A board on OVH shared hosting uses OVH's free Let's Encrypt certificate, and
every Let's Encrypt chain ends at one of two roots. The self-updater talks to
GitHub, whose API and release pages chain to Sectigo (USERTrust / Sectigo
E46), while the release files themselves come from a Let's Encrypt host. Six
roots in all, about 8 KB, still parses in a blink.

    python3 tools/gen_ca_bundle.py            # Let's Encrypt + GitHub (default)
    python3 tools/gen_ca_bundle.py --full     # the machine's whole trust store

THE TRADE: the minimal bundle trusts Let's Encrypt and GitHub's CA and
nothing else. Put the
board behind Cloudflare, or move to a host that issues through Sectigo or
DigiCert, and the handshake will fail until you rebuild with --full. A bundle
dropped at sdmc:/switch/pkHouse/cacert.pem overrides this one at runtime, so
that does not need a rebuild.

Certificates are read from the build machine's own trust store rather than
downloaded: they are already there, already vetted by the distribution, and a
script that fetches its own roots over the network is trusting the thing it is
supposed to be establishing trust for.
"""

import argparse
import pathlib
import shutil
import subprocess
import sys

# Every Let's Encrypt chain terminates at one of these. X1 signs the RSA
# intermediates (R10, R11, ...), X2 the ECDSA ones (E5, E6, ...); X2 is also
# cross-signed by X1. Servers send the intermediate themselves, so the roots
# are all we need.
LETSENCRYPT_ROOTS = ["ISRG Root X1", "ISRG Root X2"]

# GitHub (api.github.com, github.com, codeload) chains through Sectigo's DV
# E36 intermediate to Sectigo Public Server Authentication Root E46, which is
# cross-signed by USERTrust ECC. Both the root and its cross-signer are here,
# and the RSA pair beside them, so a switch between Sectigo's ECC and RSA
# chains does not break the update check. Release downloads are served from
# release-assets.githubusercontent.com, a Let's Encrypt host.
GITHUB_ROOTS = [
    "USERTrust ECC Certification Authority",
    "USERTrust RSA Certification Authority",
    "Sectigo Public Server Authentication Root E46",
    "Sectigo Public Server Authentication Root R46",
]

# Where distributions keep individual PEMs, and the concatenated bundle.
CERT_DIRS = [
    pathlib.Path("/etc/ssl/certs"),
    pathlib.Path("/usr/share/ca-certificates/mozilla"),
    pathlib.Path("/etc/pki/tls/certs"),
]
FULL_BUNDLES = [
    pathlib.Path("/etc/ssl/certs/ca-certificates.crt"),
    pathlib.Path("/etc/pki/tls/certs/ca-bundle.crt"),
    pathlib.Path("/etc/ssl/cert.pem"),
]

OUT = pathlib.Path("romfs/cacert.pem")


def openssl(*args: str, stdin: bytes | None = None) -> str:
    result = subprocess.run(
        ["openssl", *args], input=stdin, capture_output=True, check=False
    )
    if result.returncode != 0:
        raise SystemExit("openssl failed: " + result.stderr.decode(errors="replace"))
    return result.stdout.decode(errors="replace")


def describe(pem: bytes) -> tuple[str, str, str]:
    """Subject CN, SHA-256 fingerprint and expiry, straight from openssl."""
    text = openssl("x509", "-noout", "-subject", "-fingerprint", "-sha256",
                   "-enddate", stdin=pem)
    subject = fingerprint = expiry = "?"
    for line in text.splitlines():
        if line.startswith("subject="):
            subject = line.split("CN=")[-1].strip()
        elif "Fingerprint=" in line:
            fingerprint = line.split("=", 1)[1].strip()
        elif line.startswith("notAfter="):
            expiry = line.split("=", 1)[1].strip()
    return subject, fingerprint, expiry


def find_root(name: str) -> bytes:
    """One named root, from wherever this machine keeps it."""
    filename = name.replace(" ", "_") + ".pem"
    for directory in CERT_DIRS:
        candidate = directory / filename
        if candidate.is_file():
            return candidate.read_bytes()

    # Not filed under its own name: walk the concatenated bundle instead.
    for bundle in FULL_BUNDLES:
        if not bundle.is_file():
            continue
        current: list[str] = []
        for line in bundle.read_text(errors="replace").splitlines():
            if line.startswith("-----BEGIN CERTIFICATE-----"):
                current = [line]
            elif current:
                current.append(line)
                if line.startswith("-----END CERTIFICATE-----"):
                    pem = ("\n".join(current) + "\n").encode()
                    if describe(pem)[0] == name:
                        return pem
                    current = []

    raise SystemExit(
        f"could not find '{name}' on this machine.\n"
        f"Looked in: {', '.join(str(d) for d in CERT_DIRS)}\n"
        f"and in: {', '.join(str(b) for b in FULL_BUNDLES)}\n"
        "Install your distribution's ca-certificates package, or download the\n"
        "root from https://letsencrypt.org/certificates/ and check its\n"
        "fingerprint against the page before using it."
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--full", action="store_true",
                        help="ship the machine's whole trust store instead")
    args = parser.parse_args()

    if shutil.which("openssl") is None:
        raise SystemExit("openssl is not on PATH; this script needs it to read certificates")

    if not OUT.parent.is_dir():
        raise SystemExit(f"run this from the repository root ({OUT.parent} not found)")

    if args.full:
        source = next((b for b in FULL_BUNDLES if b.is_file()), None)
        if source is None:
            raise SystemExit("no system CA bundle found")
        body = source.read_bytes()
        header = (
            "# pkHouse trust store -- the full system bundle.\n"
            f"# GENERATED by tools/gen_ca_bundle.py --full from {source}\n"
            "#\n"
            "# Trusts every CA this build machine trusted. Large, and mbedTLS\n"
            "# parses all of it on every handshake -- see the module comment in\n"
            "# tools/gen_ca_bundle.py before deciding you want this.\n"
        ).encode()
        certs = body.count(b"-----BEGIN CERTIFICATE-----")
        listed: list[tuple[str, str, str]] = []
    else:
        pems = [find_root(name) for name in LETSENCRYPT_ROOTS + GITHUB_ROOTS]
        listed = [describe(p) for p in pems]
        body = b"".join(pems)
        certs = len(pems)
        lines = [
            "# pkHouse trust store -- Let's Encrypt and GitHub's CA only.",
            "# GENERATED by tools/gen_ca_bundle.py. Do not edit; regenerate.",
            "#",
            "# Every Let's Encrypt chain ends at ISRG Root X1 or X2: the GTS",
            "# board and GitHub's release downloads. GitHub's API ends at",
            "# Sectigo / USERTrust: the update check. Servers send their",
            "# intermediates themselves, so the roots are all a client needs.",
            "#",
            "# This trusts those CAs and NOTHING ELSE. A board behind",
            "# Cloudflare, or on a host that issues through another CA, will not",
            "# verify against it -- rebuild with --full, or drop a bundle at",
            "# sdmc:/switch/pkHouse/cacert.pem, which overrides this at runtime.",
            "#",
        ]
        for subject, fingerprint, expiry in listed:
            lines += [f"#   {subject}",
                      f"#     expires  {expiry}",
                      f"#     sha256   {fingerprint}"]
        lines.append("#")
        lines.append("# Check those fingerprints against https://letsencrypt.org/certificates/")
        lines.append("# and https://www.sectigo.com/knowledge-base (root certificates).")
        header = ("\n".join(lines) + "\n").encode()

    OUT.write_bytes(header + body)

    # Read it back with openssl rather than trusting that concatenation worked.
    check = openssl("crl2pkcs7", "-nocrl", "-certfile", str(OUT))
    if "BEGIN PKCS7" not in check:
        raise SystemExit("the bundle that was written does not parse")

    for subject, fingerprint, expiry in listed:
        print(f"  {subject:<16} expires {expiry}")
    print(f"wrote {OUT} - {certs} certificate(s), {OUT.stat().st_size} bytes")
    if not args.full:
        print("verify the fingerprints above against https://letsencrypt.org/certificates/"
              " and Sectigo's root list")


if __name__ == "__main__":
    main()
