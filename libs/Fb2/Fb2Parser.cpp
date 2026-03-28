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

std::string NormalizeXmlEncoding(const std::string& xmlText)
{
    if (CStringUtils::IsUtf8(xmlText))
    {
        if (xmlText.size() >= 3 &&
            static_cast<unsigned char>(xmlText[0]) == 0xEF &&
            static_cast<unsigned char>(xmlText[1]) == 0xBB &&
            static_cast<unsigned char>(xmlText[2]) == 0xBF)
        {
            return xmlText.substr(3);
        }

        return xmlText;
    }

    std::string utf8Xml = CStringUtils::Cp1251ToUtf8(xmlText);

    size_t pos = utf8Xml.find("windows-1251");
    if (pos != std::string::npos)
    {
        utf8Xml.replace(pos, 12, "utf-8");
    }

    pos = utf8Xml.find("cp1251");
    if (pos != std::string::npos)
    {
        utf8Xml.replace(pos, 6, "utf-8");
    }

    return utf8Xml;
}

std::string CollectAnnotationText(const pugi::xml_node& annotationNode)
{
    if (!annotationNode)
    {
        return {};
    }

    std::stringstream stream;
    std::function<void(pugi::xml_node)> gatherText = [&](pugi::xml_node parent)
    {
        for (auto child : parent.children())
        {
            if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
            {
                stream << child.value();
            }
            else if (child.type() == pugi::node_element)
            {
                const std::string name = child.name();
                if (name == "p" || name.find("empty-line") != std::string::npos)
                {
                    stream << "\n";
                }

                gatherText(child);
            }
        }
    };

    gatherText(annotationNode);

    std::string annotation = stream.str();
    const size_t firstNonSpace = annotation.find_first_not_of("\n\r ");
    if (firstNonSpace != std::string::npos)
    {
        annotation = annotation.substr(firstNonSpace);
    }

    return annotation;
}

std::string FindCoverHref(const pugi::xml_node& titleInfo)
{
    const auto coverpage = FindChildIgnoreCase(titleInfo, "coverpage");
    const auto image = FindChildIgnoreCase(coverpage, "image");

    for (auto attr : image.attributes())
    {
        const std::string name = attr.name();
        if (name.find("href") != std::string::npos)
        {
            std::string href = attr.value();
            if (!href.empty() && href[0] == '#')
            {
                href = href.substr(1);
            }

            return href;
        }
    }

    return {};
}

std::string CoverExtensionFromContentType(const std::string& contentType)
{
    if (contentType == "image/png") return ".png";
    if (contentType == "image/jpeg" || contentType == "image/jpg") return ".jpg";
    if (contentType == "image/webp") return ".webp";
    return ".bin";
}

void TryExtractCover(const pugi::xml_node& root, const std::string& href, SFb2Data& result)
{
    if (href.empty())
    {
        return;
    }

    for (auto binary : root.children())
    {
        const std::string nodeName = binary.name();
        if (nodeName != "binary")
        {
            continue;
        }

        std::string id;
        std::string contentType;
        for (auto attr : binary.attributes())
        {
            const std::string attrName = attr.name();
            if (attrName == "id")
            {
                id = attr.value();
            }
            else if (attrName == "content-type")
            {
                contentType = attr.value();
            }
        }

        if (id != href)
        {
            continue;
        }

        std::string base64 = binary.text().get();
        base64.erase(std::remove_if(base64.begin(), base64.end(), [](unsigned char c) { return std::isspace(c); }), base64.end());
        if (base64.empty())
        {
            break;
        }

        const std::string decoded = Utils::CBase64::Decode(base64);
        result.coverData = std::vector<uint8_t>(decoded.begin(), decoded.end());
        result.coverExt = CoverExtensionFromContentType(contentType);
        break;
    }
}

} // namespace

SFb2Data CFb2Parser::Parse(const std::string& xmlText, const std::string& context)
{
    if (xmlText.empty()) return {};

    const std::string utf8Xml = NormalizeXmlEncoding(xmlText);

    if ((kDebugDumpMax > 0) && (g_debugDumpCount.fetch_add(1) < kDebugDumpMax))
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

    auto annNode = FindChildIgnoreCase(titleInfo, "annotation");
    if (!annNode) annNode = FindChildIgnoreCase(desc, "annotation");
    res.annotation = CollectAnnotationText(annNode);

    res.keywords = GetChildText(titleInfo, "keywords");

    auto pubInfo = FindChildIgnoreCase(desc, "publish-info");
    res.publisher = GetChildText(pubInfo, "publisher");
    res.isbn      = GetChildText(pubInfo, "isbn");
    res.publishYear = GetChildText(pubInfo, "year");

    TryExtractCover(root, FindCoverHref(titleInfo), res);

    if ((kDebugDumpMax > 0) && (g_debugDumpCount.load() < kDebugDumpMax))
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
