#include "ProjectData.h"
VolumeGeometrySt VolumeData::geometry() const {
    VolumeGeometrySt g;
    if (!image)
        return g;
    for (unsigned i = 0; i < 3; ++i) {
        g.origin[i] = image->GetOrigin()[i];
        g.spacing[i] = image->GetSpacing()[i];
        g.dimensions[i] = image->GetLargestPossibleRegion().GetSize()[i];
        for (unsigned j = 0; j < 3; ++j)
            g.direction[i * 3 + j] = image->GetDirection()[i][j];
    }
    return g;
}
