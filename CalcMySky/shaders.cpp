#include "shaders.hpp"

#include <set>
#include <iomanip>
#include <iostream>
#include <QRegularExpression>
#include <QApplication>
#include <QFile>
#include <QDir>

#include "data.hpp"
#include "util.hpp"

#include "config.h"

QString withHeadersIncluded(QString src, QString const& filename);

void initConstHeader(glm::vec4 const& wavelengths)
{
    QString header=1+R"(
#ifndef INCLUDE_ONCE_2B59AE86_E78B_4D75_ACDF_5DA644F8E9A3
#define INCLUDE_ONCE_2B59AE86_E78B_4D75_ACDF_5DA644F8E9A3
const float earthRadius=)"+QString::number(atmo.earthRadius)+"; // must be in meters\n"
                         "const float atmosphereHeight="+QString::number(atmo.atmosphereHeight)+"; // must be in meters\n"
                         R"(
const vec3 earthCenter=vec3(0,0,-earthRadius);

const float dobsonUnit = 2.687e20; // molecules/m^2
const float PI=3.1415926535897932;
const float km=1000;
#define sqr(x) ((x)*(x))

uniform float sunAngularRadius=)" + toString(atmo.sunAngularRadius) + R"(;
const float moonRadius=)" + toString(moonRadius) + R"(;
const vec4 scatteringTextureSize=)" + toString(glm::vec4(atmo.scatteringTextureSize)) + R"(;
const vec2 irradianceTextureSize=)" + toString(glm::vec2(atmo.irradianceTexW, atmo.irradianceTexH)) + R"(;
const vec2 transmittanceTextureSize=)" + toString(glm::vec2(atmo.transmittanceTexW,atmo.transmittanceTexH)) + R"(;
const vec2 eclipsedSingleScatteringTextureSize=)" + toString(glm::vec2(atmo.eclipsedSingleScatteringTextureSize)) +R"(;
const vec2 lightPollutionTextureSize=)" + toString(glm::vec2(atmo.lightPollutionTextureSize)) +R"(;
const int radialIntegrationPoints=)" + toString(atmo.radialIntegrationPoints) + R"(;
const int angularIntegrationPoints=)" + toString(atmo.angularIntegrationPoints) + R"(;
#define lightPollutionAngularIntegrationPoints )" + toString(atmo.lightPollutionAngularIntegrationPoints) + R"(
const int eclipseAngularIntegrationPoints=)" + toString(atmo.eclipseAngularIntegrationPoints) + R"(;
const int numTransmittanceIntegrationPoints=)" + toString(atmo.numTransmittanceIntegrationPoints) + R"(;
)";
    for(auto const& scatterer : atmo.scatterers)
        header += "const vec4 scatteringCrossSection_"+scatterer.name+"="+toString(scatterer.scatteringCrossSection(wavelengths))+";\n";
    const auto wlI=atmo.wavelengthsIndex(wavelengths);
    header += "const vec4 groundAlbedo="+toString(atmo.groundAlbedo[wlI])+";\n";
    header += "const vec4 solarIrradianceAtTOA="+toString(atmo.solarIrradianceAtTOA[wlI])+";\n";
    header += "const vec4 lightPollutionRelativeRadiance="+toString(atmo.lightPollutionRelativeRadiance[wlI])+";\n";
    header += "const vec4 wavelengths="+toString(wavelengths)+";\n";
    header += "const int wlSetIndex="+toString(int(wlI))+";\n";

    header+="#endif\n"; // close the include guard
    virtualHeaderFiles[CONSTANTS_HEADER_FILENAME]=header;
}

QString makeDensitiesFunctions()
{
    QString header;
    QString src;
    for(auto const& scatterer : atmo.scatterers)
    {
        src += "float scattererNumberDensity_"+scatterer.name+"(float altitude)\n"
               "{\n"
               +scatterer.numberDensity+
               "}\n";
        header += "float scattererNumberDensity_"+scatterer.name+"(float altitude);\n";
    }
    for(auto const& absorber : atmo.absorbers)
    {
        src += "float absorberNumberDensity_"+absorber.name+"(float altitude)\n"
               "{\n"
               +absorber.numberDensity+
               "}\n";
        header += "float absorberNumberDensity_"+absorber.name+"(float altitude);\n";
    }

    header += "vec4 scatteringCrossSection();\n"
              "float scattererDensity(float altitude);\n";

    virtualHeaderFiles[DENSITIES_HEADER_FILENAME]=header;

    return src;
}

QString makeTransmittanceComputeFunctionsSrc(glm::vec4 const& wavelengths)
{
    const QString head=1+R"(
#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "common-functions.h.glsl"
)";
    const QString opticalDepthFunctionTemplate=R"(
vec4 opticalDepthToAtmosphereBorder_##agentSpecies(float altitude, float cosZenithAngle, vec4 crossSection)
{
    CONST float integrInterval=distanceToAtmosphereBorder(cosZenithAngle, altitude);

    CONST float R=earthRadius;
    CONST float r1=R+altitude;
    CONST float l=integrInterval;
    CONST float mu=cosZenithAngle;
    // Using midpoint rule for quadrature
    CONST float dl=integrInterval/numTransmittanceIntegrationPoints;
    float sum=0;
    for(int n=0;n<numTransmittanceIntegrationPoints;++n)
    {
        CONST float dist=(n+0.5)*dl;
        /* From law of cosines: r₂²=r₁²+l²+2r₁lμ */
        CONST float currAlt=-R+safeSqrt(sqr(r1)+sqr(dist)+2*r1*dist*mu);
        sum+=agent##NumberDensity_##agentSpecies(currAlt);
    }
    return sum*dl*crossSection;
}
)";
    QString opticalDepthFunctions;
    QString computeFunction = R"(
// This assumes that ray doesn't intersect Earth
vec4 computeTransmittanceToAtmosphereBorder(float cosZenithAngle, float altitude)
{
    CONST vec4 depth=
)";
    for(auto const& scatterer : atmo.scatterers)
    {
        opticalDepthFunctions += QString(opticalDepthFunctionTemplate).replace("##agentSpecies",scatterer.name).replace("agent##","scatterer");
        computeFunction += "        +opticalDepthToAtmosphereBorder_"+scatterer.name+
                             "(altitude,cosZenithAngle,"+toString(scatterer.extinctionCrossSection(wavelengths))+")\n";
    }
    for(auto const& absorber : atmo.absorbers)
    {
        opticalDepthFunctions += QString(opticalDepthFunctionTemplate).replace("##agentSpecies",absorber.name).replace("agent##","absorber");
        computeFunction += "        +opticalDepthToAtmosphereBorder_"+absorber.name+
                             "(altitude,cosZenithAngle,"+toString(absorber.crossSection(wavelengths))+")\n";
    }
    computeFunction += R"(      ;
    return depth; // Exponentiation will take place in sampling functions. This way we avoid underflow in texture values.
}
)";
    return head+makeDensitiesFunctions()+opticalDepthFunctions+computeFunction;
}

QString makeScattererDensityFunctionsSrc()
{
    const QString head=1+R"(
#version 330
#include "version.h.glsl"
#include "const.h.glsl"
)";
    return head+makeDensitiesFunctions();
}

QString makePhaseFunctionsSrc()
{
    QString src = 1+R"(
#version 330
#include "version.h.glsl"
#include "const.h.glsl"

)";
    QString header;
    for(auto const& scatterer : atmo.scatterers)
    {
        src += "vec4 phaseFunction_"+scatterer.name+"(float dotViewSun)\n"
               "{\n"
               +scatterer.phaseFunction+
               "}\n";
        header += "vec4 phaseFunction_"+scatterer.name+"(float dotViewSun);\n";
    }
    header+="vec4 currentPhaseFunction(float dotViewSun);\n";
    virtualHeaderFiles[PHASE_FUNCTIONS_HEADER_FILENAME]=header;
    return src;
}

QString makeTotalScatteringCoefSrc()
{
    QString src=1+R"(
#version 330
#include "version.h.glsl"
#include "const.h.glsl"
#include "densities.h.glsl"
#include "phase-functions.h.glsl"

vec4 totalScatteringCoefficient(float altitude, float dotViewInc)
{
    return
)";
    for(auto const& scatterer : atmo.scatterers)
    {
        src += "        + scatteringCrossSection_"+scatterer.name+
               " * scattererNumberDensity_"+scatterer.name+"(altitude) "
               " * phaseFunction_"+scatterer.name+"(dotViewInc)\n";
    }
    src += "        ;\n}\n";
    virtualHeaderFiles[TOTAL_SCATTERING_COEFFICIENT_HEADER_FILENAME]=
        "vec4 totalScatteringCoefficient(float altitude, float dotViewInc);\n";
    return src;
}

QString getShaderSrc(QString const& fileName, IgnoreCache ignoreCache)
{
    if(!ignoreCache)
    {
        if(const auto it=virtualSourceFiles.find(fileName); it!=virtualSourceFiles.end())
            return it->second;
        if(const auto it=virtualHeaderFiles.find(fileName); it!=virtualHeaderFiles.end())
            return it->second;
    }

    const auto appBinDir=QDir(qApp->applicationDirPath()+"/").canonicalPath();
    QString filePath=appBinDir + "/shaders/" + fileName;
    if(appBinDir==QDir(INSTALL_BINDIR).canonicalPath())
    {
        filePath=DATA_ROOT_DIR "shaders/" + fileName;
    }
    else if(appBinDir==QDir(BUILD_BINDIR "CalcMySky/").canonicalPath())
    {
        filePath=SOURCE_DIR "shaders/" + fileName;
    }
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        std::cerr << "Error opening shader file \"" << filePath.toStdString() << "\"\n";
        throw MustQuit{};
    }
    return file.readAll();
}

void defineDisabledDefinitions(QString& source)
{
    const QRegularExpression pattern("^(\\s*#[ \t]*version[ \t]+[^\n]+\\s*\n)"
                                     "(#[ \t]*definitions[ \t]*\\()"
                                     "([^)\n]+)"
                                     "(\\)[ \t]*\n)");
    const auto match = pattern.match(source);
    if(!match.hasMatch()) return;

    source.replace(pattern, "\\1\n");
    const auto definitions = match.captured(3).split(QChar(','));
    for(auto def : definitions)
    {
        def = def.trimmed();
        if(def.contains(QRegularExpression("^[0-9]")))
            continue; // not an undefined identifier
        source.replace(QRegularExpression("\\b("+def+")\\b"), "0 /*\\1*/");
    }
}

std::unique_ptr<QOpenGLShader> compileShader(QOpenGLShader::ShaderType type, QString source, QString const& description,
                                             QString* processedSource = nullptr)
{
    auto shader=std::make_unique<QOpenGLShader>(type);
    defineDisabledDefinitions(source);
    source=withHeadersIncluded(source, description);
    if(processedSource)
        *processedSource = source;
    if(!shader->compileSourceCode(source))
    {
        std::cerr << "Failed to compile " << description.toStdString() << ":\n"
                  << shader->log().toStdString() << "\n";
        std::cerr << "Source of the shader:\n________________________________________________\n";
        const auto lineCount=source.count(QChar('\n'));
        QTextStream srcStream(&source);
        int lineNumber=1;
        for(auto line=srcStream.readLine(); !line.isNull(); line=srcStream.readLine(), ++lineNumber)
        {
            std::cerr << std::setw(std::ceil(std::log10(lineCount))) << lineNumber << " " << line.toStdString() << "\n";
            const auto lineChanger = QRegularExpression("^\\s*#\\s*line\\s+([0-9]+)\\b.*").match(line);
            if(lineChanger.hasMatch())
            {
                lineNumber = lineChanger.captured(1).toInt() - 1;
                continue;
            }
        }
        std::cerr << "________________________________________________\n";
        throw MustQuit{};
    }
    if(!shader->log().isEmpty())
    {
        std::cerr << "Warnings while compiling " << description.toStdString() << ":\n"
                  << shader->log().toStdString() << "\n";
    }
    return shader;
}

std::unique_ptr<QOpenGLShader> compileShader(QOpenGLShader::ShaderType type, QString const& filename, QString* processedSource = nullptr)
{ return compileShader(type, getShaderSrc(filename), filename, processedSource); }

QString withHeadersIncluded(QString src, QString const& filename)
{
    QTextStream srcStream(&src);
    int lineNumber=1;
    int headerNumber=1;
    QString newSrc;
    for(auto line=srcStream.readLine(); !line.isNull(); line=srcStream.readLine(), ++lineNumber)
    {
        if(!line.contains(QRegularExpression("^\\s*#include(?: \"|_if\\s*\\()")))
        {
            // Not an include line, pass it to output
            newSrc.append(line+'\n');
            continue;
        }
        if(line.contains(QRegularExpression("^\\s*#include_if\\((?:0\\b[^)]*|[A-Za-z_][A-Za-z0-9_]*)\\)")))
        {
            // Disabled include, skip it (enabled one must have the condition be literal 1)
            newSrc.append('\n');
            continue;
        }
        const auto includePattern=QRegularExpression("^#include(?:_if\\s*\\(\\s*1\\s*(?:/\\*[^)]*\\*/)?\\))? \"([^\"]+)\"$").match(line);
        if(!includePattern.hasMatch())
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": syntax error in #include directive:\n" << line << "\n";
            throw MustQuit{};
        }
        const auto includeFileName=includePattern.captured(1);
        static const char headerSuffix[]=".h.glsl";
        if(!includeFileName.endsWith(headerSuffix))
        {
            std::cerr << filename.toStdString() << ":" << lineNumber << ": file to include must have suffix \""
                      << headerSuffix << "\"\n";
            throw MustQuit{};
        }
        const auto header = getShaderSrc(includeFileName);
        newSrc.append(QString("#line 1 %1 // %2\n").arg(headerNumber++).arg(includeFileName));
        newSrc.append(header);
        newSrc.append(QString("#line %1 0 // %2\n").arg(lineNumber+1).arg(filename));
    }
    return newSrc;
}

std::set<QString> getShaderFileNamesToLinkWith(QString const& filename, int recursionDepth=0)
{
    constexpr int maxRecursionDepth=50;
    if(recursionDepth>maxRecursionDepth)
    {
        std::cerr << "Include recursion depth exceeded " << maxRecursionDepth << "\n";
        throw MustQuit{};
    }
    std::set<QString> filenames;
    auto shaderSrc=getShaderSrc(filename);
    QTextStream srcStream(&shaderSrc);
    for(auto line=srcStream.readLine(); !line.isNull(); line=srcStream.readLine())
    {
        const auto includePattern=
            QRegularExpression("^#include(?:_if\\s*\\(\\s*1\\s*(?:/\\*[^)]*\\*/)?\\))? \"([^\"]+)(\\.h\\.glsl)\"$").match(line);
        if(!includePattern.hasMatch())
            continue;
        const auto includeFileBaseName=includePattern.captured(1);
        const auto headerFileName=includeFileBaseName+includePattern.captured(2);
        if(headerFileName == GLSL_EXTENSIONS_HEADER_FILENAME) // no companion source for extensions header
            continue;
        if(headerFileName == CONSTANTS_HEADER_FILENAME) // no companion source for constants header
            continue;
        if(headerFileName == RADIANCE_TO_LUMINANCE_HEADER_FILENAME) // no companion source for radiance-to-luminance conversion header
            continue;
        const auto shaderFileNameToLinkWith=includeFileBaseName+".frag";
        filenames.insert(shaderFileNameToLinkWith);
        if(shaderFileNameToLinkWith!=filename)
        {
            const auto extraFileNames=getShaderFileNamesToLinkWith(shaderFileNameToLinkWith, recursionDepth+1);
            filenames.insert(extraFileNames.begin(), extraFileNames.end());
        }
    }
    return filenames;
}

std::unique_ptr<QOpenGLShaderProgram> compileShaderProgram(QString const& mainSrcFileName,
                                                           const char* description, const UseGeomShader useGeomShader,
                                                           std::vector<std::pair<QString, QString>>* sourcesToSave)
{
    auto program=std::make_unique<QOpenGLShaderProgram>();

    auto shaderFileNames=getShaderFileNamesToLinkWith(mainSrcFileName);
    shaderFileNames.insert(mainSrcFileName);

    std::vector<std::unique_ptr<QOpenGLShader>> shaders;
    for(const auto& filename : shaderFileNames)
    {
        QString processedSource;
        shaders.emplace_back(compileShader(QOpenGLShader::Fragment, filename, &processedSource));
        program->addShader(shaders.back().get());
        if(sourcesToSave)
            sourcesToSave->push_back({filename, processedSource});
    }

    shaders.emplace_back(compileShader(QOpenGLShader::Vertex, "shader.vert"));
    program->addShader(shaders.back().get());

    if(useGeomShader)
    {
        shaders.emplace_back(compileShader(QOpenGLShader::Geometry, "shader.geom"));
        program->addShader(shaders.back().get());
    }

    if(!program->link())
    {
        // Qt prints linking errors to stderr, so don't print them again
        std::cerr << "Failed to link " << description << "\n";
        throw MustQuit{};
    }
    return program;
}

