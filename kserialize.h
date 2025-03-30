#pragma once
#ifndef K_SERIALIZE_IMPL
#define K_SERIALIZE_IMPL



#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <unordered_map>

#include <fcntl.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include "permwin.h"
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <io.h>
#define OS_WIN
#elif defined(__linux__) || defined(__gnu_linux__) || defined(linux) || defined(__linux)
#define OS_LINUX
#endif

namespace fs = std::filesystem;

struct filesystem_object {
    uint8_t isDir;
    fs::path filename;
    int32_t win_permissions;
    int32_t linux_permissions;
    uint64_t file_size;
    fs::path full_path;
};


void addToLog(std::u8string message);
void throw_u8string_error(std::u8string s);


void read_fso_isDir_size_permissions(filesystem_object& fso);
void process_directory(const fs::path& directory_path, 
                       std::unordered_map<fs::path, filesystem_object>& old_files,
                       std::vector<filesystem_object>& new_files);
void extract_old_fso_info(const fs::path& output_file_name, std::vector<filesystem_object>& old_files);
void write_fso_map_to_file(const fs::path& output_file_name, const std::vector<filesystem_object>& fso_v);
void create_files(const std::vector<filesystem_object>& fso_v, fs::path serialized_file_path, fs::path output_dir_path);

void serialzie(fs::path input_path, fs::path output_path);
void deserialize(fs::path input_file_name, fs::path output_file_path);


void fill_other_system_permissions(filesystem_object& fso, std::unordered_map<fs::path, filesystem_object>& fso_map) {
    if (fso_map.find(fso.filename) != fso_map.end()) {
        fso.win_permissions = fso_map[fso.filename].win_permissions;
        fso.linux_permissions = fso_map[fso.filename].linux_permissions;
    }
    else {
        fso.win_permissions = 0;
        fso.linux_permissions = 0;
    }
}

void read_fso_isDir_size_permissions(filesystem_object& fso) {
    fso.isDir = fs::is_directory(fso.full_path) ? 1 : 0;
    if (fso.isDir) {
        fso.file_size = 0;
    }
    else {
        fso.file_size = fs::file_size(fso.full_path);
    }

    //reading permissions
#if defined (OS_WIN)
    std::wstring widePath = fso.full_path.wstring();
    LPCWSTR filePath = widePath.c_str();
    int32_t permissions = GetCurrentUserFilePermissionsWin(filePath);

    if (permissions != 0) {
        fso.win_permissions = permissions;
        addToLog(u8"read permissions for " + fso.full_path.u8string());
    }
    else {
        throw_u8string_error(u8"failed to get current user's permissions or no explicit permissions found for " + fso.full_path.u8string());
    }
#elif defined (OS_LINUX)
    fs::file_status status = fs::status(fso.full_path);
    fs::perms permissions = status.permissions();
    fso.linux_permissions = static_cast<uint32_t>(permissions);
#endif

}

void process_directory(const fs::path& directory_path, std::unordered_map<fs::path, filesystem_object>& old_files, std::vector<filesystem_object>& new_files) {
    for (const auto& entry : fs::recursive_directory_iterator(directory_path)) {
        filesystem_object fso;
        fso.full_path = entry.path();
        fso.filename = fs::relative(entry.path(), directory_path.parent_path());
        fill_other_system_permissions(fso, old_files);
        read_fso_isDir_size_permissions(fso);
        new_files.push_back(fso);
    }
}

void extract_old_fso_info(const fs::path& output_file, std::vector<filesystem_object>& old_files) {
    std::ifstream in(output_file, std::ios::binary);
    if (!in) {
        throw_u8string_error(u8"falied to open " + output_file.u8string() + u8" for reading");
    }

    uint32_t num_objects;
    in.read(reinterpret_cast<char*>(&num_objects), sizeof(num_objects));

    for (int i = 0; i < num_objects; i++) {
        filesystem_object fso;
        in.read(reinterpret_cast<char*>(&fso.isDir), sizeof(fso.isDir));

        uint32_t filename_len;
        in.read(reinterpret_cast<char*>(&filename_len), sizeof(filename_len));

#if defined(OS_WIN)
        // read wstring from file 
        std::vector<uint8_t> buffer(filename_len);
        in.read(reinterpret_cast<char*>(buffer.data()), filename_len);
        const size_t wchar_count = filename_len / sizeof(wchar_t);
        const wchar_t* wchars = reinterpret_cast<const wchar_t*>(buffer.data());
        std::wstring ws = std::wstring(wchars, wchar_count);
        fso.filename = fs::path(ws);

#elif defined(OS_LINUX)
        std::string utf8_str;
        utf8_str.resize(filename_len);
        in.read(utf8_str.data(), filename_len);
        fso.filename = fs::u8path(utf8_str);
#endif

        in.read(reinterpret_cast<char*>(&fso.win_permissions), sizeof(fso.win_permissions));
        in.read(reinterpret_cast<char*>(&fso.linux_permissions), sizeof(fso.linux_permissions));
        in.read(reinterpret_cast<char*>(&fso.file_size), sizeof(fso.file_size));

        old_files.push_back(fso);
    }
    if (!in) {
        throw_u8string_error(u8"error reading from file (possibly incorrect data format): " + output_file.u8string());
    }

    in.close();
}

void write_fso_map_to_file(const fs::path& output_file, const std::vector<filesystem_object>& fso_v) {
    std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw_u8string_error(u8"failed to open " + output_file.u8string() + u8" for writing");
    }
    uint32_t num_objects = static_cast<uint32_t>(fso_v.size());
    out.write(reinterpret_cast<const char*>(&num_objects), sizeof(num_objects));

    for (const auto& fso : fso_v) {
        out.write(reinterpret_cast<const char*>(&fso.isDir), sizeof(fso.isDir));


#if defined(OS_WIN)
        uint32_t filename_len_bytes = static_cast<uint32_t>(fso.filename.wstring().size() * sizeof(wchar_t));
        out.write(reinterpret_cast<const char*>(&filename_len_bytes), sizeof(filename_len_bytes));
        auto wstr = fso.filename.wstring();
        out.write(reinterpret_cast<const char*>(&(wstr[0])), filename_len_bytes);

#elif defined(OS_LINUX)
        uint32_t filename_len_bytes = static_cast<uint32_t>(fso.filename.u8string().size());
        out.write(reinterpret_cast<const char*>(&filename_len_bytes), sizeof(filename_len_bytes));
        auto wstr = fso.filename.u8string();
        out.write(reinterpret_cast<const char*>(&(wstr[0])), filename_len_bytes);
#endif

        out.write(reinterpret_cast<const char*>(&fso.win_permissions), sizeof(fso.win_permissions));
        out.write(reinterpret_cast<const char*>(&fso.linux_permissions), sizeof(fso.linux_permissions));
        out.write(reinterpret_cast<const char*>(&fso.file_size), sizeof(fso.file_size));
    }

    for (const auto& fso : fso_v) {
        if (!fso.isDir && fso.file_size > 0) {
            std::ifstream in(fso.full_path, std::ios::binary);
            if (!in) {
                throw_u8string_error(u8"failed to open source file: " + fso.full_path.u8string());
            }

            //std::vector<char> buffer(fso.file_size);
            //in_file.read(buffer.data(), fso.file_size);
            //out.write(buffer.data(), fso.file_size);
            if (!(out << in.rdbuf()))
                throw_u8string_error(u8"failed to write " + fso.full_path.u8string() + u8" data to " + output_file.u8string());
        }
    }
}

void create_files(const std::vector<filesystem_object>& fso_v,
    fs::path serialized_file_path, fs::path output_dir_path) {

    std::ifstream serialized_file(serialized_file_path, std::ios::binary);
    if (!serialized_file) {
        throw_u8string_error(u8"failed to open " + serialized_file_path.u8string());
    }

    uint32_t num_fsos;
    serialized_file.read(reinterpret_cast<char*>(&num_fsos), sizeof(num_fsos));

    // data begins (4 + (1+4+filename_len+4+4+8)*num_fsos)
    size_t data_offset = sizeof(num_fsos);
    for (uint32_t i = 0; i < num_fsos; ++i) {
        uint8_t isDir;
        uint32_t filename_len;
        serialized_file.read(reinterpret_cast<char*>(&isDir), sizeof(isDir));
        serialized_file.read(reinterpret_cast<char*>(&filename_len), sizeof(filename_len));

        serialized_file.seekg(filename_len + sizeof(uint32_t) * 2 + sizeof(uint64_t), std::ios::cur);
        data_offset += sizeof(isDir) + sizeof(filename_len) + filename_len + sizeof(uint32_t) * 2 + sizeof(uint64_t);
    }

    for (const auto& fso : fso_v) {
        try {
            fs::path new_file_path = output_dir_path / fso.filename;

#if defined(OS_WIN)  
            std::wstring widePath = new_file_path.wstring();
            LPWSTR output_file_path = (LPWSTR)(widePath.c_str());

            if (!fso.isDir && !CreateFileWithInheritanceWin(output_file_path)) {
                throw_u8string_error(u8"failed to create file " + new_file_path.u8string());
            }

            if (fso.isDir) {
                if (!CreateDirectoryWithInheritedPermissions(output_file_path)) {
                    throw_u8string_error(u8"failed to create folder " + new_file_path.u8string());
                }
            }
            addToLog(u8"created " + (output_dir_path / fso.filename).u8string());

            if (!fso.isDir) {
                std::ofstream output_file(output_file_path, std::ios::binary);
                if (!output_file) {
                    throw_u8string_error(u8"failed to open " + new_file_path.u8string());
                }

                std::vector<char> buffer(fso.file_size);
                serialized_file.read(buffer.data(), fso.file_size);
                output_file.write(buffer.data(), fso.file_size);
                output_file.close();
                addToLog(u8"wrote data to " + new_file_path.u8string());
            }
            
            // if (fso.win_permissions == 0){
            //     fs::permissions(new_file_path, static_cast<fs::perms>(fso.linux_permissions));
            // }   

            if (!SetCurrentUserPermissionsWin(output_file_path, fso.win_permissions)) {
                throw_u8string_error(u8"failed to set permissions for " + new_file_path.u8string());
            }

#elif defined(OS_LINUX)
            if (fso.isDir) {
                fs::create_directory(new_file_path);
            } else {
                std::ofstream file(new_file_path);
                if (!file) throw_u8string_error(u8"failed to create " +  new_file_path.u8string());
                file.close();
            }

            if (!fso.isDir) {
                std::ofstream output_file(new_file_path, std::ios::binary);
                if (!output_file) {
                    throw_u8string_error(u8"failed to open " + new_file_path.u8string());
                }

                std::vector<char> buffer(fso.file_size);
                serialized_file.read(buffer.data(), fso.file_size);
                output_file.write(buffer.data(), fso.file_size);
                output_file.close();
                addToLog(u8"wrote data to " + new_file_path.u8string());
            }
            
            // if (fso.linux_permissions == 0){
            //     fs::permissions(new_file_path, static_cast<fs::perms>(fso.win_permissions));
            // }

            fs::permissions(new_file_path, static_cast<fs::perms>(fso.linux_permissions));
#endif
        }
        catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to process file '") +
                fso.filename.string() + "': " + e.what());
        }
    }
}

void serialize(fs::path input_path, fs::path output_path) {
    std::vector<filesystem_object> fso_v;
    if (fs::file_size(output_path) != 0)
        extract_old_fso_info(output_path, fso_v);

    std::unordered_map<fs::path, filesystem_object> fso_map;
    for (const auto& fso : fso_v) {
        fso_map[fso.filename] = fso;
    }

    fso_v.clear();

    //first object (start dir or only file)
    filesystem_object first_fso;
    first_fso.filename = input_path.filename();
    first_fso.full_path = input_path;
    fill_other_system_permissions(first_fso, fso_map);
    read_fso_isDir_size_permissions(first_fso);
    fso_v.push_back(first_fso);

    if (fs::is_directory(input_path)) {
        process_directory(input_path, fso_map, fso_v);
    }

    std::sort(fso_v.begin(), fso_v.end(),
        [](const filesystem_object& a, const filesystem_object& b) {
            return a.filename < b.filename; });

    write_fso_map_to_file(output_path, fso_v);
}

void deserialize(fs::path input_file_name, fs::path output_file_path) {
    std::vector<filesystem_object> fso_v;
    extract_old_fso_info(input_file_name, fso_v);
    create_files(fso_v, input_file_name, output_file_path);
}

#endif