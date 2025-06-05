#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#include "ReadData.h"
#include "NetworkData.h"
#include "shaderShared.hlsli"
#include "ddspp.h"
#include <chrono>

using namespace DirectX;
using Microsoft::WRL::ComPtr;


// return false from the function if there is a failure 
#define UNWRAP(result) if(FAILED(result)) return false 

constexpr uint32_t VERTS_PER_TRI = 3;
constexpr size_t BYTES_PER_DWORD = 4;
constexpr size_t DRAW_CONSTANT_NUM_DWORDS = sizeof(PerDrawConstants)/BYTES_PER_DWORD;
constexpr size_t DRAW_CONSTANT_PLAYER_NUM_DWORDS = sizeof(PlayerDrawConstants)/BYTES_PER_DWORD;
constexpr size_t BONES_PER_VERT = 4;
typedef wchar_t TexturePath_t[256];

typedef uint32_t BoneIndices[BONES_PER_VERT];
typedef float BoneWeights[BONES_PER_VERT];

inline uint32_t alignU32(uint32_t num, uint32_t alignment) {
	return ((num + alignment - 1) / alignment) * alignment;
}

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
	std::chrono::time_point<std::chrono::steady_clock> animationStartTime;
	LookDir lookDir;
	bool isHunter;
	union {
		RunnerAnimation runnerAnimation;
		HunterAnimation hunterAnimation;
	};
	bool loop = true;

	void loopAnimation(UINT8 animation) {
		loop = true;
		animationStartTime = std::chrono::steady_clock::now();
		if (isHunter) {
			hunterAnimation = (HunterAnimation)animation;
		}
		else {
			runnerAnimation = (RunnerAnimation)animation;
		}
	}
	void playAnimationToEnd(UINT8 animation) {
		loop = false;
		animationStartTime = std::chrono::steady_clock::now();
		if (isHunter) {
			hunterAnimation = (HunterAnimation)animation;
		}
		else {
			runnerAnimation = (RunnerAnimation)animation;
		}
	}

};
struct CurrPlayerRenderState {
	UINT8 playerId;
};

// we do not use a scoped enum because those cannot be implicitly cast to ints
constexpr enum RootParameters : UINT8 {
	ROOT_PARAMETERS_DESCRIPTOR_TABLE,
	ROOT_PARAMETERS_CONSTANT_PER_CALL,
	ROOT_PARAMETERS_COUNT
};


struct Descriptor {
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;
	uint32_t index;
};

// based off of https://github.com/TheSandvichMaker/HelloBindlessD3D12/blob/main/hello_bindless.cpp#L389
struct DescriptorAllocator {
	ComPtr<ID3D12DescriptorHeap> heap;
	D3D12_DESCRIPTOR_HEAP_TYPE   type;
	D3D12_CPU_DESCRIPTOR_HANDLE  cpu_base;
	D3D12_GPU_DESCRIPTOR_HANDLE  gpu_base;
	uint32_t                     stride;
	uint32_t                     at;
	uint32_t                     capacity;

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
		return true;
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

template<typename T>
struct Buffer {
	Slice<T>                   data;       // non-owning view into CPU memory
	ComPtr<ID3D12Resource>     resource;   // DX12 heap handle (each buffer has its own heap)
	void*                      shared_ptr; // GPU heap memory mapped to CPU virtual memory
	D3D12_GPU_VIRTUAL_ADDRESS  gpu_ptr;    // address of heap start
	Descriptor                 descriptor; // SRV descriptor in the SRV heapf

	bool Init(Slice<T> data       , ID3D12Device *device, DescriptorAllocator *descriptorAllocator,  const wchar_t *debugName);
	bool Init(T *ptr, uint32_t len, ID3D12Device *device, DescriptorAllocator *descriptorAllocator,  const wchar_t *debugName);
	void Release();
};

struct Vertex {
	XMFLOAT3 position;
};

// TODO: unify both vertex structs; this is just for development velocity

struct SceneHeader {
	uint32_t version;
	uint32_t numTriangles;
	uint32_t numMaterials;
	uint32_t numTextures;
	uint32_t numBones;
};


struct Triangles {
	int                len;
	XMFLOAT3          (*vertPositions)[3];
	VertexShadingData (*shadingData)[3];
	uint8_t           *materialId;
};


struct Texture {
	Slice<unsigned char> data; // owning slice of raw DDS File data
	ComPtr<ID3D12Resource> resource;
	ComPtr<ID3D12Resource> uploadHeap;
	Descriptor descriptor;
	
	bool Init(ID3D12Device *device, DescriptorAllocator *descriptorAllocator, ID3D12GraphicsCommandList *commandList, const wchar_t *filename) {
		DX::ReadDataStatus status = DX::ReadDataToSlice(filename, data);
		if (status != DX::ReadDataStatus::SUCCESS) {
			printf("fuck2\n");
			return false;
		}
		
		
		// use ddspp to decode the header and write it to a cleaner descriptor
		ddspp::Descriptor desc;
		ddspp::Result decodeResult = ddspp::decode_header(data.ptr, desc);
		if (decodeResult != ddspp::Success) {
			printf("fuck2\n");
			return false;
		}

		BYTE* initialData = data.ptr + desc.headerSize;

		if (desc.type != ddspp::Texture2D) {
			printf("fuck2\n");
			return false; // we only support 2D textures
		}
		if (desc.arraySize != 1) return false; // we do not support arrays of textures 
		// describes texture in the default heap
		D3D12_RESOURCE_DESC textureDesc = {
			.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment        = 0,
			.Width            = desc.width,
			.Height           = desc.height,
			.DepthOrArraySize = (UINT16)desc.arraySize,
			.MipLevels        = (UINT16)desc.numMips,
			.Format           = (DXGI_FORMAT)desc.format,
			.SampleDesc       = {.Count = 1, .Quality = 0},
			.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags            = D3D12_RESOURCE_FLAG_NONE,
		};
	
		
		constexpr UINT MAX_TEXTURE_SUBRESOURCE_COUNT = 16; // enough for 4k textures; may need more for a lightmap
		UINT64 textureMemorySize = 0;
		UINT numRows[MAX_TEXTURE_SUBRESOURCE_COUNT];
		UINT64 rowSizesInBytes[MAX_TEXTURE_SUBRESOURCE_COUNT];
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[MAX_TEXTURE_SUBRESOURCE_COUNT];
		const UINT64 numSubResources = desc.numMips * desc.arraySize;

		device->GetCopyableFootprints(&textureDesc, 0, (uint32_t)numSubResources, 0, layouts, numRows, rowSizesInBytes, &textureMemorySize);

		// create upload heap
		D3D12_HEAP_PROPERTIES uploadHeapProperties = { .Type = D3D12_HEAP_TYPE_UPLOAD };
		CD3DX12_RESOURCE_DESC uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(textureMemorySize);
		UNWRAP(device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&uploadResourceDesc,
			D3D12_RESOURCE_STATE_COMMON, // maybe do copy source?
			nullptr, 
			IID_PPV_ARGS(&uploadHeap)
		));
		uploadHeap->SetName(L"Texture upload heap");
		
		// map memory
		void* mapped;
		D3D12_RANGE readRange = {};
		UNWRAP(uploadHeap->Map(0, &readRange, &mapped));
		
		// copy to upload heap
		// each mip level gets its own subresource
		for (UINT mipIndex = 0; mipIndex < desc.numMips; mipIndex++) {
			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourceLayout = layouts[mipIndex];

			const UINT subResourceHeight = numRows[mipIndex]; 
			
			BYTE* destinationSubResourceMemory = (BYTE*)mapped + subResourceLayout.Offset;                                           // in CPU-GPU mapped memory
			const UINT subResourceRowPitch     = alignU32(subResourceLayout.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT); // row pitch in the GPU resource

			const UINT cpuDataRowPitch    = ddspp::get_row_pitch(desc, mipIndex);              // row pitch in the CPU buffer
			BYTE* sourceSubResourceMemory = &initialData[ddspp::get_offset(desc, mipIndex, 0)]; // in CPU-only memory
			
			// copy rows individually as there may be end-of-row padding in the GPU buffer
			for (UINT height = 0; height < subResourceHeight; height++)
			{
				memcpy(destinationSubResourceMemory, sourceSubResourceMemory, min(subResourceRowPitch, cpuDataRowPitch));
				destinationSubResourceMemory += subResourceRowPitch;
				sourceSubResourceMemory += cpuDataRowPitch; 
			}
		}

		// create default heap
		D3D12_HEAP_PROPERTIES defaultHeapProperties = { .Type = D3D12_HEAP_TYPE_DEFAULT };
		device->CreateCommittedResource(&defaultHeapProperties,
										D3D12_HEAP_FLAG_NONE,
										&textureDesc,
										D3D12_RESOURCE_STATE_COPY_DEST,
										nullptr,
										IID_PPV_ARGS(&resource));
		resource->SetName(filename);

		// we don't need an SRV desc https://alextardif.com/D3D11To12P3.html
		descriptor = descriptorAllocator->Allocate();
		device->CreateShaderResourceView(resource.Get(), nullptr, descriptor.cpu);
		
		// copy from upload heap to default heap
		for (UINT subResourceIndex = 0; subResourceIndex < desc.numMips; subResourceIndex++) {
			D3D12_TEXTURE_COPY_LOCATION destination = {
				.pResource        = resource.Get(),
				.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				.SubresourceIndex = subResourceIndex,
			};

			D3D12_TEXTURE_COPY_LOCATION source = {
				.pResource       = uploadHeap.Get(),
				.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
				.PlacedFootprint = layouts[subResourceIndex]
			};
			commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
		}

		{
			D3D12_RESOURCE_BARRIER barrier = {
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Transition = {
					.pResource = resource.Get(),
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
					.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				},
			};

			commandList->ResourceBarrier(1, &barrier);
		}
		return true;
	}
};


constexpr uint32_t SCENE_VERSION = 000'000'003;
struct Scene {
	// the whole scene file
	Slice<BYTE> data;
	SceneHeader *header;
	// buffers reference the data slice
	Buffer<XMFLOAT3>          vertexPosition;
	Buffer<VertexShadingData> vertexShading;
	Buffer<uint16_t>          materialID;
	Buffer<Material>          materials;
	union { // rraaaaaaaaagh C++ has no tagged unions
		// header->numBones == 0:
		struct {
			Buffer<XMFLOAT2>          vertexLightmapTexcoord;
			Texture                   lightmapTexture;
		};
		// header->numBones > 0:
		struct {
			Buffer<BoneIndices>       vertexBoneIdx;
			Buffer<BoneWeights>       vertexBoneWeight;
		};
	};

	Slice<Texture> textures;

	bool initialized = false;

	Scene() {
		memset(this, 0, sizeof(*this));
	};
	~Scene() { Release(); }
	
	uint32_t getNumBuffers() {
		if (header->numBones == 0) {
			return 6; // WARNING: this needs to be updated when there are lightmaps
		}
		else {
			return 6; 
		}
	}

	bool ReadToCPU(const wchar_t *filename) {
		// ------------------------------------------------------------------------------------------------------------
		// slurp data from file 
		if (DX::ReadDataToSlice(filename, data) != DX::ReadDataStatus::SUCCESS) {
			printf("fuck\n");
			return false;
		}
		
		header = reinterpret_cast<SceneHeader*>(data.ptr);
		// sanity checks
		if (header->version != SCENE_VERSION) {
			printf("eRROR: version of scene file is %d while version of parser is %d \n", header->version, SCENE_VERSION);
			return false;
		}
		if (header->numTriangles == 0) {
			printf("ERROR: scene has no triangles\n");
			return false;
		}
		return true;
	}
	bool SendToGPU(ID3D12Device *device, DescriptorAllocator *descriptorAllocator, ID3D12GraphicsCommandList *commandList) {
		// contains scene data but not the header	
		Slice<BYTE> SceneBuffers = {
			.ptr = data.ptr + sizeof(SceneHeader),
			.len = data.len - sizeof(SceneHeader)
		};
		
		uint32_t numTriangles = header->numTriangles;
		uint32_t numVerts     = numTriangles * VERTS_PER_TRI;

		// create slices to the file blob
		// evil pointer casting >:)
		Slice<XMFLOAT3> vertexPositionSlice = {
			.ptr = reinterpret_cast<XMFLOAT3*>(SceneBuffers.ptr),
			.len = numVerts
		};
		Slice<VertexShadingData> vertexShadingSlice {
			.ptr = reinterpret_cast<VertexShadingData*>(vertexPositionSlice.after()),
			.len = numVerts
		};
		Slice<uint16_t> materialIDSlice {
			.ptr = reinterpret_cast<uint16_t*>(vertexShadingSlice.after()),
			.len = numTriangles
		};
		Slice<Material> materialSlice {
			.ptr = reinterpret_cast<Material*>(materialIDSlice.after()),
			.len = header->numMaterials,
		};
		Slice<TexturePath_t> texturePathSlice{
			.ptr = reinterpret_cast<TexturePath_t*>(materialSlice.after()),
			.len = header->numTextures,
		};

		// create buffers from slices
		vertexPosition.Init(vertexPositionSlice, device, descriptorAllocator, L"Scene Vertex Position Buffer");
		vertexShading .Init(vertexShadingSlice , device, descriptorAllocator, L"Scene Vertex Shading Buffer");
		materials     .Init(materialSlice      , device, descriptorAllocator, L"Scene Material Buffer");
		
		textures = {
			.ptr = reinterpret_cast<Texture*>(calloc(texturePathSlice.len, sizeof(Texture))), 
			.len = texturePathSlice.len
		};

		// load in all textures
		Material defaultMaterial = materialSlice.ptr[0];
		for (uint32_t i = 0; i < textures.len; ++i) {	
			bool success = textures.ptr[i].Init(device, descriptorAllocator, commandList, texturePathSlice.ptr[i]);
			if (!success) {
				for (int j = 1; j < materialSlice.len; ++j) {
					Material* mat = &materialSlice.ptr[j];
					if (mat->base_color == i) mat->base_color = defaultMaterial.base_color;
					if (mat->metallic   == i) mat->metallic   = defaultMaterial.metallic;
					if (mat->roughness  == i) mat->roughness  = defaultMaterial.roughness;
					if (mat->normal     == i) mat->normal     = defaultMaterial.normal;
				}
			}
		}

		materialID.Init(materialIDSlice, device, descriptorAllocator, L"Scene Material ID Buffer");

		if (header->numBones == 0) {
			// static scenes need a lightmap
			Slice<XMFLOAT2> vertexLightmapTexcoordSlice{
				.ptr = reinterpret_cast<XMFLOAT2*>(texturePathSlice.after()),
				.len = numVerts,
			};
			
			auto l = vertexLightmapTexcoordSlice.after();
			auto f = data.after();

			if (l >= f) {
				printf("fuck\n");
			}
			vertexLightmapTexcoord.Init(vertexLightmapTexcoordSlice, device, descriptorAllocator, L"Lightmap Texcoord Buffer");

			// TODO: read lightmap texture
			lightmapTexture.Init(device, descriptorAllocator, commandList, L"./textures/lightmap32.dds");
		}
		else {
			// dynamic scenes need skinning info
			Slice<BoneIndices> vertexBoneIdxSlice {
				.ptr = reinterpret_cast<BoneIndices*>(texturePathSlice.after()),
				.len = numVerts,
			};
			Slice<BoneWeights> vertexBoneWeightSlice{
				.ptr = reinterpret_cast<BoneWeights*>(vertexBoneIdxSlice.after()),
				.len = numVerts,
			};
			
			vertexBoneIdx.Init(vertexBoneIdxSlice, device, descriptorAllocator, L"Vertex Bone Index Buffer");
			vertexBoneWeight.Init(vertexBoneWeightSlice, device, descriptorAllocator, L"Vertex Bone Weight Buffer");
		}
		initialized = true;
		return true;
	}
	void Release() {
		vertexPosition.Release();
		vertexShading.Release();
		materialID.Release();
		materials.Release();
		vertexBoneIdx.Release();
		vertexBoneWeight.Release();
		vertexLightmapTexcoord.Release();

		if (data.ptr != nullptr) free(data.ptr);
		memset(this, 0, sizeof(*this));
	}
};

struct AnimationHeader {
	uint32_t numFrames;
	uint32_t numBones;
};

struct Animation {
	Slice<BYTE> data;

	AnimationHeader *header;
	Buffer<XMFLOAT4X4> invBindTransform;
	Buffer<XMFLOAT4X4> invBindAdjTransform;

	bool Init(ID3D12Device* device, DescriptorAllocator* descriptorAllocator, const wchar_t* filename) {
		DX::ReadDataStatus status = DX::ReadDataToSlice(filename, data);
		if (status != DX::ReadDataStatus::SUCCESS) {
			return false;
		}
		
		header = reinterpret_cast<AnimationHeader*>(data.ptr);

		uint32_t bufferLength = header->numFrames * header->numBones;

		Slice<XMFLOAT4X4> invBindTransformSlice = {
			.ptr = reinterpret_cast<XMFLOAT4X4*>(header + 1),
			.len = bufferLength,
		};
		Slice<XMFLOAT4X4> invBindAdjTransformSlice = {
			.ptr = reinterpret_cast<XMFLOAT4X4*>(invBindTransformSlice.after()),
			.len = bufferLength,
		};

		invBindTransform.Init(invBindTransformSlice, device, descriptorAllocator, L"Position Transform Buffer");
		invBindAdjTransform.Init(invBindAdjTransformSlice, device, descriptorAllocator, L"Normal Transform Buffer");
		return true;
	}

	uint32_t getFrame(std::chrono::time_point<std::chrono::steady_clock> start, std::chrono::time_point<std::chrono::steady_clock> current, bool loop) {
		auto dt = current - start;
		auto dt_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(dt);
		constexpr uint32_t FRAMES_PER_SECOND = 60;
		uint32_t numFramesElapsed = (uint32_t)(dt_seconds.count() * FRAMES_PER_SECOND);
		if (loop) {
			uint32_t frame = numFramesElapsed % header->numFrames;
			return frame;
		}
		else {
			uint32_t frame = min(numFramesElapsed, header->numFrames - 1);
			return frame;
		}
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

	bool Init(ID3D12Device *device, DescriptorAllocator *descriptorAllocator) {
		{
			Vertex cubeverts[6 * 2 * 3] = {
				{ { -0.5f, -0.5f, -0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { -0.5f, 0.5f, 0.5 } },{ { -0.5f, -0.5f, -0.5 } },{ { -0.5f, 0.5f, 0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { -0.5f, 0.5f, 0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { 0.5f, 0.5f, -0.5 } },{ { 0.5f, 0.5f, -0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { 0.5f, -0.5f, 0.5 } },{ { 0.5f, 0.5f, -0.5 } },{ { 0.5f, -0.5f, 0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { 0.5f, -0.5f, 0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { -0.5f, -0.5f, -0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { 0.5f, 0.5f, -0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { -0.5f, 0.5f, -0.5 } },{ { 0.5f, -0.5f, -0.5 } },{ { -0.5f, -0.5f, -0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { -0.5f, 0.5f, 0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { 0.5f, 0.5f, 0.5 } },{ { -0.5f, -0.5f, 0.5 } },{ { 0.5f, -0.5f, 0.5 } }
			};
			const Slice<Vertex> cubeVertsSlice = {
				.ptr = cubeverts,
				.len = _countof(cubeverts),
			};

			vertexBuffer.Init(cubeVertsSlice, device, descriptorAllocator, L"Debug Cube Vertex Buffer");
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
		return true;
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


// Timer UI

struct UIVertex {
	float3 pos;
	float2 uv;
};

struct TimerUI {
	Slice<Texture> uiTextures; // clock base, hand, top
	Buffer<UIVertex> vertexBuffer;

	XMMATRIX ortho;
	float timerHandAngle = 1;  // TODO: adjust later in client game loop

	// TODO these should really be adjustable.... i can't access them yet
	float screenW = 1920.0f, screenH = 1080.0f;
	float side = 0.3f * screenH;
	float inset = 0.0f;

	bool initialized = false;

	// initialize memory
	bool Init(ID3D12Device* device, DescriptorAllocator* descriptorAllocator) {
		float leftX = screenW - side - inset;
		float rightX = screenW - inset;
		float topY = screenH - inset;
		float botY = topY - side;

		// patchwork fix for upside-down textures... just invert UV coordinates.
		UIVertex uiVerts[6] = {
			{{leftX, botY, 0}, {0, 1}},
			{{leftX, topY, 0}, {0, 0}},
			{{rightX,topY,0}, {1, 0}},

			{{leftX, botY, 0}, {0, 1}},
			{{rightX,topY,0}, {1, 0}},
			{{rightX,botY,0}, {1, 1}},
		};
		Slice<UIVertex> slice = { uiVerts, _countof(uiVerts) };

		// vertices are in pixel coordinates, then are projected using ortho projection
		ortho = XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(
			0.0f, screenW,
			0.0f, screenH,
			0.0f, 1.0f
		));

		// upload the buffer
		vertexBuffer.Init(slice, device, descriptorAllocator, L"Timer UI Vertex Buffer");
		return true;
	};

	bool SendToGPU(ID3D12Device* device, DescriptorAllocator* descriptorAllocator, ID3D12GraphicsCommandList* commandList) {
		uiTextures = { reinterpret_cast<Texture*>(calloc(3, sizeof(Texture))), 3 };
		const wchar_t* clockFiles[3] = {
			L"textures\\clock_base.dds",
			L"textures\\clock_hand.dds",
			L"textures\\clock_top.dds"
		};
		for (uint32_t i = 0; i < 3; ++i) {
			bool ok = uiTextures.ptr[i].Init(
				device,
				descriptorAllocator,
				commandList,
				clockFiles[i]);
			if (!ok) return false;
		}
		initialized = true;
		return true;
	}

	// Release GPU resources
	void Release() {
		uiTextures.release();
		vertexBuffer.Release();
	}
};

struct ShopUI {
	Slice<Texture> cardTextures;
	Slice<Texture> soulsTextures;
	Slice<Texture> coinsTextures;
	Buffer<UIVertex> cardVertexBuffer;
	Buffer<UIVertex> counterVertexBuffer;
	XMMATRIX ortho;

	XMMATRIX cardModelMatrix[3];
	XMMATRIX cardSelectedModelMatrix[3];
	XMMATRIX cardCostModelMatrix[3];
	XMMATRIX coinsModelMatrix;
	XMMATRIX soulsModelMatrix;
	float selectedCardMult = 1.2f;

	XMMATRIX scoreboardCardModelMatrix[4][10];
	float scoreboardCardMult = 0.3f;
	float spacingX = 10.0f, spacingY = 10.0f;
	float marginX = 20.0f, marginY = 540.0f; // f magic numbers...

	float screenW = 1920.0f, screenH = 1080.0f;
	float centerX = screenW * 0.5f, centerY = screenH * 0.5f, spacing = 50.0f;

	float cardW = 724.0f/1.5, cardH = 1034.0f/1.5; // magic numbers for scale!!
	float cardCenterX = cardW / 2, cardCenterY = cardH / 2;

	float counterW = 400.0f/1.5f, counterH = 200.0f/1.5f;

	uint8_t powerupIdxs[3] = {0, 0, 0};
	uint8_t powerupCosts[3] = { 0, 0, 0 };
	uint8_t currSelected = 3; // surpasses 0, 1, 2, so that nothing gets highlighted!
	uint8_t coins;
	uint8_t souls;

	bool initialized = false;

	bool Init(ID3D12Device* device, DescriptorAllocator* descriptorAllocator) {
		// patchwork fix for upside-down textures... just invert UV coordinates.
		UIVertex cardVerts[6] = {
			{{0,     0,     0}, {0, 1}},
			{{0,     cardH, 0}, {0, 0}},
			{{cardW, cardH, 0}, {1, 0}},

			{{0,     0,     0}, {0, 1}},
			{{cardW, cardH, 0}, {1, 0}},
			{{cardW, 0,     0}, {1, 1}},
		};
		Slice<UIVertex> cardSlice = { cardVerts, _countof(cardVerts) };

		UIVertex counterVerts[6] = {
			{{0,		0,			0}, {0, 1}},
			{{0,		counterH,	0}, {0, 0}},
			{{counterW, counterH,	0}, {1, 0}},

			{{0,		0,			0}, {0, 1}},
			{{counterW, counterH,	0}, {1, 0}},
			{{counterW, 0,			0}, {1, 1}},
		};
		Slice<UIVertex> counterSlice = { counterVerts, _countof(counterVerts) };

		ortho = XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(
			0.0f, screenW,
			0.0f, screenH,
			0.0f, 1.0f
		));

		cardVertexBuffer.Init(cardSlice, device, descriptorAllocator, L"Shop Card Vertex Buffer");
		counterVertexBuffer.Init(counterSlice, device, descriptorAllocator, L"Shop Counter Vertex Buffer");

		float ty = centerY - cardCenterY;
		for (int i = 0; i < 3; i++) {
			float tx = centerX - cardCenterX
				+ (i - 1) * (cardW + spacing);

			XMMATRIX m = XMMatrixTranslation(tx, ty, 0);
			cardModelMatrix[i] = XMMatrixTranspose(m);
			cardCostModelMatrix[i] = XMMatrixTranspose(XMMatrixTranslation(tx, screenH-counterH-50, 0));
			m = XMMatrixTranslation(-cardCenterX, -cardCenterY, 0)
				* XMMatrixScaling(selectedCardMult, selectedCardMult, 1)
				* XMMatrixTranslation(cardCenterX, cardCenterY, 0)
				* m;
			cardSelectedModelMatrix[i] = XMMatrixTranspose(m);

		}
		coinsModelMatrix = XMMatrixTranspose(XMMatrixTranslation(screenW - counterW - 20.0f, 20.0f + counterH, 0));
		soulsModelMatrix = XMMatrixTranspose(XMMatrixTranslation(screenW - counterW - 20.0f, 20.0f, 0));

		for (int row = 0; row < 4; row++) {
			for (int col = 0; col < 10; col++) {
				float x = marginX + col * (cardW * scoreboardCardMult + spacingX);
				float y = screenH - marginY - row * (cardH * scoreboardCardMult + spacingY);
				XMMATRIX m = XMMatrixTranslation(-cardCenterX, -cardCenterY, 0)
						   * XMMatrixScaling(scoreboardCardMult, scoreboardCardMult, 1)
						   * XMMatrixTranslation(cardCenterX, cardCenterY, 0)
						   * XMMatrixTranslation(x, y, 0);
				scoreboardCardModelMatrix[row][col] = XMMatrixTranspose(m);
			}
		}
		return true;
	}

	bool SendToGPU(ID3D12Device* device, DescriptorAllocator* descriptorAllocator, ID3D12GraphicsCommandList* commandList) {
		cardTextures = { reinterpret_cast<Texture*>(calloc(PowerupInfo.size(), sizeof(Texture))), (uint32_t) PowerupInfo.size() };
		auto it = PowerupInfo.begin();
		for (uint32_t i = 0; i < PowerupInfo.size(); ++i) {
			bool ok = cardTextures.ptr[i].Init(
				device,
				descriptorAllocator,
				commandList,
				it->second.fileLocation.c_str());
			if (!ok) return false;
			++it;
		}

		{
			coinsTextures = { reinterpret_cast<Texture*>(calloc(10, sizeof(Texture))), 10 };
			coinsTextures.ptr[0].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_0.dds");
			coinsTextures.ptr[1].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_1.dds");
			coinsTextures.ptr[2].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_2.dds");
			coinsTextures.ptr[3].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_3.dds");
			coinsTextures.ptr[4].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_4.dds");
			coinsTextures.ptr[5].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_5.dds");
			coinsTextures.ptr[6].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_6.dds");
			coinsTextures.ptr[7].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_7.dds");
			coinsTextures.ptr[8].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_8.dds");
			coinsTextures.ptr[9].Init(device, descriptorAllocator, commandList, L"textures\\coins\\coin_9.dds");
		}
		
		{
			soulsTextures = { reinterpret_cast<Texture*>(calloc(10, sizeof(Texture))), 10 };
			soulsTextures.ptr[0].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_0.dds");
			soulsTextures.ptr[1].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_1.dds");
			soulsTextures.ptr[2].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_2.dds");
			soulsTextures.ptr[3].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_3.dds");
			soulsTextures.ptr[4].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_4.dds");
			soulsTextures.ptr[5].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_5.dds");
			soulsTextures.ptr[6].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_6.dds");
			soulsTextures.ptr[7].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_7.dds");
			soulsTextures.ptr[8].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_8.dds");
			soulsTextures.ptr[9].Init(device, descriptorAllocator, commandList, L"textures\\souls\\soul_9.dds");
		}

		initialized = true;
		return true;
	}

	bool Release() {
		cardTextures.release();
		coinsTextures.release();
		soulsTextures.release();
		cardVertexBuffer.Release();
		counterVertexBuffer.Release();
	}
};

class Renderer {
public:
	bool Init(HWND window_handle);
	Renderer();
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

	static constexpr float CAMERA_DIST = 16.0f * PLAYER_SCALING_FACTOR;
	static constexpr float CAMERA_UP = 6.0f * PLAYER_SCALING_FACTOR;

	// helper getters
	UINT getWidth() { return m_width; };
	UINT getHeight() { return m_height; };

	//helper setters
	void updateCamera(float yaw, float pitch) {
		cameraYaw = yaw;
		cameraPitch = pitch;
	}

	void updateTimer(float timerFrac) {
		m_TimerUI.timerHandAngle = XM_2PI * timerFrac;
	}

	void updatePowerups(Powerup p0, Powerup p1, Powerup p2) {
		// deselect when updating powerup selection
		m_ShopUI.currSelected = 3;
		m_ShopUI.powerupIdxs[0] = PowerupInfo[p0].textureIdx;
		m_ShopUI.powerupIdxs[1] = PowerupInfo[p1].textureIdx;
		m_ShopUI.powerupIdxs[2] = PowerupInfo[p2].textureIdx;
		m_ShopUI.powerupCosts[0] = PowerupInfo[p0].cost;
		m_ShopUI.powerupCosts[1] = PowerupInfo[p1].cost;
		m_ShopUI.powerupCosts[2] = PowerupInfo[p2].cost;
	}

	void selectPowerup(uint8_t selection) {
		m_ShopUI.currSelected = selection;
	}

	void updateCurrency(uint8_t coins, uint8_t souls) {
		m_ShopUI.coins = coins;
		m_ShopUI.souls = souls;
	}

	void updatePlayerPowerups(uint8_t* playerPowerups) {
		memcpy(this->powerupInfo, playerPowerups, sizeof(PlayerPowerupPayload));
	}

	GamePhase gamePhase;
	bool activeScoreboard = true;

	// spectator camera settings
	bool detached = false;
	XMFLOAT3 freecamPos = {};
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
	
	ComPtr<ID3D12PipelineState> m_pipelineStateSkin;

	// for debug drawing
	// ComPtr<ID3D12PipelineState> m_pipelineStateDebug;

	// for timer UI
	
	ComPtr<ID3D12PipelineState> m_pipelineStateTimerUI;
	TimerUI						m_TimerUI;

	// for shop UI
	ShopUI						m_ShopUI;

	// for scoreboard
	uint8_t powerupInfo[4][20];

	// syncrhonization objects
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[FramesInFlight];
	
	// TODO: make these adjustable
	UINT m_width = 1920;
	UINT m_height = 1080;
	float m_aspectRatio = 16.0f / 9.0f;
	float m_fov = XMConvertToRadians(90 * (9.0/16.0)); 
	// ComPtr<ID3D12Resource> m_vertexBuffer;
	// D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	Scene m_scene;
	Scene m_runnerRenderBuffers;
	Scene m_hunterRenderBuffers;

	Animation m_hunterAnimations[HUNTER_ANIMATION_COUNT];
	Animation m_runnerAnimations[RUNNER_ANIMATION_COUNT];
	

	// ComPtr<ID3D12DescriptorHeap> m_resourceHeap;
	DescriptorAllocator m_resourceDescriptorAllocator;
	// ComPtr<ID3D12Resource> m_constantBuffer; // references the concept of the plan of the concept buffer or something
	UINT8 *m_pCbvDataBegin; // virtual address of the constant buffer memory

	bool MoveToNextFrame();
	bool WaitForGpu();

	XMMATRIX computeViewProject(FXMVECTOR pos, LookDir lookDir);
	XMMATRIX computeFreecamViewProject(FXMVECTOR pos, float yaw, float pitch);
	XMMATRIX computeModelMatrix(PlayerRenderState &playerRenderState);
	
	ComPtr<ID3D12Resource> m_depthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> m_depthStencilDescriptorHeap;
	
	// CAMERA CONSTANTS
	float cameraYaw   = 0.0f;
	float cameraPitch = 0.0f;
};


template<typename T>
inline bool Buffer<T>::Init(Slice<T> inData, ID3D12Device* device, DescriptorAllocator *descriptorAllocator, const wchar_t *debugName)
{
	data = inData;

	// create implicit heap and resource
	D3D12_HEAP_PROPERTIES heapProperties = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(data.numBytes());
	UNWRAP(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	));
	resource->SetName(debugName);

	
	// allocate and create SRV
	descriptor = descriptorAllocator->Allocate();
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		.Format                  = DXGI_FORMAT_UNKNOWN,
		.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer = {
			.FirstElement        = 0,
			.NumElements         = data.len,
			.StructureByteStride = sizeof(T)
		}
	};
	device->CreateShaderResourceView(resource.Get(), &desc, descriptor.cpu);


	// map GPU memory
	D3D12_RANGE nullRange = {};
	UNWRAP(
		resource->Map(0, &nullRange, &shared_ptr)
	);


	// copy data to GPU
	memcpy(shared_ptr, data.ptr, data.numBytes());
	return true;
}

template<typename T>
inline bool Buffer<T>::Init(T *ptr, uint32_t len, ID3D12Device *device, DescriptorAllocator *descriptorAllocator, const wchar_t *debugName) {
	this->Init(Slice<T>{ptr, len}, device, descriptorAllocator, debugName);
}
template<typename T>
inline void Buffer<T>::Release()
{
	if (resource) {
		resource->Unmap(0, nullptr);
		resource->Release();
	}
	memset(this, 0, sizeof(*this));
}
