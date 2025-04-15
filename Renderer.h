#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <stdexcept>
#include <shellapi.h>
#include <direct.h>

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

class Renderer {
public:
	bool init(HWND window_handle);
private:
	static const UINT FramesInFlight = 2;
	ComPtr<ID3D12Resource> m_renderTargets[FramesInFlight];
	ComPtr<ID3D12Debug1> m_debug_controller;
	ComPtr<ID3D12Device> m_device;
#if defined(_DEBUG)
	ComPtr<ID3D12DebugDevice> m_debug_device;
#endif
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<IDXGISwapChain3> m_swapChain;
	UINT m_rtvDescriptorSize;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;

	UINT m_frameIndex;
	HANDLE m_fence_event;
	ID3D12Fence* m_fence;
	UINT64 m_fence_value;

	UINT m_width = 640;
	UINT m_height = 480;
};
