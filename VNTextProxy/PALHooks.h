#pragma once

#include <d3d11.h>

namespace PALGrabCurrentText {
	bool Install();
	const unsigned char* get();
}

namespace PALStateDetection {
	bool Install(HMODULE hPalDll);

	// Passive getters - call proactively to query PAL engine state.
	// Returns -1 if the function is not available.
	int CallPalTaskGetState();
	int CallPalFontGetType();
	int CallPalEffectEnableIs();
}

namespace PALVideoFix {
	bool Install();
}

namespace DirectShowVideoScale {
	bool Install();

	// DX11 video rendering support
	void InitializeDX11(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	void CleanupDX11();
}