#ifndef INCLUDE_ONCE_38607EE2_F57E_42D5_9068_AF4A0CEB0A2E
#define INCLUDE_ONCE_38607EE2_F57E_42D5_9068_AF4A0CEB0A2E

#include <memory>
#include <vector>
#include <string_view>
#include <QString>
#include <glm/glm.hpp>

struct Texture4DLayer
{
    std::unique_ptr<glm::vec4[]> data;
    unsigned sizes3d[3];
    unsigned numAltLayers;
};

Texture4DLayer loadCompressedTexture4D(QString const& path, float altitudeCoord);
void saveTexture4DCompressed(const glm::vec4* pixels, std::vector<int> const& sizes,
                             int texSavePrecision, QString const& path);

#endif
