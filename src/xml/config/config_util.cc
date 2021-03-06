/* <xml/config/config_util.cc>

   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   ----------------------------------------------------------------------------

   Implements <xml/config/config_util.h>.
 */

#include <xml/config/config_util.h>

#include <cctype>
#include <cstring>
#include <limits>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <strings.h>
#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XercesDefs.hpp>

#include <xml/dom_document_util.h>
#include <xml/dom_parser_with_line_info.h>

using namespace xercesc;

using namespace Base;
using namespace Xml;
using namespace Xml::Config;

DOMDocument *Xml::Config::ParseXmlConfig(const void *buf, size_t buf_size,
    const char *expected_encoding) {
  assert(buf);
  assert(expected_encoding);

  try {
    MemBufInputSource input_source(
        reinterpret_cast<const XMLByte *>(buf), buf_size,
        /* Note: The contents of this blurb apparently don't matter, so I'm
           just making up some reasonable looking text.  It looks like it's
           meaningful when using a DTD (see
           https://en.wikipedia.org/wiki/XML_Catalog ). */
        "XML config file");
    TDomParserWithLineInfo parser;
    parser.parse(input_source);
    auto doc = MakeDomDocumentUniquePtr(parser.adoptDocument());
    const XMLCh *doc_encoding = doc->getXmlEncoding();

    if (!doc_encoding || TranscodeToString(doc_encoding).empty()) {
      throw TMissingEncoding();
    }

    auto trans_doc_encoding = GetTranscoded(doc_encoding);

    if (XMLString::compareIString(trans_doc_encoding.get(),
        expected_encoding)) {
      throw TWrongEncoding(trans_doc_encoding.get(), expected_encoding);
    }

    return doc.release();
  } catch (const XMLException &x) {
    throw TXmlException(x);
  } catch (const SAXParseException &x) {
    throw TSaxParseException(x);
  } catch (const DOMException &x) {
    throw TDomException(x);
  }
}

bool Xml::Config::IsAllWhitespace(const DOMText &node) {
  assert(&node);
  auto transcoded = GetTranscoded(node.getData());
  const char *data = transcoded.get();

  for (size_t i = 0; data[i]; ++i) {
    if (!std::isspace(data[i])) {
      return false;
    }
  }

  return true;
}

std::unordered_map<std::string, const DOMElement *>
Xml::Config::GetSubsectionElements(const DOMElement &parent,
    const std::vector<std::pair<std::string, bool>> &subsection_vec,
    bool allow_unknown_subsection) {
  assert(&parent);
  std::unordered_map<std::string, const DOMElement *> result;
  std::unordered_map<std::string, bool> subsection_map;

  for (const auto &item : subsection_vec) {
    subsection_map.insert(item);
  }

  for (const DOMNode *child = parent.getFirstChild();
      child;
      child = child->getNextSibling()) {
    switch (child->getNodeType()) {
      case DOMNode::ELEMENT_NODE: {
        const DOMElement *elem = static_cast<const DOMElement *>(child);
        std::string name(TranscodeToString(elem->getTagName()));

        if (subsection_map.count(name)) {
          auto ret = result.insert(std::make_pair(name, elem));

          if (!ret.second) {
            throw TDuplicateElement(*elem);
          }
        } else if (!allow_unknown_subsection) {
          throw TUnknownElement(*elem);
        }

        break;
      }
      case DOMNode::TEXT_NODE:
      case DOMNode::CDATA_SECTION_NODE: {
        const DOMText *text = static_cast<const DOMText *>(child);

        if (!IsAllWhitespace(*text)) {
          throw TUnexpectedText(*text);
        }

        break;
      }
      default: {
        break;  // ignore other node types
      }
    }
  }

  for (const auto &item : subsection_vec) {
    if (item.second && !result.count(item.first)) {
      throw TMissingChildElement(parent, item.first.c_str());
    }
  }

  return std::move(result);
}

std::vector<const DOMElement *>
Xml::Config::GetItemListElements(const DOMElement &parent,
    const char *item_name) {
  assert(&parent);
  std::vector<const DOMElement *> result;

  for (const DOMNode *child = parent.getFirstChild();
      child;
      child = child->getNextSibling()) {
    switch (child->getNodeType()) {
      case DOMNode::ELEMENT_NODE: {
        const DOMElement *elem = static_cast<const DOMElement *>(child);
        std::string name(TranscodeToString(elem->getTagName()));

        if (name != item_name) {
          throw TUnexpectedElementName(*elem, item_name);
        }

        result.push_back(elem);
        break;
      }
      case DOMNode::TEXT_NODE:
      case DOMNode::CDATA_SECTION_NODE: {
        const DOMText *text = static_cast<const DOMText *>(child);

        if (!IsAllWhitespace(*text)) {
          throw TUnexpectedText(*text);
        }

        break;
      }
      default: {
        break;  // ignore other node types
      }
    }
  }

  return std::move(result);
}

void Xml::Config::RequireNoChildElement(const DOMElement &elem) {
  assert(&elem);

  for (const DOMNode *child = elem.getFirstChild();
      child;
      child = child->getNextSibling()) {
    if (child->getNodeType() == DOMNode::ELEMENT_NODE) {
      throw TUnknownElement(*static_cast<const DOMElement *>(child));
    }
  }
}

void Xml::Config::RequireNoGrandchildElement(const DOMElement &elem) {
  assert(&elem);

  for (const DOMNode *child = elem.getFirstChild();
      child;
      child = child->getNextSibling()) {
    if (child->getNodeType() == DOMNode::ELEMENT_NODE) {
      RequireNoChildElement(*static_cast<const DOMElement *>(child));
    }
  }
}

void Xml::Config::RequireLeaf(const DOMElement &elem) {
  assert(&elem);

  if (elem.getFirstChild()) {
    throw TExpectedLeaf(elem);
  }
}

void Xml::Config::RequireAllChildElementLeaves(const DOMElement &elem) {
  assert(&elem);

  for (const DOMNode *child = elem.getFirstChild();
      child;
      child = child->getNextSibling()) {
    if (child->getNodeType() == DOMNode::ELEMENT_NODE) {
      RequireLeaf(*static_cast<const DOMElement *>(child));
    }
  }
}

TOpt<std::string> TAttrReader::GetOptString(const DOMElement &elem,
    const char *attr_name, unsigned int opts) {
  assert(&elem);
  assert(attr_name);
  assert(opts == (opts & TRIM_WHITESPACE));
  TOpt<std::string> result;
  const DOMAttr *attr = elem.getAttributeNode(GetTranscoded(attr_name).get());

  if (attr) {
    std::string value(TranscodeToString(attr->getValue()));

    if (opts & TRIM_WHITESPACE) {
      boost::algorithm::trim(value);
    }

    result.MakeKnown(std::move(value));
  }

  return std::move(result);
}

std::string TAttrReader::GetString(const DOMElement &elem,
    const char *attr_name, unsigned int opts) {
  assert(&elem);
  assert(attr_name);
  assert(opts == (opts & (THROW_IF_EMPTY | TRIM_WHITESPACE)));
  const DOMAttr *attr = elem.getAttributeNode(GetTranscoded(attr_name).get());

  if (!attr) {
    throw TMissingAttrValue(elem, attr_name);
  }

  std::string result(TranscodeToString(attr->getValue()));

  if (opts & TRIM_WHITESPACE) {
    boost::algorithm::trim(result);
  }

  if ((opts & THROW_IF_EMPTY) && result.empty()) {
    throw TMissingAttrValue(elem, attr_name);
  }

  return std::move(result);
}

static bool StringToBool(const std::string &s, const char *true_value,
    const char *false_value, bool case_sensitive, const DOMElement &elem,
    const char *attr_name) {
  int (*compare_fn)(const char *, const char *) =
      case_sensitive ? std::strcmp : strcasecmp;
  bool is_true = (compare_fn(s.c_str(), true_value) == 0);
  bool is_false = (compare_fn(s.c_str(), false_value) == 0);
  assert(!(is_true && is_false));

  if (!is_true && !is_false) {
    throw TInvalidBoolAttr(elem, attr_name, s.c_str(), true_value,
        false_value);
  }

  return is_true;
}

TOpt<bool> TAttrReader::GetOptNamedBool(const DOMElement &elem,
    const char *attr_name, const char *true_value, const char *false_value,
    unsigned int opts) {
  assert(&elem);
  assert(attr_name);
  assert(true_value);
  assert(false_value);
  assert(opts == (opts & (REQUIRE_PRESENCE | CASE_SENSITIVE)));
  TOpt<bool> result;
  TOpt<std::string> opt_s = GetOptString(elem, attr_name, TRIM_WHITESPACE);

  if (opt_s.IsUnknown()) {
    if (opts & REQUIRE_PRESENCE) {
      throw TMissingAttrValue(elem, attr_name);
    }
  } else if (!(*opt_s).empty()) {
    result.MakeKnown(StringToBool(*opt_s, true_value, false_value,
        (opts & CASE_SENSITIVE) != 0, elem, attr_name));
  }

  return result;
}

bool TAttrReader::GetNamedBool(const xercesc::DOMElement &elem,
    const char *attr_name, const char *true_value, const char *false_value,
    unsigned int opts) {
  assert(&elem);
  assert(attr_name);
  assert(true_value);
  assert(false_value);
  assert(opts == (opts & CASE_SENSITIVE));
  std::string s = GetString(elem, attr_name, TRIM_WHITESPACE | THROW_IF_EMPTY);
  return StringToBool(s, true_value, false_value, (opts & CASE_SENSITIVE) != 0,
      elem, attr_name);
}

/* Note: On entry, leading and trailing whitespace has been trimmed from
   'value'. */
static uint32_t ExtractMultiplier(std::string &value, unsigned int opts) {
  assert(!value.empty());
  uint32_t mult = 1;
  using TOpts = TAttrReader::TOpts;

  /* In the case where 'value' is the letter 'k' or 'm' by itself, don't do
     anything.  Then our caller will handle the invalid input.  Here we depend
     on leading and trailing whitespace being trimmed from 'value'. */
  if (value.size() > 1) {
    switch (value.back()) {
      case 'k':
      case 'K': {
        if (opts & TOpts::ALLOW_K) {
          mult = 1024;
        }
        break;
      }
      case 'm':
      case 'M': {
        if (opts & TOpts::ALLOW_M) {
          mult = 1024 * 1024;
        }
        break;
      }
      default: {
        break;
      }
    }

    if (mult != 1) {
      /* Eliminate trailing 'k' or 'm', and any resulting trailing whitespace.
       */
      value.pop_back();
      boost::algorithm::trim_right(value);
    }
  }

  return mult;
}

template <typename T>
static T AttrToInt(const std::string &attr, const DOMElement &elem,
    const char *attr_name, unsigned int opts) {
  std::string attr_copy(attr);
  T mult = ExtractMultiplier(attr_copy, opts);
  T value = 0;

  try {
    value = boost::lexical_cast<T>(attr_copy);
  } catch (const boost::bad_lexical_cast &) {
    if (std::is_signed<T>::value) {
      throw TInvalidSignedIntegerAttr(elem, attr_name, attr.c_str());
    } else {
      throw TInvalidUnsignedIntegerAttr(elem, attr_name, attr.c_str());
    }
  }

  if ((value < (std::numeric_limits<T>::min() / mult)) ||
      (value > (std::numeric_limits<T>::max() / mult))) {
    throw TAttrOutOfRange(elem, attr_name, attr.c_str());
  }

  return value * mult;
}

template <typename T>
static TOpt<T> GetOpt64BitIntAttr(const DOMElement &elem,
    const char *attr_name, const char *empty_value_name, unsigned int opts) {
  assert(&elem);
  assert(attr_name);
  using TOpts = TAttrReader::TOpts;
  assert(opts == (opts & (TOpts::REQUIRE_PRESENCE | TOpts::STRICT_EMPTY_VALUE |
      TOpts::ALLOW_K | TOpts::ALLOW_M)));
  TOpt<T> result;
  TOpt<std::string> opt_s = TAttrReader::GetOptString(elem, attr_name,
      TOpts::TRIM_WHITESPACE);

  if (opt_s.IsUnknown()) {
    if (opts & TOpts::REQUIRE_PRESENCE) {
      throw TMissingAttrValue(elem, attr_name);
    }
  } else {
    std::string s(std::move(*opt_s));

    if (s.empty()) {
      if (empty_value_name && (opts & TOpts::STRICT_EMPTY_VALUE)) {
        throw TMissingAttrValue(elem, attr_name);
      }
    } else if (!empty_value_name || (s != empty_value_name)) {
      result.MakeKnown(AttrToInt<T>(s, elem, attr_name, opts));
    }
  }

  return result;
}

TOpt<uint64_t> TAttrReader::GetOptUint64(const DOMElement &elem,
    const char *attr_name, const char *empty_value_name, unsigned int opts) {
  return GetOpt64BitIntAttr<uint64_t>(elem, attr_name, empty_value_name, opts);
}

TOpt<int64_t> TAttrReader::GetOptInt64(const DOMElement &elem,
    const char *attr_name, const char *empty_value_name, unsigned int opts) {
  return GetOpt64BitIntAttr<int64_t>(elem, attr_name, empty_value_name, opts);
}

template <typename T>
static T Get64BitIntAttr(const DOMElement &elem,
    const char *attr_name, unsigned int opts) {
  assert(&elem);
  assert(attr_name);
  using TOpts = TAttrReader::TOpts;
  assert(opts == (opts & (TOpts::ALLOW_K | TOpts::ALLOW_M)));
  std::string s = TAttrReader::GetString(elem, attr_name,
      TOpts::TRIM_WHITESPACE | TOpts::THROW_IF_EMPTY);
  return AttrToInt<T>(s, elem, attr_name, opts);
}

uint64_t TAttrReader::GetUint64(const DOMElement &elem, const char *attr_name,
    unsigned int opts) {
  return Get64BitIntAttr<uint64_t>(elem, attr_name, opts);
}

int64_t TAttrReader::GetInt64(const DOMElement &elem, const char *attr_name,
    unsigned int opts) {
  return Get64BitIntAttr<int64_t>(elem, attr_name, opts);
}
