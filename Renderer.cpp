#pragma once
#include "Renderer.h"

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

bool Renderer::init() {
	UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	// Create a Debug Controller to track errors
	ID3D12Debug* dc;
	if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)))) return false;
	if (FAILED(dc->QueryInterface(IID_PPV_ARGS(&m_debug_controller)))) return false;
	m_debug_controller->EnableDebugLayer();
	m_debug_controller->SetEnableGPUBasedValidation(true);
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	dc->Release();
#endif
	
	ComPtr<IDXGIFactory4> factory;
	return true;
}
