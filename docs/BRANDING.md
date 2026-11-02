# Builds, attribution and branding

The GPL licenses the code. Historical credit and responsibility for a particular
build are different facts. Forks and modified distributions are welcome,
including commercial distributions under the license.

## Identify a derivative accurately

- Keep required original copyright/license notices and third-party attribution.
- State that the software was modified, with relevant dates, as GPL section 5 requires.
- Identify the actual distributor and describe the build as modified or unofficial.
  An accurate description such as “Based on Cue by .tinypeep” is appropriate.
- Do not imply that Gabriel Benzoni, .tinypeep or the official Cue maintainers
  created, reviewed, support or endorse changes they did not make or approve.
- Review About, executable and installer publisher fields, icons, package names,
  screenshots, update destinations and download-page presentation together.
  Historical author credit belongs in a clearly attributed original-project context.

No blanket ban on the word Cue, requirement to surrender attribution, restriction
on forks or mandatory preservation of all Git commits is imposed here. Descriptive
use and other rights under applicable law remain available. This is not a claim
that a name or mark is registered or that all proposed uses are legally prohibited.

## Build identity

Normal source builds use `CUE_OFFICIAL_BUILD=0`: About and executable product
metadata identify an unofficial build; the original author's credit is explicitly
labelled as original-project credit. `CUE_DISTRIBUTOR` can be defined to identify
the actual distributor. Set package identifiers, install locations, publisher
registry entries, icons, links (including `CUE_CONTACT_EMAIL` in `src/core/version.h`, the
About e-mail address) and update infrastructure appropriately for an
independently distributed fork; the build marker alone is not a complete repackaging tool.

The default updater does not fetch/install official binaries for an unofficial
build. A derivative can implement its own update destination and policy.

Official .tinypeep release automation explicitly uses `make OFFICIAL=1` and
`make OFFICIAL=1 setup`. This switch is an identity declaration, not technical
protection against impersonation. It should only be used for an authorized official
release. Local builds remain fully usable without it. `build.bat` produces an
unofficial source build by default.

## Legal basis and limits

GPL sections 4–6 already address notices, modifications and corresponding source.
Section 7 permits certain additional terms, including reasonable attribution,
origin marking, publicity-name limits and reserving trademark rights. Such terms
can only be imposed on material for which the party has the appropriate rights;
they cannot simply be added retrospectively to other contributors' code.

This release adopts **no new section 7 copyright terms**. A broader binding
non-endorsement or trademark policy would need an ownership and jurisdiction
review before adoption. The original GPL text is unchanged.

References: [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.html), particularly
sections 4, 5, 7 and 10; [GNU licensing FAQ](https://www.gnu.org/licenses/gpl-faq.html).
