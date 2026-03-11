# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['png2pgd_ge.py'],
    pathex=[],
    binaries=[],
    datas=[],
    hiddenimports=['numpy', 'cv2', 'xxhash', 'numba', 'pgd_optimizer', 'pgd_numba_accelerator', 'pgd_gpu_accelerator', 'pgd_promax_optimizer', 'progress_utils'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='png2pgd_ge',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
