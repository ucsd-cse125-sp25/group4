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
    inline std::vector<uint8_t> ReadData(_In_z_ const wchar_t* name)
    {
        std::ifstream inFile(name, std::ios::in | std::ios::binary | std::ios::ate);

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
        if (!inFile)
        {
            wchar_t moduleName[_MAX_PATH] = {};
            if (!GetModuleFileNameW(nullptr, moduleName, _MAX_PATH))
                throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "GetModuleFileNameW");

            wchar_t drive[_MAX_DRIVE];
            wchar_t path[_MAX_PATH];

            if (_wsplitpath_s(moduleName, drive, _MAX_DRIVE, path, _MAX_PATH, nullptr, 0, nullptr, 0))
                throw std::runtime_error("_wsplitpath_s");

            wchar_t filename[_MAX_PATH];
            if (_wmakepath_s(filename, _MAX_PATH, drive, path, name, nullptr))
                throw std::runtime_error("_wmakepath_s");

            inFile.open(filename, std::ios::in | std::ios::binary | std::ios::ate);
        }
#endif

        if (!inFile)
            throw std::runtime_error("ReadData");

        const std::streampos len = inFile.tellg();
        if (!inFile)
            throw std::runtime_error("ReadData");

        std::vector<uint8_t> blob;
        blob.resize(size_t(len));

        inFile.seekg(0, std::ios::beg);
        if (!inFile)
            throw std::runtime_error("ReadData");

        inFile.read(reinterpret_cast<char*>(blob.data()), len);
        if (!inFile)
            throw std::runtime_error("ReadData");

        inFile.close();

        return blob;
    }


    inline ReadDataStatus ReadDataToPtr(_In_z_ const wchar_t* name, Slice<BYTE>& slice, bool appendNullTerminator = false)
    {
        std::ifstream inFile(name, std::ios::in | std::ios::binary | std::ios::ate);

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
        if (!inFile)
        {
            wchar_t moduleName[_MAX_PATH] = {};
            if (!GetModuleFileNameW(nullptr, moduleName, _MAX_PATH))
                return ReadDataStatus::ERROR_SYSTEM;
                // throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "GetModuleFileNameW");

            wchar_t drive[_MAX_DRIVE];
            wchar_t path[_MAX_PATH];

            if (_wsplitpath_s(moduleName, drive, _MAX_DRIVE, path, _MAX_PATH, nullptr, 0, nullptr, 0))
                return ReadDataStatus::ERROR_SPLIT_PATH;
                // throw std::runtime_error("_wsplitpath_s");

            wchar_t filename[_MAX_PATH];
            if (_wmakepath_s(filename, _MAX_PATH, drive, path, name, nullptr))
                return ReadDataStatus::ERROR_MAKE_PATH;
                // throw std::runtime_error("_wmakepath_s");

            inFile.open(filename, std::ios::in | std::ios::binary | std::ios::ate);
        }
#endif

        if (!inFile)
			return ReadDataStatus::ERROR_FILE_OPEN;
            // throw std::runtime_error("ReadData");

        const std::streampos len = inFile.tellg();
        if (!inFile)
			return ReadDataStatus::ERROR_GET_READ_POSITION;
            // throw std::runtime_error("ReadData");
        
        size_t sliceLength = appendNullTerminator ? (size_t)len + 1 : (size_t)len;
		slice = {
			.ptr = (BYTE*)malloc(sliceLength),
            .len = (uint32_t)sliceLength,
		};
        if (slice.ptr == nullptr) return ReadDataStatus::ERROR_OOM;

        inFile.seekg(0, std::ios::beg);
        if (!inFile)
			return ReadDataStatus::ERROR_SEEK_FILE;
            // throw std::runtime_error("ReadData");

        inFile.read(reinterpret_cast<char*>(slice.ptr), len);
        if (!inFile)
			return ReadDataStatus::ERROR_READ;
            // throw std::runtime_error("ReadData");
        if (appendNullTerminator) {
            slice.ptr[slice.len - 1] = '\0';
        }
        inFile.close();

        return ReadDataStatus::SUCCESS;
    }
}
