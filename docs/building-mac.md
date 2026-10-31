## Build instructions for macOS

### Prepare folder

Choose a folder for the future build, for example **/Users/user/TBuild**. It will be named ***BuildPath*** in the rest of this document. All commands will be launched from Terminal.

**Note about disk space:** The full build process will require approximately **55 GB** of free space. This includes:
- **~35 GB** for libraries (when building for both x64 and arm64 architectures)
- **~20 GB** for the compiled Telegram app (in the `out` folder)

### Clone source code and prepare libraries

Go to ***BuildPath*** and run

    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    brew install git automake libtool cmake wget pkg-config gnu-tar ninja nasm meson

    sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

    git clone --recursive https://github.com/AyuGram/AyuGramDesktop.git tdesktop
    ./tdesktop/Telegram/build/prepare/mac.sh

### Building the project

Go to ***BuildPath*/tdesktop/Telegram** and run

    ./configure.sh -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627

Then launch Xcode, open ***BuildPath*/tdesktop/out/Telegram.xcodeproj** and build for Debug / Release.

### Custom macOS builds

The `mac` GitHub Actions workflow accepts two inputs through **Run workflow**:

| Input | Default | Purpose |
| --- | --- | --- |
| `bundle_id` | `one.ayugram.AyuGramDesktop` | Identifies the application and its data directory. |
| `app_name` | `AyuGram` | Names the application bundle and executable, without the `.app` extension. |

Application names support ASCII letters, digits, spaces, underscores and hyphens,
starting with a letter or digit and without trailing spaces. Bundle identifiers
use dot-separated components containing ASCII letters, digits or hyphens.
Tag builds use the defaults. Each run produces an ad hoc signed arm64 application
in `<bundle_id>-mac-arm64.zip`.

For two independent applications, run the workflow twice with different bundle
identifiers and names. Keep each bundle identifier unchanged on subsequent builds.

Non-App Store builds now store data in
`~/Library/Application Support/<bundle_id>/`, including `tdata`, settings and
cache. Changing the name keeps that profile; changing the bundle id
selects a different one. The old `AyuGram Desktop` directory is not migrated
automatically, so the first launch with the new path starts with an empty
profile. Existing data remains in its original directory.

Explicit `-workdir` arguments and `TelegramForcePortable` still
override the normal data path. Do not point two applications at the same data
directory. Both applications can register `tg://` links, but macOS chooses one
default handler for that scheme. Automatic updates are disabled in this workflow;
install subsequent builds manually.

The same options are available when configuring locally:

```sh
./configure.sh \
  -D TDESKTOP_API_ID=2040 \
  -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627 \
  -D AYUGRAM_MAC_BUNDLE_ID=one.ayugram.Second \
  -D AYUGRAM_MAC_APP_NAME="AyuGram Second"
```
