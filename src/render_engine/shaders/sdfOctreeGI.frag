// The version tag is inserted by code
// #define USE_TRILINEAR_INTERPOLATION
// #define USE_TRICUBIC_INTERPOLATION

#define MAX_AO_ITERATIONS 8

uniform vec3 startGridSize;
layout(std430, binding = 3) buffer octree
{
    uint octreeData[];
};

const uint isLeafMask = 1 << 31;
const uint isMarkedMask = 1 << 30;
const uint childrenIndexMask = ~(isLeafMask | isMarkedMask);

uint roundFloat(float a)
{
    return (a >= 0.5) ? 1 : 0;
}

uniform float epsilon;

// GI Options
uniform bool useIndirect;
uniform int numSamples;
uniform int maxDepth;
uniform int maxRaycastIterations;
uniform bool useDirectSphereSampling;
uniform int frameIndex;

// GI Skybox
uniform bool useCubemapSkybox;
uniform vec3 skyboxColor;
uniform samplerCube cubemapSkybox;

//Options 
uniform int maxShadowIterations;
uniform bool useSoftShadows;

//Lighting
uniform int lightNumber;
uniform vec3 lightPos[4];
uniform float lightIntensity[4];
uniform vec3 lightColor[4];
uniform float lightRadius[4];

//Material
uniform float matMetallic;
uniform float matRoughness;
uniform vec3 matAlbedo;
uniform vec3 matF0;

uniform float minBorderValue;
uniform float distanceScale;
uniform float time;

uniform mat4 worldToStartGridMatrix;
uniform mat4 modelMatrix;

uniform bool simple;

in vec3 gridPosition;
in vec3 gridNormal;
in vec3 cameraPos;

out vec4 fragColor;

// Light functions
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

#ifdef USE_TRICUBIC_INTERPOLATION
float getDistance(vec3 point)
{
    vec3 fracPart = point * startGridSize;
    ivec3 arrayPos = ivec3(floor(fracPart));

    if(arrayPos.x < 0 || arrayPos.y < 0 || arrayPos.z < 0 ||
       arrayPos.x >= startGridSize.x || arrayPos.y >= startGridSize.y || arrayPos.z >= startGridSize.z)
    {
            vec3 q = abs(point - vec3(0.5)) - 0.5;
            return length(max(q, vec3(0.0)))/distanceScale + minBorderValue;
    }

    fracPart = fract(fracPart);

    int index = arrayPos.z * int(startGridSize.y * startGridSize.x) +
                arrayPos.y * int(startGridSize.x) +
                arrayPos.x;
    uint currentNode = octreeData[index];

    while(!bool(currentNode & isLeafMask))
    {
        uint childIdx = (roundFloat(fracPart.z) << 2) + 
                        (roundFloat(fracPart.y) << 1) + 
                         roundFloat(fracPart.x);

        currentNode = octreeData[(currentNode & childrenIndexMask) + childIdx];
        fracPart = fract(2.0 * fracPart);
    }

    uint vIndex = currentNode & childrenIndexMask;

    return 0.0
         + uintBitsToFloat(octreeData[vIndex + 0]) + uintBitsToFloat(octreeData[vIndex + 1]) * fracPart[0] + uintBitsToFloat(octreeData[vIndex + 2]) * fracPart[0] * fracPart[0] + uintBitsToFloat(octreeData[vIndex + 3]) * fracPart[0] * fracPart[0] * fracPart[0] + uintBitsToFloat(octreeData[vIndex + 4]) * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 5]) * fracPart[0] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 6]) * fracPart[0] * fracPart[0] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 7]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 8]) * fracPart[1] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 9]) * fracPart[0] * fracPart[1] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 10]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 11]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 12]) * fracPart[1] * fracPart[1] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 13]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 14]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] + uintBitsToFloat(octreeData[vIndex + 15]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1]
         + uintBitsToFloat(octreeData[vIndex + 16]) * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 17]) * fracPart[0] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 18]) * fracPart[0] * fracPart[0] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 19]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 20]) * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 21]) * fracPart[0] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 22]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 23]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 24]) * fracPart[1] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 25]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 26]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 27]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 28]) * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 29]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 30]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 31]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2]
         + uintBitsToFloat(octreeData[vIndex + 32]) * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 33]) * fracPart[0] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 34]) * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 35]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 36]) * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 37]) * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 38]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 39]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 40]) * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 41]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 42]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 43]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 44]) * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 45]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 46]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 47]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2]
         + uintBitsToFloat(octreeData[vIndex + 48]) * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 49]) * fracPart[0] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 50]) * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 51]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 52]) * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 53]) * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 54]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 55]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 56]) * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 57]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 58]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 59]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 60]) * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 61]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 62]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + uintBitsToFloat(octreeData[vIndex + 63]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2];
}

vec3 getGradient(vec3 point)
{
    vec3 fracPart = point * startGridSize;
    ivec3 arrayPos = ivec3(floor(fracPart));
    fracPart = fract(fracPart);
    int index = arrayPos.z * int(startGridSize.y * startGridSize.x) +
                arrayPos.y * int(startGridSize.x) +
                arrayPos.x;
    uint currentNode = octreeData[index];

    while(!bool(currentNode & isLeafMask))
    {
        uint childIdx = (roundFloat(fracPart.z) << 2) + 
                        (roundFloat(fracPart.y) << 1) + 
                         roundFloat(fracPart.x);

        currentNode = octreeData[(currentNode & childrenIndexMask) + childIdx];
        fracPart = fract(2.0 * fracPart);
    }

    uint vIndex = currentNode & childrenIndexMask;

    return normalize(vec3((1 * uintBitsToFloat(octreeData[vIndex + 1]) + 2 * uintBitsToFloat(octreeData[vIndex + 2]) * fracPart[0] + 3 * uintBitsToFloat(octreeData[vIndex + 3]) * fracPart[0] * fracPart[0] + 1 * uintBitsToFloat(octreeData[vIndex + 5]) * fracPart[1] + 2 * uintBitsToFloat(octreeData[vIndex + 6]) * fracPart[0] * fracPart[1] + 3 * uintBitsToFloat(octreeData[vIndex + 7]) * fracPart[0] * fracPart[0] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 9]) * fracPart[1] * fracPart[1] + 2 * uintBitsToFloat(octreeData[vIndex + 10]) * fracPart[0] * fracPart[1] * fracPart[1] + 3 * uintBitsToFloat(octreeData[vIndex + 11]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 13]) * fracPart[1] * fracPart[1] * fracPart[1] + 2 * uintBitsToFloat(octreeData[vIndex + 14]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] + 3 * uintBitsToFloat(octreeData[vIndex + 15]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1]
        + 1 * uintBitsToFloat(octreeData[vIndex + 17]) * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 18]) * fracPart[0] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 19]) * fracPart[0] * fracPart[0] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 21]) * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 22]) * fracPart[0] * fracPart[1] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 23]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 25]) * fracPart[1] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 26]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 27]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 29]) * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 30]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 31]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2]
        + 1 * uintBitsToFloat(octreeData[vIndex + 33]) * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 34]) * fracPart[0] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 35]) * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 37]) * fracPart[1] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 38]) * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 39]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 41]) * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 42]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 43]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 45]) * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 46]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 47]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2]
        + 1 * uintBitsToFloat(octreeData[vIndex + 49]) * fracPart[2] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 50]) * fracPart[0] * fracPart[2] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 51]) * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 53]) * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 54]) * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 55]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 57]) * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 58]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 59]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 61]) * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 62]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 63]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2]),
        (1 * uintBitsToFloat(octreeData[vIndex + 4]) + 1 * uintBitsToFloat(octreeData[vIndex + 5]) * fracPart[0] + 1 * uintBitsToFloat(octreeData[vIndex + 6]) * fracPart[0] * fracPart[0] + 1 * uintBitsToFloat(octreeData[vIndex + 7]) * fracPart[0] * fracPart[0] * fracPart[0] + 2 * uintBitsToFloat(octreeData[vIndex + 8]) * fracPart[1] + 2 * uintBitsToFloat(octreeData[vIndex + 9]) * fracPart[0] * fracPart[1] + 2 * uintBitsToFloat(octreeData[vIndex + 10]) * fracPart[0] * fracPart[0] * fracPart[1] + 2 * uintBitsToFloat(octreeData[vIndex + 11]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] + 3 * uintBitsToFloat(octreeData[vIndex + 12]) * fracPart[1] * fracPart[1] + 3 * uintBitsToFloat(octreeData[vIndex + 13]) * fracPart[0] * fracPart[1] * fracPart[1] + 3 * uintBitsToFloat(octreeData[vIndex + 14]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] + 3 * uintBitsToFloat(octreeData[vIndex + 15]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1]
        + 1 * uintBitsToFloat(octreeData[vIndex + 20]) * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 21]) * fracPart[0] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 22]) * fracPart[0] * fracPart[0] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 23]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 24]) * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 25]) * fracPart[0] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 26]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 27]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 28]) * fracPart[1] * fracPart[1] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 29]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 30]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 31]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2]
        + 1 * uintBitsToFloat(octreeData[vIndex + 36]) * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 37]) * fracPart[0] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 38]) * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 39]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 40]) * fracPart[1] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 41]) * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 42]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 43]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 44]) * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 45]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 46]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 47]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2]
        + 1 * uintBitsToFloat(octreeData[vIndex + 52]) * fracPart[2] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 53]) * fracPart[0] * fracPart[2] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 54]) * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] * fracPart[2] + 1 * uintBitsToFloat(octreeData[vIndex + 55]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 56]) * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 57]) * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 58]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 59]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 60]) * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 61]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 62]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 63]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] * fracPart[2]),
        (1 * uintBitsToFloat(octreeData[vIndex + 16]) + 1 * uintBitsToFloat(octreeData[vIndex + 17]) * fracPart[0] + 1 * uintBitsToFloat(octreeData[vIndex + 18]) * fracPart[0] * fracPart[0] + 1 * uintBitsToFloat(octreeData[vIndex + 19]) * fracPart[0] * fracPart[0] * fracPart[0] + 1 * uintBitsToFloat(octreeData[vIndex + 20]) * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 21]) * fracPart[0] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 22]) * fracPart[0] * fracPart[0] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 23]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 24]) * fracPart[1] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 25]) * fracPart[0] * fracPart[1] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 26]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 27]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 28]) * fracPart[1] * fracPart[1] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 29]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 30]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] + 1 * uintBitsToFloat(octreeData[vIndex + 31]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1]
        + 2 * uintBitsToFloat(octreeData[vIndex + 32]) * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 33]) * fracPart[0] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 34]) * fracPart[0] * fracPart[0] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 35]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 36]) * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 37]) * fracPart[0] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 38]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 39]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 40]) * fracPart[1] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 41]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 42]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 43]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 44]) * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 45]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 46]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] + 2 * uintBitsToFloat(octreeData[vIndex + 47]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2]
        + 3 * uintBitsToFloat(octreeData[vIndex + 48]) * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 49]) * fracPart[0] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 50]) * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 51]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 52]) * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 53]) * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 54]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 55]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 56]) * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 57]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 58]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 59]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 60]) * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 61]) * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 62]) * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2] + 3 * uintBitsToFloat(octreeData[vIndex + 63]) * fracPart[0] * fracPart[0] * fracPart[0] * fracPart[1] * fracPart[1] * fracPart[1] * fracPart[2] * fracPart[2])
    ));
}
#endif

#ifdef USE_TRILINEAR_INTERPOLATION

float getDistance(vec3 point)
{
    vec3 fracPart = point * startGridSize;
    ivec3 arrayPos = ivec3(floor(fracPart));

    if(arrayPos.x < 0 || arrayPos.y < 0 || arrayPos.z < 0 ||
       arrayPos.x >= startGridSize.x || arrayPos.y >= startGridSize.y || arrayPos.z >= startGridSize.z)
    {
            vec3 q = abs(point - vec3(0.5)) - 0.5;
            return length(max(q, vec3(0.0)))/distanceScale + minBorderValue;
    }

    fracPart = fract(fracPart);

    int index = arrayPos.z * int(startGridSize.y * startGridSize.x) +
                arrayPos.y * int(startGridSize.x) +
                arrayPos.x;
    uint currentNode = octreeData[index];

    while(!bool(currentNode & isLeafMask))
    {
        uint childIdx = (roundFloat(fracPart.z) << 2) + 
                        (roundFloat(fracPart.y) << 1) + 
                         roundFloat(fracPart.x);

        currentNode = octreeData[(currentNode & childrenIndexMask) + childIdx];
        fracPart = fract(2.0 * fracPart);
    }

    uint vIndex = currentNode & childrenIndexMask;

    float d00 = uintBitsToFloat(octreeData[vIndex]) * (1.0f - fracPart.x) +
                uintBitsToFloat(octreeData[vIndex + 1]) * fracPart.x;
    float d01 = uintBitsToFloat(octreeData[vIndex + 2]) * (1.0f - fracPart.x) +
                uintBitsToFloat(octreeData[vIndex + 3]) * fracPart.x;
    float d10 = uintBitsToFloat(octreeData[vIndex + 4]) * (1.0f - fracPart.x) +
                uintBitsToFloat(octreeData[vIndex + 5]) * fracPart.x;
    float d11 = uintBitsToFloat(octreeData[vIndex + 6]) * (1.0f - fracPart.x) +
                uintBitsToFloat(octreeData[vIndex + 7]) * fracPart.x;

    float d0 = d00 * (1.0f - fracPart.y) + d01 * fracPart.y;
    float d1 = d10 * (1.0f - fracPart.y) + d11 * fracPart.y;

    return d0 * (1.0f - fracPart.z) + d1 * fracPart.z;
}

vec3 getGradient(vec3 point)
{
    vec3 fracPart = point * startGridSize;
    ivec3 arrayPos = ivec3(floor(fracPart));
    fracPart = fract(fracPart);
    int index = arrayPos.z * int(startGridSize.y * startGridSize.x) +
                arrayPos.y * int(startGridSize.x) +
                arrayPos.x;
    uint currentNode = octreeData[index];

    while(!bool(currentNode & isLeafMask))
    {
        uint childIdx = (roundFloat(fracPart.z) << 2) + 
                        (roundFloat(fracPart.y) << 1) + 
                         roundFloat(fracPart.x);

        currentNode = octreeData[(currentNode & childrenIndexMask) + childIdx];
        fracPart = fract(2.0 * fracPart);
    }

    uint vIndex = currentNode & childrenIndexMask;

    float gx = 0.0;
    {
        float d00 = uintBitsToFloat(octreeData[vIndex + 0]) * (1.0f - fracPart.y) +
                    uintBitsToFloat(octreeData[vIndex + 2]) * fracPart.y;
        float d01 = uintBitsToFloat(octreeData[vIndex + 1]) * (1.0f - fracPart.y) +
                    uintBitsToFloat(octreeData[vIndex + 3]) * fracPart.y;
        float d10 = uintBitsToFloat(octreeData[vIndex + 4]) * (1.0f - fracPart.y) +
                    uintBitsToFloat(octreeData[vIndex + 6]) * fracPart.y;
        float d11 = uintBitsToFloat(octreeData[vIndex + 5]) * (1.0f - fracPart.y) +
                    uintBitsToFloat(octreeData[vIndex + 7]) * fracPart.y;

        float d0 = d00 * (1.0f - fracPart.z) + d10 * fracPart.z;
        float d1 = d01 * (1.0f - fracPart.z) + d11 * fracPart.z;

        gx = d1 - d0;
    }

    float gy = 0.0;
    float gz = 0.0;
    {
        float d00 = uintBitsToFloat(octreeData[vIndex + 0]) * (1.0f - fracPart.x) +
                    uintBitsToFloat(octreeData[vIndex + 1]) * fracPart.x;
        float d01 = uintBitsToFloat(octreeData[vIndex + 2]) * (1.0f - fracPart.x) +
                    uintBitsToFloat(octreeData[vIndex + 3]) * fracPart.x;
        float d10 = uintBitsToFloat(octreeData[vIndex + 4]) * (1.0f - fracPart.x) +
                    uintBitsToFloat(octreeData[vIndex + 5]) * fracPart.x;
        float d11 = uintBitsToFloat(octreeData[vIndex + 6]) * (1.0f - fracPart.x) +
                    uintBitsToFloat(octreeData[vIndex + 7]) * fracPart.x;

        {
            float d0 = d00 * (1.0f - fracPart.z) + d10 * fracPart.z;
            float d1 = d01 * (1.0f - fracPart.z) + d11 * fracPart.z;

            gy = d1 - d0;
        }

        {
            float d0 = d00 * (1.0f - fracPart.y) + d01 * fracPart.y;
            float d1 = d10 * (1.0f - fracPart.y) + d11 * fracPart.y;

            gz = d1 - d0;
        }
    }

    return normalize(vec3(gx, gy, gz));
}
#endif


//SCENE
float map(vec3 pos)
{
    // vec3 aPos = pos + vec3(-0.5, -0.1, -0.5);
    // return min(distanceScale * getDistance(pos), max(length(aPos.xz) - 1.3, abs(aPos.y) - 0.07));
    return distanceScale * getDistance(pos);
}

//Gradient of the scene
vec3 mapGradient(vec3 pos)
{
    // Get the exact gradient
    // return getGradient(pos);

    // Get an approximation of the gradient
    float dist = map(pos);
    return normalize(vec3(
        map(pos + vec3(epsilon, 0, 0)) - dist,
        map(pos + vec3(0, epsilon, 0)) - dist,
        map(pos + vec3(0, 0, epsilon)) - dist
    ));
}

//LIGHTING
float softshadow(vec3 ro, vec3 rd)
{
    float res = 1.0;
    float ph = 1e20;
    float t = 0.009;
    for( int i=0; i < 512 && t < 10.0; i++ )
    {
        float h = map(ro + rd*t);
        if( h < 1e-4 ) return 0.0;
        // float y = h * h / (2.0 * ph);
        // float d = sqrt(h * h - y * y);
        // res = min(res, d / max(0.0,t-y));
        res = min(res, h/t);
        ph = h;
        t += h;
    }
    return res;
}

float softshadowToPoint(vec3 ro, vec3 rd, float far)
{
    float res = 1.0;
    float ph = 1e20;
    float t = 0.005;
    for( int i=0; i < 512 && t < far; i++ )
    {
        float h = map(ro + rd*t);
        if( h < 1e-3 ) return 0.0;
        // float y = h * h / (2.0 * ph);
        // float d = sqrt(h * h - y * y);
        // res = min(res, d / max(0.0,t-y));
        res = min(res, h/t);
        ph = h;
        t += h;
    }
    return res;
}

//Attempt to apply over relaxation to soft shadows too
float softshadowOR(vec3 ro, vec3 rd, float far, float omega)
{
    float res = 1.0;
    float ph = 0.0;
    float t = 0.005;
    float stepLength = 0.0;
    for( int i=0; i < maxShadowIterations && t < far; i++ )
    {
        float h = map(ro + rd * t);
        bool fail = omega > 1.0f && (h + ph) < stepLength;
        if (fail)
        {
            stepLength -= omega * stepLength;
            omega = 1.0f;
        }
        else
        {
            stepLength = h * omega;
        }

        ph = h;

        if(!fail && h < epsilon ) return 0.0;

        if (!fail) res = min(res, h/t);

        t += stepLength;
    }
    return res;
}

//Inigo Quilez improved soft shadow
float softshadow( in vec3 ro, in vec3 rd, float mint, float maxt, float w )
{
    float res = 1.0;
    float t = mint;
    for( int i=0; i<maxShadowIterations && t<maxt; i++ )
    {
        float h = map(ro + t*rd);
        res = min( res, h/(w*t) );
        t += clamp(h, 0.005, 0.50);
        if( res<-1.0 || t>maxt ) break;
    }
    res = max(res,-1.0);
    return 0.25*(1.0+res)*(1.0+res)*(2.0-res);
}

/*
vec3 mapColor(vec3 pos, vec3 N, vec3 V)
{
    //Normal
    // vec3 N = normalize(gridNormal);
    //View vector
    // vec3 V = normalize(cameraPos - pos);

    //Plane vs model
    vec3 aPos = pos + vec3(-0.5, -0.1, -0.5);
    float fd = max(length(aPos.xz) - 1.3, abs(aPos.y) - 0.07);


    // Fresnel parameter
    vec3 F0 = mix(matF0, matAlbedo, matMetallic);

    vec3 Lo = vec3(0.0);

    // Directional lights
    for (int i = 0; i < lightNumber; i++) 
    {
        float distToLight = length(lightPos[i] - pos);
        vec3 L = normalize(lightPos[i] - pos);
        vec3 H = normalize(V + L);

        vec3 sunColor = lightIntensity[i] * lightColor[i];

        float coneAngle = atan(lightRadius[i]/distToLight);
        float solidAngle = PI * sin(coneAngle) * pow((lightRadius[i]/distToLight), 2.0);
        //float intensity = useSoftShadows ? min(atan(softshadowToPoint(pos + epsilon * N, L, distToLight)) / coneAngle, 1.0) : 1.0f;
        //float intensity = useSoftShadows ? min(atan(softshadowOR(pos + epsilon * N, L, distToLight, overRelaxation)) / coneAngle, 1.0) : 1.0f;
        float intensity = useSoftShadows ? softshadow(pos + epsilon * N, L, 0.005, distToLight, solidAngle) : 1.0f;
        vec3 radiance = sunColor * intensity;
        
        // Cook-torrance brdf
        float NDF = DistributionGGX(N, H, matRoughness);        
        float G = GeometrySmith(N, V, L, matRoughness);      
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - matMetallic;	  
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + epsilon;
        vec3 specular = numerator / denominator;  
            
        // Add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * matAlbedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.5) * matAlbedo; // Ambient light estimation
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
   
    return color;
}

void main()
{
    vec3 outColor = vec3(0.9);
    
    vec3 hitPoint = gridPosition;
    outColor = mapColor(hitPoint, cameraPos);

    fragColor = vec4(outColor, 1.0);
}
*/

uint OCTREENODE_LEAF_MASK = 0x80000000;
uint OCTREENODE_CHILDREN_MASK = 0x7FFFFFFF;

struct OctreeNode 
{
    uint value;

    vec3 bboxMin;
    vec3 bboxMax;

    vec4 color;
};

layout(std140, binding = 4) buffer sceneOctree
{
    OctreeNode data[];
};


vec3 getSceneOctreeColor(vec3 point)
{
    uint idx = 0;
    if (point.x < data[idx].bboxMin.x || point.x > data[idx].bboxMax.x ||
        point.y < data[idx].bboxMin.y || point.y > data[idx].bboxMax.y ||
        point.z < data[idx].bboxMin.z || point.z > data[idx].bboxMax.z)
    {
        return vec3(0.0);
    }

    while (!bool(data[idx].value & OCTREENODE_LEAF_MASK))
    {
        uint childIdx = data[idx].value & OCTREENODE_CHILDREN_MASK;

        for (int i = 0; i < 8; ++i) 
        {
            vec3 childBboxMin = data[childIdx + i].bboxMin;
            vec3 childBboxMax = data[childIdx + i].bboxMax;

            if (point.x >= childBboxMin.x && point.x <= childBboxMax.x &&
                point.y >= childBboxMin.y && point.y <= childBboxMax.y &&
                point.z >= childBboxMin.z && point.z <= childBboxMax.z)
            {
                idx = childIdx + i;
                break;
            }
        }
    }

    return data[idx].color.rgb;
}

uint pcg_hash(uint seed)
{
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float randomFloat(uint seed, out uint newSeed)
{
    newSeed = pcg_hash(seed);
    float val = float(newSeed) / float(4294967295u); // divide by max uint32 number
    return val;
}

float randomFloatRange(float min, float max, uint seed, out uint outSeed)
{
    return min + randomFloat(seed, outSeed) * (max - min);
}

vec3 randomCosineWeightedHemispherePoint(vec3 rand, vec3 n) {
    float r = rand.x * 0.5 + 0.5; // [-1..1) -> [0..1)
    float angle = (rand.y + 1.0) * PI; // [-1..1] -> [0..2*PI)
    float sr = sqrt(r);
    vec2 p = vec2(sr * cos(angle), sr * sin(angle));
    /*
     * Unproject disk point up onto hemisphere:
     * 1.0 == sqrt(x*x + y*y + z*z) -> z = sqrt(1.0 - x*x - y*y)
     */
    vec3 ph = vec3(p.xy, sqrt(1.0 - p*p));
    /*
     * Compute some arbitrary tangent space for orienting
     * our hemisphere 'ph' around the normal. We use the camera's up vector
     * to have some fix reference vector over the whole screen.
     */
    vec3 tangent = normalize(rand);
    vec3 bitangent = cross(tangent, n);
    tangent = cross(bitangent, n);
  
    /* Make our hemisphere orient around the normal. */
    return tangent * ph.x + bitangent * ph.y + n * ph.z;
}

vec3 randomSpherePoint(vec3 rand) {
    float ang1 = (rand.x + 1.0) * PI; // [-1..1) -> [0..2*PI)
    float u = rand.y; // [-1..1), cos and acos(2v-1) cancel each other out, so we arrive at [-1..1)
    float u2 = u * u;
    float sqrt1MinusU2 = sqrt(1.0 - u2);
    float x = sqrt1MinusU2 * cos(ang1);
    float y = sqrt1MinusU2 * sin(ang1);
    float z = u;
    return vec3(x, y, z);
}

vec3 randomHemispherePoint(vec3 rand, vec3 n) {
    vec3 v = randomSpherePoint(rand);
    return v * sign(dot(v, n));
}

bool raycast(vec3 startPos, vec3 dir, out vec3 resultPos)
{
    resultPos = startPos;
    float lastDistance = 1e8;
    uint it = 0;
    while (lastDistance > 1e-5 && it < maxRaycastIterations)
    {
        lastDistance = map(resultPos);

        float dist = max(lastDistance, 0.0);
        resultPos += dir * dist;
        it += 1;
    }
    return lastDistance <= 1e-5;
}

vec3 getColor(vec3 pos, vec3 N, vec3 V)
{
    vec3 color = vec3(0.0);
    for (int i = 0; i < lightNumber; ++i)
    {
        vec3 colorLight = lightIntensity[i] * lightColor[i];

        float distToLight = length(lightPos[i] - pos);
        vec3 L = normalize(lightPos[i] - pos);

        float coneAngle = atan(lightRadius[i]/distToLight);
        float solidAngle = PI * sin(coneAngle) * pow((lightRadius[i]/distToLight), 2.0);

        float shadow = useSoftShadows ? softshadow(pos + epsilon * N, L, 0.005, distToLight, solidAngle) : 1.0f;

        vec3 Lo = shadow * colorLight * max(dot(N, L), 0.0);
        color += Lo;
    }

    return color;
}

vec3 sphereSamplingDirectLight(vec3 pos, vec3 N, uint seed, out float solidAngle) 
{
    // TODO: For the moment using only one light

    uint seedLocal = seed;

    float distToLight = distance(lightPos[0], pos);
    vec3 colorLight = lightIntensity[0] * lightColor[0];

    float cosAngle = lightRadius[0] / distToLight;
    solidAngle = 2.0 * PI * (1.0 - cosAngle);

    vec2 rangeX = vec2(lightPos[0].x - lightRadius[0], lightPos[0].x + lightRadius[0]);
    vec2 rangeY = vec2(lightPos[0].y - lightRadius[0], lightPos[0].y + lightRadius[0]);
    vec2 rangeZ = vec2(lightPos[0].z - lightRadius[0], lightPos[0].z + lightRadius[0]);
    
    // float pdf = solidAngle / (4.0 * PI * distToLight * distToLight);

    vec3 color = vec3(0.0);
    for (int i = 0; i < numSamples; ++i)
    {
        vec3 lightPoint = vec3(randomFloatRange(rangeX.x, rangeX.y, seedLocal, seedLocal), 
                               randomFloatRange(rangeY.x, rangeY.y, seedLocal, seedLocal), 
                               randomFloatRange(rangeZ.x, rangeZ.y, seedLocal, seedLocal));
        vec3 lightDir = normalize(lightPoint - pos);

        vec3 hitPosition;
        bool hit = raycast(pos + epsilon * N, lightDir, hitPosition);

        if (!hit)
        {
            color += colorLight * max(dot(N, lightDir), 0.0); // / pdf;
        } 
    }

    return color / float(numSamples);
}

vec3 getDirectLighting(vec3 pos, vec3 N, vec3 V, uint seed, out float solidAngle)
{
    if (useDirectSphereSampling)
    {
        return sphereSamplingDirectLight(pos, N, seed, solidAngle);
    } 
    else
    {
        return getColor(pos, N, V);
    }
}

vec3 getSkyboxColor(vec3 direction)
{
    if (useCubemapSkybox) 
    {
        return texture(cubemapSkybox, direction).rgb;
    } 
    else  
    {
        return skyboxColor;
    }
}

#define indirectLightRec(name, name0)                                              \
vec3 name(vec3 pos, vec3 N, vec3 V, int depth, uint seed)                          \
{                                                                                  \
    float solidAngle;                                                              \
    vec3 directLight = getDirectLighting(pos, N, V, seed, solidAngle);             \
    if (depth == 0) return directLight * (matAlbedo/PI);                           \
                                                                                   \
    vec3 indirectLight = vec3(0.0);                                                \
    for (int i = 0; i < numSamples; ++i)                                           \
    {                                                                              \
        seed += uint(i) + uint(depth);                                             \
                                                                                   \
        float r1 = randomFloatRange(-1, 1, seed, seed);                            \
        float r2 = randomFloatRange(-1, 1, seed, seed);                            \
        float r3 = randomFloatRange(-1, 1, seed, seed);                            \
                                                                                   \
        vec3 direction = randomHemispherePoint(vec3(r1, r2, r3), N);               \
        direction = normalize(direction);                                          \
                                                                                   \
        float pdf = 1.0 / (2.0 * PI);                                              \
                                                                                   \
        vec3 hitPosition;                                                          \
        bool hit = raycast(pos + epsilon * N, direction, hitPosition);             \
                                                                                   \
        if (hit)                                                                   \
        {                                                                          \
            vec3 gradient = mapGradient(hitPosition);                              \
            vec3 NIndirect;                                                        \
            if (dot(direction, gradient) > 0.0) NIndirect = -gradient;             \
            else NIndirect = gradient;                                             \
                                                                                   \
            NIndirect = normalize(NIndirect);                                      \
                                                                                   \
            vec3 VIndirect = -direction;                                           \
                                                                                   \
            vec3 indirectColor =                                                   \
                name0(hitPosition, NIndirect, VIndirect, depth-1, seed)            \
                * max(dot(N, direction), 0.0);                                     \
            indirectLight += indirectColor / pdf;                                  \
        }                                                                          \
        else                                                                       \
        {                                                                          \
            indirectLight += getSkyboxColor(direction)                             \
                * max(dot(N, direction), 0.0) / pdf;                               \
        }                                                                          \
    }                                                                              \
                                                                                   \
    indirectLight /= float(numSamples);                                            \
    vec3 albedo = getSceneOctreeColor(pos);                                        \
                                                                                   \
    return (directLight + indirectLight) * (albedo / PI);                       \
}

vec3 indirectLightDepth5(vec3 pos, vec3 N, vec3 V, int depth, uint seed) { float solidAngle; return getDirectLighting(pos, N, V, seed, solidAngle) * (matAlbedo/PI); }
indirectLightRec(indirectLightDepth4, indirectLightDepth5);
indirectLightRec(indirectLightDepth3, indirectLightDepth4);
indirectLightRec(indirectLightDepth2, indirectLightDepth3);
indirectLightRec(indirectLightDepth1, indirectLightDepth2);
indirectLightRec(indirectLightDepth0, indirectLightDepth1);

void main()
{
    if (simple) {
        return;
    }

    uint seed = pcg_hash(pcg_hash(uint(gl_FragCoord.x)) + pcg_hash(uint(gl_FragCoord.y)) + pcg_hash(uint(time + frameIndex)));

    vec3 N = normalize(gridNormal);
    vec3 V = normalize(cameraPos - gridPosition);

    vec3 color = vec3(0.0);
    if (!useIndirect) 
    {
        float solidAngle;
        color = getDirectLighting(gridPosition, N, V, seed, solidAngle) * (matAlbedo / PI);
    } 
    else
    {
        color = indirectLightDepth0(gridPosition, N, V, maxDepth, seed);
    }

    fragColor = vec4(color, 1.0);
}
