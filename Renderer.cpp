#pragma once
#include "Renderer.h"

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

// return false from the function if there is a failure 
#define UNWRAP(result) if(FAILED(result)) return false

bool Renderer::init(HWND window_handle) {
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
	
	// get the device
	if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)))) return false;
	
	// get the debug device
#if defined(_DEBUG)
	if (FAILED(m_device->QueryInterface(m_debug_device.GetAddressOf()))) return false;
#endif
	
	D3D12_COMMAND_QUEUE_DESC queueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0 // we only have 1 gpu with index 0
	};
	
	// create command queue and allocator
	if (FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)))) return false;

	
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FramesInFlight;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;


	ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        window_handle,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));
	
	// no fullscreen yet
	if (FAILED(factory->MakeWindowAssociation(window_handle, DXGI_MWA_NO_ALT_ENTER))) return false;
	
	// tbh idk what this line does
	if (FAILED(swapChain.As(&m_swapChain))) return false;

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// create descriptor heap for each swap chain buffer 
	{
		// describe and create a render target view (RTV) descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FramesInFlight;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		UNWRAP(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	
	// create frame resources
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		for (UINT n = 0; n < FramesInFlight; ++n) {
			UNWRAP(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += m_rtvDescriptorSize;
		}
	}
	UNWRAP(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&m_commandAllocator)));
	

	// create command list

	UNWRAP(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&m_commandList)));
	
	// we don't record any commands yet
	UNWRAP(m_commandList->Close());
	/*
	// create a synchronization fence
	if (FAILED(
		m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))
	)) return false;
	
	// TODO: find out where this fits into things
	D3D12_RESOURCE_BARRIER barrier = {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = {
			.pResource = texResource,
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE,
			.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		}
	};
	*/
	return true;
} 