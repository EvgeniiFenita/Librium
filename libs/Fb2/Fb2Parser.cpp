#include "Fb2Parser.hpp"

#include <pugixml.hpp>

#include <array>
#include <cstring>

namespace {

pugi::xml_node FindChild(const pugi::xml_node& parent, const char* localName) 
{
    for (auto& child : parent.children()) 
{
        const char* name  = child.name();
        const char* colon = strchr(name, ':');
        const char* local = colon ? colon + 1 : name;
        if (strcmp(local, localName) == 0)
            return child;
    }
    return {};
}

std::string CollectText(const pugi::xml_node& node) 
{
    std::string result;
    for (auto& child : node) 
{
        if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) 
{
            result += child.value();
        }
        else
        {
            auto sub = CollectText(child);
            if (!sub.empty()) 
{
                if (!result.empty() && result.back() != ' ')
                    result += ' ';
                result += sub;
            }
        }
    }
    return result;
}

constexpr std::array<int8_t, 256> MakeB64Table() 
{
    std::array<int8_t, 256> t{};
    t.fill(-1);
    for (int i = 0; i < 26; ++i) 
{ t['A'+i] = i; t['a'+i] = 26+i; }
    for (int i = 0; i < 10; ++i)   t['0'+i] = 52+i;
    t['+'] = 62; t['/'] = 63; t['='] = 0;
    return t;
}

std::vector<uint8_t> DecodeBase64(const std::string& encoded) 
{
    static constexpr auto TABLE = MakeB64Table();
    std::vector<uint8_t> out;
    out.reserve(encoded.size() * 3 / 4);
    int buf = 0, bits = 0;
    for (unsigned char c : encoded) 
{
        if (c == '\n' || c == '\r' || c == ' ') continue;
        if (c == '=') break;
        int8_t val = TABLE[c];
        if (val < 0) continue;
        buf = (buf << 6) | val;
        bits += 6;
        if (bits >= 8) 
{
            bits -= 8;
            out.push_back(static_cast<uint8_t>(buf >> bits));
            buf &= (1 << bits) - 1;
        }
    }
    return out;
}

} // namespace

namespace Librium::Fb2 {

CFb2Data CFb2Parser::Parse(const std::string& xmlText) 
{
    std::vector<uint8_t> data(xmlText.begin(), xmlText.end());
    return Parse(data);
}

CFb2Data CFb2Parser::Parse(const std::vector<uint8_t>& data) 
{
    CFb2Data result;

    if (data.empty()) 
{
        result.parseError = "Empty input";
        return result;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result pr = doc.load_buffer(
        data.data(), data.size(),
        pugi::parse_default | pugi::parse_ws_pcdata_single,
        pugi::encoding_auto);

    if (!pr) 
{
        result.parseError = std::string("XML error: ") + pr.description();
        return result;
    }

    auto root = doc.first_child();
    if (!root) 
{
        result.parseError = "No root element";
        return result;
    }

    auto description  = FindChild(root, "description");
    auto titleInfo    = FindChild(description, "title-info");
    auto publishInfo  = FindChild(description, "publish-info");

    // Annotation
    auto ann = FindChild(titleInfo, "annotation");
    if (ann) 
{
        result.annotation = CollectText(ann);
        // Trim leading/trailing whitespace
        auto& a = result.annotation;
        while (!a.empty() && (a.front() == ' ' || a.front() == '\n')) a.erase(a.begin());
        while (!a.empty() && (a.back()  == ' ' || a.back()  == '\n')) a.pop_back();
    }

    // Keywords
    auto kw = FindChild(titleInfo, "keywords");
    if (kw) result.keywords = kw.text().get();

    // Publish info
    if (publishInfo) 
{
        auto pub = FindChild(publishInfo, "publisher");
        if (pub) result.publisher = pub.text().get();
        auto isbn = FindChild(publishInfo, "isbn");
        if (isbn) result.isbn = isbn.text().get();
        auto year = FindChild(publishInfo, "year");
        if (year) result.publishYear = year.text().get();
    }

    // Cover
    auto coverpage = FindChild(titleInfo, "coverpage");
    if (coverpage) 
{
        auto img = FindChild(coverpage, "image");
        if (img) 
{
            std::string href;
            for (auto& attr : img.attributes()) 
{
                std::string aname = attr.name();
                if (aname == "href" || aname.find("href") != std::string::npos) 
{ href = attr.value(); break; }
            }
            if (!href.empty() && href[0] == '#') 
{
                std::string bid = href.substr(1);
                for (auto& node : root.children()) 
{
                    std::string nname = node.name();
                    if (nname == "binary" || (nname.size() > 6 &&
                        nname.substr(nname.size()-6) == "binary")) 
{
                        auto idAttr = node.attribute("id");
                        if (idAttr && bid == idAttr.value()) 
{
                            result.coverMime = node.attribute("content-type").value();
                            result.coverData = DecodeBase64(node.text().get());
                            break;
                        }
                    }
                }
            }
        }
    }

    return result;
}

} // namespace Librium::Fb2






