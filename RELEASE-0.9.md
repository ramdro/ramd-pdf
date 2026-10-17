# RAMD PDF 0.9 Full

The full desktop release of the RAMD PDF workspace, by RAMD Studios.

## Downloads
- Windows x64: Setup.exe installer or Portable.zip.
- Linux Mint 22.x / Ubuntu 24.04 amd64: .deb installer.
- Fedora 43 x86_64: .fc43.x86_64.rpm.
- Fedora 44 x86_64: .fc44.x86_64.rpm.

Choose the package matching your operating system. All packages contain version 0.9.0. No account is required; core PDF tools work offline.

## Included
Redesigned library, light and dark themes, favorites and recent files, filtering and sorting, saved reading positions, named page bookmarks, drag-and-drop opening, PDF merging and page extraction. Existing annotation tools, templates and printing are included. The Windows executable and installer now embed the app icon.

## Validation
The release workflow requires all three test suites to pass on Windows, Ubuntu 24.04 and Fedora 43/44 before publishing. Linux packages are installed and launched in clean containers. Windows dependencies and PDF tools are tested without the development environment on PATH.

## Installation
Open the downloaded installer, or use:
- Mint/Ubuntu: sudo apt install ./ramd-pdf-0.9.0-mint22-amd64.deb
- Fedora 44: sudo dnf install ./ramd-pdf-0.9.0-1.fc44.x86_64.rpm
- Fedora 43: sudo dnf install ./ramd-pdf-0.9.0-1.fc43.x86_64.rpm

SHA256SUMS.txt contains checksums for every installer. Windows builds are not code-signed. Native desktop testing beyond the automated checks is still limited. This release does not include macOS, mobile, ARM, Mint 21 or LMDE builds.
