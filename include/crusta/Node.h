#ifndef _Node_H_
#define _Node_H_

#include <vector>

#include <crusta/QuadtreeFileSpecs.h>
#include <crusta/QuadNodeData.h>
#include <crusta/TreeIndex.h>
#include <crusta/Scope.h>

/**\todo remove fix preprocessor to eliminate outliers that break average height
centroid */
#define USING_AVERAGE_HEIGHT 0

BEGIN_CRUSTA

class QuadTerrain;

/** stores aspects of the terrain required for approximation management */
class Node
{
public:
    typedef std::vector<Node*> ChildBlocks;

    Node();

    /** compute the various "cached values" (e.g. bounding sphere, centroid,
        etc.) */
    void init(const DemHeight range[2]);

    /** pointer to the tree containing the node (to access shared data) */
    QuadTerrain* terrain;
    
    /** uniquely characterize this node's "position" in the tree. The tree
     index must correlate with the global hierarchy of the data
     sources */
    TreeIndex index;
    /** caches this node's scope for visibility and lod evaluation */
    Scope scope;

///\todo remove use centroid once average height works
    Scope::Vertex boundingCenter;
    /** radius of a sphere containing the node */
    Scope::Scalar boundingRadius;
    /** centroid of the node geometry on the average elevation shell */
    DemHeight centroid[3];
    /** the range of the elevation values */
    DemHeight elevationRange[2];
    /** index of the DEM tile in the database */
    DemFile::TileIndex demTile;
    /** index of the DEM tiles of the children. This is to avoid having to
     read the indices from file everytime a child needs to be fetched */
    DemFile::TileIndex childDemTiles[4];
    /** index of the color texture tile in the database */
    ColorFile::TileIndex colorTile;
    /** index of the color tiles of the children. This is to avoid having to
        read the indices from file everytime a child needs to be fetched */
    ColorFile::TileIndex childColorTiles[4];
    
    /** cache buffer containing the data related to this node */
    MainCacheBuffer* mainBuffer;
    /** cache buffer containing the GL data related to this node (can be
     stored here, since an ActiveNode exists only in a GL context) */
    VideoCacheBuffer* videoBuffer;
    
    /** pointer to the parent node in the tree */
    Node* parent;
    /** pointer to the children nodes in the tree */
    Node* children;
};

END_CRUSTA

#endif //_Node_H_
