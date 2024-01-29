#include <vector>
#include <string_view>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

void generateInterpolationGuides2D(glm::vec4 const* data, unsigned vecIndex,
                                   unsigned width, unsigned height, unsigned rowStride,
                                   bool detailedSideIsWidth, uint8_t* angles);
void generateInterpolationGuidesForScatteringTexture(std::string_view filePath,
                                                     std::vector<glm::vec4> const& pixels,
                                                     std::vector<int> const& sizes);
