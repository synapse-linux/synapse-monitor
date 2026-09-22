<!-- SPDX-License-Identifier: MIT -->
# Alpha 13 source status

Alpha 13 is a development source candidate on the integration line, not an
accepted release or authorization to install it. Its operational C17 core,
transport, typed model and read-only authority remain those of the retained
Alpha 12 baseline. The documents naming Alpha 12 describe that inherited
operational contract; localization and split installation are the Alpha 13 delta.

The qualified Alpha 12 commit is retained as ancestry, not imported under a new
identity or rewritten. The Alpha 13 source adds 64 embedded locale catalogs,
bounded locale selection, English fallback, RTL presentation, explicit headless
build behavior and disjoint core/GUI install targets. Only en_US and it_IT contain
authored translations; the other 62 catalogs remain explicit English fallback.

Source publication does not transfer a local package's identity to a public
archive. Consumers must pin the exact admitted commit and hash the archive they
actually retrieve. Git archive metadata and documentation amendments can change
archive/package bytes without changing operational source. Existing local
qualification artifacts retain their original inputs and names.

## Remaining gates

- Review all required translations and native font, layout, RTL and accessibility
  behavior. Catalog presence is not production-complete translation.
- Pin and qualify the complete maintained CachyOS composition and coherent Qt/libc
  cohort. Software/offscreen execution does not establish native Wayland,
  compositor, multi-monitor, hardware or physical resource acceptance.
- Build and qualify real packages against their final public source identity,
  with dependency, reproducibility, hardening, ownership and lifecycle checks.
- Obtain distinct release/signing/publication and target deployment approval.
- Adopt the future shared graphical-plugin API through a separately specified
  contract and pilot. A desktop entry is not plugin compliance.
- Keep the console contract provisional until explicitly stabilized; then supply
  the required localized man pages while preserving every CLI literal.

## License scope

The existing MIT License applies to first-party Monitor source, tests and build
files. SPDX completion records that existing grant; it is not a license migration.
Qt, system libraries, fonts and other dependencies retain their own terms. No
third-party font, icon collection, executable or model is embedded in this source
candidate. The pinned locale inventory retains its upstream provenance metadata;
its quality fields do not describe Monitor translation completeness.
