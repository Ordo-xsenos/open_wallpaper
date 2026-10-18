# Security policy

## Supported versions

AetherWall is in preview. Security fixes target the current `main` development
line and new preview builds. Older previews do not have a separate maintenance
or guaranteed backport policy. Check the exact affected build/commit when reporting.

## Report a vulnerability privately

Use [Report a vulnerability](https://github.com/Ordo-xsenos/open_wallpaper/security/advisories/new)
under **Security → Advisories**. This requires repository private vulnerability
reporting to be enabled. If the button is unavailable, open a public issue asking
only for a private reporting channel; do not include the vulnerability details,
exploit, secrets, or sensitive logs. Maintainers must provide a private channel
before receiving those details.

Include:

- Affected version/commit, Windows build, and relevant configuration.
- Reproduction steps and a minimal proof of concept using your own test data.
- Expected and observed behavior, impact, and prerequisites for exploitation.
- Relevant sanitized logs, affected package/parser/IPC/server paths, and any fix suggestion.

Do not use public Issues or PRs to disclose an unpatched vulnerability. Do not test
against other users' machines, libraries, or hosted services without permission.
Keep credentials, personal information, and third-party copyrighted content out of
reports. Use private advisories to coordinate fixes and disclosure; response and
resolution times are not guaranteed.

Ordinary rendering bugs and feature requests belong in the
[issue forms](https://github.com/Ordo-xsenos/open_wallpaper/issues/new/choose).
