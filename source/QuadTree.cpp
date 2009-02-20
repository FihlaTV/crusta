#include <QuadTree.h>

#include <GL/GLGeometryWrappers.h>

#include <LodEvaluator.h>
#include <VisibilityEvaluator.h>

//- helpers
namespace {

template <class PointParam>
inline
PointParam
toSphere(const PointParam& p)
{
    typedef typename PointParam::Scalar Scalar;

    double len = Geometry::mag(p);
    return PointParam(Scalar(p[0]/len), Scalar(p[1]/len), Scalar(p[2]/len));
}

template <class PointParam>
inline
PointParam
centroid(const PointParam& p0, const PointParam& p1, const PointParam& p2)
{
    typedef typename PointParam::Scalar Scalar;

    return PointParam( (p0[0] + p1[0] + p2[0]) / Scalar(3),
                       (p0[1] + p1[1] + p2[1]) / Scalar(3),
                       (p0[2] + p1[2] + p2[2]) / Scalar(3) );
}

template <class PointParam>
inline
PointParam
centroid(const PointParam& p0, const PointParam& p1,
         const PointParam& p2, const PointParam& p3)
{
    typedef typename PointParam::Scalar Scalar;

    return PointParam( (p0[0] + p1[0] + p2[0] + p3[0]) / Scalar(4),
                       (p0[1] + p1[1] + p2[1] + p3[1]) / Scalar(4),
                       (p0[2] + p1[2] + p2[2] + p3[2]) / Scalar(4) );
}

}


BEGIN_CRUSTA

void QuadTree::
setSplitOnSphere(const bool onSphere)
{
    splitOnSphere = onSphere;
}

QuadTree::
QuadTree(const Scope& scope) :
    splitOnSphere(true)
{
    root = new Node;
    root->scope = scope;
}

QuadTree::
~QuadTree()
{
    deleteSubTree(root);
    delete root;
}

QuadTree::Node::
Node() :
    parent(NULL), children(NULL)
{}


QuadTree::Node* QuadTree::
getNeighbor(const Neighbor neighbor)
{
///\todo implement
    return NULL;
}

void QuadTree::
deleteSubTree(Node* base)
{
    if (base->children != NULL)
    {
        for (uint i=0; i<4; ++i)
            deleteSubTree(&base->children[i]);

        delete[] base->children;
        base->children = NULL;
    }
}

void QuadTree::
discardSubTree(Node* base)
{
    if (base->children != NULL)
    {
        //recurse down to the bottom of the tree first
        for (uint i=0; i<4; ++i)
            discardSubTree(&base->children[i]);

        //on our way back up the tree, discard our children if they aren't
        //subtrees and if they don't link to data that is still available.
        bool deleteChildren = true;
        for (uint i=0; i<4; ++i)
        {
            Node* child = &base->children[i];
            if (child->children!=NULL)
            {
                deleteChildren = false;
                break;
            }
            uint numData = static_cast<uint>(child->data.size());
            for (uint j=0; j<numData; ++j)
            {
                if (child->data[j] != NULL)
                {
                    deleteChildren = false;
                    break;
                }
            }
            if (!deleteChildren)
                break;
        }

        if (deleteChildren)
        {
            delete[] base->children;
            base->children = NULL;
        }
    }
}

void QuadTree::
addDataSlots(Node* node, uint numDataSlots)
{
    //add the new slot to the current node
    uint newSize = static_cast<uint>(node->data.size()) + numDataSlots;
    node->data.resize(newSize, NULL);
    //add the new slot to the children
    if (node->children != NULL)
    {
        for (uint i=0; i<4; ++i)
            addDataSlots(&node->children[i], numDataSlots);
    }
}

void QuadTree::
refine(Node* node, VisibilityEvaluator& visibility, LodEvaluator& lod)
{
//- evaluate node
    node->visible = visibility.evaluate(node->scope);
    if (!node->visible)
    {
        node->leaf = true;
        discardSubTree(node);
        return;
    }

    node->lod = lod.evaluate(node->scope);

    if (node->lod > 1.0)
    {
        node->leaf = false;

        if (node->children == NULL)
            split(node);

        for (uint i=0; i<4; ++i)
            refine(&node->children[i], visibility, lod);
    }
    else
    {
        node->leaf = true;
        discardSubTree(node);
    }
}

void QuadTree::
split(Node* node)
{
    node->children = new Node[4];
    uint numDataSlots = static_cast<uint>(node->data.size());
    for (uint i=0; i<4; ++i)
    {
        node->children[i].data.resize(numDataSlots);
        uint level = node->index.level;
        node->children[i].index = Node::TreeIndex(i, level+1, i << level);
    }

    const Point* corners = node->scope.corners;

    Point mids[4];
    uint midIndices[8] = {
        Scope::UPPER_LEFT,  Scope::LOWER_LEFT,
        Scope::LOWER_LEFT,  Scope::LOWER_RIGHT,
        Scope::LOWER_RIGHT, Scope::UPPER_RIGHT,
        Scope::UPPER_LEFT,  Scope::UPPER_RIGHT
    };
    for (uint i=0, j=0; i<4; ++i)
    {
        const Point& c1 = corners[midIndices[j++]];
        const Point& c2 = corners[midIndices[j++]];
        mids[i] = Geometry::mid(c1, c2);
        mids[i] = splitOnSphere ? toSphere(mids[i]) : mids[i];
    }

    Point center = centroid(corners[0], corners[1], corners[2], corners[3]);
    center = splitOnSphere ? toSphere(center) : center;

    const Point* newCorners[16] = {
        &corners[Scope::UPPER_LEFT], &mids[0], &center, &mids[3],
        &mids[3], &center, &mids[2], &corners[Scope::UPPER_RIGHT],
        &mids[0], &corners[Scope::LOWER_LEFT], &mids[1], &center,
        &center, &mids[1], &corners[Scope::LOWER_RIGHT], &mids[2]
    };
    for (uint i=0, j=0; i<4; ++i)
    {
        for (uint k=0; k<4; ++k, ++j)
                node->children[i].scope.corners[k] = (*newCorners[j]);
    }

    for (uint i=0; i<4; ++i)
        node->children[i].parent = node;
}

void QuadTree::
traverseLeaves(Node* node, gridProcessing::ScopeCallback& callback,
               GLContextData& contextData)
{
    if (!node->leaf)
    {
        for (uint i=0; i<4; ++i)
            traverseLeaves(&(node->children[i]), callback, contextData);
    }
    else
    {
        //construct the scope data
        gridProcessing::ScopeData scopeData;
        scopeData.scope = &(node->scope);
        scopeData.data  = &(node->data);

        callback.traversal(scopeData, contextData);
    }
}

void QuadTree::
draw()
{
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POLYGON_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glLineWidth(1.0f);
    glColor3f(1.0f,1.0f,1.0f);

    drawTree(root);
    glPopAttrib();
}

void QuadTree::
drawTree(Node* node)
{
    if (!node->leaf)
    {
        for (uint i=0; i<4; ++i)
            drawTree(&(node->children[i]));
    }
    else
    {
        if (node->visible)
        {
            glBegin(GL_QUADS);
            for(int i=0; i<4; ++i)
            {
                glNormal(node->scope.corners[i] - Point::origin);
                glVertex(node->scope.corners[i]);
            }
            glEnd();
        }
    }
}


void QuadTree::
addDataSlots(uint numSlots)
{
    addDataSlots(root, numSlots);
}

void QuadTree::
refine(VisibilityEvaluator& visibility, LodEvaluator& lod)
{
    refine(root, visibility, lod);
}

void QuadTree::
traverseLeaves(gridProcessing::ScopeCallbacks& callbacks,
               GLContextData& contextData)
{
    uint numCallbacks = static_cast<uint>(callbacks.size());
    for (uint i=0; i<numCallbacks; ++i)
    {
        callbacks[i].preTraversal(contextData);
        traverseLeaves(root, callbacks[i], contextData);
        callbacks[i].postTraversal(contextData);
    }
}

END_CRUSTA
