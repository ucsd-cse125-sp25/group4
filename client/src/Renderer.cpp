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
	UNWRAP(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_device)));
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

	// read files
	{
		// frame-independent and textures
		m_scene.ReadToCPU(L"bedroomv5.jj");
		m_hunterRenderBuffers.ReadToCPU(L"monsterv2.jj");
		m_runnerRenderBuffers.ReadToCPU(L"playerDOLLv4_modified.jj");

	}
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
		
		uint32_t numSceneTextures = m_scene.header->numTextures;
		uint32_t numSceneBuffers = m_scene.getNumBuffers();
		uint32_t numSceneDescriptors = numSceneTextures + numSceneBuffers;

		uint32_t numHunterTextures = m_hunterRenderBuffers.header->numTextures;
		uint32_t numHunterBuffers = m_hunterRenderBuffers.getNumBuffers() + (2 * HUNTER_ANIMATION_COUNT);
		uint32_t numHunterDescriptors = numHunterTextures + numHunterBuffers;

		uint32_t numRunnerTextures = m_runnerRenderBuffers.header->numTextures;
		uint32_t numRunnerBuffers = m_runnerRenderBuffers.getNumBuffers() + (2 * RUNNER_ANIMATION_COUNT);
		uint32_t numRunnerDescriptors = numRunnerTextures + numRunnerBuffers;

		uint32_t numTimerUITextures = 3; // clock base, hand, top
		uint32_t numTimerUIVertexBuffers = 3;

		uint32_t numShopUIVertexBuffers = 2;
		uint32_t numShopUITextures = PowerupInfo.size() + 20; // 10 for each counter of souls and coins

		uint32_t numScreenVertexBuffers = 1;
		uint32_t numScreenUITextures = 3; // win, lose, start
		uint32_t capacity = 
			  numSceneDescriptors + numHunterDescriptors + numRunnerDescriptors + 
			+ numTimerUITextures + numTimerUIVertexBuffers
			+ numShopUITextures + numShopUIVertexBuffers
			+ numScreenVertexBuffers + numScreenUITextures;
		// all resource descriptors go here.
		// THIS SHOULD BE CHANGED WHEN ADDING UI ELEMENTS ..
		m_resourceDescriptorAllocator.Init(
			m_device.Get(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			capacity,
			L"Resource Descriptor Heap");

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
		D3D12_DESCRIPTOR_RANGE1 ranges[1] = {
			{
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			.NumDescriptors = m_resourceDescriptorAllocator.capacity,
			.BaseShaderRegister = 0,
			.RegisterSpace = 0,
			.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
			.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
			}
		};

		D3D12_ROOT_PARAMETER1 parameters[ROOT_PARAMETERS_COUNT] = {};
		parameters[ROOT_PARAMETERS_DESCRIPTOR_TABLE] = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1,
				.pDescriptorRanges = &ranges[0],
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX,
		};
		// modelViewProject matrix
		parameters[ROOT_PARAMETERS_CONSTANT_PER_CALL] = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = 1,
				.RegisterSpace = 0,
				.Num32BitValues = max(DRAW_CONSTANT_NUM_DWORDS, DRAW_CONSTANT_PLAYER_NUM_DWORDS),
			},
		};
		// may add more parameters in the future for indices of resources

		D3D12_STATIC_SAMPLER_DESC sampler = {
			.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.MipLODBias = 0,
			.MaxAnisotropy = 0,
			.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
			.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
			.MinLOD = 0.0f,
			.MaxLOD = D3D12_FLOAT32_MAX,
			.ShaderRegister = 0,
			.RegisterSpace = 0,
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		};

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
			.Desc_1_1 = {
				.NumParameters = ROOT_PARAMETERS_COUNT,
				.pParameters = parameters,
				.NumStaticSamplers = 1,
				.pStaticSamplers = &sampler,
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
	// create pipeline states
	{
		
		UINT compileFlags = 0;
#if defined(_DEBUG)
		compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
		{
			// --------------------------------------------------------------------
			// describe Main Pipeline State Object (PSO) 
			
			Slice<BYTE> vertexShaderBytecode;
			if (DX::ReadDataToSlice(L"vs.cso", vertexShaderBytecode) != DX::ReadDataStatus::SUCCESS) return false;
			Slice<BYTE> pixelShaderBytecode;
			if (DX::ReadDataToSlice(L"ps.cso", pixelShaderBytecode) != DX::ReadDataStatus::SUCCESS) return false;


			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
				.pRootSignature = m_rootSignature.Get(),
				.VS = {
					.pShaderBytecode = vertexShaderBytecode.ptr,
					.BytecodeLength = vertexShaderBytecode.len,
				},
				.PS = {
					.pShaderBytecode = pixelShaderBytecode.ptr,
					.BytecodeLength  = pixelShaderBytecode.len,
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
				.DSVFormat = DXGI_FORMAT_D32_FLOAT,
				.SampleDesc = {
					.Count = 1,
				},
			};
			psoDesc.RasterizerState.FrontCounterClockwise = true;
			// psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // DEBUG: disable culling

			// create the pipeline state object
			UNWRAP(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
		}
		{
			// --------------------------------------------------------------------
			// describe Player Pipeline State Object (PSO) 
			
			Slice<BYTE> vertexShaderBytecode;
			if (DX::ReadDataToSlice(L"skin_vs.cso", vertexShaderBytecode) != DX::ReadDataStatus::SUCCESS) return false;
			Slice<BYTE> pixelShaderBytecode;
			if (DX::ReadDataToSlice(L"skin_ps.cso", pixelShaderBytecode) != DX::ReadDataStatus::SUCCESS) return false;


			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
				.pRootSignature = m_rootSignature.Get(),
				.VS = {
					.pShaderBytecode = vertexShaderBytecode.ptr,
					.BytecodeLength = vertexShaderBytecode.len,
				},
				.PS = {
					.pShaderBytecode = pixelShaderBytecode.ptr,
					.BytecodeLength  = pixelShaderBytecode.len,
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
				.DSVFormat = DXGI_FORMAT_D32_FLOAT,
				.SampleDesc = {
					.Count = 1,
				},
			};
			psoDesc.RasterizerState.FrontCounterClockwise = true;
			// psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // DEBUG: disable culling

			// create the pipeline state object
			UNWRAP(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateSkin)));
		}
		/*
		{
			// --------------------------------------------------------------------
			// describe Debug Pipeline State Object (PSO) 
			
			Slice<BYTE> vertexShaderBytecode;
			if (DX::ReadDataToSlice(L"dbg_cube_vs.cso", vertexShaderBytecode) != DX::ReadDataStatus::SUCCESS) return false;
			Slice<BYTE> pixelShaderBytecode;
			if (DX::ReadDataToSlice(L"dbg_cube_ps.cso", pixelShaderBytecode) != DX::ReadDataStatus::SUCCESS) return false;


			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
				.pRootSignature = m_rootSignature.Get(),
				.VS = {
					.pShaderBytecode = vertexShaderBytecode.ptr,
					.BytecodeLength = vertexShaderBytecode.len,
				},
				.PS = {
					.pShaderBytecode = pixelShaderBytecode.ptr,
					.BytecodeLength  = pixelShaderBytecode.len,
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
				.DSVFormat = DXGI_FORMAT_D32_FLOAT,
				.SampleDesc = {
					.Count = 1,
				},
			};
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // DEBUG: disable culling

			// create the pipeline state object
			UNWRAP(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateDebug)));
		}*/
		{
			// --------------------------------------------------------------------
			// describe Timer UI Pipeline State Object (PSO) 

			Slice<BYTE> vertexShaderBytecode;
			if (DX::ReadDataToSlice(L"timerui_vs.cso", vertexShaderBytecode) != DX::ReadDataStatus::SUCCESS) return false;
			Slice<BYTE> pixelShaderBytecode;
			if (DX::ReadDataToSlice(L"timerui_ps.cso", pixelShaderBytecode) != DX::ReadDataStatus::SUCCESS) return false;

			// alpha blending for UI:
			D3D12_BLEND_DESC blendDesc = {
				.AlphaToCoverageEnable = FALSE,
				.IndependentBlendEnable = FALSE,
			};
			blendDesc.RenderTarget[0] = {
				.BlendEnable = TRUE,
				.LogicOpEnable = FALSE,
				.SrcBlend = D3D12_BLEND_SRC_ALPHA,
				.DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
				.BlendOp = D3D12_BLEND_OP_ADD,
				.SrcBlendAlpha = D3D12_BLEND_ONE,
				.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
				.BlendOpAlpha = D3D12_BLEND_OP_ADD,
				.LogicOp = D3D12_LOGIC_OP_NOOP,
				.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
			};

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
				.pRootSignature = m_rootSignature.Get(),
				.VS = {
					.pShaderBytecode = vertexShaderBytecode.ptr,
					.BytecodeLength = vertexShaderBytecode.len,
				},
				.PS = {
					.pShaderBytecode = pixelShaderBytecode.ptr,
					.BytecodeLength = pixelShaderBytecode.len,
				},
				.BlendState = blendDesc,
				.SampleMask = UINT_MAX,
				.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
				// NO DEPTH for UI
				.DepthStencilState = {
					.DepthEnable = FALSE,
					.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
					.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS,
					.StencilEnable = FALSE,
				},
				.InputLayout = {},
				.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				.NumRenderTargets = 1,
				.RTVFormats = {DXGI_FORMAT_R8G8B8A8_UNORM},
				.DSVFormat = DXGI_FORMAT_D32_FLOAT,
				.SampleDesc = {
					.Count = 1,
				},

			};
			psoDesc.RasterizerState.FrontCounterClockwise = true;
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // DEBUG: disable culling

			UNWRAP(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateTimerUI)));
		}
		
	}

	// ----------------------------------------------------------------------------------------------------------------
	// create command list
	UNWRAP(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocators[m_frameIndex].Get(),
		nullptr,
		IID_PPV_ARGS(&m_commandList)));
	
	UNWRAP(m_commandList->Close());
	
	
	
	// ----------------------------------------------------------------------------------------------------------------
	// create vertex buffer for debug cubes
	
	{
		debugCubes.Init(m_device.Get(), &m_resourceDescriptorAllocator);
	}
	

	// ----------------------------------------------------------------------------------------------------------------
	// create vertex buffer for timer UI
	
	{
		m_TimerUI.Init(m_device.Get(), &m_resourceDescriptorAllocator);
	}

	// ----------------------------------------------------------------------------------------------------------------
	// create vertex buffer for shop UI

	{
		m_ShopUI.Init(m_device.Get(), &m_resourceDescriptorAllocator);
	}

	// ----------------------------------------------------------------------------------------------------------------
	// create vertex buffer for screenUI

	{
		m_ScreenUI.Init(m_device.Get(), &m_resourceDescriptorAllocator);
	}
	// create buffers for animation
	{
		// animation data
		m_hunterAnimations[HUNTER_ANIMATION_IDLE].Init(m_device.Get(), &m_resourceDescriptorAllocator, L"Idle.janim");
		m_hunterAnimations[HUNTER_ANIMATION_CHASE].Init(m_device.Get(), &m_resourceDescriptorAllocator, L"Chase.janim");
		m_hunterAnimations[HUNTER_ANIMATION_ATTACK].Init(m_device.Get(), &m_resourceDescriptorAllocator, L"Attack.janim");

		m_runnerAnimations[RUNNER_ANIMATION_WALK].Init(m_device.Get(), &m_resourceDescriptorAllocator, L"Walk.janim");
		m_runnerAnimations[RUNNER_ANIMATION_DODGE].Init(m_device.Get(), &m_resourceDescriptorAllocator, L"Dodge.janim");
	}
	
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

	// record start times
	auto time = std::chrono::steady_clock::now();
	for (PlayerRenderState& state : players) {
		state.animationStartTime = time;
	}
	// players[0].isHunter = true;
	return true;
}

Renderer::Renderer() {
	return;
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


// transforms right handed world space global coordinates
// to left handed view space coordinates
inline XMMATRIX XM_CALLCONV XMMatrixLookToRHToLH
(
    FXMVECTOR EyePosition,
    FXMVECTOR EyeDirection, 
    FXMVECTOR UpDirection
) noexcept
{
    assert(!XMVector3Equal(EyeDirection, XMVectorZero()));
    assert(!XMVector3IsInfinite(EyeDirection));
    assert(!XMVector3Equal(UpDirection, XMVectorZero()));
    assert(!XMVector3IsInfinite(UpDirection));

    XMVECTOR R2 = XMVector3Normalize(EyeDirection);

    XMVECTOR R0 = XMVector3Cross(R2, UpDirection);
    R0 = XMVector3Normalize(R0);

    XMVECTOR R1 = XMVector3Cross(R0, R2);

    XMVECTOR NegEyePosition = XMVectorNegate(EyePosition);

    XMVECTOR D0 = XMVector3Dot(R0, NegEyePosition);
    XMVECTOR D1 = XMVector3Dot(R1, NegEyePosition);
    XMVECTOR D2 = XMVector3Dot(R2, NegEyePosition);

    XMMATRIX M;
    M.r[0] = XMVectorSelect(D0, R0, g_XMSelect1110.v);
    M.r[1] = XMVectorSelect(D1, R1, g_XMSelect1110.v);
    M.r[2] = XMVectorSelect(D2, R2, g_XMSelect1110.v);
    M.r[3] = g_XMIdentityR3.v;

    M = XMMatrixTranspose(M);

    return M;
}
XMMATRIX Renderer::computeViewProject(FXMVECTOR pos, LookDir lookDir) {
	using namespace DirectX;
	XMVECTOR model_fwd = { 0, 1, 0, 0 };
    XMVECTOR rotation =
    	XMVector3TransformNormal(
    		model_fwd,
    		 XMMatrixRotationX(cameraPitch) * XMMatrixRotationZ(cameraYaw));

	XMVECTOR camPos = pos - rotation * CAMERA_DIST + XMVECTORF32{ 0, 0, CAMERA_UP, 0 };

	XMVECTOR model_up = { 0, 0, 1, 0 }; 
	
	XMMATRIX view = XMMatrixLookToRHToLH(camPos, pos - camPos, model_up);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, 0.01f, 100.0f);

	return XMMatrixTranspose(view * proj);
}

XMMATRIX Renderer::computeFreecamViewProject(FXMVECTOR camPos, float yaw, float pitch) {
	using namespace DirectX;
	XMVECTOR model_fwd = { 0, 1, 0, 0 };
	XMVECTOR rotation =
		XMVector3TransformNormal(
			model_fwd,
			XMMatrixRotationX(pitch) * XMMatrixRotationZ(yaw));
	rotation = XMVector3Normalize(rotation);
	XMVECTOR model_up = { 0, 0, 1, 0 };
	XMMATRIX view = XMMatrixLookToRHToLH(camPos, rotation, model_up);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, 0.01f, 100.0f);

	return XMMatrixTranspose(view * proj);
}

XMMATRIX Renderer::computeModelMatrix(PlayerRenderState &playerRenderState) {
	float uniformScale = PLAYER_SCALING_FACTOR;
	XMMATRIX scale = XMMatrixScaling(uniformScale, uniformScale, uniformScale);
	XMMATRIX rotate;
	if (playerRenderState.isHunter) {
		rotate = XMMatrixRotationZ(playerRenderState.lookDir.yaw + XM_PI );
	}
	else {
		rotate = XMMatrixRotationZ(playerRenderState.lookDir.yaw /* + XM_PI */);
	}
	XMMATRIX translate = XMMatrixTranslation(playerRenderState.pos.x, playerRenderState.pos.y, playerRenderState.pos.z - 1.0f * uniformScale);
	return XMMatrixTranspose(scale * rotate * translate);
}

bool Renderer::Render() {
	// CREATE A COMMAND LIST
	// should occur after all of its command lists have executed (use fences)
	UNWRAP(m_commandAllocators[m_frameIndex]->Reset());

	// should occur if and only if the command list has been executed
	UNWRAP(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));
	
	
	// Set necessary state
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	if (!m_scene.initialized) {
		m_scene.SendToGPU(m_device.Get(), &m_resourceDescriptorAllocator, m_commandList.Get());
	}
	if (!m_hunterRenderBuffers.initialized) {
		m_hunterRenderBuffers.SendToGPU(m_device.Get(), &m_resourceDescriptorAllocator, m_commandList.Get());
	}
	if (!m_runnerRenderBuffers.initialized) {
		m_runnerRenderBuffers.SendToGPU(m_device.Get(), &m_resourceDescriptorAllocator, m_commandList.Get());
	}
	if (!m_TimerUI.initialized) {
		m_TimerUI.SendToGPU(m_device.Get(), &m_resourceDescriptorAllocator, m_commandList.Get());
	}
	if (!m_ShopUI.initialized) {
		m_ShopUI.SendToGPU(m_device.Get(), &m_resourceDescriptorAllocator, m_commandList.Get());
	}
	if (!m_ScreenUI.initialized) {
		m_ScreenUI.SendToGPU(m_device.Get(), &m_resourceDescriptorAllocator, m_commandList.Get());
	}
	
	// set heaps for constant buffer
	ID3D12DescriptorHeap *ppHeaps[] = {m_resourceDescriptorAllocator.heap.Get()};
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	 m_commandList->SetGraphicsRootDescriptorTable(0, m_resourceDescriptorAllocator.gpu_base);
	


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
	float clearColor[] = {0.1f, 0.1f, 0.2f, 1.0f};
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(m_depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	
	// camera logic
	XMMATRIX viewProject;
	if (detached) {
		XMVECTOR camPos = XMLoadFloat3(&freecamPos);
		viewProject = computeFreecamViewProject(camPos, cameraYaw, cameraPitch);
	}
	else {
		XMVECTOR playerPos = XMLoadFloat3(&players[currPlayer.playerId].pos);
		viewProject = computeViewProject(playerPos, {}); // lookat is not used
	}


	// draw scene
	{
		PerDrawConstants drawConstants = {
			.viewProject           = viewProject,
			.modelMatrix           = XMMatrixIdentity(),
			.modelInverseTranspose = XMMatrixIdentity(),
			.vpos_idx              = m_scene.vertexPosition.descriptor.index,
			.vshade_idx            = m_scene.vertexShading.descriptor.index,
			.material_ids_idx      = m_scene.materialID.descriptor.index,
			.materials_idx         = m_scene.materials.descriptor.index,
			.first_texture_idx     = m_scene.textures.ptr[0].descriptor.index,
			.lightmap_texcoord_idx = m_scene.vertexLightmapTexcoord.descriptor.index,
			.lightmap_texture_idx  = m_scene.lightmapTexture.descriptor.index
		};
	
		m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &drawConstants, 0);
		m_commandList->DrawInstanced(3 * m_scene.vertexPosition.data.len, 1, 0, 0);
	}

	auto time = std::chrono::steady_clock::now();
	// draw players
	m_commandList->SetPipelineState(m_pipelineStateSkin.Get());
	for (UINT8 i = 0; i < 4; ++i) {
		XMMATRIX modelMatrix = computeModelMatrix(players[i]);
		XMMATRIX modelInverseTranspose = XMMatrixInverse(nullptr, XMMatrixTranspose(modelMatrix));
		bool loop = players[i].loop;
		if (players[i].isHunter) {
			UINT8 animationIdx = players[i].hunterAnimation;
			PlayerDrawConstants drawConstants = {
				.viewProject              = viewProject,
				.modelMatrix              = modelMatrix,
				.modelInverseTranspose    = modelInverseTranspose,
				.vpos_idx                 = m_hunterRenderBuffers.vertexPosition.descriptor.index,
				.vshade_idx               = m_hunterRenderBuffers.vertexShading.descriptor.index ,
				.material_ids_idx         = m_hunterRenderBuffers.materialID.descriptor.index,
				.materials_idx            = m_hunterRenderBuffers.materials.descriptor.index,
				.first_texture_idx        = m_hunterRenderBuffers.textures.ptr[0].descriptor.index,
				.vbone_idx                = m_hunterRenderBuffers.vertexBoneIdx.descriptor.index,
				.vweight_idx              = m_hunterRenderBuffers.vertexBoneWeight.descriptor.index,
				.bone_transforms_idx      = m_hunterAnimations[animationIdx].invBindTransform.descriptor.index,
				.bone_adj_transforms_idx  = m_hunterAnimations[animationIdx].invBindAdjTransform.descriptor.index,
				.frame_number             = m_hunterAnimations[animationIdx].getFrame(players[i].animationStartTime, time, loop),
				.num_bones                = m_hunterRenderBuffers.header->numBones,
			};
			m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_PLAYER_NUM_DWORDS, &drawConstants, 0);
			m_commandList->DrawInstanced(3 * m_hunterRenderBuffers.vertexPosition.data.len, 1, 0, 0);
		}
		else {
			UINT8 animationIdx = players[i].runnerAnimation;
			PlayerDrawConstants drawConstants = {
				.viewProject              = viewProject,
				.modelMatrix              = modelMatrix,
				.modelInverseTranspose    = modelInverseTranspose,
				.vpos_idx                 = m_runnerRenderBuffers.vertexPosition.descriptor.index,
				.vshade_idx               = m_runnerRenderBuffers.vertexShading.descriptor.index ,
				.material_ids_idx         = m_runnerRenderBuffers.materialID.descriptor.index,
				.materials_idx            = m_runnerRenderBuffers.materials.descriptor.index,
				.first_texture_idx        = m_runnerRenderBuffers.textures.ptr[0].descriptor.index,
				.vbone_idx                = m_runnerRenderBuffers.vertexBoneIdx.descriptor.index,
				.vweight_idx              = m_runnerRenderBuffers.vertexBoneWeight.descriptor.index,
				.bone_transforms_idx      = m_runnerAnimations[animationIdx].invBindTransform.descriptor.index,
				.bone_adj_transforms_idx  = m_runnerAnimations[animationIdx].invBindAdjTransform.descriptor.index,
				.frame_number             = m_runnerAnimations[animationIdx].getFrame(players[i].animationStartTime, time, loop),
				.num_bones                = m_runnerRenderBuffers.header->numBones,
			};
			m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_PLAYER_NUM_DWORDS, &drawConstants, 0);
			m_commandList->DrawInstanced(3 * m_runnerRenderBuffers.vertexPosition.data.len, 1, 0, 0);
		}
	}
	
	// draw Timer UI
	if (gamePhase != GamePhase::START_MENU) {
		PerDrawConstants dc = {
			.viewProject = m_TimerUI.ortho,
			.modelMatrix = XMMatrixIdentity(),
			.modelInverseTranspose = XMMatrixIdentity(),
			.vpos_idx = m_TimerUI.vertexBuffer.descriptor.index,
			.vshade_idx = m_scene.vertexShading.descriptor.index,
			.first_texture_idx = m_TimerUI.uiTextures.ptr[0].descriptor.index,
		};
		// Base layer
		m_commandList->SetPipelineState(m_pipelineStateTimerUI.Get());
		m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &dc, 0);
		m_commandList->DrawInstanced(m_TimerUI.vertexBuffer.data.len, 1, 0, 0);

		// Hand layer
		{
			float centerX = m_TimerUI.screenW - m_TimerUI.inset - m_TimerUI.side * 0.5f;
			float centerY = m_TimerUI.screenH - m_TimerUI.inset - m_TimerUI.side * 0.5f;
			// minus timer angle so that the timerHandAngle represents a clockwise rotation.
			XMMATRIX M = XMMatrixTranslation(-centerX, -centerY, 0) * XMMatrixRotationZ(-m_TimerUI.timerHandAngle) * XMMatrixTranslation(centerX, centerY, 0);

			dc.modelMatrix = XMMatrixTranspose(M);
			dc.first_texture_idx = m_TimerUI.uiTextures.ptr[1].descriptor.index;
			m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &dc, 0);
			m_commandList->DrawInstanced(m_TimerUI.vertexBuffer.data.len, 1, 0, 0);
		}

		// Top Layer
		dc.modelMatrix = XMMatrixIdentity();
		dc.first_texture_idx = m_TimerUI.uiTextures.ptr[2].descriptor.index;
		m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &dc, 0);
		m_commandList->DrawInstanced(m_TimerUI.vertexBuffer.data.len, 1, 0, 0);
	}

	// draw SHOP if in shop...
	if (gamePhase == GamePhase::SHOP_PHASE) {
		PerDrawConstants dc = {
			.viewProject = m_ShopUI.ortho,
			.modelMatrix = XMMatrixIdentity(),
			.modelInverseTranspose = XMMatrixIdentity(),
			.vpos_idx = m_ShopUI.cardVertexBuffer.descriptor.index,
			.vshade_idx = m_scene.vertexShading.descriptor.index,
		};

		m_commandList->SetPipelineState(m_pipelineStateTimerUI.Get());
		for (int i = 0; i < 3; i++) {
			dc.vpos_idx = m_ShopUI.cardVertexBuffer.descriptor.index;
			if (i == m_ShopUI.currSelected) {
				dc.modelMatrix = m_ShopUI.cardSelectedModelMatrix[i];
			}
			else {
				dc.modelMatrix = m_ShopUI.cardModelMatrix[i];
			}
			dc.first_texture_idx = m_ShopUI.cardTextures.ptr[m_ShopUI.powerupIdxs[i]].descriptor.index;
			m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &dc, 0);
			m_commandList->DrawInstanced(m_ShopUI.cardVertexBuffer.data.len, 1, 0, 0);
			
			// draw cost separately
			dc.vpos_idx = m_ShopUI.counterVertexBuffer.descriptor.index;
			dc.modelMatrix = m_ShopUI.cardCostModelMatrix[i];
			dc.first_texture_idx = m_ShopUI.coinsTextures.ptr[m_ShopUI.powerupCosts[i]].descriptor.index;
			m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &dc, 0);
			m_commandList->DrawInstanced(m_ShopUI.cardVertexBuffer.data.len, 1, 0, 0);
		}

		// draw counter for coins and souls
		dc.vpos_idx = m_ShopUI.counterVertexBuffer.descriptor.index;
		dc.modelMatrix = m_ShopUI.coinsModelMatrix;
		dc.first_texture_idx = m_ShopUI.coinsTextures.ptr[m_ShopUI.coins].descriptor.index;
		m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &dc, 0);
		m_commandList->DrawInstanced(m_ShopUI.cardVertexBuffer.data.len, 1, 0, 0);

		dc.modelMatrix = m_ShopUI.soulsModelMatrix;
		dc.first_texture_idx = m_ShopUI.soulsTextures.ptr[m_ShopUI.souls].descriptor.index;
		m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &dc, 0);
		m_commandList->DrawInstanced(m_ShopUI.cardVertexBuffer.data.len, 1, 0, 0);
	}
	
	if (activeScoreboard) {
		PerDrawConstants dc = {
				.viewProject = m_ShopUI.ortho,
				.modelMatrix = XMMatrixIdentity(),
				.modelInverseTranspose = XMMatrixIdentity(),
				.vpos_idx = m_ShopUI.cardVertexBuffer.descriptor.index,
				.vshade_idx = m_scene.vertexShading.descriptor.index,
		};
		m_commandList->SetPipelineState(m_pipelineStateTimerUI.Get());

		for (int row = 0; row < 4; row++) {
			for (int col = 0; col < 10; col++) {
				uint8_t p = powerupInfo[row][col];
				if (p == 255) break;
				dc.modelMatrix = m_ShopUI.scoreboardCardModelMatrix[row][col];
				dc.first_texture_idx = m_ShopUI.cardTextures.ptr[PowerupInfo[(Powerup) p].textureIdx].descriptor.index;
				m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &dc, 0);
				m_commandList->DrawInstanced(m_ShopUI.cardVertexBuffer.data.len, 1, 0, 0);
			}
		}
	}

	if (gamePhase == GamePhase::START_MENU || gamePhase == GamePhase::GAME_END) {
		PerDrawConstants dc = {
		.viewProject = m_ScreenUI.ortho,
		.modelMatrix = XMMatrixIdentity(),
		.modelInverseTranspose = XMMatrixIdentity(),
		.vpos_idx = m_ScreenUI.screenVertexBuffer.descriptor.index,
		.vshade_idx = m_scene.vertexShading.descriptor.index,
		};
		m_commandList->SetPipelineState(m_pipelineStateTimerUI.Get());

		uint8_t texIdx = 0;
		switch (gamePhase) {
		case GamePhase::START_MENU:
			texIdx = 0;
			break;
		case GamePhase::GAME_END:
			if (winner == 1) texIdx = 1;
			else texIdx = 2;
			break;
		}
		dc.first_texture_idx = m_ScreenUI.screenTextures.ptr[texIdx].descriptor.index;
		m_commandList->SetGraphicsRoot32BitConstants(1, DRAW_CONSTANT_NUM_DWORDS, &dc, 0);
		m_commandList->DrawInstanced(m_ScreenUI.screenVertexBuffer.data.len, 1, 0, 0);
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
	// m_constantBufferData.viewProject = computeViewProject(playerState.pos, playerState.lookDir);
	// memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
}

void Renderer::DBG_DrawCube(XMFLOAT3 inMin, XMFLOAT3 inMax)
{
	assert(debugCubes.transforms.size() < debugCubes.transforms.max_size());
	XMVECTOR min = XMLoadFloat3(&inMin);
	XMVECTOR max = XMLoadFloat3(&inMax);
	XMVECTOR scale = 0.5 * (max - min);
	XMVECTOR origin = 0.5 * (max + min);

	XMMATRIX transform = XMMatrixScalingFromVector(scale) * XMMatrixTranslationFromVector(origin);
	XMMATRIX transposed = XMMatrixTranspose(transform);
	debugCubes.transforms.push_back(transposed);
	debugCubes.UpdateGPUSide();
}
