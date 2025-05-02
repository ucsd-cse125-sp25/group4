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

/* Blender Import Script
import bpy
import mathutils

scene = bpy.data.scenes[0]

cube = scene.objects.get("Cube")
cubemesh = cube.data
verts = cubemesh.vertices
scenevertices = []
for tri in cubemesh.loop_triangles:
    for vidx in tri.vertices:
        vertex = verts[vidx]
        position_local = mathutils.Vector((vertex.co.x, vertex.co.y, vertex.co.z, 1)) 
        position_global =  cube.matrix_world @ position_local
        scenevertices.append([position_global.x, position_global.y, position_global.z])
print(scenevertices)
*/

struct LookDir {
	// pitch and yaw match Blender's camera x and z rotations respectively
	float pitch; // (0, pi), with pi/2 on the plane z=0
	float yaw; // (-pi, pi], with 0 on the plane x=0
};
struct TEMPPlayerState {
	XMVECTOR pos;
	LookDir lookDir;
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
	ROOT_PARAMETERS_CONSTANT_MODEL_VIEW_PROJECT,
	ROOT_PARAMETERS_COUNT
};

// TODO: have 2 constant buffers
// 1 is updated per-frame
// 1 is updated per-tick
// maybe a 3rd is updated sporadically

struct SceneConstantBuffer {
    XMMATRIX viewProject;
	float padding[48];
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant buffer must be 256-byte aligned");

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
	Slice<T> data;
	ComPtr<ID3D12Resource> resource;
	void *shared_ptr; 
	D3D12_GPU_VIRTUAL_ADDRESS gpu_ptr;
	// uint32_t offset;

	bool Init(ID3D12Device* device, Slice<T> data, const wchar_t* debugName);
	void Release();
};

struct Vertex {
	XMFLOAT3 position;
};

// TODO: unify both vertex structs; this is just for development velocity
struct SceneVertex {
	XMFLOAT3 position;
};

struct Scene {
	// Slice<BYTE> buf;
	Buffer<SceneVertex> vertexBuffer;

	bool Init(ID3D12Device* device, const wchar_t *filename) {
		Slice<SceneVertex> tmp{};
		// slurp data from file 
		tmp.len = DX::ReadDataToPtr(filename, reinterpret_cast<BYTE**>( & tmp.ptr));
		if (tmp.len <= 0) {
			// something went wrong
			return false;
		}
		vertexBuffer.Init(device, tmp, L"Scene Vertex Buffer");
	};
	void Release() {
		if (vertexBuffer.data.ptr != nullptr) {
			free(vertexBuffer.data.ptr);
		}
	}
};

struct DX12Descriptor {
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;
};

class Renderer {
public:
	bool Init(HWND window_handle);

	bool Render();
	void OnUpdate();
	~Renderer();
	// TODO: have a constant buffer for each frame
	SceneConstantBuffer m_constantBufferData; // temporary storage of constant buffer on the CPU side

	TEMPPlayerState playerState = {
		.pos = {6, -6, 2.5, 1},
		.lookDir = {
			.pitch = XMConvertToRadians(-73),
			.yaw = XMConvertToRadians(-45),
		},
	};
	PlayerRenderState players[4] = {
		{
			.pos = {-4, -4, 0},
			.lookDir = {
				.pitch = XMConvertToRadians(0),
				.yaw = XMConvertToRadians(-10),
			},
		},
		{
			.pos = {4, -4, 0},
			.lookDir = {
				.pitch = XMConvertToRadians(0),
				.yaw = XMConvertToRadians(-20),
			},
		},
		{
			.pos = {-4, 4, 0},
			.lookDir = {
				.pitch = XMConvertToRadians(0),
				.yaw = XMConvertToRadians(-30),
			},
		},
		{
			.pos = {4, 4, 0},
			.lookDir = {
				.pitch = XMConvertToRadians(0),
				.yaw = XMConvertToRadians(-45),
			},
		}
	};
	CurrPlayerRenderState currPlayer = { 0 };
	int dbg_NumTrisToDraw = 3;
private:

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

	Buffer<Vertex> m_vertexBufferBindless;
	

	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
	ComPtr<ID3D12Resource> m_constantBuffer; // references the concept of the plan of the concept buffer or something
	UINT8 *m_pCbvDataBegin; // virtual address of the constant buffer memory

	bool MoveToNextFrame();
	bool WaitForGpu();

	XMMATRIX computeViewProject(FXMVECTOR pos, LookDir lookDir);
	XMMATRIX computeModelMatrix(PlayerRenderState &playerRenderState);
	
	ComPtr<ID3D12Resource> m_depthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> m_depthStencilDescriptorHeap;
	
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
