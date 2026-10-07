// Generated install projection; no asset definitions or native selection state.
// GPL-3.0-or-later.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
namespace wxl::story::models
{
    constexpr size_t kRecordSize = 456, kHeaderSize = 12, kMaximum = 50;
    constexpr char kMagic[8] = {'W','X','L','S','E','L','1',0};
    struct Entry
    {
        uint32_t realm = 0;
        uint64_t guid = 0;
        char name[48]{}, realmName[128]{}, model[260]{};
        std::array<uint8_t,8> appearance{}; // race,class,sex,skin,face,style,colour,facial
    };
    inline bool Text(const char* text, size_t capacity)
    { return text[0] && std::memchr(text,0,capacity); }
    inline bool ModelPath(const char* text)
    {
        if (!Text(text,260) || std::strncmp(text,"Character\\",10)) return false;
        const size_t size = std::strlen(text);
        if (size < 13 || std::strcmp(text+size-3,".m2")) return false;
        const char* part = text;
        for (const char* p=text; ; ++p)
        {
            if (*p=='/' || *p==':') return false;
            if (*p=='\\' || !*p)
            {
                const size_t n = p-part;
                if (!n || (n==1 && part[0]=='.') || (n==2 && part[0]=='.' && part[1]=='.')) return false;
                if (!*p) break;
                part=p+1;
            }
        }
        return true;
    }
    inline bool Parse(const uint8_t* data, size_t size, std::vector<Entry>& result)
    {
        result.clear();
        if (!data || size<kHeaderSize || std::memcmp(data,kMagic,8)) return false;
        uint32_t count; std::memcpy(&count,data+8,4);
        if (count>kMaximum || size!=kHeaderSize+count*kRecordSize) return false;
        std::vector<Entry> parsed;
        for (uint32_t i=0; i<count; ++i)
        {
            const auto p=data+kHeaderSize+i*kRecordSize;
            Entry row;
            std::memcpy(&row.realm,p,4); std::memcpy(&row.guid,p+4,8);
            std::memcpy(row.name,p+12,48); std::memcpy(row.realmName,p+60,128);
            std::memcpy(row.model,p+188,260); std::memcpy(row.appearance.data(),p+448,8);
            if (!row.realm || !row.guid || row.guid>UINT32_MAX || !Text(row.name,48) ||
                !Text(row.realmName,128) || !ModelPath(row.model) || !row.appearance[0] ||
                !row.appearance[1] || row.appearance[2]>1) return false;
            for (const auto& old : parsed)
                if (old.guid==row.guid || old.realm!=row.realm || std::strcmp(old.realmName,row.realmName)) return false;
            parsed.push_back(row);
        }
        result.swap(parsed);
        return true;
    }
    inline const Entry* Find(const std::vector<Entry>& map, const char* realm, uint64_t guid,
                             const char* name, const uint8_t* appearance)
    {
        if (!realm || !name || !appearance) return nullptr;
        for (const auto& row : map)
            if (row.guid==guid && !std::strcmp(row.realmName,realm) && !std::strcmp(row.name,name) &&
                !std::memcmp(row.appearance.data(),appearance,8)) return &row;
        return nullptr;
    }
    // Keep scene, flags and stock failure semantics with the native instance factory.
    template<class Factory>
    auto Create(const Entry* entry, const char* stock, uint32_t flags, Factory factory)
    {
        if (entry)
            if (auto instance=factory(entry->model,flags)) return instance;
        return factory(stock,flags);
    }
}
