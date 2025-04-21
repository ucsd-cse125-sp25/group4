#pragma once
#include "Renderer.h"

// return false from the function if there is a failure 
#define UNWRAP(result) if(FAILED(result)) return false

bool Renderer::Init(HWND window_handle) {

	// ----------------------------------------------------------------------------------------------------------------
	// initialize viewport and scissor
	m_viewport = {
	   .TopLeftX = 0.0f,
	   .TopLeftY = 0.0f,
	   .Width =  static_cast<float>(m_width),
	   .Height = static_cast<float>(m_height),
	   .MinDepth = D3D12_MIN_DEPTH,
	   .MaxDepth = D3D12_MAX_DEPTH,
	};
	m_scissorRect = {
		.left = 0,
		.top = 0,
		.right = static_cast<LONG>(m_width),
		.bottom = static_cast<LONG>(m_height)
	};

	UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	// ----------------------------------------------------------------------------------------------------------------
	// initialize debug controller 
	{
		ID3D12Debug* dc;

		UNWRAP(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)));
		UNWRAP(dc->QueryInterface(IID_PPV_ARGS(&m_debugController)));

		m_debugController->EnableDebugLayer();
		m_debugController->SetEnableGPUBasedValidation(true);

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		dc->Release();
	}
#endif
	ComPtr<IDXGIFactory4> factory;
	UNWRAP(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
	
	// ----------------------------------------------------------------------------------------------------------------
	// create adapter
    ComPtr<IDXGIAdapter1> adapter;
	for (UINT adapterIndex = 0;
		 DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter);
		 ++adapterIndex)
	{

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
	

	// ----------------------------------------------------------------------------------------------------------------
	// initialize devices
	UNWRAP(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
#if defined(_DEBUG)
	UNWRAP(m_device->QueryInterface(m_debugDevice.GetAddressOf()));
#endif
	

	// ----------------------------------------------------------------------------------------------------------------
	// command queue and command allocators

	D3D12_COMMAND_QUEUE_DESC queueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0 // we only have 1 gpu with index 0
	};

	// create command queue and allocator
	UNWRAP(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// ----------------------------------------------------------------------------------------------------------------
	// swap chain 
	
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FramesInFlight;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;


	ComPtr<IDXGISwapChain1> swapChain;
    UNWRAP(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        window_handle,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));
	
	// no fullscreen yet
	UNWRAP(factory->MakeWindowAssociation(window_handle, DXGI_MWA_NO_ALT_ENTER));
	
	// tbh idk what this line does
	UNWRAP(swapChain.As(&m_swapChain));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// ----------------------------------------------------------------------------------------------------------------
	// descriptor heaps 
	{
		// describe and create a render target view (RTV) descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = FramesInFlight,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			.NodeMask = 0,
		};

		UNWRAP(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	
	// ----------------------------------------------------------------------------------------------------------------
	// create frame resources
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		for (UINT n = 0; n < FramesInFlight; ++n) {
			UNWRAP(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += m_rtvDescriptorSize;
		
			// UNWRAP(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&m_commandAllocators[n])));
			UNWRAP(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
		}
	}


	// ----------------------------------------------------------------------------------------------------------------
	// create empty root signature 
	{
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob>  signature;
		ComPtr<ID3DBlob>  error;
		UNWRAP(D3D12SerializeRootSignature(
			&rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signature,
			&error
		));
		UNWRAP(m_device->CreateRootSignature(
			0,
			signature->GetBufferPointer(),
			signature->GetBufferSize(),
			IID_PPV_ARGS(&m_rootSignature)
		));
	}
	
	// ----------------------------------------------------------------------------------------------------------------
	// create pipeline state 
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;
		
		UINT compileFlags = 0;
#if defined(_DEBUG)
		compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		// --------------------------------------------------------------------
		// shader compilation 
		UNWRAP(D3DCompileFromFile(
			L"shaders.hlsl",
			nullptr,
			nullptr,
			"VSMain",
			"vs_5_0",
			compileFlags,
			0,
			&vertexShader,
			nullptr
		));

		UNWRAP(D3DCompileFromFile(
			L"shaders.hlsl",
			nullptr,
			nullptr,
			"PSMain",
			"ps_5_0",
			compileFlags,
			0,
			&pixelShader,
			nullptr
		));

		// --------------------------------------------------------------------
		// vertex layout
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{
				.SemanticName = "POSITION",
				.SemanticIndex = 0,
				.Format = DXGI_FORMAT_R32G32B32_FLOAT,
				.InputSlot = 0,
				.AlignedByteOffset = 0,
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			},
			{
				.SemanticName = "COLOR",
				.SemanticIndex = 0,
				.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.InputSlot = 0,
				.AlignedByteOffset = 12,
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			},

		};
		
		// --------------------------------------------------------------------
		// describe Pipeline State Object (PSO) 
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
			.pRootSignature = m_rootSignature.Get(),
			.VS =  CD3DX12_SHADER_BYTECODE(vertexShader.Get()),
			.PS =  CD3DX12_SHADER_BYTECODE(pixelShader.Get()),
			.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT), 
			.SampleMask = UINT_MAX,
			.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
			.DepthStencilState = {
				.DepthEnable = FALSE,
				.StencilEnable = FALSE,
			},
			.InputLayout = {inputElementDescs, _countof(inputElementDescs)},
			.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.NumRenderTargets = 1,
			.RTVFormats = {DXGI_FORMAT_R8G8B8A8_UNORM},
			.SampleDesc = {
				.Count = 1,
			},
		};

		// create the pipeline state object
		UNWRAP(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	// ----------------------------------------------------------------------------------------------------------------
	// create command list
	UNWRAP(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocators[m_frameIndex].Get(),
		nullptr,
		IID_PPV_ARGS(&m_commandList)));
	
	// we don't record any commands yet
	UNWRAP(m_commandList->Close());
	
	// ----------------------------------------------------------------------------------------------------------------
	// create vertex buffer 
	{
		// Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };
		const UINT vertexBufferSize = sizeof(triangleVertices);
		
		D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		// TODO: Optimize out the upload heap		
		UNWRAP(m_device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)
		));
		
		// copy triangle to vertex buffer
		{
			UINT8 *pVertexDataBegin; // pointer to vertex buffer CPU
			D3D12_RANGE readRange = { .Begin = 0, .End = 0 };
			// initialize vertex buffer CPU pointer
			UNWRAP(m_vertexBuffer->Map(0, &readRange, (void **)(&pVertexDataBegin))); 
			memcpy(pVertexDataBegin, triangleVertices, vertexBufferSize);
			m_vertexBuffer->Unmap(0, nullptr);
		}

		// initialize the vertex buffer view
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;

	}
	
	// create synchronization fence
	{
		UNWRAP(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValues[m_frameIndex]++; // TODO: wtf does this mean?

		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr) {
			UNWRAP(HRESULT_FROM_WIN32(GetLastError()));
		}

		UNWRAP(WaitForGpu());
	}
	return true;
}

bool Renderer::WaitForGpu() {
	// add signal to command queue
	UNWRAP(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	// wait until the fence has been processed
	UNWRAP(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// increment the fence value for the current frame
	m_fenceValues[m_frameIndex]++;
	return true;
}

bool Renderer::Render() {
	// CREATE A COMMAND LIST
	// should occur after all of its command lists have executed (use fences)
	UNWRAP(m_commandAllocators[m_frameIndex]->Reset());

	// should occur if and only if the command list has been executed
	UNWRAP(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));
	
	
	// Set necessary state
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// barrier BEFORE using the back buffer render target
	D3D12_RESOURCE_BARRIER barrier_render_target = {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = {
			.pResource =  m_renderTargets[m_frameIndex].Get(),
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
			.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
		}
	};
	m_commandList->ResourceBarrier(1,&barrier_render_target);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	// WARNING: sus; might be prone to overflow
	rtvHandle.ptr += m_frameIndex * m_rtvDescriptorSize;
	
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);


	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->DrawInstanced(3, 1, 0, 0);
	

	// barrier BEFORE presenting the back buffer 
	D3D12_RESOURCE_BARRIER barrier_present = {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = {
			.pResource =  m_renderTargets[m_frameIndex].Get(),
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
			.StateAfter = D3D12_RESOURCE_STATE_PRESENT,
		}
	};

	m_commandList->ResourceBarrier(1, &barrier_present);
	UNWRAP(m_commandList->Close());

	// COMMMAND LIST DONE
	// EXECUTE COMMAND LIST

	ID3D12CommandList *ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	UNWRAP(m_swapChain->Present(1, 0));
	
	UNWRAP(MoveToNextFrame());
	return true;
}

bool Renderer::MoveToNextFrame() {
	// schedule a signal in the command queue
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	UNWRAP(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// update the frame index
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// if the next frame is not ready to be rendered yet, wait until it is ready
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
		UNWRAP(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
	}

	// set fence value for the next frame
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
	return true;
}

Renderer::~Renderer() {
	WaitForGpu();
	CloseHandle(m_fenceEvent);
}

