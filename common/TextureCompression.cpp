#include "TextureCompression.hpp"
#include <fpzip.h>
#include <QFile>
#include "util.hpp"

namespace
{
constexpr std::string_view MAGIC_4D{"\0\0fpz\xff""4d", 8};
constexpr int NUM_VEC_COMPONENTS = 4;
}

Texture4DLayer loadCompressedTexture4D(QString const& path, const float altitudeCoord)
{
    QFile in(path);
    if(!in.open(QFile::ReadOnly))
        throw DataLoadError("Failed to open \"" + path + "\": " + in.errorString());

    std::string magic(MAGIC_4D.size(), '\0');
    if(in.read(magic.data(), magic.size()) != qint64(MAGIC_4D.size()))
        throw DataLoadError("Failed to read header magic in \"" + path + "\"");
    if(magic != MAGIC_4D)
        throw DataLoadError("Bad magic in \"" + path + "\"");

    uint16_t sizes[4] = {};
    if(in.read(reinterpret_cast<char*>(sizes), sizeof sizes) != sizeof sizes)
        throw DataLoadError("Failed to read altitude layer count in \"" + path + "\"");

    // FIXME: check that the file is really big enough to fit all the sizes from the header
    const auto altLayerCount = sizes[3];
    if(!altLayerCount) throw DataLoadError("No altitude layers found in \""+path+"\"");
    const auto altTexIndex = altitudeCoord==1 ? altLayerCount-2 : altitudeCoord*(altLayerCount-1);
    const auto floorAltIndex = std::floor(altTexIndex);
    const auto fractAltIndex = altTexIndex-floorAltIndex;

    std::vector<uint32_t> sizePerLayer(altLayerCount);
    const qint64 sizeOfLayerSizes = sizePerLayer.size() * sizeof sizePerLayer[0];
    if(in.read(reinterpret_cast<char*>(sizePerLayer.data()), sizeOfLayerSizes) != sizeOfLayerSizes)
        throw DataLoadError("Failed to read layer sizes in \"" + path + "\"");

    const auto layersOffset = in.pos();
    const auto altLayerSize = size_t(sizes[0]) * sizes[1] * sizes[2];
    const std::unique_ptr<glm::vec4[]> data[2] = {std::unique_ptr<glm::vec4[]>{new glm::vec4[altLayerSize]},
                                                  std::unique_ptr<glm::vec4[]>{new glm::vec4[altLayerSize]}};
    const std::unique_ptr<float[]> transposed(new float[altLayerSize * NUM_VEC_COMPONENTS]);
    for(int l = 0; l < 2; ++l)
    {
        const auto altLayer = floorAltIndex + l;
        auto offset = layersOffset;
        for(int n = 0; n < altLayer; ++n)
            offset += sizePerLayer[n];
        if(!in.seek(offset))
            throw DataLoadError(QString("Failed to seek to altitude layer %1 in \"%2\"").arg(floorAltIndex).arg(path));

        const qint64 sizeToRead = sizePerLayer[altLayer];
        std::unique_ptr<char[]> buffer(new char[sizeToRead]);
        if(in.read(buffer.get(), sizeToRead) != sizeToRead)
            throw DataLoadError(QString("Failed to read altitude layer %1 in \"%2\"").arg(altLayer).arg(path));

        const std::unique_ptr<FPZ, void(*)(FPZ*)> fpz(fpzip_read_from_buffer(buffer.get()),
                                                      fpzip_read_close);
        if(!fpzip_read_header(fpz.get()))
            throw DataLoadError(QString("Failed to read FPZIP header for altitude layer %1 (offset=%2) in "
                                        "\"%3\": %4").arg(altLayer).arg(offset).arg(path).arg(fpzip_errstr[fpzip_errno]));
        if(fpz->type != FPZIP_TYPE_FLOAT)
            throw DataLoadError(QString("Bad FPZIP type %1 in altitude layer %2 in "
                                        "\"%3\"").arg(fpz->type).arg(altLayer).arg(path));
        if(fpz->nf != NUM_VEC_COMPONENTS)
            throw DataLoadError(QString("Bad FPZIP field count %1 in altitude layer %2 in "
                                        "\"%3\"").arg(fpz->nf).arg(altLayer).arg(path));
        if(fpz->nx != sizes[0] || fpz->ny != sizes[1] || fpz->nz != sizes[2])
            throw DataLoadError(QString(u8"Mismatch between FPZIP dimensions %1×%2×%3 and dimensions in header %4×%5×%6")
                                    .arg(fpz->nx).arg(fpz->ny).arg(fpz->nz).arg(sizes[0]).arg(sizes[1]).arg(sizes[2]));

        if(!fpzip_read(fpz.get(), transposed.get()))
            throw DataLoadError("Failed to read FPZIP-compressed data");

        for(size_t pixInLayer = 0; pixInLayer < altLayerSize; ++pixInLayer)
            for(int comp = 0; comp < NUM_VEC_COMPONENTS; ++comp)
                 data[l][pixInLayer][comp] = transposed[altLayerSize * comp + pixInLayer];
    }

    Texture4DLayer out;
    out.sizes3d[0] = sizes[0];
    out.sizes3d[1] = sizes[1];
    out.sizes3d[2] = sizes[2];
    out.numAltLayers = altLayerCount;

    out.data.reset(new glm::vec4[altLayerSize]);
    for(size_t n = 0; n < altLayerSize; ++n)
    {
        const glm::vec4 lower = data[0][n], upper = data[1][n];
        out.data[n] = lower + fractAltIndex*(upper-lower);
    }

    return out;
}

void saveTexture4DCompressed(const glm::vec4*const pixels, std::vector<int> const& sizes, int texSavePrecision, QString const& path)
{
    if(sizes.size() != 4)
        throw std::invalid_argument("Bad sizes passed to saveTexture4DCompressed()");

    if(!texSavePrecision)
        texSavePrecision = 32;

    QFile out(path);
    if(!out.open(QFile::WriteOnly))
        throw DataSaveError("Failed to open file: " + out.errorString());

    out.write(MAGIC_4D.data(), MAGIC_4D.size());

    const uint16_t sizesToWrite[4] = {uint16_t(sizes[0]),
                                      uint16_t(sizes[1]),
                                      uint16_t(sizes[2]),
                                      uint16_t(sizes[3])};
    out.write(reinterpret_cast<const char*>(&sizesToWrite), sizeof sizesToWrite);
    const size_t altLayerCount = sizes[3];

    std::vector<uint32_t> sizePerLayer(altLayerCount); // Write zeros as initial placeholders
    out.write(reinterpret_cast<const char*>(sizePerLayer.data()), sizePerLayer.size() * sizeof sizePerLayer[0]);

    const auto altLayerSize = size_t(sizes[0]) * sizes[1] * sizes[2];
    std::vector<char> buffer(sizeof pixels[0] * altLayerSize/* compressed data are expected to be much less than this*/);

    std::unique_ptr<float[]> transposed(new float[altLayerSize * NUM_VEC_COMPONENTS]);

    for(size_t layerN = 0; layerN < altLayerCount; ++layerN)
    {
        const std::unique_ptr<FPZ, void(*)(FPZ*)> fpz(fpzip_write_to_buffer(buffer.data(), buffer.size()),
                                                      fpzip_write_close);
        if(!fpz)
            throw DataSaveError(QString("Failed to write to FPZIP buffer: %1").arg(fpzip_errstr[fpzip_errno]));
        fpz->type = FPZIP_TYPE_FLOAT;
        fpz->prec = texSavePrecision + 8/*exponent is also counted by fpzip*/;
        fpz->nx = sizes[0];
        fpz->ny = sizes[1];
        fpz->nz = sizes[2];
        fpz->nf = NUM_VEC_COMPONENTS;
        if(!fpzip_write_header(fpz.get()))
        {
            throw DataSaveError(QString("Failed to write FPZIP header at altitude layer %1: %2")
                                    .arg(layerN).arg(fpzip_errstr[fpzip_errno]));
        }

        // fpzip expects different fields to be in separate outer dimensions, while we are storing
        // them in the innermost dimension. For optimal compression we need to transpose input data.
        for(size_t pixInLayer = 0; pixInLayer < altLayerSize; ++pixInLayer)
            for(int comp = 0; comp < NUM_VEC_COMPONENTS; ++comp)
                transposed[altLayerSize * comp + pixInLayer] = pixels[altLayerSize * layerN + pixInLayer][comp];

        const auto compressedSize = fpzip_write(fpz.get(), transposed.get());
        if(!compressedSize)
        {
            throw DataSaveError(QString("Failed to compress altitude layer %1: %2")
                                    .arg(layerN).arg(fpzip_errstr[fpzip_errno]));
        }

        out.write(buffer.data(), compressedSize);
        sizePerLayer[layerN] = compressedSize;
    }

    if(!out.seek(MAGIC_4D.size() + sizeof altLayerCount))
        throw DataSaveError("Failed to seek to layer sizes offset");

    // Overwrite the placeholders with the actual values
    out.write(reinterpret_cast<const char*>(sizePerLayer.data()), sizePerLayer.size() * sizeof sizePerLayer[0]);

    out.close();
    if(out.error())
        throw DataSaveError("Failed to write file: " + out.errorString());
}
