#version 330
#extension GL_ARB_shading_language_420pack : enable

const float PI=3.14159265358979323846;
vec3 calcViewDir();

uniform float frameNum;
uniform float sunAzimuthInPhoto;
uniform sampler2D photo;

out vec4 color;

vec3 sphericalToCartesian(float azimuth, float altitude)
{
    return vec3(sin(azimuth)*cos(altitude),
                cos(azimuth)*cos(altitude),
                sin(altitude));
}

float angularDistance(vec2 azAlt1, vec2 azAlt2)
{
    const float dotProd=sin(azAlt1.y)*sin(azAlt2.y)+cos(azAlt1.y)*cos(azAlt2.y)*cos(azAlt1.x-azAlt2.x);
    return acos(clamp(dotProd,-1,1));
}

float angularDistanceFromAxisToOffsetFromImageCenter(float declination)
{
    return -1.67366915678586 + 825.757082363537*declination - 40.0070433677833*declination*declination;
}

vec2 imgCoordsForDir(float azimuth, float altitude)
{
    const float degree=PI/180;
    const vec2 imageCenterPixel=textureSize(photo,0)/2;
    const vec2 cameraAxisInSphericalCoords=vec2(-31.7280741604907, 45.1754692438333)*degree;
    const vec3 cameraAxisDir=vec3(-0.370718738944346, 0.599587457927117, 0.709268987685216);
    const vec4 referenceStar=vec4(vec2(1134, 1221), vec2(336.0015, 9.1607)*degree); // Mirach
    const vec3 refStarDir=sphericalToCartesian(referenceStar.z,referenceStar.w);
    const vec2 refStarPos=referenceStar.xy;
    // x direction of the reference coordinate system
    const vec3 refStarDirProjectedToCameraPlaneNormalized=normalize(refStarDir-cameraAxisDir*dot(cameraAxisDir,refStarDir));
    // y direction of the reference coordinate system
    const vec3 refStarNormalInCameraPlane = cross(refStarDirProjectedToCameraPlaneNormalized,cameraAxisDir);
    const vec3 rayDir=sphericalToCartesian(azimuth,altitude);
    const vec3 rayDirInCameraPlane = normalize(rayDir-cameraAxisDir*dot(cameraAxisDir,rayDir));
    const float xRef = dot(rayDirInCameraPlane,refStarDirProjectedToCameraPlaneNormalized);
    const float yRef = -dot(rayDirInCameraPlane,refStarNormalInCameraPlane);

    // Angle of rotation of the reference coordinate system around the camera axis
    const vec2 refStarPosRelativeToImageCenter=refStarPos - imageCenterPixel;
    const float phi=atan(refStarPosRelativeToImageCenter.y,refStarPosRelativeToImageCenter.x);
    const vec2 rotatedXYref=mat2(cos(phi),sin(phi),
                                -sin(phi),cos(phi))*vec2(xRef,yRef);
    return imageCenterPixel+rotatedXYref*angularDistanceFromAxisToOffsetFromImageCenter(angularDistance(vec2(azimuth,altitude),cameraAxisInSphericalCoords));
}

vec3 srgb2xyz(const vec3 srgb)
{
    const vec3 rgbl=pow(srgb, vec3(2.2)); // FIXME: approximation
    return rgbl*mat3(0.4124564,0.3575761,0.1804375,
                     0.2126729,0.7151522,0.0721750,
                     0.0193339,0.1191920,0.9503041);
}

void main()
{
    if(gl_FragCoord.x-0.5!=frameNum) discard;

    vec3 view_direction=calcViewDir();

    const float phi=sunAzimuthInPhoto;
    const float theta=asin(view_direction.z/length(view_direction));

    const vec2 texCoord=imgCoordsForDir(phi,theta)/textureSize(photo,0);

    const vec3 srgb = texture(photo, texCoord).rgb;
    color = vec4(srgb2xyz(srgb),1);
}
