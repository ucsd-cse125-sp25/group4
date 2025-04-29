#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;


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

// TODO: have 2 constant buffers
// 1 is updated per-frame
// 1 is updated per-tick
// maybe a 3rd is updated sporadically

struct SceneConstantBuffer {
    XMMATRIX viewProject;
	float padding[48];
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant buffer must be 256-byte aligned");

struct Vertex {
	XMFLOAT3 position;
};

struct Buffer {
	ComPtr<ID3D12Resource> gpuBuffer;
	UINT numElems;
	Vertex *data;
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
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	

	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
	ComPtr<ID3D12Resource> m_constantBuffer; // references the concept of the plan of the concept buffer or something
	UINT8 *m_pCbvDataBegin; // virtual address of the constant buffer memory

	bool MoveToNextFrame();
	bool WaitForGpu();

	XMMATRIX computeViewProject(XMVECTOR pos, LookDir lookDir);
	
	ComPtr<ID3D12Resource> m_depthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> m_depthStencilDescriptorHeap;
	
};

