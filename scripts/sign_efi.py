import os, sys, struct, shutil, subprocess

EFI_PATH = os.path.join("disk", "EFI", "BOOT", "BOOTX64.EFI")
CER_ROOT = "CustomSecureBoot.cer"
CER_DISK = os.path.join("disk", "CustomSecureBoot.cer")

def verify_pe_signature(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 0x100 or data[:2] != b"MZ":
        return 0
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    magic = struct.unpack_from("<H", data, pe_off + 24)[0]
    dd_off = pe_off + 24 + (144 if magic == 0x20B else 128)
    cert_off, cert_size = struct.unpack_from("<II", data, dd_off)
    return cert_size if (cert_off > 0 and cert_size > 0) else 0

def sign_windows():
    ps = (
        '$c = Get-ChildItem Cert:\\CurrentUser\\My | '
        'Where-Object { $_.Subject -eq "CN=sonionOS Secure Boot" } | Select-Object -First 1; '
        'if (-not $c) { '
        '$c = New-SelfSignedCertificate -Subject "CN=sonionOS Secure Boot" '
        '-Type CodeSigningCert -KeyAlgorithm RSA -KeyLength 2048 '
        '-HashAlgorithm SHA256 -CertStoreLocation "Cert:\\CurrentUser\\My" }; '
        'Export-Certificate -Cert $c -FilePath "CustomSecureBoot.cer" -Type CERT | Out-Null; '
        '$s = Set-AuthenticodeSignature -FilePath "disk\\EFI\\BOOT\\BOOTX64.EFI" '
        '-Certificate $c -HashAlgorithm SHA256; '
        'if (-not $s.SignerCertificate) { exit 1 }'
    )
    res = subprocess.run(["powershell", "-NoProfile", "-Command", ps])
    if res.returncode != 0:
        print("[ERROR] Windows Authenticode signing failed!")
        sys.exit(1)
    if os.path.exists(CER_ROOT):
        shutil.copyfile(CER_ROOT, CER_DISK)

def main():
    if not os.path.exists(EFI_PATH):
        print(f"[ERROR] {EFI_PATH} not found!")
        sys.exit(1)

    if os.name == "nt":
        sign_windows()
    elif shutil.which("sbsign") and os.path.exists("CustomSecureBoot.key"):
        subprocess.run(["sbsign", "--key", "CustomSecureBoot.key", "--cert", CER_ROOT,
                        "--output", EFI_PATH, EFI_PATH], check=True)
    else:
        print("[SECURE BOOT] Skipping signing on non-Windows host (no sbsign/key)")
        return

    sig_bytes = verify_pe_signature(EFI_PATH)
    if sig_bytes == 0:
        print("[ERROR] PE32+ Certificate Table empty after signing!")
        sys.exit(1)
    print(f"[SECURE BOOT] Signed {EFI_PATH} (PE32+ PKCS#7 Signature: {sig_bytes} bytes)")

if __name__ == "__main__":
    main()
