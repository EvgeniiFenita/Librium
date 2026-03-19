#include "Fb2Parser.hpp"
#include "Log/Logger.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <sstream>

namespace Librium::Fb2 {

namespace {

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

    res.keywords = GetChildText(titleInfo, "keywords");

    auto pubInfo = desc.child("publish-info");
    res.publisher = GetChildText(pubInfo, "publisher");
    res.isbn      = GetChildText(pubInfo, "isbn");
    res.publishYear = GetChildText(pubInfo, "year");

    return res;
}

SFb2Data CFb2Parser::Parse(const std::vector<uint8_t>& data)
{
    if (data.empty()) return {};
    std::string xml(reinterpret_cast<const char*>(data.data()), data.size());
    return Parse(xml);
}

} // namespace Librium::Fb2
