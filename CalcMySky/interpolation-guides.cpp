/*
 * CalcMySky - a simulator of light scattering in planetary atmospheres
 * Copyright © 2025 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include "interpolation-guides.hpp"
#include <cmath>
#include <vector>
#include <limits>
#include <sstream>
#include <iostream>
#include <QFile>
#include "util.hpp"
#include "../common/util.hpp"

/* Glossary:
 *  * Guide line — a line originating at the guide row and ending at a neighboring
 *     target row. The guide itself (an abstract entity) is supposed to be between the rows.
 *  * Guide origin — the point where the guide line originates. It's represented by
 *     an (integral) index in the guide row.
 *  * Guide target — the point that the guide line ends with in the neighboring row.
 *     It's represented by a fractional index in the row.
 *  * Guide value — the point that the guide line crosses, given a position between
 *     rows. It's represented by a fractional index in the row.
 */

namespace
{

// v2v = vector to value
inline float v2v(glm::vec4 const& v)
{
    constexpr int vecIndex = 1; // XXX: maybe we should average over all components, or do something else to get better results.
    return v[vecIndex];
}

int countMaxima(glm::vec4 const*const rowData, const unsigned rowLength)
{
    if(rowLength < 2) return 1;

    int numMaxima = 0;

    auto diff = v2v(rowData[1]) - v2v(rowData[0]);
    if(diff < 0)
    {
        // If we start with decrease, one local maximum is at the starting border. It's not
        // necessarily f'(x)=0 kind of maximum, but we might have the largest element here.
        ++numMaxima;
    }

    for(unsigned c = 2; c < rowLength; ++c)
    {
        const auto newDiff = v2v(rowData[c]) - v2v(rowData[c-1]);
        if(diff > 0 && newDiff < 0)
        {
            // Maximum crossed
            ++numMaxima;
        }
        diff = newDiff;
    }

    if(diff > 0)
    {
        // If we end with increase, one local maximum is at the ending border. It's not
        // necessarily f'(x)=0 kind of maximum, but we might have the largest element here.
        ++numMaxima;
    }

    return numMaxima;
}

bool minimumIsSinglePoint(glm::vec4 const*const rowData, const int rowLength)
{
    int minimumPos = -1;
    for(int c = 1; c < rowLength-1; ++c)
    {
        if(v2v(rowData[c-1]) > v2v(rowData[c]) && v2v(rowData[c]) < v2v(rowData[c+1]))
        {
            minimumPos = c;
            break;
        }
    }
    if(minimumPos < 0)
        return false; // No minimum at all...

    const auto oldMaximumCount = countMaxima(rowData, rowLength);

    // Check whether we can remove this minimum by averaging the neighboring points.
    std::vector<glm::vec4> copy(rowData, rowData+rowLength);
    copy[minimumPos] = (copy[minimumPos-1] + copy[minimumPos+1]) / 2.f;

    const auto newMaximumCount = countMaxima(copy.data(), rowLength);

    return newMaximumCount < oldMaximumCount;
}

float interpolateN(const float N0, const float N1, const float valN0, const float valN1, const float value)
{
    return N0+(N1-N0)*(value-valN0)/(valN1-valN0);
}

enum DirectionInRow
{
    DIR_UP   = +1,
    DIR_DOWN = -1,
};

constexpr int POINT_NOT_FOUND = -1;

/*!
 * Find intersection of the given value with the scaled & shifted values of the given row. The result is the
 * fractional index: such that the given value is between the values of the two closest integer indices.
 * Scaling & shifting makes all the values non-negative, and brings the maxima of each row to one normalized
 * value. This lets us easily trace the maximum. The assumption is that the row has only one maximum.
 */
float findIntersection(glm::vec4 const*const row, const unsigned rowLength,
                       const float globalMinValue, const float rowMaxValue,
                       const float value, const unsigned startingPosition, const DirectionInRow dir)
{
    const auto valueAtStartPos = (v2v(row[startingPosition]) - globalMinValue) / (rowMaxValue - globalMinValue);
    const bool wantGrowing = value > valueAtStartPos;
    const int endN = dir==DIR_DOWN ? 0 : rowLength-1;
    for(int n = startingPosition; n != endN; n += dir)
    {
        // Scaling & shifting is done here
        const auto valN  = (v2v(row[n]) - globalMinValue) / (rowMaxValue - globalMinValue);
        const auto valN1 = (v2v(row[n+dir]) - globalMinValue) / (rowMaxValue - globalMinValue);

        if(wantGrowing)
        {
            // If overshot or matched, we've found it
            if(valN1 >= value)
                return interpolateN(n, n+dir, valN, valN1, value);
        }
        else
        {
            // If undershot or matched, we've found it
            if(valN1 <= value)
                return interpolateN(n, n+dir, valN, valN1, value);
        }
    }
    return POINT_NOT_FOUND;
}

enum TargetRowDir
{
    // Numeric values refer to offset in the data (in units of stride)
    ROW_ABOVE = -1,
    ROW_BELOW = +1,
};

/*!
 * Compute guides that define lines between rows, originating at the points in the row above and going
 * down if targetRowDir==ROW_BELOW, or at the points in the row below and going up if targetRowDir==ROW_ABOVE.
 *
 * In the following figures the guide row is denoted by "-----", the originating points by "o", and the
 * target points by "*" if they don't end at integral row points, and by "Ж" if they do. Target-row points
 * are denoted by "t".
 *
 * When targetRowDir==ROW_BELOW, the lines might look something like this:
 *    o-----o-----o-----o-----o-----o-----o-----o-----o
 *     \     \     \     \    |     |     |    /     /
 *      \     \     \     \   |     |     |   /     /
 *    t  *  t  *  t  *  t  *  Ж     Ж     Ж  *  t  *  t
 *
 * When targetRowDir==ROW_ABOVE, the lines might look something like this:
 * *  t  *  t  *  t  *  t     Ж     Ж     Ж     t  *  Ж
 *  \     \     \     \       |     |     |       /   |
 *   \     \     \     \      |     |     |      /    |
 *    o-----o-----o-----o-----o-----o-----o-----o-----o
 *
 * There's numRows-1 total guide rows, because the last (or first,
 * depending on targetRowDir) row has nothing to point to.
 *
 * Return value of this function is a table of guide targets, indexed by guide origin coordinates.
 */
std::vector<float> computeGuidesBetweenRows(glm::vec4 const*const data,
                                            const int numRows, const int numCols, const unsigned rowStride,
                                            std::vector<int> const& maxPositionsPerRow, const float globalMin,
                                            const TargetRowDir targetRowDir)
{
    std::vector<float> guideTargets(numCols*(numRows-1), NAN);
    for(int originRow = 0; originRow != numRows-1; ++originRow)
    {
        const auto currRow = targetRowDir==ROW_BELOW ? originRow : originRow+1;
        const auto nextRow = currRow + targetRowDir;

        const auto currRowData = data+currRow*rowStride;
        const auto nextRowData = data+nextRow*rowStride;

        const auto currRowMax = v2v(currRowData[maxPositionsPerRow[currRow]]);
        const auto nextRowMax = v2v(nextRowData[maxPositionsPerRow[nextRow]]);

        // Looking in the target row for the (fractional) column that corresponds to the column in the current row.
        for(int currCol = 0; currCol < numCols; ++currCol)
        {
            const auto currValue = v2v(currRowData[currCol]);
            const auto dirInRow = currCol > maxPositionsPerRow[currRow] ? DIR_UP : DIR_DOWN;
            if(currRowMax - globalMin == 0 || nextRowMax - globalMin == 0)
            {
                // Either this row or the next one are constant zeros, let
                // the interpolation work in the standard bilinear mode.
                guideTargets[originRow*numCols+currCol] = currCol;
                continue;
            }
            const auto currRowNormalizedValue = (         currValue        - globalMin) / (currRowMax - globalMin);
            const auto nextRowNormalizedValue = (v2v(nextRowData[currCol]) - globalMin) / (nextRowMax - globalMin);
            if(currRowNormalizedValue == nextRowNormalizedValue)
            {
                // Special case when values in the current and the next row underflow to zero (or are equal for
                // another reason, which is still likely to be a plateau). We don't want them all to interpolate
                // to a faraway nonzero point. Instead we want to simply associate with the closest one.
                guideTargets[originRow*numCols+currCol] = currCol;
                continue;
            }
            auto colInNextRow = findIntersection(nextRowData, numCols,
                                                 globalMin, nextRowMax, currRowNormalizedValue,
                                                 maxPositionsPerRow[nextRow], dirInRow);
            if(colInNextRow == POINT_NOT_FOUND)
            {
                // No intersection found, this means all values in the next row are larger (after shifting&scaling)
                // than the (shifted&scaled) current value. Then just take the minimum value as the target.
                // Since we assume that there's only one maximum in the data, we can just take the far end of the
                // corresponding side of the domain.
                colInNextRow = dirInRow==DIR_UP ? numCols-1 : 0;
            }
            guideTargets[originRow*numCols+currCol] = colInNextRow;
        }
    }
    return guideTargets;
}

// posBetweenRows is the distance from the guide row above to the target row below, in units of rows: [0,1)
float getTopDownGuideValue(float const*const guidesRow, const int posInRow, const float posBetweenRows)
{
    return (guidesRow[posInRow]-posInRow)*posBetweenRows+posInRow;
}

// posBetweenRows is the distance from target row above to the guide row below, in units of rows: [0,1)
float getBottomUpGuideValue(float const*const guidesRow, const int posInRow, const float posBetweenRows)
{
    return (guidesRow[posInRow]-posInRow)*(1-posBetweenRows)+posInRow;
}

enum class GuideType
{
    TopDown,  // The guide whose guide row is above target row
    BottomUp, // The guide whose guide row is below target row
};

struct GuideBetweenRows
{
    int origin;
    float target;
    float valueInTheMiddle;
};
// Finds the guide whose value in the middle between rows is the closest to
// the value provided. Returns the guide origin if successful, -1 otherwise.
GuideBetweenRows findNearestGuideBetweenRows(float const*const guidesRow, const int rowLength, const float value,
                                             const GuideType guideType, const DirectionInRow searchDir)
{
    const int startPos = searchDir==DIR_UP ?      0      : rowLength-1;
    const int   endPos = searchDir==DIR_UP ? rowLength-1 :      -1    ;
    for(int pos = startPos; pos != endPos; pos += searchDir)
    {
        const auto guideValue = guideType==GuideType::TopDown ? getTopDownGuideValue (guidesRow, pos, 0.5)
                                                              : getBottomUpGuideValue(guidesRow, pos, 0.5);
        if(searchDir == DIR_UP)
        {
            if(guideValue >= value) return {pos,guidesRow[pos],guideValue};
        }
        else
        {
            if(guideValue <= value) return {pos,guidesRow[pos],guideValue};
        }
    }
    return {POINT_NOT_FOUND,POINT_NOT_FOUND,NAN};
}

// Calculates angle of the guide line deviation from the bilinear-mode interpolation line,
// which would go from the origin column to the same column in the target row.
float calcAngle(const int guideOrigin, const float guideTarget, const GuideType guideType)
{
    return std::atan(guideType==GuideType::TopDown ? guideTarget-guideOrigin : guideOrigin-guideTarget);
}

}

/*
 * Rows are considered detailed enough to be thought of as smooth functions. The guides generated by this function
 * define the angles at which the interpolation between these rows/columns should be done. This in particular lets
 * the interpolant follow the maximum of each row as if it linearly shifts between each row, instead of simply
 * decaying and letting the maximum in the next row increase separately.
 */
void generateInterpolationGuides2D(glm::vec4 const*const data,
                                   const unsigned width, const unsigned height, const unsigned rowStride, int16_t* angles,
                                   const int altIndex, const int secondDimIndex, const char*const secondDimName,
                                   const bool needCheckForMultipleMaxima)
{
    if(width==0 || height==0)
    {
        std::cerr << "generateInterpolationGuides2D: empty input\n";
        throw MustQuit{};
    }

    const int numRows = height;
    const int numCols = width;

    if(needCheckForMultipleMaxima)
    {
        for(int row = 0; row < numRows; ++row)
        {
            const auto rowData = data+row*rowStride;
            const auto numMaxima = countMaxima(rowData, numCols);
            if(numMaxima > 1)
            {
                // One single-pixel dip usually doesn't create much problems, so don't report this case of multiple maxima.
                if(numMaxima == 2 && !minimumIsSinglePoint(rowData,numCols))
                {
                    std::cerr << "\nwarning: " << numMaxima << " maxima instead of supported 1 in row " << row
                              << " at altitude index " << altIndex << ", " << secondDimName << " index " << secondDimIndex
                              << ".\n";
                    std::cerr << "Row data:\n";
                    for(int c = 0; c < numCols; ++c)
                        std::cerr << v2v(rowData[c]) << (c==numCols-1 ? "\n" : ",");
                }
            }
        }
    }

    std::vector<int> maxPositionsPerRow(numRows);
    std::vector<float> minimaPerRow(numRows);
    for(int row = 0; row < numRows; ++row)
    {
        const auto rowData = data+row*rowStride;
        const auto minmax = std::minmax_element(rowData, rowData+numCols,
                                                [](glm::vec4 const& v1, glm::vec4 const& v2)
                                                { return v2v(v1) < v2v(v2); });
        minimaPerRow[row] = v2v(*minmax.first);
        maxPositionsPerRow[row] = minmax.second - rowData;
    }
    const auto globalMin = *std::min_element(minimaPerRow.begin(), minimaPerRow.end());
    // We need both top-down and bottom-up guides because if we track only lines in one direction, in case of many-to-one
    // converging lines we'll lose some target points away from the attractor, getting empty space where there should
    // actually be lines. This will reduce quality of the guides.
    const auto guidesTopDown  = computeGuidesBetweenRows(data, numRows, numCols, rowStride,
                                                         maxPositionsPerRow, globalMin, ROW_BELOW);
    const auto guidesBottomUp = computeGuidesBetweenRows(data, numRows, numCols, rowStride,
                                                         maxPositionsPerRow, globalMin, ROW_ABOVE);

    for(int row = 0; row < numRows-1; ++row)
    {
        for(int col = 0; col < numCols; ++col)
        {
            const auto guidesRowTD = &guidesTopDown [row*numCols];
            const auto guidesRowBU = &guidesBottomUp[row*numCols];

            // "Above" and "below" refer to whether the guide is larger or smaller than col. We are looking for two
            // closest guides that enclose current column as computed in the middle between the guide and the target rows.
            const auto guideAboveTD = findNearestGuideBetweenRows(guidesRowTD, numCols, col, GuideType::TopDown , DIR_UP);
            const auto guideBelowTD = findNearestGuideBetweenRows(guidesRowTD, numCols, col, GuideType::TopDown , DIR_DOWN);
            const auto guideAboveBU = findNearestGuideBetweenRows(guidesRowBU, numCols, col, GuideType::BottomUp, DIR_UP);
            const auto guideBelowBU = findNearestGuideBetweenRows(guidesRowBU, numCols, col, GuideType::BottomUp, DIR_DOWN);

            float guideAngleAbove;
            if(guideAboveTD.origin == POINT_NOT_FOUND)
                guideAngleAbove = calcAngle(guideAboveBU.origin, guideAboveBU.target, GuideType::BottomUp);
            else if(guideAboveBU.origin == POINT_NOT_FOUND)
                guideAngleAbove = calcAngle(guideAboveTD.origin, guideAboveTD.target, GuideType::TopDown);
            else if(guideAboveTD.valueInTheMiddle < guideAboveBU.valueInTheMiddle)
                guideAngleAbove = calcAngle(guideAboveTD.origin, guideAboveTD.target, GuideType::TopDown);
            else
                guideAngleAbove = calcAngle(guideAboveBU.origin, guideAboveBU.target, GuideType::BottomUp);

            float guideAngleBelow;
            if(guideBelowTD.origin == POINT_NOT_FOUND)
                guideAngleBelow = calcAngle(guideBelowBU.origin, guideBelowBU.target, GuideType::BottomUp);
            else if(guideBelowBU.origin == POINT_NOT_FOUND)
                guideAngleBelow = calcAngle(guideBelowTD.origin, guideBelowTD.target, GuideType::TopDown);
            else if(guideBelowTD.valueInTheMiddle > guideBelowBU.valueInTheMiddle)
                guideAngleBelow = calcAngle(guideBelowTD.origin, guideBelowTD.target, GuideType::TopDown);
            else
                guideAngleBelow = calcAngle(guideBelowBU.origin, guideBelowBU.target, GuideType::BottomUp);

            // For simplicity we take simple arithmetic mean. Maybe we should weigh it by distance
            // to the guide line, but for now I think the simple way is sufficient.
            const auto anglesTypeMax = std::numeric_limits<std::remove_reference_t<decltype(angles[0])>>::max();
            static_assert(anglesTypeMax>0);
            angles[row*rowStride+col] = std::lround(anglesTypeMax/M_PI_2 * 0.5*(guideAngleAbove+guideAngleBelow));
        }
    }
}

void generateInterpolationGuidesForScatteringTexture(const std::string_view filePath, std::vector<glm::vec4> const& pixels,
                                                     std::vector<int> const& sizes)
{
    std::cerr << indentOutput() << "Generating interpolation guides:\n";
    const auto filePathQt = QByteArray::fromRawData(filePath.data(), filePath.size());
    const std::string_view ext = ".f32";
    if(!filePathQt.endsWith(ext.data()))
    {
        std::cerr << "wrong input filename extension\n";
        throw MustQuit{};
    }

    const auto altLayerCount = sizes[3];
    const auto szaLayerCount = sizes[2];
    const auto dVSLayerCount = sizes[1];
    const auto vzaPointCount = sizes[0];
    // Handle dimensions VZA-dotViewSun
    {
        OutputIndentIncrease incr;
        std::cerr << indentOutput() << "Generating interpolation guides for VZA-dotViewSun dimensions... ";

        const auto outputFilePath = filePathQt.left(filePathQt.size() - ext.size()) + "-dims01.guides2d";
        QFile out(outputFilePath);
        if(!out.open(QFile::WriteOnly))
        {
            std::cerr << "failed to open interpolation guides file for writing: " << out.errorString().toStdString() << "\n";
            throw MustQuit{};
        }
        {
            // Guides represent points between rows, so there's one less of them than rows.
            uint16_t outputSizes[4] = {uint16_t(sizes[0]), uint16_t(sizes[1]-1), uint16_t(sizes[2]), uint16_t(sizes[3])};
            if(out.write(reinterpret_cast<const char*>(outputSizes), sizeof outputSizes) != sizeof outputSizes)
            {
                std::cerr << "failed to write interpolation guides header: " << out.errorString().toStdString() << "\n";
                throw MustQuit{};
            }
        }

        uint16_t rowStride = vzaPointCount, height = dVSLayerCount;
        std::vector<int16_t> angles(rowStride*(height-1));
        for(int altIndex = 0; altIndex < altLayerCount; ++altIndex)
        {
            std::ostringstream ss;
            ss << altIndex << " of " << altLayerCount << " layers done ";
            std::cerr << ss.str();

            for(int szaIndex = 0; szaIndex < szaLayerCount; ++szaIndex)
            {
                const int altSliceOffset = altIndex*szaLayerCount*dVSLayerCount*vzaPointCount;
                const int szaSubsliceOffset = szaIndex*vzaPointCount*dVSLayerCount;
                const int aboveHorizonHalfSpaceOffset = vzaPointCount/2 + 1; // +1 skips zenith point, because it may have an extraneous maximum
                const int aboveHorizonHalfSpaceSize = vzaPointCount/2 - 1;   // -1 takes into account the +1 in the offset
                std::fill(angles.begin(), angles.end(), 0.f);
                generateInterpolationGuides2D(&pixels[altSliceOffset + szaSubsliceOffset + aboveHorizonHalfSpaceOffset],
                                              aboveHorizonHalfSpaceSize, height, rowStride,
                                              angles.data()+aboveHorizonHalfSpaceOffset, altIndex, szaIndex, "SZA", true);
                out.write(reinterpret_cast<const char*>(angles.data()), angles.size()*sizeof angles[0]);
            }

            // Clear previous status and reset cursor position
            const auto statusWidth=ss.tellp();
            std::cerr << std::string(statusWidth, '\b') << std::string(statusWidth, ' ')
                      << std::string(statusWidth, '\b');
        }
        std::cerr << "done\n";
        std::cerr << indentOutput() << "Saving interpolation guides to \"" << outputFilePath.toStdString() << "\"... ";

        out.close();
        if(out.error())
        {
            std::cerr << "failed to write file: " << out.errorString().toStdString() << "\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
    // Handle dimensions VZA-SZA
    {
        OutputIndentIncrease incr;
        std::cerr << indentOutput() << "Generating interpolation guides for VZA-SZA dimensions... ";

        const auto outputFilePath = filePathQt.left(filePathQt.size() - ext.size()) + "-dims02.guides2d";
        QFile out(outputFilePath);
        if(!out.open(QFile::WriteOnly))
        {
            std::cerr << "failed to open interpolation guides file for writing: " << out.errorString().toStdString() << "\n";
            throw MustQuit{};
        }
        {
            // Guides represent points between rows, so there's one less of them than rows.
            uint16_t outputSizes[4] = {uint16_t(sizes[0]), uint16_t(sizes[1]), uint16_t(sizes[2]-1), uint16_t(sizes[3])};
            if(out.write(reinterpret_cast<const char*>(outputSizes), sizeof outputSizes) != sizeof outputSizes)
            {
                std::cerr << "failed to write interpolation guides header: " << out.errorString().toStdString() << "\n";
                throw MustQuit{};
            }
        }

        uint16_t rowStride = vzaPointCount*dVSLayerCount, height = szaLayerCount;
        std::vector<int16_t> angles(rowStride*(height-1));
        for(int altIndex = 0; altIndex < altLayerCount; ++altIndex)
        {
            std::ostringstream ss;
            ss << altIndex << " of " << altLayerCount << " layers done ";
            std::cerr << ss.str();

            std::fill(angles.begin(), angles.end(), 0.f);
            for(int dVSIndex = 0; dVSIndex < dVSLayerCount; ++dVSIndex)
            {
                const int altSliceOffset = altIndex*szaLayerCount*dVSLayerCount*vzaPointCount;
                const int dVSSubsliceOffset = vzaPointCount*dVSIndex;
                const int aboveHorizonHalfSpaceOffset = vzaPointCount/2 + 1; // +1 skips zenith point, because it may have an extraneous maximum
                const int aboveHorizonHalfSpaceSize = vzaPointCount/2 - 1;   // -1 takes into account the +1 in the offset
                generateInterpolationGuides2D(&pixels[altSliceOffset + dVSSubsliceOffset + aboveHorizonHalfSpaceOffset],
                                              aboveHorizonHalfSpaceSize, height, rowStride,
                                              angles.data() + dVSSubsliceOffset + aboveHorizonHalfSpaceOffset,
                                              altIndex, dVSIndex, "dotViewSun", false/*same rows, no need to recheck*/);
            }
            out.write(reinterpret_cast<const char*>(angles.data()), angles.size()*sizeof angles[0]);

            // Clear previous status and reset cursor position
            const auto statusWidth=ss.tellp();
            std::cerr << std::string(statusWidth, '\b') << std::string(statusWidth, ' ')
                      << std::string(statusWidth, '\b');
        }
        std::cerr << "done\n";
        std::cerr << indentOutput() << "Saving interpolation guides to \"" << outputFilePath.toStdString() << "\"... ";

        out.close();
        if(out.error())
        {
            std::cerr << "failed to write file: " << out.errorString().toStdString() << "\n";
            throw MustQuit{};
        }
        std::cerr << "done\n";
    }
}
