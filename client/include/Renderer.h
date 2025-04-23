#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

// Should the renderer contain the scene?
// I feel like changes to the scene should be reflected in changes to the renderer
// Changing a scene means that you would have to copy new memory to the GPU
// Okay what data need to be changed?
// each frame the players move around
// so we need to update their global transform matrices
// each frame the players poses also get updated
// so we need to update pose transform matrices for skinning
// across most frames, the number of objects remains the same
// but what if we want to add a new object?
// then we would have to copy it to the GPU memory
// so the scene and the renderer are deeply tied
// but do we need to worry about that yet?
// i say right now, we keep the scene and the renderer in one object 
// and abstract as needed

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct SceneConstantBuffer {
	XMFLOAT4 offset;
	float padding[60]; // padding so that the buffer is 256-byte aligned
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant buffer must be 256-byte aligned");
class Renderer {
public:
	bool Init(HWND window_handle);

	bool Render();
	void OnUpdate();
	~Renderer();

	SceneConstantBuffer m_constantBufferData; // temporary storage of constant buffer on the CPU side
	
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

	struct Vertex {
        XMFLOAT3 position;
        XMFLOAT4 color;
    };
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;


	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
	ComPtr<ID3D12Resource> m_constantBuffer; // references the concept of the plan of the concept buffer or something
	UINT8 *m_pCbvDataBegin; // virtual address of the constant buffer memory

	bool MoveToNextFrame();
	bool WaitForGpu();
};