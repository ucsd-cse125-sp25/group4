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
	// initialize debug controller 
	{
		ID3D12Debug* dc;

		if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)))) return false;
		if (FAILED(dc->QueryInterface(IID_PPV_ARGS(&m_debug_controller)))) return false;

		m_debug_controller->EnableDebugLayer();
		m_debug_controller->SetEnableGPUBasedValidation(true);

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		dc->Release();
	}
#endif
	ComPtr<IDXGIFactory4> factory;
	if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)))) return false;
	
	// create Adapter
    ComPtr<IDXGIAdapter1> adapter;
	for (UINT adapterIndex = 0;
		 DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter);
		 ++adapterIndex) {

		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		// Don't select the Basic Render Driver adapter.
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

		// Check if the adapter supports Direct3D 12, and use that for the rest
		// of the application
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
										_uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
		// Else we won't use this iteration's adapter, so release it
		adapter->Release();
	}



	return true;
} 