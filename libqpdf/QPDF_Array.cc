#include <qpdf/QPDF_Array.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QUtil.hh>
#include <stdexcept>

static const QPDFObjectHandle null_oh = QPDFObjectHandle::newNull();

QPDF_Array::QPDF_Array(std::vector<QPDFObjectHandle> const& v) :
    QPDFValue(::ot_array, "array")
{
    setFromVector(v);
}

QPDF_Array::QPDF_Array(
    std::vector<std::shared_ptr<QPDFObject>>&& v, bool sparse) :
    QPDFValue(::ot_array, "array"),
    sparse(sparse)
{
    setFromVector(std::move(v));
}

QPDF_Array::QPDF_Array(SparseOHArray const& items) :
    QPDFValue(::ot_array, "array"),
    sparse(true),
    sp_elements(items)

{
}

QPDF_Array::QPDF_Array(OHArray const& items) :
    QPDFValue(::ot_array, "array"),
    sparse(false),
    elements(items)
{
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(std::vector<QPDFObjectHandle> const& items)
{
    return do_create(new QPDF_Array(items));
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(
    std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse)
{
    return do_create(new QPDF_Array(std::move(items), sparse));
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(SparseOHArray const& items)
{
    return do_create(new QPDF_Array(items));
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(OHArray const& items)
{
    return do_create(new QPDF_Array(items));
}

std::shared_ptr<QPDFObject>
QPDF_Array::copy(bool shallow)
{
    if (sparse) {
        return create(shallow ? sp_elements : sp_elements.copy());
    } else {
        return create(shallow ? elements : elements.copy());
    }
}

void
QPDF_Array::disconnect()
{
    if (sparse) {
        sp_elements.disconnect();
    } else {
        elements.disconnect();
    }
}

std::string
QPDF_Array::unparse()
{
    if (sparse) {
        std::string result = "[ ";
        size_t size = sp_elements.size();
        for (size_t i = 0; i < size; ++i) {
            result += sp_elements.at(i).unparse();
            result += " ";
        }
        result += "]";
        return result;
    } else {
        std::string result = "[ ";
        auto size = elements.elements.size();
        for (int i = 0; i < int(size); ++i) {
            result += getItem(i).unparse();
            result += " ";
        }
        result += "]";
        return result;
    }
}

JSON
QPDF_Array::getJSON(int json_version)
{
    if (sparse) {
        JSON j = JSON::makeArray();
        size_t size = sp_elements.size();
        for (size_t i = 0; i < size; ++i) {
            j.addArrayElement(sp_elements.at(i).getJSON(json_version));
        }
        return j;
    } else {
        JSON j = JSON::makeArray();
        size_t size = elements.elements.size();
        for (int i = 0; i < int(size); ++i) {
            j.addArrayElement(getItem(i).getJSON(json_version));
        }
        return j;
    }
}

int
QPDF_Array::getNItems() const
{
    if (sparse) {
        // This should really return a size_t, but changing it would break
        // a lot of code.
        return QIntC::to_int(sp_elements.size());
    } else {
        return QIntC::to_int(elements.elements.size());
    }
}

QPDFObjectHandle
QPDF_Array::getItem(int n) const
{
    if (sparse) {
        if ((n < 0) || (n >= QIntC::to_int(sp_elements.size()))) {
            throw std::logic_error(
                "INTERNAL ERROR: bounds error accessing QPDF_Array element");
        }
        return sp_elements.at(QIntC::to_size(n));
    } else {
        if ((n < 0) || (n >= QIntC::to_int(elements.elements.size()))) {
            throw std::logic_error(
                "INTERNAL ERROR: bounds error accessing QPDF_Array element");
        }
        auto const& obj = elements.elements.at(size_t(n));
        return obj ? obj : null_oh;
    }
}

void
QPDF_Array::getAsVector(std::vector<QPDFObjectHandle>& v) const
{
    if (sparse) {
        size_t size = sp_elements.size();
        for (size_t i = 0; i < size; ++i) {
            v.push_back(sp_elements.at(i));
        }
    } else {
        v = std::vector<QPDFObjectHandle>(
            elements.elements.cbegin(), elements.elements.cend());
    }
}

void
QPDF_Array::setItem(int n, QPDFObjectHandle const& oh)
{
    if (sparse) {
        sp_elements.setAt(QIntC::to_size(n), oh);
    } else {
        elements.setAt(QIntC::to_size(n), oh);
    }
}

void
QPDF_Array::setFromVector(std::vector<QPDFObjectHandle> const& v)
{
    if (sparse) {
        sp_elements = SparseOHArray();
        for (auto const& iter: v) {
            sp_elements.append(iter);
        }
    } else {
        elements = OHArray();
        for (auto const& iter: v) {
            elements.elements.push_back(iter.getObj());
        }
    }
}

void
QPDF_Array::setFromVector(std::vector<std::shared_ptr<QPDFObject>>&& v)
{
    if (sparse) {
        sp_elements = SparseOHArray();
        for (auto&& item: v) {
            if (item->getTypeCode() != ::ot_null ||
                item->getObjGen().isIndirect()) {
                sp_elements.append(std::move(item));
            } else {
                ++sp_elements.n_elements;
            }
        }
    } else {
        elements = OHArray();
        for (auto&& item: v) {
            elements.elements.push_back(std::move(item));
        }
    }
}

void
QPDF_Array::insertItem(int at, QPDFObjectHandle const& item)
{
    if (sparse) {
        // As special case, also allow insert beyond the end
        if ((at < 0) || (at > QIntC::to_int(sp_elements.size()))) {
            throw std::logic_error(
                "INTERNAL ERROR: bounds error accessing QPDF_Array element");
        }
        sp_elements.insert(QIntC::to_size(at), item);
    } else {
        // As special case, also allow insert beyond the end
        if ((at < 0) || (at > QIntC::to_int(elements.elements.size()))) {
            throw std::logic_error(
                "INTERNAL ERROR: bounds error accessing QPDF_Array element");
        }
        elements.insert(QIntC::to_size(at), item);
    }
}

void
QPDF_Array::appendItem(QPDFObjectHandle const& item)
{
    if (sparse) {
        sp_elements.append(item);
    } else {
        elements.elements.push_back(item.getObj());
    }
}

void
QPDF_Array::eraseItem(int at)
{
    if (sparse) {
        sp_elements.erase(QIntC::to_size(at));
    } else {
        elements.erase(QIntC::to_size(at));
    }
}
