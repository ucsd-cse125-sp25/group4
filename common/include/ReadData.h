//--------------------------------------------------------------------------------------
// File: ReadData.h
//
// Helper for loading binary data files from disk
//
// For Windows desktop apps, it looks for files in the same folder as the running EXE if
// it can't find them in the CWD
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//-------------------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <windows.h>
#include <assert.h>

template<typename T>
struct Slice {
	T*       ptr; // pointer to first element
	uint32_t len; // length in number of elements


	inline UINT numBytes() {
		return len * sizeof(T);
	}
	
	// returns the pointer to the first byte beyond the end of the array
	inline BYTE* after() {
		return reinterpret_cast<BYTE*>(&(ptr[len]));
	}

    inline void release() {
        if (ptr != nullptr) {
            free(ptr);
        }
        memset(this, 0, sizeof(*this));
    }
};

namespace DX
{
    enum class ReadDataStatus : int {
        SUCCESS,
        ERROR_SYSTEM,
        ERROR_SPLIT_PATH,
        ERROR_MAKE_PATH,
        ERROR_FILE_OPEN,
        ERROR_GET_READ_POSITION,
        ERROR_OOM,
        ERROR_SEEK_FILE,
        ERROR_READ,
    };
    
    // reads bytes from a file into a slice
    // a null terminator may optionally be inserted at the end of the buffer
    // the caller is responsible for freeing the memory allocated by this function
    // unless this function does not run successfully
    template<typename T>
    inline ReadDataStatus ReadDataToSlice(_In_z_ const wchar_t* name, Slice<T>& slice, bool appendNullTerminator = false)
    {
        if (appendNullTerminator) {
            assert(sizeof(T) == sizeof(BYTE));
        }
        std::ifstream inFile(name, std::ios::in | std::ios::binary | std::ios::ate);

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
        if (!inFile)
        {
            wchar_t moduleName[_MAX_PATH] = {};
            if (!GetModuleFileNameW(nullptr, moduleName, _MAX_PATH)) return ReadDataStatus::ERROR_SYSTEM;

            wchar_t drive[_MAX_DRIVE];
            wchar_t path[_MAX_PATH];

            if (_wsplitpath_s(moduleName, drive, _MAX_DRIVE, path, _MAX_PATH, nullptr, 0, nullptr, 0))
                return ReadDataStatus::ERROR_SPLIT_PATH;

            wchar_t filename[_MAX_PATH];
            if (_wmakepath_s(filename, _MAX_PATH, drive, path, name, nullptr)) return ReadDataStatus::ERROR_MAKE_PATH;

            inFile.open(filename, std::ios::in | std::ios::binary | std::ios::ate);
        }
#endif

        if (!inFile) return ReadDataStatus::ERROR_FILE_OPEN;

        const std::streampos len = inFile.tellg();
        if (!inFile) return ReadDataStatus::ERROR_GET_READ_POSITION;
        
        size_t sliceLength = appendNullTerminator ? (size_t)len + 1 : (size_t)len;
        size_t numElems = sliceLength / sizeof(T);
		slice = {
			.ptr = (T*)malloc(numElems * sizeof(T)),
            .len = (uint32_t)numElems,
		};
        if (slice.ptr == nullptr) return ReadDataStatus::ERROR_OOM;

        inFile.seekg(0, std::ios::beg);
        if (!inFile) {
            slice.release();
            return ReadDataStatus::ERROR_SEEK_FILE;
        }
        
        char* charptr = reinterpret_cast<char*>(slice.ptr);
        inFile.read(charptr, len);
        if (!inFile) {
            slice.release();
            return ReadDataStatus::ERROR_READ;
        }
        if (appendNullTerminator) charptr[slice.len - 1] = '\0';

        inFile.close();

        return ReadDataStatus::SUCCESS;
    }
}
