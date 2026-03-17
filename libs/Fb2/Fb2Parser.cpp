#include "Fb2Parser.hpp"
#include "Log/Logger.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <sstream>

namespace Librium::Fb2 {

namespace {

std::string GetNodeText(const pugi::xml_node& node)
{
    return node.child_value();
}

std::string GetChildText(const pugi::xml_node& parent, const char* name)
{
    return parent.child(name).text().get();
}

} // namespace

SFb2Data CFb2Parser::Parse(const std::string& xmlText)
{
    SFb2Data res;
    pugi::xml_document doc;
    auto result = doc.load_string(xmlText.c_str());
    if (!result)
    {
        LOG_DEBUG("FB2 XML parse error: {}", result.description());
        res.parseError = result.description();
        return res;
    }

    auto root = doc.child("FictionBook");
    auto desc = root.child("description");
    auto titleInfo = desc.child("title-info");

    res.annotation = "";
    for (auto p : titleInfo.child("annotation").children("p"))
    {
        if (!res.annotation.empty()) res.annotation += "\n";
        res.annotation += p.text().get();
    }

    res.keywords = titleInfo.child("keywords").text().get();

    auto pubInfo = desc.child("publish-info");
    res.publisher = pubInfo.child("publisher").text().get();
    res.isbn = pubInfo.child("isbn").text().get();
    res.publishYear = pubInfo.child("year").text().get();

    // Cover
    auto coverPage = titleInfo.child("coverpage");
    auto image = coverPage.child("image");
    std::string href = image.attribute("l:href").value();
    if (href.empty()) href = image.attribute("xlink:href").value();
    if (!href.empty())
    {
        if (href[0] == '#') href.erase(0, 1);
        for (auto binary : root.children("binary"))
        {
            if (std::string(binary.attribute("id").value()) == href)
            {
                res.coverMime = binary.attribute("content-type").value();
                std::string base64 = binary.text().get();
                // We'd need a base64 decoder here to actually fill coverData
                // For now, let's just keep the metadata
                break;
            }
        }
    }

    return res;
}

SFb2Data CFb2Parser::Parse(const std::vector<uint8_t>& data)
{
    if (data.empty()) return {};
    std::string xml(reinterpret_cast<const char*>(data.data()), data.size());
    return Parse(xml);
}

} // namespace Librium::Fb2
