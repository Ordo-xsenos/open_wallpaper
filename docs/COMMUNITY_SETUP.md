# GitHub community setup

Files in Git cannot set the repository's About description or enable private
vulnerability reporting. Repository administrators must apply these settings once.
In the current development environment both API writes failed with
`error connecting to api.github.com`; neither external setting is verified as done.

From an authenticated GitHub CLI session with repository administration access:

```sh
gh repo edit Ordo-xsenos/open_wallpaper --description 'Live wallpapers for Windows 11 with Direct3D 11 scenes, multi-monitor support, and a Qt manager.'
gh api --method PUT repos/Ordo-xsenos/open_wallpaper/private-vulnerability-reporting
gh api repos/Ordo-xsenos/open_wallpaper --jq .description
gh api repos/Ordo-xsenos/open_wallpaper/private-vulnerability-reporting --jq .enabled
```

Verify the exact description and `true`, then check **Security → Advisories →
Report a vulnerability**. GitHub's private vulnerability reporting feature requires
a public repository and an owner/admin or other eligible security role. If the
repository is private or your policy disallows the feature, maintainers must supply
a private contact before collecting sensitive reports. SECURITY.md describes the
safe fallback when the button is unavailable.

After merging the files into the default branch, review **Insights → Community
Standards** (or `/community`) and confirm README, Code of conduct, Contributing,
License, Security policy, Issue templates, and Pull request template are detected.
The Description item depends on the API/UI change above. Test **New issue** to see
Bug report and Feature request forms plus the private security link.

Project documents and forms use English; README translations are linked in their
first line. Changes to shared installation/feature claims should be reflected in
all three versions. No app UI language default is changed by this documentation.
