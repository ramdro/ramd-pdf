# Windows builds

Open Actions > Build Windows installer. A successful run provides RAMD-PDF-Windows-x64 under Artifacts, containing the EXE installer and portable ZIP. After merging, Run workflow can start another build manually.

The workflow builds on Windows with MSYS2 UCRT64, tests PDF saving, bundles Qt/Poppler/qpdf DLLs, tests without the development PATH, and generates a per-user installer with Inno Setup. It does not publish a GitHub release or change the website.

These are unsigned test builds. Test the installer and PDF workflows on a clean Windows computer before releasing. A startup smoke test is not full manual validation. Runtime license notices, package versions, and a project source snapshot are included. Review app licensing and third-party source obligations before public distribution.
