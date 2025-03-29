#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstring> 

namespace fs = std::filesystem;

DWORD GetCurrentUserFilePermissionsWin(LPCWSTR filePath);
BOOL CreateFileWithInheritanceWin(LPCWSTR filePath);
BOOL CreateDirectoryWithInheritedPermissions(LPCWSTR lpPathName);
BOOL SetCurrentUserPermissionsWin(LPWSTR filePath, DWORD permissionMask);


DWORD GetCurrentUserFilePermissionsWin(LPCWSTR filePath) {
    PSECURITY_DESCRIPTOR pSD = NULL;
    PACL pDacl = NULL;
    BOOL bDaclPresent = FALSE;
    BOOL bDaclDefaulted = FALSE;
    DWORD dwRet = 0;
    DWORD currentUserMask = 0;

    // Get current user's SID
    HANDLE hToken = NULL;
    PTOKEN_USER pTokenUser = NULL;
    DWORD dwSize = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        std::wcerr << L"OpenProcessToken failed: " << GetLastError() << std::endl;
        return 0;
    }

    // Get token user information size
    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
    pTokenUser = (PTOKEN_USER)malloc(dwSize);
    if (!pTokenUser) {
        std::wcerr << L"Memory allocation failed" << std::endl;
        CloseHandle(hToken);
        return 0;
    }

    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
        std::wcerr << L"GetTokenInformation failed: " << GetLastError() << std::endl;
        free(pTokenUser);
        CloseHandle(hToken);
        return 0;
    }

    // Get the security descriptor for the file
    dwRet = GetNamedSecurityInfoW(
        filePath,
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        NULL, NULL, &pDacl, NULL, &pSD);

    if (dwRet != ERROR_SUCCESS) {
        std::wcerr << L"GetNamedSecurityInfo failed: " << dwRet << std::endl;
        free(pTokenUser);
        CloseHandle(hToken);
        return 0;
    }

    // Check if DACL is present
    if (!GetSecurityDescriptorDacl(pSD, &bDaclPresent, &pDacl, &bDaclDefaulted) || !bDaclPresent) {
        std::wcerr << L"No DACL present or error getting DACL" << std::endl;
        LocalFree(pSD);
        free(pTokenUser);
        CloseHandle(hToken);
        return 0;
    }

    // Iterate through ACEs in the DACL
    ACL_SIZE_INFORMATION aclSizeInfo;
    if (!GetAclInformation(pDacl, &aclSizeInfo, sizeof(aclSizeInfo), AclSizeInformation)) {
        std::wcerr << L"GetAclInformation failed" << std::endl;
        LocalFree(pSD);
        free(pTokenUser);
        CloseHandle(hToken);
        return 0;
    }

    for (DWORD i = 0; i < aclSizeInfo.AceCount; i++) {
        ACCESS_ALLOWED_ACE* pAce = NULL;
        if (!GetAce(pDacl, i, (LPVOID*)&pAce)) {
            std::wcerr << L"GetAce failed for index " << i << std::endl;
            continue;
        }

        // Check if this ACE applies to the current user
        if (EqualSid((PSID)&pAce->SidStart, pTokenUser->User.Sid)) {
            currentUserMask = pAce->Mask;
            break;
        }
    }

    LocalFree(pSD);
    free(pTokenUser);
    CloseHandle(hToken);

    return currentUserMask;
}


BOOL CreateFileWithInheritanceWin(LPCWSTR filePath) {
    HANDLE hFile = CreateFileW(
        filePath,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::wcerr << L"CreateFile failed: " << GetLastError() << std::endl;
        return FALSE;
    }
    CloseHandle(hFile);
    return TRUE;
}

BOOL CreateDirectoryWithInheritedPermissions(LPCWSTR lpPathName) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = FALSE;

    // Set this to NULL to get inheritance from parent directory
    sa.lpSecurityDescriptor = NULL;
    if (!CreateDirectoryW(lpPathName, &sa)) {
        return FALSE;
    }

    return TRUE;
}

BOOL SetCurrentUserPermissionsWin(LPWSTR filePath, DWORD permissionMask) {
    // Get current user's SID
    HANDLE hToken = NULL;
    PTOKEN_USER pTokenUser = NULL;
    DWORD dwSize = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        std::wcerr << L"OpenProcessToken failed: " << GetLastError() << std::endl;
        return FALSE;
    }

    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
    pTokenUser = (PTOKEN_USER)malloc(dwSize);
    if (!pTokenUser) {
        std::wcerr << L"Memory allocation failed" << std::endl;
        CloseHandle(hToken);
        return FALSE;
    }

    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
        std::wcerr << L"GetTokenInformation failed: " << GetLastError() << std::endl;
        free(pTokenUser);
        CloseHandle(hToken);
        return FALSE;
    }

    // Get existing DACL (with all permissions)
    PACL pOldDacl = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    DWORD dwRes = GetNamedSecurityInfoW(
        filePath,
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION,
        NULL, NULL, &pOldDacl, NULL, &pSD);

    if (dwRes != ERROR_SUCCESS) {
        std::wcerr << L"GetNamedSecurityInfo failed: " << dwRes << std::endl;
        free(pTokenUser);
        CloseHandle(hToken);
        return FALSE;
    }

    // Prepare explicit access entries for all existing permissions
    ACL_SIZE_INFORMATION aclSizeInfo;
    if (!GetAclInformation(pOldDacl, &aclSizeInfo, sizeof(aclSizeInfo), AclSizeInformation)) {
        std::wcerr << L"GetAclInformation failed" << std::endl;
        LocalFree(pSD);
        free(pTokenUser);
        CloseHandle(hToken);
        return FALSE;
    }

    std::vector<EXPLICIT_ACCESS_W> eaList;

    // Add all existing permissions (except current user)
    for (DWORD i = 0; i < aclSizeInfo.AceCount; i++) {
        ACE_HEADER* pAceHeader = NULL;
        if (!GetAce(pOldDacl, i, (LPVOID*)&pAceHeader)) {
            std::wcerr << L"GetAce failed for index " << i << std::endl;
            continue;
        }

        // Skip if this is the current user's ACE
        if (pAceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE) {
            ACCESS_ALLOWED_ACE* pAce = (ACCESS_ALLOWED_ACE*)pAceHeader;
            if (EqualSid((PSID)&pAce->SidStart, pTokenUser->User.Sid)) {
                continue;
            }
        }

        EXPLICIT_ACCESS_W ea;
        ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS_W));
        ea.grfAccessMode = (pAceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE) ? GRANT_ACCESS : DENY_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        if (fs::is_directory(fs::path(filePath))) {
            ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        }
        if (pAceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE) {
            ACCESS_ALLOWED_ACE* pAce = (ACCESS_ALLOWED_ACE*)pAceHeader;
            ea.grfAccessPermissions = pAce->Mask;
            ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            ea.Trustee.ptstrName = (LPWSTR)&pAce->SidStart;
        }
        else if (pAceHeader->AceType == ACCESS_DENIED_ACE_TYPE) {
            ACCESS_DENIED_ACE* pAce = (ACCESS_DENIED_ACE*)pAceHeader;
            ea.grfAccessPermissions = pAce->Mask;
            ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            ea.Trustee.ptstrName = (LPWSTR)&pAce->SidStart;
        }

        eaList.push_back(ea);
    }

    //Add current user's new permissions
    EXPLICIT_ACCESS_W currentUserEA = { 0 };
    currentUserEA.grfAccessPermissions = permissionMask;
    currentUserEA.grfAccessMode = GRANT_ACCESS;
    currentUserEA.grfInheritance = NO_INHERITANCE;
    if (fs::is_directory(fs::path(filePath))) {
        currentUserEA.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    }
    currentUserEA.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    currentUserEA.Trustee.TrusteeType = TRUSTEE_IS_USER;
    currentUserEA.Trustee.ptstrName = (LPWSTR)pTokenUser->User.Sid;
    eaList.push_back(currentUserEA);

    // Create new DACL with all permissions
    PACL pNewDacl = NULL;
    dwRes = SetEntriesInAclW(eaList.size(), eaList.data(), NULL, &pNewDacl);
    if (dwRes != ERROR_SUCCESS) {
        std::wcerr << L"SetEntriesInAcl failed: " << dwRes << std::endl;
        LocalFree(pSD);
        free(pTokenUser);
        CloseHandle(hToken);
        return FALSE;
    }

    // Apply the new DACL to the file
    dwRes = SetNamedSecurityInfoW(
        filePath,
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        NULL, NULL, pNewDacl, NULL);

    LocalFree(pNewDacl);
    LocalFree(pSD);
    free(pTokenUser);
    CloseHandle(hToken);

    return dwRes == ERROR_SUCCESS;
}
