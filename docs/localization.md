<!-- SPDX-License-Identifier: MIT -->
# GUI localization and packaging boundary

The package embeds exactly the 64 locale identifiers pinned in
`gui/i18n/locales.json`. Its upstream quality fields describe the CachyOS
inventory, **not** Monitor translation quality. Monitor has 157 stable translation
IDs per catalog. Only en_US and it_IT contain authored copy; 62 catalogs contain
explicit English development fallback. `coverage.json` records authored/fallback
counts and `productionComplete: false`. Native review of new locale layouts and
fully reviewed translations remain release gates.

`tools/catalogs.py --check` validates the exact catalog set, IDs, placeholders,
coverage, explicit fallback comments and generated native allowlist. Generation
refuses to overwrite any additional catalog. Every catalog still passes
`lrelease -fail-on-unfinished -fail-on-invalid`; unfinished content is not hidden
by disabling the compiler checks or pretending English copy is a translation.
Future authored translations require a reviewed coverage/tooling change rather
than silently changing the fallback claim.

Launch selection accepts a bounded pinned locale ID, hyphenated region spelling,
UTF-8/utf8 suffix, or the Qt canonical name for that pinned ID. The documented
base aliases en/it/cs/fi/tr/pt select en_US/it_IT/cs_CZ/fi_FI/tr_TR/pt_PT.
Unknown or malformed requests (including prefix lookalikes and arbitrary
modifiers) select en_US. Empty launch requests use the first nonempty LC_ALL,
LC_MESSAGES, LANG in that order; missing/unknown values still select en_US.

The native owner loads English first and a selected package-owned catalog second.
Missing messages use English; a missing selected catalog resets to English and
LTR. A missing English resource fails initialization. No filesystem path, URL,
shell evaluation or remote translation is accepted. Language does not become
backend argv. No translation selector or transport access is added to QML.

Arabic, Persian and Hebrew select RTL before QML creation. Owned QML inherits
layout mirroring; this is tested offscreen, not a claim of native accessibility
or complete script/font coverage. Generic monospace typography is preserved.

The CLI/TUI remains deterministic en_US and contracts remain locale-neutral.
Information commands are compared byte-for-byte across all 64 locale requests.
The command surface is still provisional, so this change does not declare the
CLI definitive or bypass the later 64-locale man-page gate.

`install-cli` and `install-gui` have disjoint payload ownership. The GUI package
requires the exact matching core version/release. Neither payload installs a
collector, service, autostart or user state. Missing optional collector data stays
unavailable. This remains a standalone graphical frontend awaiting the separately
specified shared plugin contract/pilot, not a compliance claim based on a launcher.
