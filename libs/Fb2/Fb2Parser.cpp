#include "Fb2Parser.hpp"
#include "Log/Logger.hpp"
#include "Utils/Base64.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <sstream>
#include <atomic>
#include <cstring>

#include "Utils/StringUtils.hpp"

#include <functional>

namespace Librium::Fb2 {

using Utils::CStringUtils;

namespace {

#ifndef LIBRIUM_FB2_DEBUG_DUMP
static constexpr int kDebugDumpMax = 0;
#else
static constexpr int kDebugDumpMax = 5;
#endif

std::atomic<int> g_debugDumpCount{0};

pugi::xml_node FindChildIgnoreCase(const pugi::xml_node& parent, const char* name)
{
    if (!parent) return {};
    for (auto child : parent.children())
    {
        const char* nodeName = child.name();
        const char* lastColon = strrchr(nodeName, ':');
        const char* baseName = lastColon ? lastColon + 1 : nodeName;
        
#ifdef _WIN32
        if (_stricmp(baseName, name) == 0)
#else
        if (strcasecmp(baseName, name) == 0)
#endif
            return child;
    }
    return {};
}

std::string GetChildText(const pugi::xml_node& parent, const char* name)
{
    auto child = FindChildIgnoreCase(parent, name);
    return child ? child.text().get() : "";
}

} // namespace

SFb2Data CFb2Parser::Parse(const std::string& xmlText, const std::string& context)
{
    if (xmlText.empty()) return {};

    std::string utf8Xml;

    // 1. Check if it's already valid UTF-8.
    // If it's valid UTF-8, we use it AS IS, regardless of what the XML header says.
    // This is the most robust way to handle "liar" headers in Russian libraries.
    if (CStringUtils::IsUtf8(xmlText))
    {
        // Still check for BOM and strip it if present
        if (xmlText.size() >= 3 && (unsigned char)xmlText[0] == 0xEF && (unsigned char)xmlText[1] == 0xBB && (unsigned char)xmlText[2] == 0xBF)
            utf8Xml = xmlText.substr(3);
        else
            utf8Xml = xmlText;
    }
    else
    {
        // 2. Not valid UTF-8. It's likely Windows-1251 (legacy Russian).
        utf8Xml = CStringUtils::Cp1251ToUtf8(xmlText);

        // Patch XML declaration if it points to legacy encoding
        size_t pos = utf8Xml.find("windows-1251");
        if (pos != std::string::npos) utf8Xml.replace(pos, 12, "utf-8");
        pos = utf8Xml.find("cp1251");
        if (pos != std::string::npos) utf8Xml.replace(pos, 6, "utf-8");
    }

    if (kDebugDumpMax > 0 && g_debugDumpCount.fetch_add(1) < kDebugDumpMax)
    {
        std::string preview = utf8Xml.substr(0, 1000);
        LOG_DEBUG("FB2 XML Raw Preview (first 1000 chars):\n{}", preview);
    }

    SFb2Data res;
    pugi::xml_document doc;

    auto result = doc.load_string(utf8Xml.c_str(), pugi::parse_default | pugi::parse_declaration);
    if (!result)
    {
        LOG_DEBUG("FB2 XML parse error in [{}]: {} (offset {})", context.empty() ? "unknown" : context, result.description(), result.offset);
        res.parseError = result.description();
        return res;
    }
    auto root = FindChildIgnoreCase(doc, "FictionBook");
    if (!root) root = doc.first_child(); 

    auto desc = FindChildIgnoreCase(root, "description");
    auto titleInfo = FindChildIgnoreCase(desc, "title-info");

    res.annotation = "";
    auto annNode = FindChildIgnoreCase(titleInfo, "annotation");
    if (!annNode) annNode = FindChildIgnoreCase(desc, "annotation");

    if (annNode)
    {
        std::stringstream ss;
        // Recursive lambda to gather all text nodes
        std::function<void(pugi::xml_node)> gatherText = [&](pugi::xml_node parent) {
            for (auto child : parent.children())
            {
                if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
                {
                    ss << child.value();
                }
                else if (child.type() == pugi::node_element)
                {
                    std::string name = child.name();
                    if (name == "p" || name.find("empty-line") != std::string::npos)
                        ss << "\n";
                    gatherText(child);
                }
            }
        };
        gatherText(annNode);
        res.annotation = ss.str();
        
        // Trim leading newlines
        size_t first = res.annotation.find_first_not_of("\n\r ");
        if (first != std::string::npos) res.annotation = res.annotation.substr(first);
    }

    res.keywords = GetChildText(titleInfo, "keywords");

    auto pubInfo = FindChildIgnoreCase(desc, "publish-info");
    res.publisher = GetChildText(pubInfo, "publisher");
    res.isbn      = GetChildText(pubInfo, "isbn");
    res.publishYear = GetChildText(pubInfo, "year");

    auto coverpage = FindChildIgnoreCase(titleInfo, "coverpage");
    auto image = FindChildIgnoreCase(coverpage, "image");
    std::string href;
    for (auto attr : image.attributes())
    {
        std::string name = attr.name();
        if (name.find("href") != std::string::npos)
        {
            href = attr.value();
            break;
        }
    }
    if (!href.empty() && href[0] == '#') href = href.substr(1);

    if (!href.empty())
    {
        for (auto binary : root.children())
        {
            std::string name = binary.name();
            if (name == "binary")
            {
                std::string idVal;
                std::string ctype;
                for (auto attr : binary.attributes())
                {
                    std::string aname = attr.name();
                    if (aname == "id") idVal = attr.value();
                    else if (aname == "content-type") ctype = attr.value();
                }
                
                if (idVal == href)
                {
                    std::string b64 = binary.text().get();
                    b64.erase(std::remove_if(b64.begin(), b64.end(), [](unsigned char c) { return std::isspace(c); }), b64.end());
                    
                    if (!b64.empty())
                    {
                        std::string decoded = Utils::CBase64::Decode(b64);
                        res.coverData = std::vector<uint8_t>(decoded.begin(), decoded.end());
                        
                        if (ctype == "image/png") res.coverExt = ".png";
                        else if (ctype == "image/jpeg" || ctype == "image/jpg") res.coverExt = ".jpg";
                        else if (ctype == "image/webp") res.coverExt = ".webp";
                        else res.coverExt = ".bin";
                    }
                    break;
                }
            }
        }
    }

    if (kDebugDumpMax > 0 && g_debugDumpCount.load() < kDebugDumpMax)
    {
        LOG_DEBUG("FB2 Parse Result: annotation_len={}, keywords='{}', publisher='{}'", 
            res.annotation.length(), res.keywords, res.publisher);
    }

    return res;
}

SFb2Data CFb2Parser::Parse(const std::vector<uint8_t>& data, const std::string& context)
{
    if (data.empty()) return {};
    std::string xml(reinterpret_cast<const char*>(data.data()), data.size());
    return Parse(xml, context);
}

} // namespace Librium::Fb2
