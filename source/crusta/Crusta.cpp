#include <crusta/Crusta.h>

#include <Geometry/OrthogonalTransformation.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <GL/GLContextData.h>
#include <GL/GLTransformationWrappers.h>
#include <Misc/ThrowStdErr.h>
#include <Vrui/Vrui.h>

#include <crusta/checkGl.h>
#include <crusta/DataManager.h>
#include <crusta/map/MapManager.h>
#include <crusta/QuadCache.h>
#include <crusta/QuadTerrain.h>
#include <crusta/Section.h>
#include <crusta/Sphere.h>
#include <crusta/SurfaceTool.h>
#include <crusta/Tool.h>
#include <crusta/Triacontahedron.h>
#include <crusta/Triangle.h>


///todo debug
#include <crusta/CrustaVisualizer.h>
#define CV(x) CrustaVisualizer::x


BEGIN_CRUSTA


#if CRUSTA_ENABLE_DEBUG
int CRUSTA_DEBUG_LEVEL_MIN = 40;
int CRUSTA_DEBUG_LEVEL_MAX = 100;
#endif //CRUSTA_ENABLE_DEBUG

#if DEBUG_INTERSECT_CRAP
///\todo debug remove
bool DEBUG_INTERSECT = false;
#define DEBUG_INTERSECT_PEEK 0
#endif //DEBUG_INTERSECT_CRAP

///\todo OMG this needs to be integrated into the code properly (VIS 2010)
const int   Crusta::lineDataTexSize     = 512;
const float Crusta::lineDataCoordStep   = 1.0f / lineDataTexSize;
const float Crusta::lineDataStartCoord  = 0.5f * lineDataCoordStep;

///\todo OMG this needs to be integrated into the code properly (VIS 2010)
class TargaImage
{
public:
    TargaImage() : byteCount(0), pixels(NULL)
    {
        size[0] = size[1] = 0;
    }
    ~TargaImage()
    {
        destroy();
    }

    bool load(const char* filename)
    {
        FILE* file;
        uint8 type[4];
        uint8 info[6];

        file = fopen(filename, "rb");
        if (file == NULL)
            return false;

        fread (&type, sizeof(char), 3, file);
        fseek (file, 12, SEEK_SET);
        fread (&info, sizeof(char), 6, file);

        //image type either 2 (color) or 3 (greyscale)
        if (type[1]!=0 || (type[2]!=2 && type[2]!=3))
        {
            fclose(file);
            return false;
        }

        size[0]   = info[0] + info[1]*256;
        size[1]   = info[2] + info[3]*256;
        byteCount = info[4] / 8;

        if (byteCount!=3 && byteCount!=4)
        {
            fclose(file);
            return false;
        }

        int imageSize = size[0]*size[1] * byteCount;

        //allocate memory for image data
        pixels = new uint8[imageSize];

        //read in image data
        fread(pixels, sizeof(uint8), imageSize, file);

        //close file
        fclose(file);

        return true;
    }

    void destroy()
    {
        size[0] = size[1] = 0;
        byteCount = 0;
        delete[] pixels;
        pixels = NULL;
    }

    int    size[2];
    int    byteCount;
    uint8* pixels;
};

CrustaGlData::
CrustaGlData()
{
    /* Initialize the required extensions: */
    if(!GLEXTFramebufferObject::isSupported())
    {
        Misc::throwStdErr("LightingShader: GL_EXT_framebuffer_object not"
                          " supported");
    }
    GLEXTFramebufferObject::initExtension();

    QuadTerrain::generateVertexAttributeTemplate(vertexAttributeTemplate);
    QuadTerrain::generateIndexTemplate(indexTemplate);

    //create the framebuffer to be used to attach and render the coverage maps
    glGenFramebuffersEXT(1, &coverageFbo);

    glPushAttrib(GL_TEXTURE_BIT);

    glGenTextures(1, &symbolTex);
    glBindTexture(GL_TEXTURE_2D, symbolTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    TargaImage atlas;
    if (atlas.load("Crusta_MapSymbolAtlas.tga"))
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas.size[0], atlas.size[1], 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, atlas.pixels);

        atlas.destroy();
    }
    else
    {
        float defaultTexel[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                     GL_RGBA, GL_FLOAT, defaultTexel);
    }
    CHECK_GLA

    glPopAttrib();

    //create the shader to process the line coverages into the corresponding map
    static const char* vp = "\
//        uniform mat4 transform;\n\
        void main()\n\
        {\n\
#if 1\n\
            gl_Position = gl_Vertex;\n\
#else\n\
            gl_Position = transform * gl_Vertex;\n\
#endif\n\
        }\n\
        \n";

    static const char* fp = "\
        void main()\n\
        {\n\
            gl_FragColor = vec4(100.0);\n\
        }\n\
        \n";
    lineCoverageShader.compileVertexShaderFromString(vp);
    lineCoverageShader.compileFragmentShaderFromString(fp);
    lineCoverageShader.linkShader();

#if 0
    lineCoverageShader.useProgram();
    lineCoverageTransformUniform = lineCoverageShader.getUniformLocation(
        "transform");
    lineCoverageShader.disablePrograms();
#endif
}

CrustaGlData::
~CrustaGlData()
{
    glDeleteBuffers(1, &vertexAttributeTemplate);
    glDeleteBuffers(1, &indexTemplate);
}



void Crusta::
init(const std::string& demFileBase, const std::string& colorFileBase)
{
    //initialize the surface transformation tool
    SurfaceTool::init();
    //initialize the abstract crusta tool (adds an entry to the VRUI menu)
    Vrui::ToolFactory* crustaTool = Tool::init(NULL);

    /* start the frame counting at 2 because initialization code uses unsigneds
    that are initialized with 0. Thus if crustaFrameNumber starts at 0, the
    init code wouldn't be able to retrieve any cache buffers since all the
    buffers of the current and previous frame are locked */
    currentFrame      = 2;
    lastScaleFrame    = 2;
    isTexturedTerrain = true;
    verticalScale     = 0.0;
    newVerticalScale  = 1.0;

    Triacontahedron polyhedron(SPHEROID_RADIUS);

    cache    = new Cache(4096, 1024, 1024, this);
    dataMan  = new DataManager(&polyhedron, demFileBase, colorFileBase, this);
    mapMan   = new MapManager(crustaTool, this);

    globalElevationRange[0] =  Math::Constants<Scalar>::max;
    globalElevationRange[1] = -Math::Constants<Scalar>::max;

    uint numPatches = polyhedron.getNumPatches();
    renderPatches.resize(numPatches);
    for (uint i=0; i<numPatches; ++i)
    {
        renderPatches[i] = new QuadTerrain(i, polyhedron.getScope(i), this);
        const QuadNodeMainData& root = renderPatches[i]->getRootNode();
        globalElevationRange[0] = std::min(globalElevationRange[0],
                                           Scalar(root.elevationRange[0]));
        globalElevationRange[1] = std::max(globalElevationRange[1],
                                           Scalar(root.elevationRange[1]));
    }
}

void Crusta::
shutdown()
{
    delete mapMan;
    for (RenderPatches::iterator it=renderPatches.begin();
         it!=renderPatches.end(); ++it)
    {
        delete *it;
    }
    delete dataMan;
    delete cache;
}

Point3 Crusta::
snapToSurface(const Point3& pos, Scalar elevationOffset)
{
//- find the base patch
    MainCacheBuffer*  nodeBuf = NULL;
    QuadNodeMainData* node    = NULL;
    for (int patch=0; patch<static_cast<int>(renderPatches.size()); ++patch)
    {
        //grab specification of the patch
        nodeBuf = cache->getMainCache().findCached(TreeIndex(patch));
        assert(nodeBuf != NULL);
        node = &nodeBuf->getData();

        //check containment
        if (node->scope.contains(pos))
            break;
        else
            node = NULL;
    }

    assert(node != NULL);
    assert(node->index.patch < static_cast<uint>(renderPatches.size()));

//- grab the finest level data possible
    const Vector3 vpos(pos);
    while (true)
    {
        //figure out the appropriate child by comparing against the mid-planes
        Point3 mid1, mid2;
        mid1 = Geometry::mid(node->scope.corners[0], node->scope.corners[1]);
        mid2 = Geometry::mid(node->scope.corners[2], node->scope.corners[3]);
        Vector3 vertical = Geometry::cross(Vector3(mid1), Vector3(mid2));
        vertical.normalize();

        mid1 = Geometry::mid(node->scope.corners[1], node->scope.corners[3]);
        mid2 = Geometry::mid(node->scope.corners[0], node->scope.corners[2]);
        Vector3 horizontal = Geometry::cross(Vector3(mid1), Vector3(mid2));
        horizontal.normalize();

        int childId = vpos*vertical   < 0 ? 0x1 : 0x0;
        childId    |= vpos*horizontal < 0 ? 0x2 : 0x0;

        //is it even possible to retrieve higher res data?
        if (node->childDemTiles[childId]   ==   DemFile::INVALID_TILEINDEX &&
            node->childColorTiles[childId] == ColorFile::INVALID_TILEINDEX)
        {
            break;
        }

        //try to grab the child for evaluation
        MainCache& mainCache      = cache->getMainCache();
        TreeIndex childIndex      = node->index.down(childId);
        MainCacheBuffer* childBuf = mainCache.findCached(childIndex);
        if (childBuf == NULL)
        {
///\todo Vis2010 simplify. Don't allow loads of nodes from here
//            mainCache.request(MainCache::Request(0.0, nodeBuf, childId));
            break;
        }
        else
        {
            //switch to the child for traversal continuation
            nodeBuf = childBuf;
            node    = &childBuf->getData();
        }
    }

//- locate the cell of the refinement containing the point
    int numLevels = 1;
    int res = TILE_RESOLUTION-1;
    while (res > 1)
    {
        ++numLevels;
        res >>= 1;
    }

///\todo optimize the containement checks as above by using vert/horiz split
    Scope scope   = node->scope;
    int offset[2] = { 0, 0 };
    int shift     = (TILE_RESOLUTION-1) >> 1;
    for (int level=1; level<numLevels; ++level)
    {
        //compute the coverage for the children
        Scope childScopes[4];
        scope.split(childScopes);

        //find the child containing the point
        for (int i=0; i<4; ++i)
        {
            if (childScopes[i].contains(pos))
            {
                //adjust the current bottom-left offset and switch to that scope
                offset[0] += i&0x1 ? shift : 0;
                offset[1] += i&0x2 ? shift : 0;
                shift    >>= 1;
                scope      = childScopes[i];
                break;
            }
        }
    }

//- sample the cell
    static const int tileRes = TILE_RESOLUTION;
    int linearOffset = offset[1]*tileRes + offset[0];
    QuadNodeMainData::Vertex* cellV = node->geometry + linearOffset;
    DemHeight*                cellH = node->height   + linearOffset;
    const QuadNodeMainData::Vertex::Position* positions[4] = {
        &(cellV->position), &((cellV+1)->position),
        &((cellV+tileRes)->position), &((cellV+tileRes+1)->position) };
    const DemHeight* heights[4] = {
        cellH, cellH+1, cellH+tileRes, cellH+tileRes+1
    };
    //construct the corners of the current cell
    Vector3 cellCorners[4];
    for (int i=0; i<4; ++i)
    {
        for (int j=0; j<3; ++j)
            cellCorners[i][j] = (*(positions[i]))[j] + node->centroid[j];
        Vector3 extrude(cellCorners[i]);
        extrude.normalize();
        extrude *= *(heights[i]);
        cellCorners[i] += extrude;
    }

    //intersect triangles of current cell
    Triangle t0(cellCorners[0], cellCorners[3], cellCorners[2]);
    Triangle t1(cellCorners[0], cellCorners[1], cellCorners[3]);

    Ray ray(pos, -Vector3(pos));
    HitResult hit = t0.intersectRay(ray);
    if (!hit.isValid())
    {
        hit = t1.intersectRay(ray);
        if (!hit.isValid())
        {
            Scalar height = node->height[offset[1]*TILE_RESOLUTION + offset[0]];
            height       += SPHEROID_RADIUS + elevationOffset;

            Vector3 toPos = Vector3(pos);
            toPos.normalize();
            toPos *= height;
            return Point3(toPos[0], toPos[1], toPos[2]);
        }
    }

    return ray(hit.getParameter());
}

HitResult Crusta::
intersect(const Ray& ray) const
{
#if DEBUG_INTERSECT_CRAP
if (DEBUG_INTERSECT) {
CrustaVisualizer::clearAll();
CrustaVisualizer::addRay(ray, 0);
#if DEBUG_INTERSECT_PEEK
CrustaVisualizer::peek();
#endif //DEBUG_INTERSECT_PEEK
//CrustaVisualizer::show("Ray");
} //DEBUG_INTERSECT
#endif //DEBUG_INTERSECT_CRAP

    const Scalar& verticalScale = getVerticalScale();

    //intersect the ray with the global outer shells to determine starting point
    Sphere shell(Point3(0), SPHEROID_RADIUS +
                 verticalScale*globalElevationRange[1]);
    Scalar gin, gout;
    bool intersects = shell.intersectRay(ray, gin, gout);
    gin = std::max(gin, 0.0);

    if (!intersects)
        return HitResult();

    shell.setRadius(SPHEROID_RADIUS + verticalScale*globalElevationRange[0]);
    HitResult hit = shell.intersectRay(ray);
    if (hit.isValid())
        gout = hit.getParameter();

#if DEBUG_INTERSECT_CRAP
if (DEBUG_INTERSECT) {
CrustaVisualizer::addHit(ray, HitResult(gin), 8);
#if DEBUG_INTERSECT_PEEK
CrustaVisualizer::peek();
#endif //DEBUG_INTERSECT_PEEK
} //DEBUG_INTERSECT
#endif //DEBUG_INTERSECT_CRAP

    //find the patch containing the entry point
    Point3 entry = ray(gin);
    const QuadTerrain*      patch = NULL;
    const QuadNodeMainData* node  = NULL;
    for (RenderPatches::const_iterator it=renderPatches.begin();
         it!=renderPatches.end(); ++it)
    {
        node = &((*it)->getRootNode());
        if (node->scope.contains(entry))
        {
            patch = *it;
            break;
        }
    }

    assert(patch!=NULL && node!=NULL);

#if DEBUG_INTERSECT_CRAP
if (DEBUG_INTERSECT) {
{
Point3s verts;
verts.resize(2);
verts[0] = ray(gin);
verts[1] = ray(gout);
CrustaVisualizer::addPrimitive(GL_POINTS, verts, 9, Color(0.2, 0.1, 0.9, 1.0));
#if DEBUG_INTERSECT_PEEK
CrustaVisualizer::peek();
#endif //DEBUG_INTERSECT_PEEK
}
} //DEBUG_INTERSECT
#endif //DEBUG_INTERSECT_CRAP

    //traverse terrain patches until intersection or ray exit
    Scalar tin           = gin;
    Scalar tout          = 0;
    int    sideIn        = -1;
    int    sideOut       = -1;
    int    mapSide[4][4] = {{2,3,0,1}, {1,2,3,0}, {0,1,2,3}, {3,0,1,2}};
    Triacontahedron polyhedron(SPHEROID_RADIUS);
#if DEBUG_INTERSECT_CRAP
int patchesVisited = 0;
#endif //DEBUG_INTERSECT_CRAP
    while (true)
    {
        hit = patch->intersect(ray, tin, sideIn, tout, sideOut, gout);
        if (hit.isValid())
            break;

        //move to the patch on the exit side
        tin = tout;
        if (tin > gout)
            break;

#if DEBUG_INTERSECT_CRAP
const QuadTerrain* oldPatch = patch;
#endif //DEBUG_INTERSECT_CRAP

        Polyhedron::Connectivity neighbors[4];
        polyhedron.getConnectivity(patch->getRootNode().index.patch, neighbors);
        patch  = renderPatches[neighbors[sideOut][0]];
        sideIn = mapSide[neighbors[sideOut][1]][sideOut];

#if DEBUG_INTERSECT_CRAP
Scalar E = 0.00001;
int sides[4][2] = {{3,2}, {2,0}, {0,1}, {1,3}};
const Scope& oldS = oldPatch->getRootNode().scope;
const Scope& newS = patch->getRootNode().scope;
assert(Geometry::dist(oldS.corners[sides[sideOut][0]], newS.corners[sides[sideIn][1]])<E);
assert(Geometry::dist(oldS.corners[sides[sideOut][1]], newS.corners[sides[sideIn][0]])<E);
std::cerr << "visited: " << ++patchesVisited << std::endl;
#endif //DEBUG_INTERSECT_CRAP
    }

    return hit;
}


void Crusta::
intersect(Shape::ControlPointHandle start,
          Shape::IntersectionFunctor& callback) const
{
    //find the patch containing the entry point
    const Point3&           entry = start->pos;
    const QuadTerrain*      patch = NULL;
    const QuadNodeMainData* node  = NULL;
    for (RenderPatches::const_iterator it=renderPatches.begin();
         it!=renderPatches.end(); ++it)
    {
        node = &((*it)->getRootNode());
        if (node->scope.contains(entry))
        {
            patch = *it;
            break;
        }
    }

    assert(patch!=NULL && node!=NULL);

    //traverse terrain patches until intersection or ray exit
    Ray ray(start->pos, (++Shape::ControlPointHandle(start))->pos);

    Scalar tin           = 0;
    Scalar tout          = 0;
    int    sideIn        = -1;
    int    sideOut       = -1;
    int    mapSide[4][4] = {{2,3,0,1}, {1,2,3,0}, {0,1,2,3}, {3,0,1,2}};
    Triacontahedron polyhedron(SPHEROID_RADIUS);
    while (true)
    {
        patch->intersect(callback, ray, tin, sideIn, tout, sideOut);
        if (tout >= 1.0)
            break;

        //move to the patch on the exit side
        tin = tout;

        Polyhedron::Connectivity neighbors[4];
        polyhedron.getConnectivity(patch->getRootNode().index.patch, neighbors);
        patch  = renderPatches[neighbors[sideOut][0]];
        sideIn = mapSide[neighbors[sideOut][1]][sideOut];
    }
}


const FrameNumber& Crusta::
getCurrentFrame() const
{
    return currentFrame;
}

const FrameNumber& Crusta::
getLastScaleFrame() const
{
    return lastScaleFrame;
}

void Crusta::
useTexturedTerrain(bool useTex)
{
    isTexturedTerrain = useTex;
}

void Crusta::
setVerticalScale(double nVerticalScale)
{
    newVerticalScale = nVerticalScale;
}

double Crusta::
getVerticalScale() const
{
    return verticalScale;
}

Point3 Crusta::
mapToScaledGlobe(const Point3& pos)
{
    Vector3 toPoint(pos[0], pos[1], pos[2]);
    Vector3 onSurface(toPoint);
    onSurface.normalize();
    onSurface *= SPHEROID_RADIUS;
    toPoint   -= onSurface;
    toPoint   *= verticalScale;
    toPoint   += onSurface;

    return Point3(toPoint[0], toPoint[1], toPoint[2]);
}

Point3 Crusta::
mapToUnscaledGlobe(const Point3& pos)
{
    Vector3 toPoint(pos[0], pos[1], pos[2]);
    Vector3 onSurface(toPoint);
    onSurface.normalize();
    onSurface *= SPHEROID_RADIUS;
    toPoint   -= onSurface;
    toPoint   /= verticalScale;
    toPoint   += onSurface;

    return Point3(toPoint[0], toPoint[1], toPoint[2]);
}

Cache* Crusta::
getCache() const
{
    return cache;
}

DataManager* Crusta::
getDataManager() const
{
    return dataMan;
}

MapManager* Crusta::
getMapManager() const
{
    return mapMan;
}


void Crusta::
submitActives(const Actives& touched)
{
    Threads::Mutex::Lock lock(activesMutex);
    actives.insert(actives.end(), touched.begin(), touched.end());
}


void Crusta::
frame()
{
    ++currentFrame;
CRUSTA_DEBUG_OUT(8, "\n\n\n--------------------------------------\n%u\n\n\n",
static_cast<unsigned int>(currentFrame));

    //check for scale changes since the last frame
    if (verticalScale  != newVerticalScale)
    {
        verticalScale  = newVerticalScale;
        assert(currentFrame>0);
        lastScaleFrame = currentFrame-1;
    }

    //make sure all the active nodes are current
    confirmActives();

    //process the requests from the last frame
    cache->getMainCache().frame();

    //let the map manager update all the mapping stuff
    mapMan->frame();
}

void Crusta::
display(GLContextData& contextData)
{
    CHECK_GLA

    CrustaGlData* glData = contextData.retrieveDataItem<CrustaGlData>(this);
    glData->videoCache = &getCache()->getVideoCache(contextData);
    glData->lineCache  = &getCache()->getGpuLineCache(contextData);

//- prepare the renderable representation
    std::vector<QuadNodeMainData*> renderNodes;

    //generate the terrain representation
    for (RenderPatches::const_iterator it=renderPatches.begin();
         it!=renderPatches.end(); ++it)
    {
        (*it)->prepareDisplay(contextData, renderNodes);
        CHECK_GLA
    }

///\todo remove
//std::cerr << "Num render nodes: " << renderNodes.size() << std::endl;

    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);
    glPushAttrib(GL_TEXTURE_BIT);

    //update the map data
///\todo integrate properly (VIS 2010)
    mapMan->updateLineData(renderNodes);

//- draw the current terrain and map data
///\todo integrate properly (VIS 2010)
//bind the texture that contains the symbol images
glActiveTexture(GL_TEXTURE5);
glBindTexture(GL_TEXTURE_2D, glData->symbolTex);

    //have the QuadTerrain draw the surface approximation
    CHECK_GLA

    //draw the terrain
    glData->terrainShader.useTextureForColor(isTexturedTerrain);
    glData->terrainShader.update();
    glData->terrainShader.enable();
    glData->terrainShader.setTextureStep(TILE_TEXTURE_COORD_STEP);
    glData->terrainShader.setVerticalScale(getVerticalScale());

///\todo this needs to be tweakable
    float scaleFac = Vrui::getNavigationTransformation().getScaling();
    glData->terrainShader.setLineCoordScale(scaleFac);
    float lineWidth = 0.1f / scaleFac;
    glData->terrainShader.setLineWidth(lineWidth);

    QuadTerrain::display(contextData, glData, renderNodes, getCurrentFrame());

    glData->terrainShader.disable();

    //let the map manager draw all the mapping stuff
//    mapMan->display(contextData);
    CHECK_GLA

    glPopAttrib();
    glActiveTexture(activeTexture);
}


void Crusta::
confirmActives()
{
    MainCache& mainCache = getCache()->getMainCache();
    for (Actives::iterator it=actives.begin(); it!=actives.end(); ++it)
    {
        if (!mainCache.isCurrent(*it))
            (*it)->getData().computeBoundingSphere(getVerticalScale());
    }
    actives.clear();
}


void Crusta::
initContext(GLContextData& contextData) const
{
    CrustaGlData* glData   = new CrustaGlData();
    contextData.addDataItem(this, glData);
}



void Crusta::
confirmLineCoverageRemoval(Shape* shape, Shape::ControlPointHandle cp)
{
    for (RenderPatches::const_iterator it=renderPatches.begin();
         it!=renderPatches.end(); ++it)
    {
        const QuadNodeMainData* node = &((*it)->getRootNode());
        (*it)->confirmLineCoverageRemoval(node, shape, cp);
    }
}

void Crusta::
validateLineCoverage()
{
    for (RenderPatches::const_iterator it=renderPatches.begin();
         it!=renderPatches.end(); ++it)
    {
        const QuadNodeMainData* node = &((*it)->getRootNode());
        (*it)->validateLineCoverage(node);
    }
}


END_CRUSTA
