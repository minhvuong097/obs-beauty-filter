# Security policy

## Supported versions

| Version | Supported |
|---|---|
| 1.8.x | ✅ |
| 1.7.x | ✅ (fixes are backported only for security issues) |
| ≤ 1.6.x | ❌ |

Only builds published from this repository's GitHub Actions runs are supported.
If you rebuild from source, you are responsible for the toolchain and the OBS
version you link against.

## Reporting a vulnerability

Please **do not open a public issue** for a security problem.

Use GitHub's private reporting flow:
[Security → Report a vulnerability](https://github.com/minhvuong097/obs-beauty-filter/security/advisories/new).
It creates a private advisory visible only to the maintainer.

Include:

- the plugin version shown in the OBS log
  (`[obs-beauty-filter] version 1.8.0 loaded`) or in the properties page;
- OBS version, OS and GPU/driver;
- what an attacker can achieve and the steps to reproduce;
- a crash dump or OBS log if the plugin crashed.

There is **no paid bug bounty**. This is a volunteer project; reports are handled
on a best-effort basis, normally within 7 days. Once a fix is ready it is
released as a new minor version, and the advisory is published with credit
unless you ask to stay anonymous.

## Scope

In scope:

- memory-safety bugs in the plugin (out-of-bounds reads/writes, use-after-free,
  integer overflow in size calculations);
- shader or effect files that can crash or hang OBS;
- the Windows installer and the ZIP/tarball layouts (path traversal, writing
  outside the plugin directory, privilege escalation);
- the release pipeline (a published asset that differs from what CI built).

Out of scope:

- vulnerabilities in OBS Studio itself, Qt, Mesa, or the GPU driver — report
  those upstream;
- a malicious OBS scene collection or third-party plugin;
- the demo photo or the brand assets being usable without attribution;
- "the filter can be used to alter someone's appearance" — that is the feature.

## Verifying a download

Every release ships `SHA256SUMS.txt`. On Linux/macOS:

```sh
sha256sum -c SHA256SUMS.txt
```

On Windows (PowerShell):

```powershell
Get-FileHash -Algorithm SHA256 .\obs-beauty-filter-1.8.0-windows-x64-setup.exe
```

Compare the value with `SHA256SUMS.txt` and with the checksum printed in the
GitHub Actions log for that tag. The Windows installer is **not** Authenticode
signed — a code-signing certificate is a paid subscription and this project has
no funding. Verify the checksum before choosing **More info → Run anyway** in
SmartScreen. The same applies to macOS: builds are not notarized (that requires
a paid Apple Developer account); Gatekeeper will warn on first launch.

## Safe defaults

- The plugin performs no network access, spawns no process and reads no file
  outside its own data directory and the user's custom-preset JSON.
- Custom presets are stored under the OBS user configuration; malformed JSON is
  rejected, not executed.
- No analytics, telemetry or auto-update.
