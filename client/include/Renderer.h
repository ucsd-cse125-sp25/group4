#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#include "ReadData.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;


// return false from the function if there is a failure 
#define UNWRAP(result) if(FAILED(result)) return false 

constexpr uint32_t VERTS_PER_TRI = 3;

struct LookDir {
	// (-pi/2, pi/2)
	// increasing pitch means looking further up
	// 0 means looking parallel to the ground
	float pitch;  
	// (-infinity, infinity) 
	// increasing yaw means looking further:
	// * left if you're the player
	// * counterclockwise if you're looking down from the +Z axis
	// * in the rotation direction of the shortest rotation from (1, 0, 0) to (0, 1, 0) (the xy bivector) 
	// multiples of 2pi mean looking toward +y
	float yaw; 
};

struct PlayerRenderState {
	XMFLOAT3 pos;
	LookDir lookDir;
};
struct CurrPlayerRenderState {
	UINT8 playerId;
};

// we do not use a scoped enum because those cannot be implicitly cast to ints
constexpr enum RootParameters : UINT8 {
	ROOT_PARAMETERS_DESCRIPTOR_TABLE,
	ROOT_PARAMETERS_CONSTANT_PER_CALL,
	ROOT_PARAMETERS_CONSTANT_DEBUG_CUBE_MATRICES,
	ROOT_PARAMETERS_COUNT
};

// TODO: have 2 constant buffers
// 1 is updated per-frame
// 1 is updated per-tick
// maybe a 3rd is updated sporadically

template<typename T>
struct Slice {
	T* ptr;
	UINT len;

	inline UINT numBytes() {
		return len * sizeof(T);
	}
};

template<typename T>
struct Buffer {
	Slice<T>                   data;
	ComPtr<ID3D12Resource>     resource;
	void                      *shared_ptr; 
	D3D12_GPU_VIRTUAL_ADDRESS  gpu_ptr;
	// uint32_t offset;

	bool Init(ID3D12Device* device, Slice<T> data, const wchar_t* debugName);
	void Release();
};

struct Vertex {
	XMFLOAT3 position;
};

// TODO: unify both vertex structs; this is just for development velocity

struct SceneHeader {
	int32_t version;
	int32_t numTriangles;
	int32_t firstTriangle;
};

struct VertexShadingData {
	XMFLOAT3 normal;
	XMFLOAT2 texcoord;
};

struct Triangles {
	int                len;
	XMFLOAT3          (*vertPositions)[3];
	VertexShadingData *shadingData[3];
	uint8_t           *materialId;
};

struct Descriptor {
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;
	uint32_t index;
};

// based off of https://github.com/TheSandvichMaker/HelloBindlessD3D12/blob/main/hello_bindless.cpp#L389
struct DescriptorAllocator {
	ComPtr<ID3D12DescriptorHeap> heap;
	D3D12_DESCRIPTOR_HEAP_TYPE type;
	D3D12_CPU_DESCRIPTOR_HANDLE cpu_base;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_base;
	uint32_t stride;
	uint32_t at;
	uint32_t capacity;

	bool Init(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE inputType, uint32_t inputCapacity, const wchar_t *name) {
		at = 0;
		type = inputType;
		capacity = inputCapacity;
		D3D12_DESCRIPTOR_HEAP_DESC desc = {
			.Type = type,
			.NumDescriptors = capacity, 
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		};
		UNWRAP(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
		heap->SetName(name);

		cpu_base = heap->GetCPUDescriptorHandleForHeapStart();
		gpu_base = heap->GetGPUDescriptorHandleForHeapStart();
		stride = device->GetDescriptorHandleIncrementSize(type);
	}

	Descriptor Allocate() {
		assert(at < capacity);
		uint32_t index = at;
		at++;
		return Descriptor{
			.cpu = { cpu_base.ptr + stride * index},
			.gpu = { gpu_base.ptr + stride * index},
			.index = index,
		};
	}
};


constexpr uint32_t SCENE_VERSION = 000'000'000;
enum SceneBufferType {
	SCENE_BUFFER_TYPE_VERTEX_POSITION,
	SCENE_BUFFER_TYPE_VERTEX_SHADING,
	SCENE_BUFFER_TYPE_MATERIAL_ID,
	SCENE_BUFFER_TYPE_COUNT
};
struct Scene {
	// the whole scene file
	Slice<BYTE> data;
	// wide pointers to the scene file (excluding headers) 
	Triangles triangles;
	// GPU heap
	ComPtr<ID3D12Resource> resource;
	void *shared_ptr;
	// GPU Descriptors
	Descriptor descriptors[SCENE_BUFFER_TYPE_COUNT];

	bool Init(ID3D12Device *device, DescriptorAllocator *descriptorAllocator, const wchar_t *filename) {
		// ------------------------------------------------------------------------------------------------------------
		// slurp data from file 
		data.len = DX::ReadDataToPtr(filename, &data.ptr);
		if (data.len <= 0) {
			printf("ERROR: failed to read scene file\n");
			return false;
		}
		assert(data.ptr != nullptr);
		
		SceneHeader* header = reinterpret_cast<SceneHeader*>(data.ptr);
		if (header->version != SCENE_VERSION) {
			printf("ERROR: version of scene file is %d while version of parser is %d \n", header->version, SCENE_VERSION);
			return false;
		}
		if (header->numTriangles == 0) {
			printf("ERROR: scene has no triangles\n");
			return false;
		}
		/*
		if (header.firstTriangle + numTriangles >= data.len) {
			printf("ERROR: described triangle buffer is out of bounds\n");
			return false;
		}*/

		// contains scene data but not the header	
		Slice<BYTE> SceneBuffers = {
			.ptr = data.ptr + sizeof(SceneHeader),
			.len = data.len - sizeof(SceneHeader)
		};

		triangles = Triangles{
			.len = header->numTriangles,
			.vertPositions = reinterpret_cast<XMFLOAT3(*)[3]>(&SceneBuffers.ptr[header->firstTriangle]),
			.shadingData = nullptr,
			.materialId = nullptr,
		};

		D3D12_HEAP_PROPERTIES heapProperties = { .Type = D3D12_HEAP_TYPE_UPLOAD };
		CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(SceneBuffers.len);
		
		// allocate GPU memory for the whole scene
		UNWRAP(device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&resource)
		));
		resource->SetName(L"Scene Buffers");

		D3D12_RANGE nullRange = {};
		UNWRAP(
			resource->Map(0, &nullRange, &shared_ptr)
		);
		// copy scene to GPU
		memcpy(shared_ptr, SceneBuffers.ptr, SceneBuffers.len);
		
		uint32_t bufferLengths[SCENE_BUFFER_TYPE_COUNT];
		uint32_t bufferOffsets[SCENE_BUFFER_TYPE_COUNT];
		bufferLengths[SCENE_BUFFER_TYPE_VERTEX_POSITION] = triangles.len * VERTS_PER_TRI;
		bufferOffsets[SCENE_BUFFER_TYPE_VERTEX_POSITION] = 0;
		bufferLengths[SCENE_BUFFER_TYPE_VERTEX_SHADING] = triangles.len;
		bufferOffsets[SCENE_BUFFER_TYPE_VERTEX_SHADING] = 0; // CHANGE LATER
		bufferLengths[SCENE_BUFFER_TYPE_MATERIAL_ID] = triangles.len;
		bufferOffsets[SCENE_BUFFER_TYPE_MATERIAL_ID] = 0; // CHANGE LATER

		for (unsigned int i = 0; i < SCENE_BUFFER_TYPE_COUNT; ++i) {
			descriptors[i] = descriptorAllocator->Allocate();
			
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
				.Format = DXGI_FORMAT_UNKNOWN,
				.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Buffer = {
					.FirstElement = bufferOffsets[i],
					.NumElements = bufferLengths[i],
					.StructureByteStride = sizeof(Vertex) // WARNING, this must be changed for other data types
				}
			};
			device->CreateShaderResourceView(resource.Get(), &desc, descriptors[i].cpu);
		}
	}
	void Release() {
		resource->Unmap(0, nullptr);
		if (data.ptr != nullptr) {
			free(data.ptr);
		}
		memset(this, 0, sizeof(this));
	}
};


constexpr int MAX_DEBUG_CUBES = 1024;
struct DebugCubes {
	std::vector<XMMATRIX> transforms;
	// GPU heap
	ComPtr<ID3D12Resource> resource;
	void *shared_ptr;
	// GPU Descriptors
	Descriptor descriptor;
	
	// vertices
	Buffer<Vertex> vertexBuffer;
	Descriptor vertexBufferDescriptor;

	bool Init(ID3D12Device *device, DescriptorAllocator *descriptorAllocator) {
		{
			Vertex cubeverts[6 * 2 * 3] = {
				{ { -0.5f, -0.5f, -0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { -0.5f, 0.5f, 0.5 } },{ { -0.5f, -0.5f, -0.5 } },{ { -0.5f, 0.5f, 0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { -0.5f, 0.5f, 0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { 0.5f, 0.5f, -0.5 } },{ { 0.5f, 0.5f, -0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { 0.5f, -0.5f, 0.5 } },{ { 0.5f, 0.5f, -0.5 } },{ { 0.5f, -0.5f, 0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { 0.5f, -0.5f, 0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { -0.5f, -0.5f, -0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { 0.5f, 0.5f, -0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { -0.5f, -0.5f, -0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { -0.5f, 0.5f, 0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { 0.5f, -0.5f, 0.5 } }
			};
			const Slice<Vertex> cubeVertsSlice = {
				.ptr = cubeverts,
				.len = _countof(cubeverts),
			};

			vertexBuffer.Init(device, cubeVertsSlice, L"Debug Cube Vertex Buffer");

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

			vertexBufferDescriptor = descriptorAllocator->Allocate();
			device->CreateShaderResourceView(vertexBuffer.resource.Get(), &desc, vertexBufferDescriptor.cpu);
		}
		transforms.reserve(MAX_DEBUG_CUBES);

		D3D12_HEAP_PROPERTIES heapProperties = { .Type = D3D12_HEAP_TYPE_UPLOAD };
		CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(MAX_DEBUG_CUBES * sizeof(XMMATRIX));

		// allocate GPU memory for the transformation matrices
		UNWRAP(device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&resource)
		));
		resource->SetName(L"Scene Debug Cubes");

		D3D12_RANGE nullRange = {};
		UNWRAP(
			resource->Map(0, &nullRange, &shared_ptr)
		);

		descriptor = descriptorAllocator->Allocate();
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Buffer = {
				.FirstElement = 0,
				.NumElements = MAX_DEBUG_CUBES,
				.StructureByteStride = sizeof(XMMATRIX)
			}
		};
		device->CreateShaderResourceView(resource.Get(), &desc, descriptor.cpu);
	}
	void UpdateGPUSide() {
		// copy scene to GPU
		memcpy(shared_ptr, transforms.data(), transforms.size() * sizeof(XMMATRIX));
	}
	void Release() {
		resource->Unmap(0, nullptr);
		shared_ptr = nullptr;
		descriptor = {};
	}
};

class Renderer {
public:
	bool Init(HWND window_handle);

	bool Render();
	void OnUpdate();
	~Renderer();
	// TODO: have a constant buffer for each frame
	// SceneConstantBuffer m_constantBufferData; // temporary storage of constant buffer on the CPU side

	PlayerRenderState players[4] = {
		{
			.pos = {-4, -4, 0},
			.lookDir = {
				.pitch = XMConvertToRadians(0),
				.yaw = XMConvertToRadians(10),
			},
		},
		{
			.pos = {4, -4, 0},
			.lookDir = {
				.pitch = XMConvertToRadians(0),
				.yaw = XMConvertToRadians(20),
			},
		},
		{
			.pos = {-4, 4, 0},
			.lookDir = {
				.pitch = XMConvertToRadians(0),
				.yaw = XMConvertToRadians(30),
			},
		},
		{
			.pos = {4, 4, 0},
			.lookDir = {
				.pitch = XMConvertToRadians(0),
				.yaw = XMConvertToRadians(45),
			},
		}
	};
	CurrPlayerRenderState currPlayer = { 0 };
	void DBG_DrawCube(XMFLOAT3 min, XMFLOAT3 max);

	// helper getters
	UINT getWidth() { return m_width; };
	UINT getHeight() { return m_height; };

	//helper setters
	void updateCamera(float yaw, float pitch) {
		cameraYaw = yaw;
		cameraPitch = pitch;
	}
private:
	DebugCubes debugCubes;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

	static const UINT FramesInFlight = 2;
	ComPtr<ID3D12Resource> m_renderTargets[FramesInFlight];
	ComPtr<ID3D12Device> m_device;
#if defined(_DEBUG)
	ComPtr<ID3D12Debug1> m_debugController;
	ComPtr<ID3D12DebugDevice> m_debugDevice;
#endif
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[FramesInFlight];
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<IDXGISwapChain3> m_swapChain;
	UINT m_rtvDescriptorSize;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	// for debug drawing
	ComPtr<ID3D12PipelineState> m_pipelineStateDebug;

	// syncrhonization objects
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[FramesInFlight];
	
	// TODO: make these adjustable
	UINT m_width = 1920;
	UINT m_height = 1080;
	float m_aspectRatio = 16.0f / 9.0f;
	float m_fov = XMConvertToRadians(40 * (9.0/16.0)); 
	// ComPtr<ID3D12Resource> m_vertexBuffer;
	// D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	Descriptor m_vertexBufferDescriptor;
	Buffer<Vertex> m_vertexBufferBindless;
	Scene m_scene;
	

	// ComPtr<ID3D12DescriptorHeap> m_resourceHeap;
	DescriptorAllocator m_resourceDescriptorAllocator;
	// ComPtr<ID3D12Resource> m_constantBuffer; // references the concept of the plan of the concept buffer or something
	UINT8 *m_pCbvDataBegin; // virtual address of the constant buffer memory

	bool MoveToNextFrame();
	bool WaitForGpu();

	XMMATRIX computeViewProject(FXMVECTOR pos, LookDir lookDir);
	XMMATRIX computeModelMatrix(PlayerRenderState &playerRenderState);
	
	ComPtr<ID3D12Resource> m_depthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> m_depthStencilDescriptorHeap;
	
	// CAMERA CONSTANTS
	float cameraYaw   = 0.0f;
	float cameraPitch = 0.0f;
	static constexpr float CAMERA_DIST = 32.0f;
	static constexpr float CAMERA_UP = 6.0f;
};


template<typename T>
inline bool Buffer<T>::Init(ID3D12Device* device, Slice<T> data, const wchar_t *debugName)
{
	this->data = {
		data.ptr,
		data.len
	};
	D3D12_HEAP_PROPERTIES heapProperties = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	D3D12_RESOURCE_DESC resourceDesc = {
		.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width            = data.numBytes(),
		.Height           = 1,
		.DepthOrArraySize = 1,
		.MipLevels        = 1,
		.Format           = DXGI_FORMAT_UNKNOWN,
		.SampleDesc       = { .Count = 1, .Quality = 0 },
		.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
	};

	UNWRAP(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	));
	resource->SetName(debugName);
	
	D3D12_RANGE nullRange = {};
	UNWRAP(
		resource->Map(0, &nullRange, &shared_ptr)
	);
	memcpy(shared_ptr, data.ptr, data.numBytes());
	return true;
}

template<typename T>
inline void Buffer<T>::Release()
{
	resource->Unmap(0, nullptr);
	resource->Release();
}
