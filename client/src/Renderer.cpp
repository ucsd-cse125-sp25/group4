#pragma once
#include "Renderer.h"
#include "ReadData.h"


int __UNUSED;
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
	ComPtr<ID3D12Device> tempDevice;
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
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&tempDevice))))
		{
			printf("device found\n");
			D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{D3D_SHADER_MODEL_6_6};

			if (FAILED(tempDevice->CheckFeatureSupport(
				D3D12_FEATURE_SHADER_MODEL,
				&shaderModel,
				sizeof(shaderModel)
			))) {
				printf("Device does not support SM 6.6. Searching for other devices... \n");
				continue;
			}
			
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
	
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
		.Width = m_width,
		.Height = m_height,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {
			.Count = 1,
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = FramesInFlight,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
	};


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

		// create constant buffer view (CBV) descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, // descriptor heap can be referenced by a root table
			.NumDescriptors = 2,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, // descriptor heap should be bound to the pipeline
		};
		UNWRAP(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));

		// create Depth Stencil View (DSV) descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			.NumDescriptors = 1,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};
		UNWRAP(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_depthStencilDescriptorHeap)));
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
	// create root signature 
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {
			.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1,
		};
		
		// fall back to older version of root signature featureset if 1.1 is not available
		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}
		
		D3D12_DESCRIPTOR_RANGE1 ranges[1] = { {
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
			.NumDescriptors = 1,
			.BaseShaderRegister = 0,
			.RegisterSpace = 0,
			.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC,
			.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
		} };

		D3D12_ROOT_PARAMETER1 parameters[ROOT_PARAMETERS_COUNT] = {};
		// currently, this descriptor table is not used
		parameters[ROOT_PARAMETERS_DESCRIPTOR_TABLE] = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1,
				.pDescriptorRanges = &ranges[0],
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX,
		};
		// modelViewProject matrix
		parameters[ROOT_PARAMETERS_CONSTANT_MODEL_VIEW_PROJECT] = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = 1,
				.RegisterSpace = 0,
				.Num32BitValues = 16,
			}
		};
		// may add more parameters in the future for indices of resources

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
			.Desc_1_1 = {
				.NumParameters = ROOT_PARAMETERS_COUNT,
				.pParameters = parameters,
				.NumStaticSamplers = 0,
				.pStaticSamplers = nullptr,
				.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED,
			}
		};
 
		ComPtr<ID3DBlob>  signature;
		ComPtr<ID3DBlob>  error;
		UNWRAP(D3DX12SerializeVersionedRootSignature(
			&rootSignatureDesc,
			featureData.HighestVersion,
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
		
		UINT compileFlags = 0;
#if defined(_DEBUG)
		compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		// --------------------------------------------------------------------
		// describe Pipeline State Object (PSO) 
		
		// TODO: use slices instead
		std::vector<uint8_t> vertexShaderBytecode = DX::ReadData(L"vs.cso");
		std::vector<uint8_t> pixelShaderBytecode = DX::ReadData(L"ps.cso");


		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
			.pRootSignature = m_rootSignature.Get(),
			.VS = {
				.pShaderBytecode = vertexShaderBytecode.data(),
				.BytecodeLength = vertexShaderBytecode.size(),
			},
			.PS = {
				.pShaderBytecode = pixelShaderBytecode.data(),
				.BytecodeLength  = pixelShaderBytecode.size(),
			},
			.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
			.SampleMask = UINT_MAX,
			.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
			.DepthStencilState = {
				.DepthEnable = TRUE,
				.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
				.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
				.StencilEnable = FALSE,
			},
			.InputLayout = {},
			.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.NumRenderTargets = 1,
			.RTVFormats = {DXGI_FORMAT_R8G8B8A8_UNORM},
			.SampleDesc = {
				.Count = 1,
			},
		};
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // DEBUG: disable culling

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
		
		// 6 faces
		// 2 triangles per face
		// 3 vertices per triangle
		// 3 floats per vertex
	    Vertex cubeverts[6 * 2 * 3] = {
	    	{ { -1.0f, -1.0f, -1.0 } },{ { -1.0f, -1.0f, 1.0 } },{ { -1.0f, 1.0f, 1.0 } },{ { -1.0f, -1.0f, -1.0 } },{ { -1.0f, 1.0f, 1.0 } },{ { -1.0f, 1.0f, -1.0 } },{ { -1.0f, 1.0f, -1.0 } },{ { -1.0f, 1.0f, 1.0 } },{ { 1.0f, 1.0f, 1.0 } },{ { -1.0f, 1.0f, -1.0 } },{ { 1.0f, 1.0f, 1.0 } },{ { 1.0f, 1.0f, -1.0 } },{ { 1.0f, 1.0f, -1.0 } },{ { 1.0f, 1.0f, 1.0 } },{ { 1.0f, -1.0f, 1.0 } },{ { 1.0f, 1.0f, -1.0 } },{ { 1.0f, -1.0f, 1.0 } },{ { 1.0f, -1.0f, -1.0 } },{ { 1.0f, -1.0f, -1.0 } },{ { 1.0f, -1.0f, 1.0 } },{ { -1.0f, -1.0f, 1.0 } },{ { 1.0f, -1.0f, -1.0 } },{ { -1.0f, -1.0f, 1.0 } },{ { -1.0f, -1.0f, -1.0 } },{ { -1.0f, 1.0f, -1.0 } },{ { 1.0f, 1.0f, -1.0 } },{ { 1.0f, -1.0f, -1.0 } },{ { -1.0f, 1.0f, -1.0 } },{ { 1.0f, -1.0f, -1.0 } },{ { -1.0f, -1.0f, -1.0 } },{ { 1.0f, 1.0f, 1.0 } },{ { -1.0f, 1.0f, 1.0 } },{ { -1.0f, -1.0f, 1.0 } },{ { 1.0f, 1.0f, 1.0 } },{ { -1.0f, -1.0f, 1.0 } },{ { 1.0f, -1.0f, 1.0 } }
	    };
		// TEST: bindless vertex buffer
		{
			const Slice<Vertex> cubeVertsSlice = {
				.ptr = cubeverts,
				.len = _countof(cubeverts),
			};

			m_vertexBufferBindless.Init(m_device.Get(), cubeVertsSlice, L"Bindless Vertex Buffer");

			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
				.Format = DXGI_FORMAT_UNKNOWN,
				.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Buffer = {
					.FirstElement = 0,
					.NumElements = cubeVertsSlice.len,
					.StructureByteStride = sizeof(Vertex)
}
			};

			// TODO: Refactor holy shit this is awful
	        D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
			UINT cbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			D3D12_CPU_DESCRIPTOR_HANDLE vertexBufferBindlessDescriptorCPUHandle = { cbvHandle.ptr + cbvDescriptorSize };
			m_device->CreateShaderResourceView(m_vertexBufferBindless.resource.Get(), &desc, vertexBufferBindlessDescriptorCPUHandle);
		}
	}
		
	// ----------------------------------------------------------------------------------------------------------------
	// create constant buffer 
	{
		m_constantBufferData.viewProject = computeViewProject(playerState.pos, playerState.lookDir);

		const UINT constantBufferSize = sizeof(SceneConstantBuffer);
		D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD); 
		D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
		UNWRAP(m_device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_constantBuffer)
		));

		// create constant buffer view
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {
			.BufferLocation = m_constantBuffer->GetGPUVirtualAddress(),
			.SizeInBytes = constantBufferSize,
		};
		m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
		
		// initialize constant buffer
		// we don't unmap until the app closes
		D3D12_RANGE readRange = { 0, 0 };
		UNWRAP(m_constantBuffer->Map(0, &readRange, (void **)(&m_pCbvDataBegin)));
		memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
	}
	// ----------------------------------------------------------------------------------------------------------------
	// create depth stencil buffer 
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {
			.Format = DXGI_FORMAT_D32_FLOAT,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE,
		};

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {
			.Format = DXGI_FORMAT_D32_FLOAT,
			.DepthStencil = {
				.Depth = 1.0f,
				.Stencil = 0,
			}
		};

		D3D12_HEAP_PROPERTIES depthStencilHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC depthStencilBufferResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_D32_FLOAT, m_width, m_height,
			1, 0,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		UNWRAP(m_device->CreateCommittedResource(
			&depthStencilHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilBufferResourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, 
			IID_PPV_ARGS(&m_depthStencilBuffer)
		));
		m_depthStencilDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");
		m_device->CreateDepthStencilView(
			m_depthStencilBuffer.Get(),
			&depthStencilDesc,
			m_depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
		);
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

XMMATRIX Renderer::computeViewProject(FXMVECTOR pos, LookDir lookDir) {
	/*
	XMVECTOR lookVec = XMVECTORF32{0, 0, -1, 0}; // start by looking down
	lookVec = XMVector3Transform(lookVec, XMMatrixRotationX(lookDir.pitch)); // look up/down
	lookVec = XMVector3Transform(lookVec, XMMatrixRotationZ(lookDir.yaw)); // look left/right
	const XMVECTOR up = XMVECTORF32{ 0, 0, 1, 0 }; // Z is up

	XMMATRIX view = XMMatrixLookAtLH(pos, XMVectorZero(), up);
	XMMATRIX projected = view * XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, 0.01, 100);
	XMMATRIX transposed = XMMatrixTranspose(projected);
	return transposed;
	*/
	return XMMatrixTranspose(
		XMMatrixLookAtLH(XMVECTORF32{ 0, 0, 33, 1 }, XMVectorZero(), XMVECTORF32{ 0, 1, 0, 0 })
		* XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, 0.01, 100)
	);
}

XMMATRIX Renderer::computeModelMatrix(PlayerRenderState &playerRenderState) {
	XMMATRIX rotate = XMMatrixRotationZ(playerRenderState.lookDir.yaw);
	XMMATRIX translate = XMMatrixTranslation(playerRenderState.pos.x, playerRenderState.pos.y, playerRenderState.pos.z);
	return XMMatrixTranspose(rotate * translate);
}

bool Renderer::Render() {
	// CREATE A COMMAND LIST
	// should occur after all of its command lists have executed (use fences)
	UNWRAP(m_commandAllocators[m_frameIndex]->Reset());

	// should occur if and only if the command list has been executed
	UNWRAP(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));
	
	
	// Set necessary state
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	
	// set heaps for constant buffer
	ID3D12DescriptorHeap *ppHeaps[] = {m_cbvHeap.Get()};
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());

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


	// clear buffers
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(m_depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	
	XMMATRIX viewProject = computeViewProject(playerState.pos, playerState.lookDir);

	for (UINT8 i = 0; i < 4; ++i) {
		XMMATRIX modelMatrix = computeModelMatrix(players[i]);
		XMMATRIX modelViewProjectMatrix = viewProject * modelMatrix;
		m_commandList->SetGraphicsRoot32BitConstants(1, 16, &modelViewProjectMatrix, 0);
		m_commandList->DrawInstanced(m_vertexBufferBindless.data.len, 1, 0, 0);
	}

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

void Renderer::OnUpdate() {
	m_constantBufferData.viewProject = computeViewProject(playerState.pos, playerState.lookDir);
	memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
}