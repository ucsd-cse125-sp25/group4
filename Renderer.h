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

// using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Renderer {
public:
	bool init();
private:
	ComPtr<ID3D12Debug1> m_debug_controller;
};
