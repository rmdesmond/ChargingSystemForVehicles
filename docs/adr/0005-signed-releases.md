## Options and decisions

1. **Where the version string comes from.** (a) `git describe` at configure
time, written by `configure_file`. (b) The same command at build time,
from a custom target that rewrites `version.cpp` only when the string
changes. **Decision: (b).** With (a), tagging a commit and rebuilding
gives a binary that still says the old string until someone re-runs
cmake; a release whose name and contents disagree is the kind of mistake
nobody notices until a device reports it. (b) costs one `git` call per
build and relinks nothing on an ordinary build. `scripts/package.sh`
asks git the same question, so the file name and the binary agree.

2. **How the signing job gets the password.** minisign reads the password
from stdin when stdin is not a terminal (checked with 0.11), so the
secret key file and its password are two repository secrets and
`scripts/sign-release.sh` pipes the password in. Alternative: an
unencrypted key (`minisign -G -W`), one secret instead of two. Kept the
password: a leaked secret store then still needs a second secret, and
the same key file works on a developer machine with a prompt.

3. **Which public key the installer trusts.** The key is a string inside
`scripts/install-release.sh`, not a file read from the tarball or
downloaded next to it. A tarball an attacker can replace is a tarball
whose key an attacker can replace. `keys/release.pub` is committed for
people and for the signing job's self-check; a test fails if the two
ever differ.

4. **The signing job runs after the checks, in the same workflow.** The
`release` job `needs` build, lint, test, sil and trace on the tagged
commit. Alternative: a separate `release.yml` that trusts the tag. A tag
can be pushed on a commit that never saw a pull request; this way it is
tested first, at the cost of two extra minutes on a tag push, which is
not a pull request and so is outside the 10-minute rule.

5. **Install layout on the device.** `/opt/charger/releases/<version>/`
plus a `current` symlink flipped atomically after verification; the unit
starts `/opt/charger/current/bin/charger-daemon`. Alternative: overwrite
`/opt/charger/bin` in place. The symlink is what makes "a refused
release leaves the running one untouched" true by construction, and it
is the layout `scripts/deploy.sh` (dev loop, `releases/dev`) and
tomorrow's RAUC slot both fit into.

6. **minisign in the build image.** One more pinned package
(`minisign=0.11-1`) so the installer's refusal of a tampered release is a
test that runs on every pull request, not a bench anecdote. Alternative:
keep the refusal a Pi-only check. The image rebuild is a one-time cost.
Not done, on purpose: no configuration file in the package (the daemon has
none yet; `etc/` arrives with the OCPP client), and no hardware check in the
installer (the `hardware=` line in `MANIFEST` is for the OTA bundle).
