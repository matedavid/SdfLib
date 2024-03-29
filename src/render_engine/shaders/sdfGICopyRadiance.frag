in vec3 gridPosition;
in mat4 fragInverseWorldToStartGridMatrix;

uniform bool reset;

uint OCTREENODE_LEAF_MASK = 0x80000000;
uint OCTREENODE_CHILDREN_MASK = 0x7FFFFFFF;

struct OctreeNode 
{
    // 32 bits
    // - bit 31:   isLeaf
    // - bit 30-0: children idx
    uint data;

    vec3 bboxMin;
    vec3 bboxMax;

    vec4 color;
    // roughness, metallic, -, -
    vec4 material;

    // radiance caching
    vec4 readRadiance;
    vec4 writeRadiance;
};

layout(std140, binding = 4) buffer SceneOctree 
{
    OctreeNode sceneData[];
};

int getSceneOctreeColor(vec3 gridPoint)
{
    vec3 point = (fragInverseWorldToStartGridMatrix * vec4(gridPoint, 1.0)).xyz;

    uint idx = 0;
    if (point.x < sceneData[idx].bboxMin.x || point.x > sceneData[idx].bboxMax.x ||
        point.y < sceneData[idx].bboxMin.y || point.y > sceneData[idx].bboxMax.y ||
        point.z < sceneData[idx].bboxMin.z || point.z > sceneData[idx].bboxMax.z)
    {
        return -1; 
    }

    while (!bool(sceneData[idx].data & OCTREENODE_LEAF_MASK))
    {
        uint prevIdx = idx;
        OctreeNode node = sceneData[idx];

        uint firstChildIdx = node.data & OCTREENODE_CHILDREN_MASK;

        for (int i = 0; i < 8; ++i) 
        {
            uint childIdx = firstChildIdx + i;

            vec3 childBboxMin = sceneData[childIdx].bboxMin;
            vec3 childBboxMax = sceneData[childIdx].bboxMax;

            if (point.x >= childBboxMin.x && point.x <= childBboxMax.x &&
                point.y >= childBboxMin.y && point.y <= childBboxMax.y &&
                point.z >= childBboxMin.z && point.z <= childBboxMax.z)
            {
                idx = childIdx;
                break;
            }
        }

        if (prevIdx == idx)
        {
            // Should not happen
            return -1;
        }
    }

    return int(idx);
}

void main() 
{
    int idx = getSceneOctreeColor(gridPosition);
    sceneData[idx].readRadiance = sceneData[idx].writeRadiance;

    if (reset)
    {
        sceneData[idx].readRadiance  = vec4(0.0);
        sceneData[idx].writeRadiance = vec4(0.0);
    }
}