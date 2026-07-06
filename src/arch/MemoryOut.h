// ################################################################################################################
// # native Interface Design (natID)
// # Licensed under the Creative Commons Attribution-NoDerivatives (CC BY-ND), version 4.
// #
// # You may use this code under the terms of the Creative Commons Attribution-NoDerivatives (CC BY-ND), version 4.
// #
// # Contact: idzafic at etf.unsa.ba  or idzafic at gmail.com
// ################################################################################################################

// arch/MemoryOut.h — In-memory archive output (buffer-backed ArchiveOut).
// Provides the same put() interface as FileOut but writes into a
// growable memory buffer.

#pragma once
#include <arch/ArchiveOut.h>
#include <cnt/PushBackVector.h>
#include <cstring>
#include <fstream>

namespace arch
{

// A minimal IBinSerializer that writes into a memory buffer.
// Named MemBufferSerializer to avoid conflict with the templated
// MemorySerializer forward-declared in arch/Header.h.
class MemBufferSerializer : public IBinSerializer
{
protected:
    cnt::PushBackVector<char> _buf;
    td::UINT4 _pos = 0;

public:
    MemBufferSerializer()
    {
        _buf.reserve(8192);
    }

    bool open(const char* /*name*/) override { return true; }

    void write(const char* bytes, td::UINT4 nBytes) override
    {
        for (td::UINT4 i = 0; i < nBytes; ++i)
            _buf.push_back(bytes[i]);
        _pos += nBytes;
    }

    void read(char* /*bytes*/, td::UINT4 /*nBytes*/) override
    {
        // MemoryOut is write-only
    }

    void close() override {}
    void release() override {}

    bool goTo(td::LUINT8 pos) override
    {
        _pos = (td::UINT4)pos;
        return (_pos <= _buf.size());
    }

    const char* data() const { return (_buf.size() > 0) ? &_buf[0] : nullptr; }
    td::UINT4 size() const { return (td::UINT4)_buf.size(); }

    void reset()
    {
        _buf.clean();
        _pos = 0;
    }
};


class MemoryOut : public ArchiveOut
{
protected:
    MemBufferSerializer _memSer;

public:
    enum class PageSize { Normal, Small, Large };

    static MemoryOut* allocate(PageSize /*size*/)
    {
        return new MemoryOut();
    }

    bool open(const char* /*name*/)
    {
        return true;
    }

    void release()
    {
        delete this;
    }

    template <size_t size>
    MemoryOut(const char(&strMajorVersion)[size])
        : ArchiveOut(strMajorVersion, _memSer)
    {
    }

    MemoryOut()
        : ArchiveOut("DMOD", _memSer)
    {
    }

    ~MemoryOut()
    {
    }

    // Write raw text/data (the main interface used by the converter)
    using ArchiveOut::put;

    // Write the accumulated buffer to a file stream
    void writeToFile(std::ofstream& f)
    {
        if (_memSer.size() > 0 && _memSer.data())
            f.write(_memSer.data(), _memSer.size());
    }

    const char* data() const { return _memSer.data(); }
    td::UINT4 size() const { return _memSer.size(); }

    void reset()
    {
        _memSer.reset();
    }
};

} // namespace arch
