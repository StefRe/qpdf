#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QUtil.hh>
#include <cstdlib>

class DiscardContents: public QPDFObjectHandle::ParserCallbacks
{
  public:
    ~DiscardContents() override = default;
    void
    handleObject(QPDFObjectHandle) override
    {
    }
    void
    handleEOF() override
    {
    }
};

class FuzzHelper
{
  public:
    FuzzHelper(unsigned char const* data, size_t size);
    void run();

  private:
    std::shared_ptr<QPDF> getQpdf();
    void testOutlines();
    void doChecks();

    Buffer input_buffer;
    Pl_Discard discard;
};

FuzzHelper::FuzzHelper(unsigned char const* data, size_t size) :
    // We do not modify data, so it is safe to remove the const for Buffer
    input_buffer(const_cast<unsigned char*>(data), size)
{
}

std::shared_ptr<QPDF>
FuzzHelper::getQpdf()
{
    auto is =
        std::shared_ptr<InputSource>(new BufferInputSource("fuzz input", &this->input_buffer));
    auto qpdf = QPDF::create();
    qpdf->setMaxWarnings(200);
    qpdf->processInputSource(is);
    return qpdf;
}

void
FuzzHelper::testOutlines()
{
    std::shared_ptr<QPDF> q = getQpdf();
    std::list<std::vector<QPDFOutlineObjectHelper>> queue;
    QPDFOutlineDocumentHelper odh(*q);
    queue.push_back(odh.getTopLevelOutlines());
    while (!queue.empty()) {
        for (auto& ol: *(queue.begin())) {
            ol.getDestPage();
            queue.push_back(ol.getKids());
        }
        queue.pop_front();
    }
}

void
FuzzHelper::doChecks()
{
    // Limit the memory used to decompress JPEG files during fuzzing. Excessive memory use during
    // fuzzing is due to corrupt JPEG data which sometimes cannot be detected before
    // jpeg_start_decompress is called. During normal use of qpdf very large JPEGs can occasionally
    // occur legitimately and therefore must be allowed during normal operations.
    Pl_DCT::setMemoryLimit(100'000'000);
    Pl_DCT::setScanLimit(50);

    Pl_PNGFilter::setMemoryLimit(1'000'000);
    Pl_TIFFPredictor::setMemoryLimit(1'000'000);
    Pl_Flate::setMemoryLimit(200'000);

    // Do not decompress corrupt data. This may cause extended runtime within jpeglib without
    // exercising additional code paths in qpdf, and potentially causing counterproductive timeouts.
    Pl_DCT::setThrowOnCorruptData(true);

    // Get as much coverage as possible in parts of the library that
    // might benefit from fuzzing.
    std::cerr << "\ninfo: starting testOutlines\n";
    testOutlines();
}

void
FuzzHelper::run()
{
    // The goal here is that you should be able to throw anything at
    // libqpdf and it will respond without any memory errors and never
    // do anything worse than throwing a QPDFExc or
    // std::runtime_error. Throwing any other kind of exception,
    // segfaulting, or having a memory error (when built with
    // appropriate sanitizers) will all cause abnormal exit.
    try {
        doChecks();
    } catch (QPDFExc const& e) {
        std::cerr << "QPDFExc: " << e.what() << std::endl;
    } catch (std::runtime_error const& e) {
        std::cerr << "runtime_error: " << e.what() << std::endl;
    }
}

extern "C" int
LLVMFuzzerTestOneInput(unsigned char const* data, size_t size)
{
#ifndef _WIN32
    // Used by jpeg library to work around false positives in memory
    // sanitizer.
    setenv("JSIMD_FORCENONE", "1", 1);
#endif
    FuzzHelper f(data, size);
    f.run();
    return 0;
}
