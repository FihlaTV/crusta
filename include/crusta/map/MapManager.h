#ifndef _MapManager_H_
#define _MapManager_H_

#include <GL/gl.h>

#include <crusta/basics.h>


class GLContextData;

namespace Vrui {
    class ToolFactory;
}


BEGIN_CRUSTA


class Polyline;
class PolylineRenderer;


class MapManager
{
public:
    typedef std::vector<Polyline*> PolylinePtrs;

    MapManager(Vrui::ToolFactory* parentToolFactory);
    ~MapManager();

    /** Destroy all the current map features */
    void deleteAllShapes();

    /** Load a new mapping dataset */
    void load(const char* filename);
    /** Save the current mapping dataset */
    void save(const char* filename, const char* format);

    Scalar getSelectDistance() const;
    Scalar getPointSelectionBias() const;

    void addPolyline(Polyline* line);
    PolylinePtrs& getPolylines();
    void removePolyline(Polyline* line);

    void frame();
    void display(GLContextData& contextData) const;

protected:
    Scalar selectDistance;
    Scalar pointSelectionBias;

    PolylinePtrs      polylines;
    PolylineRenderer* polylineRenderer;
};


END_CRUSTA


#endif //_MapManager_H_
