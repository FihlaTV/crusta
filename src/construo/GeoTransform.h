/***********************************************************************
GeoTransform - Class for transformations from system to world
coordinates where system coordinates are already in geographic
coordinates (lat/long). However, system coordinates are typically in
degrees, and world coordinates are in radians. So there's still some
conversion to be done.
Copyright (c) 2007-2008 Oliver Kreylos

This file is part of the DEM processing and visualization package.

The DEM processing and visualization package is free software; you can
redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

The DEM processing and visualization package is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with the DEM processing and visualization package; if not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA
***********************************************************************/

#ifndef _GeoTransform_H_
#define _GeoTransform_H_

#include <construo/ImageTransform.h>

#include <construo/vrui.h>

namespace crusta {

class GeoTransform : public ImageTransform
{
public:
    virtual Point::Scalar getFinestResolution(const int size[2]) const;
	virtual Point systemToWorld(const Point& systemPoint) const;
	virtual Point worldToSystem(const Point& worldPoint) const;
	virtual Box systemToWorld(const Box& systemBox) const;
	virtual Box worldToSystem(const Box& worldBox) const;
	virtual Point imageToWorld(const Point& imagePoint) const;
	virtual Point imageToWorld(int imageX,int imageY) const;
	virtual Point worldToImage(const Point& worldPoint) const;
	virtual Box imageToWorld(const Box& imageBox) const;
	virtual Box worldToImage(const Box& worldBox) const;
	virtual bool isCompatible(const ImageTransform& other) const;
};

} //namespace crusta

#endif //_GeoTransform_H_
