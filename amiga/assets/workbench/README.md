# Workbench icon

`harrier_amiga.exe.info` is the classic Kickstart/Workbench 1.x tool icon
installed beside `harrier_amiga.exe` by `amiga-build.ps1`. The icon uses a
64 KiB Workbench stack and therefore supports launching the game by
double-clicking it on a mounted hard disk.

The artwork is derived deterministically from the checked-in Harrier pixels
in `promoted_assets.h`:

- `harrier_amiga-icon-source.png` is the generated indexed source.
- `harrier_amiga-icon.png` is the OCS palette-validated 48x22 image.
- `harrier_amiga-icon.validation.json` records the validation result.
- `harrier_amiga.exe.info` is the generated classic DiskObject.

Regenerate the source and DiskObject with:

```powershell
python tools/create-amiga-workbench-icon.py `
  --header amiga/assets/promoted_assets.h `
  --source amiga/assets/workbench/harrier_amiga-icon-source.png `
  --png amiga/assets/workbench/harrier_amiga-icon.png `
  --info amiga/assets/workbench/harrier_amiga.exe.info
```
