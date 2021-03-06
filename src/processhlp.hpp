/*
    * WinDBG Anti-RootKit extension
    * Copyright � 2013-2018  Vyacheslav Rusakoff
    * 
    * This program is free software: you can redistribute it and/or modify
    * it under the terms of the GNU General Public License as published by
    * the Free Software Foundation, either version 3 of the License, or
    * (at your option) any later version.
    * 
    * This program is distributed in the hope that it will be useful,
    * but WITHOUT ANY WARRANTY; without even the implied warranty of
    * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    * GNU General Public License for more details.
    * 
    * You should have received a copy of the GNU General Public License
    * along with this program.  If not, see <http://www.gnu.org/licenses/>.

    * This work is licensed under the terms of the GNU GPL, version 3.  See
    * the COPYING file in the top-level directory.
*/

//////////////////////////////////////////////////////////////////////////
//  Include this after "#define EXT_CLASS WDbgArk" only
//////////////////////////////////////////////////////////////////////////

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef PROCESSHLP_HPP_
#define PROCESSHLP_HPP_

#include <engextcpp.hpp>

#include <string>
#include <memory>

#include "manipulators.hpp"
#include "strings.hpp"
#include "symcache.hpp"
#include "dummypdb.hpp"
#include "processimplicithlp.hpp"
#include "objhelper.hpp"
#include "systemver.hpp"

namespace wa {

class WDbgArkRemoteTypedProcess : public WDbgArkImplicitProcess, public ExtRemoteTyped {
 public:
    explicit WDbgArkRemoteTypedProcess(const std::shared_ptr<WDbgArkSymCache> &sym_cache,
                                       const std::shared_ptr<WDbgArkDummyPdb> &dummy_pdb) : WDbgArkImplicitProcess(),
                                                                                            ExtRemoteTyped(),
                                                                                            m_sym_cache(sym_cache),
                                                                                            m_dummy_pdb(dummy_pdb) {}

    explicit WDbgArkRemoteTypedProcess(const std::shared_ptr<WDbgArkSymCache> &sym_cache,
                                       const std::shared_ptr<WDbgArkDummyPdb> &dummy_pdb,
                                       PCSTR Expr) : WDbgArkImplicitProcess(),
                                                     ExtRemoteTyped(Expr),
                                                     m_sym_cache(sym_cache),
                                                     m_dummy_pdb(dummy_pdb) { Init(); }

    explicit WDbgArkRemoteTypedProcess(const std::shared_ptr<WDbgArkSymCache> &sym_cache,
                                       const std::shared_ptr<WDbgArkDummyPdb> &dummy_pdb,
                                       const DEBUG_TYPED_DATA* Typed) : WDbgArkImplicitProcess(),
                                                                        ExtRemoteTyped(Typed),
                                                                        m_sym_cache(sym_cache),
                                                                        m_dummy_pdb(dummy_pdb) { Init(); }

    explicit WDbgArkRemoteTypedProcess(const std::shared_ptr<WDbgArkSymCache> &sym_cache,
                                       const std::shared_ptr<WDbgArkDummyPdb> &dummy_pdb,
                                       const ExtRemoteTyped &Typed) : WDbgArkImplicitProcess(),
                                                                      ExtRemoteTyped(Typed),
                                                                      m_sym_cache(sym_cache),
                                                                      m_dummy_pdb(dummy_pdb) { Init(); }

    WDbgArkRemoteTypedProcess(const std::shared_ptr<WDbgArkSymCache> &sym_cache,
                              const std::shared_ptr<WDbgArkDummyPdb> &dummy_pdb,
                              PCSTR Expr,
                              ULONG64 Offset) : WDbgArkImplicitProcess(),
                                                ExtRemoteTyped(Expr, Offset),
                                                m_sym_cache(sym_cache),
                                                m_dummy_pdb(dummy_pdb) { Init(); }

    WDbgArkRemoteTypedProcess(const std::shared_ptr<WDbgArkSymCache> &sym_cache,
                              const std::shared_ptr<WDbgArkDummyPdb> &dummy_pdb,
                              PCSTR Type,
                              ULONG64 Offset,
                              bool PtrTo,
                              PULONG64 CacheCookie = nullptr,
                              PCSTR LinkField = nullptr) : WDbgArkImplicitProcess(),
                                                           ExtRemoteTyped(Type, Offset, PtrTo, CacheCookie, LinkField),
                                                           m_sym_cache(sym_cache),
                                                           m_dummy_pdb(dummy_pdb) { Init(); }

    virtual ~WDbgArkRemoteTypedProcess() = default;

    virtual WDbgArkRemoteTypedProcess& operator=(const DEBUG_TYPED_DATA* Typed) {
        Copy(Typed);
        Init();
        return *this;
    }

    virtual WDbgArkRemoteTypedProcess& operator=(const ExtRemoteTyped& Typed) {
        Copy(Typed);
        Init();
        return *this;
    }

    HRESULT SetImplicitProcess() {
        return WDbgArkImplicitProcess::SetImplicitProcess(*this);
    }

    uint64_t GetDataOffset() const { return m_Offset; }

    WDbgArkRemoteTypedProcess GetSystemProcess() const {
        const std::string proc("nt!_EPROCESS");

        return WDbgArkRemoteTypedProcess(m_sym_cache,
                                         m_dummy_pdb,
                                         proc.c_str(),
                                         GetDataOffset(),
                                         true,
                                         m_sym_cache->GetCookieCache(proc),
                                         "ActiveProcessLinks");
    }

    bool IsSystemProcess() const {
        return (GetDataOffset() == GetSystemProcess().GetDataOffset());
    }

    bool IsMinimalProcess() {
        if ( !HasField("Flags3") ) {
            return false;
        }

        return (Field("Flags3").GetUlong() & EPROCESS_FLAGS3_MINIMAL);
    }

    bool GetProcessImageFileName(std::string* image_name) {
        auto img_name = Field("ImageFileName");

        char buffer[100] = { 0 };
        image_name->assign(img_name.GetString(buffer, static_cast<ULONG>(sizeof(buffer)), img_name.GetTypeSize()));

        return true;
    }

    bool GetSeAuditProcessCreationInfoImagePath(std::wstring* image_path) {
        auto image_file_name = Field("SeAuditProcessCreationInfo").Field("ImageFileName");

        if ( !image_file_name.GetPtr() ) {
            return false;
        }

        const auto [result, path] = UnicodeStringStructToString(image_file_name.Field("Name"));

        if ( SUCCEEDED(result) ) {
            *image_path = path;
            return true;
        }

        return false;
    }

    bool GetProcessFileObjectOld(ExtRemoteTyped* file_object) {
        auto section = Field("SectionObject");
        auto offset = section.GetPtr();

        if ( !offset ) {
            return false;
        }

        auto segment = ExtRemoteTyped(m_section_object_str.c_str(),
                                      offset,
                                      false,
                                      m_sym_cache->GetCookieCache(m_section_object_str),
                                      nullptr).Field("Segment");

        offset = segment.GetPtr();

        if ( !offset ) {
            return false;
        }

        const std::string segm("nt!_SEGMENT");
        auto ca = ExtRemoteTyped(segm.c_str(),
                                 offset,
                                 false,
                                 m_sym_cache->GetCookieCache(segm),
                                 nullptr).Field("ControlArea");

        if ( !ca.GetPtr() ) {
            return false;
        }

        return GetFileObjectByFilePointer(ca.Field("FilePointer"), file_object);
    }

    bool GetProcessFileObjectNew(ExtRemoteTyped* file_object) {
        auto offset = Field("SectionObject").GetPtr();

        if ( !offset ) {
            return false;
        }

        auto section = ExtRemoteTyped(m_section_object_str.c_str(),
                                      offset,
                                      false,
                                      m_sym_cache->GetCookieCache(m_section_object_str),
                                      nullptr);

        ExtRemoteTyped ca;

        if ( !GetSectionControlAreaNew(section, &ca) ) {
            return false;
        }

        return GetFileObjectByFilePointer(ca.Field("FilePointer"), file_object);
    }

    bool GetProcessFileObject(ExtRemoteTyped* file_object) {
        auto result = m_new_sec_object ? GetProcessFileObjectNew(file_object) : GetProcessFileObjectOld(file_object);
        return result;
    }

    bool GetProcessImageFilePathByImageFilePointer(std::wstring* image_path) {
        if ( !HasField("ImageFilePointer") ) {
            return false;
        }

        auto img_fp = Field("ImageFilePointer");

        if ( !img_fp.GetPtr() ) {
            return false;
        }

        const auto [result, path] = UnicodeStringStructToString(img_fp.Field("FileName"));

        if ( FAILED(result) ) {
            return false;
        }

        *image_path = path;
        return true;
    }

    bool GetProcessImageFilePathByFileObject(std::wstring* image_path) {
        ExtRemoteTyped file_object;

        if ( !GetProcessFileObject(&file_object) ) {
            return false;
        }

        const auto [result, path] = UnicodeStringStructToString(file_object.Field("FileName"));

        if ( FAILED(result) ) {
            return false;
        }

        *image_path = path;
        return true;
    }

    bool GetProcessImageFilePathByFilePointerFileObject(std::wstring* image_path) {
        auto result = GetProcessImageFilePathByImageFilePointer(image_path);

        if ( !result ) {
            result = GetProcessImageFilePathByFileObject(image_path);
        }

        return result;
    }

    bool GetProcessImageFilePath(std::wstring* image_path) {
        auto result = GetSeAuditProcessCreationInfoImagePath(image_path);

        if ( !result ) {
            result = GetProcessImageFilePathByFilePointerFileObject(image_path);
        }

        return result;
    }

    bool IsWow64Process() {
        if ( g_Ext->IsCurMachine32() ) {
            return false;
        }

        return (Field(m_wow64_proc_field_name.c_str()).GetPtr() != 0ULL);
    }

    uint64_t GetWow64ProcessPeb32() {
        if ( m_new_wow64 ) {
            return Field(m_wow64_proc_field_name.c_str()).Field("Peb").GetPtr();
        } else {
            return Field(m_wow64_proc_field_name.c_str()).GetPtr();
        }
    }

    uint64_t GetWow64InfoPtr() {
        const auto wow64peb = GetWow64ProcessPeb32();

        if ( !wow64peb ) {
            return 0ULL;
        }

        return reinterpret_cast<uint64_t>(RtlOffsetToPointer(wow64peb, m_sym_cache->GetTypeSize("nt!_PEB32")));
    }

    uint64_t GetWow64InstrumentationCallback() {
        const auto offset = GetWow64InfoPtr();

        if ( !offset ) {
            return 0ULL;
        }

        // check that PEB32 is not paged out
        try {
            const std::string peb32("nt!_PEB32");
            ExtRemoteTyped(peb32.c_str(),
                           GetWow64ProcessPeb32(),
                           false,
                           m_sym_cache->GetCookieCache(peb32),
                           nullptr).Field("Ldr").GetUlong();
        } catch ( const ExtRemoteException& ) {
            return 0ULL;
        }

        const auto wow_info = m_dummy_pdb->GetShortName() + "!_WOW64_INFO";
        ExtRemoteTyped wow64info(wow_info.c_str(), offset, false, m_sym_cache->GetCookieCache(wow_info), nullptr);

        return static_cast<uint64_t>(wow64info.Field("InstrumentationCallback").GetUlong());
    }

    // 6000 - 9600 x64 only
    // 10240+ x86/x64 ()
    uint64_t GetInstrumentationCallback() {
        if ( g_Ext->IsCurMachine32() ) {
            // 10240+ x86 process only: _EPROCESS->InstrumentationCallback
            return Field("InstrumentationCallback").GetPtr();
        }

        if ( !IsWow64Process() ) {
            // native x64 process: _EPROCESS->_KPROCESS->InstrumentationCallback
            return Field("Pcb").Field("InstrumentationCallback").GetPtr();
        } else {
            // WOW64 process
            return GetWow64InstrumentationCallback();
        }
    }

    uint64_t GetProcessApiSetMap() {
        return Field("Peb").Field("ApiSetMap").GetPtr();
    }

 private:
    void Init() {
        InitWow64Info();
        InitSectionInfo();
    }

    void InitWow64Info() {
        if ( m_sym_cache->GetTypeSize("nt!_EWOW64PROCESS") != 0UL ) {
            m_new_wow64 = true;     // 10586+
        }

        if ( g_Ext->IsCurMachine64() ) {
            if ( HasField("WoW64Process") ) {
                m_wow64_proc_field_name = "WoW64Process";
            } else {
                m_wow64_proc_field_name = "Wow64Process";
            }
        }
    }

    void InitSectionInfo() {
        if ( m_sym_cache->GetTypeSize(m_section_object_str.c_str()) != 0UL ) {
            m_new_sec_object = true;
        } else {
            WDbgArkSystemVer system_ver;

            if ( system_ver.IsInited() ) {
                if ( system_ver.GetStrictVer() == WXP_VER ) {
                    m_section_object_str.assign(m_dummy_pdb->GetShortName() + "!_SECTION_WXP");
                } else if ( system_ver.IsBuildInRangeStrict(W2K3_VER, W7SP1_VER) ) {
                    m_section_object_str.assign(m_dummy_pdb->GetShortName() + "!_SECTION_W2K3");
                } else if ( system_ver.GetStrictVer() == W8RTM_VER ) {
                    m_section_object_str.assign(m_dummy_pdb->GetShortName() + "!_SECTION_W8");
                } else if ( system_ver.GetStrictVer() == W81RTM_VER ) {
                    m_section_object_str.assign(m_dummy_pdb->GetShortName() + "!_SECTION_W81");
                }
            }
        }
    }

    bool GetFileObjectByFilePointer(const ExtRemoteTyped &file_pointer, ExtRemoteTyped* file_object) {
        auto fp = const_cast<ExtRemoteTyped&>(file_pointer);

        uint64_t offset = 0;

        if ( fp.HasField("Object") ) {
            offset = ExFastRefGetObject(fp.Field("Object").GetPtr());
        } else {
            offset = fp.GetPtr();
        }

        if ( !offset ) {
            return false;
        }

        const std::string file_obj("nt!_FILE_OBJECT");
        file_object->Set(file_obj.c_str(), offset, false, m_sym_cache->GetCookieCache(file_obj), nullptr);

        return true;
    }

    bool GetSectionControlAreaNew(const ExtRemoteTyped &section_object, ExtRemoteTyped* control_area) {
        auto section = const_cast<ExtRemoteTyped&>(section_object);

        auto ca = section.Field("u1.ControlArea");
        const auto ca_ptr = ca.GetPtr();

        if ( !ca_ptr ) {
            return false;
        }

        const auto offset = ca_ptr & ~SECTION_FLAG_VALID_MASK;
        const auto flags = ca_ptr & SECTION_FLAG_VALID_MASK;

        const std::string cont_area("nt!_CONTROL_AREA");

        if ( flags != 0 ) {
            const std::string file_obj("nt!_FILE_OBJECT");
            auto file_object = ExtRemoteTyped(file_obj.c_str(),
                                              offset,
                                              false,
                                              m_sym_cache->GetCookieCache(file_obj),
                                              nullptr);

            auto sec_obj_ptr = file_object.Field("SectionObjectPointer");

            if ( sec_obj_ptr.GetPtr() != 0ULL ) {
                if ( (flags & SECTION_FLAG_REMOTE_IMAGE_FILE_OBJECT) &&
                     sec_obj_ptr.Field("ImageSectionObject").GetPtr() != 0ULL ) {
                    control_area->Set(cont_area.c_str(),
                                      sec_obj_ptr.Field("ImageSectionObject").GetPtr(),
                                      false,
                                      m_sym_cache->GetCookieCache(cont_area),
                                      nullptr);
                    return true;
                }

                if ( (flags & SECTION_FLAG_REMOTE_DATA_FILE_OBJECT) &&
                     sec_obj_ptr.Field("DataSectionObject").GetPtr() != 0ULL ) {
                    control_area->Set(cont_area.c_str(),
                                      sec_obj_ptr.Field("DataSectionObject").GetPtr(),
                                      false,
                                      m_sym_cache->GetCookieCache(cont_area),
                                      nullptr);
                    return true;
                }
            }
        } else {
            control_area->Set(cont_area.c_str(), offset, false, m_sym_cache->GetCookieCache(cont_area), nullptr);
            return true;
        }

        return false;
    }

 private:
    bool m_new_wow64 = false;
    bool m_new_sec_object = false;
    std::string m_section_object_str{ "nt!_SECTION" };
    std::string m_wow64_proc_field_name{};
    std::shared_ptr<WDbgArkSymCache> m_sym_cache{ nullptr };
    std::shared_ptr<WDbgArkDummyPdb> m_dummy_pdb{ nullptr };
};

}   // namespace wa

#endif  // PROCESSHLP_HPP_
